// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
// All rights reserved.

#include "baseboard.hpp"
#include "cpu.hpp"
#include "dimm.hpp"
#include "firmware_inventory.hpp"
#include "pcieslot.hpp"
#include "smbios_mdrv2.hpp"
#include "system.hpp"
#include "test_mock_helpers.hpp"
#include "tpm.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>
#include <sdbusplus/timer.hpp>
#include <xyz/openbmc_project/Smbios/MDR_V2/error.hpp>
#include <xyz/openbmc_project/Smbios/MDR_V2/server.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "mdrv2.hpp"
#undef private
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <atomic>
#include <future>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
constexpr auto signalDrainWait = std::chrono::milliseconds{20};
constexpr auto retryTestSlack = std::chrono::milliseconds{250};
constexpr auto retryTestWait =
    phosphor::smbios::MDRV2::inventoryAnchorRetryInterval + retryTestSlack;
#ifdef CUSTOM_DBUS_PATH
constexpr auto anchorPath =
    "/xyz/openbmc_project/inventory/system/chassis/Chassis_0";
constexpr auto laterInventoryPath =
    "/xyz/openbmc_project/inventory/system/chassis/GPU_0";
#else
constexpr auto anchorPath =
    "/xyz/openbmc_project/inventory/system/board/motherboard";
constexpr auto laterInventoryPath =
    "/xyz/openbmc_project/inventory/system/board/GPU_0";
#endif

class AnchorMapper
{
  public:
    AnchorMapper()
    {
        std::promise<void> ready;
        auto future = ready.get_future();
        worker = std::thread([this, &ready] {
            try
            {
                conn = std::make_shared<sdbusplus::asio::connection>(io);
                conn->request_name(phosphor::smbios::mapperBusName);
                server = std::make_unique<sdbusplus::asio::object_server>(conn);
                auto iface =
                    server->add_interface(phosphor::smbios::mapperPath,
                                          phosphor::smbios::mapperInterface);
                iface->register_method("GetSubTreePaths",
                                       [this](const std::string&, int32_t,
                                              const std::vector<std::string>&) {
                                           ++queries;
                                           return paths;
                                       });
                iface->register_method(
                    "GetSubTree", [](const std::string&, int32_t,
                                     const std::vector<std::string>&) {
                        return std::vector<std::pair<
                            std::string,
                            std::vector<std::pair<
                                std::string, std::vector<std::string>>>>>{};
                    });
                iface->initialize();
                ready.set_value();
            }
            catch (...)
            {
                ready.set_exception(std::current_exception());
                return;
            }
            io.run();
        });
        // Fail rather than skip if the private test bus cannot own the mapper.
        try
        {
            future.get();
        }
        catch (...)
        {
            worker.join();
            throw;
        }
    }

    ~AnchorMapper()
    {
        io.stop();
        worker.join();
    }

    void announce(const std::string& path, bool anchor)
    {
        std::promise<void> done;
        auto future = done.get_future();
        boost::asio::post(io, std::bind_front(&AnchorMapper::sendAnnouncement,
                                              this, path, anchor, &done));
        future.get();
    }

    std::atomic<size_t> queries{0};

  private:
    void sendAnnouncement(const std::string& path, bool anchor,
                          std::promise<void>* done)
    {
        if (anchor)
        {
            paths = {path};
        }
        auto msg = conn->new_signal("/xyz/openbmc_project/inventory",
                                    "org.freedesktop.DBus.ObjectManager",
                                    "InterfacesAdded");
        boost::container::flat_map<
            std::string, boost::container::flat_map<
                             std::string, std::variant<std::string, uint64_t>>>
            interfaces;
#ifdef CUSTOM_DBUS_PATH
        interfaces[phosphor::smbios::chassisInterface] = {};
#else
        interfaces[phosphor::smbios::systemInterface] = {};
#endif
        msg.append(sdbusplus::message::object_path(path), interfaces);
        msg.signal_send();
        done->set_value();
    }

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::unique_ptr<sdbusplus::asio::object_server> server;
    std::vector<std::string> paths;
    std::thread worker;
};

class InventoryAnchorTest : public phosphor::smbios::test::TestFixtureBase
{
  protected:
    void runFor(std::chrono::milliseconds duration)
    {
        boost::asio::steady_timer timer(*io, duration);
        timer.async_wait([this](const boost::system::error_code&) {
            io->stop();
        });
        io->restart();
        io->run();
    }
};

TEST_F(InventoryAnchorTest, StartupMatchStopsAfterAnchorDiscovery)
{
    AnchorMapper mapper;
    auto server = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(
        io, conn, server, "/tmp/nonexistent-smbios-6870715",
        phosphor::smbios::defaultObjectPath,
        phosphor::smbios::defaultInventoryPath);
    // Isolate the startup listener from the separate System/ProcessorModule
    // data-refresh listener. Explicit refresh is checked below.
    mdr.interfaceAddedMatch.reset();
    mdr.systemInfoUpdate();
    ASSERT_NE(mdr.motherboardConfigMatch, nullptr);
    ASSERT_EQ(mdr.system, nullptr);
    ASSERT_TRUE(mdr.inventoryAnchorRetryPending);

    // Exercise the exhausted-signal path as well as normal timer discovery.
    mdr.inventoryAnchorRetryTimer.cancel();
    runFor(signalDrainWait);
    mdr.inventoryAnchorRetryCount = mdr.inventoryAnchorRetryLimit;
    mdr.inventoryAnchorRetryExhausted = true;
    const auto queriesBeforeAnchor = mapper.queries.load();
    mapper.announce(anchorPath, true);
    // Dispatch the signal, but stop well before the retry timer can fire.
    runFor(signalDrainWait);
    EXPECT_EQ(mapper.queries.load(), queriesBeforeAnchor);
    EXPECT_TRUE(mdr.inventoryAnchorRetryPending);
    EXPECT_FALSE(mdr.inventoryAnchorRetryExhausted);
    EXPECT_EQ(mdr.system, nullptr);
    runFor(retryTestWait);
    ASSERT_NE(mdr.system, nullptr);
    EXPECT_EQ(mdr.motherboardConfigMatch, nullptr);
    EXPECT_FALSE(mdr.inventoryAnchorRetryPending);
    EXPECT_FALSE(mdr.inventoryAnchorRetryExhausted);
    EXPECT_EQ(mdr.inventoryAnchorRetryCount, 0u);

    // A later startup-matching announcement must not query or rebuild
    // inventory.
    auto queries = mapper.queries.load();
    auto* system = mdr.system.get();
    mapper.announce(laterInventoryPath, false);
    runFor(retryTestWait);
    EXPECT_EQ(mapper.queries.load(), queries);
    EXPECT_EQ(mdr.system.get(), system);
    EXPECT_FALSE(mdr.inventoryAnchorRetryPending);

    // An explicit data refresh still uses the normal inventory update path.
    mdr.systemInfoUpdate();
    EXPECT_GT(mapper.queries.load(), queries);
    EXPECT_NE(mdr.system, nullptr);
    EXPECT_EQ(mdr.motherboardConfigMatch, nullptr);
}
} // namespace
