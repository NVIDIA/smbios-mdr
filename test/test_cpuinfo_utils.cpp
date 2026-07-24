/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "cpuinfo_utils.hpp"

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace cpu_info
{
extern void updatePowerState(const std::string& newState);
extern void updateBiosDone(bool newState);
extern void updateOsState(const std::string& newState);
} // namespace cpu_info

namespace cpu_info
{

namespace
{

constexpr const char* hostService = "xyz.openbmc_project.State.Host";
constexpr const char* hostObject = "/xyz/openbmc_project/state/host0";
constexpr const char* hostStateRunning =
    "xyz.openbmc_project.State.Host.HostState.Running";
constexpr const char* miscService = "xyz.openbmc_project.Host.Misc.Manager";
constexpr const char* miscObject = "/xyz/openbmc_project/misc/platform_state";
constexpr const char* miscInterface = "xyz.openbmc_project.State.Host.Misc";
constexpr const char* osService = "xyz.openbmc_project.State.Host0";
constexpr const char* osInterface =
    "xyz.openbmc_project.State.OperatingSystem.Status";

void pumpCpuInfoIo(std::chrono::milliseconds timeout)
{
    auto& ioc = dbus::getIOContext();
    boost::asio::steady_timer stopTimer(ioc, timeout);
    stopTimer.async_wait([&ioc](const boost::system::error_code&) {
        ioc.stop();
    });
    ioc.restart();
    ioc.run();
}

class FakeHostStateServices
{
  public:
    FakeHostStateServices()
    {
        std::promise<bool> ready;
        auto future = ready.get_future();
        worker = std::thread([this, &ready]() { run(&ready); });
        started = future.get();
    }

    ~FakeHostStateServices()
    {
        if (io)
        {
            io->stop();
        }
        if (worker.joinable())
        {
            worker.join();
        }
    }

    bool ok() const
    {
        return started;
    }

    void setHostState(const std::string& value)
    {
        setProperty(hostIface, "CurrentHostState", value);
    }

    void setBiosDone(bool value)
    {
        setProperty(miscIface, "CoreBiosDone", value);
    }

    void setOsState(const std::string& value)
    {
        setProperty(osIface, "OperatingSystemState", value);
    }

    void addWrongObjectOsInterface()
    {
        addInterface("/xyz/openbmc_project/state/host1", osInterface,
                     "OperatingSystemState", std::string{"Standby"});
    }

    void addWrongNamedInterface()
    {
        addInterface(hostObject,
                     "xyz.openbmc_project.State.OperatingSystem.Other",
                     "OperatingSystemState", std::string{"Standby"});
    }

    void addWrongTypeOsInterface()
    {
        addInterface(hostObject, osInterface, "OperatingSystemState",
                     uint32_t{7}, &osIface);
    }

    void replaceOsInterface(const std::string& value)
    {
        auto done = std::make_shared<std::promise<void>>();
        auto future = done->get_future();
        boost::asio::post(*io, [this, value, done]() {
            if (osIface)
            {
                server->remove_interface(osIface);
                dynamicIfaces.erase(std::remove(dynamicIfaces.begin(),
                                                dynamicIfaces.end(), osIface),
                                    dynamicIfaces.end());
                osIface.reset();
            }

            auto iface = server->add_interface(hostObject, osInterface);
            iface->register_property(
                "UnrelatedProperty", uint32_t{1},
                sdbusplus::asio::PropertyPermission::readWrite);
            iface->register_property(
                "OperatingSystemState", value,
                sdbusplus::asio::PropertyPermission::readWrite);
            iface->initialize();
            osIface = iface;
            dynamicIfaces.push_back(iface);
            done->set_value();
        });
        future.wait();
    }

  private:
    template <typename PropertyType>
    void setProperty(
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
        const std::string& property, const PropertyType& value)
    {
        auto done = std::make_shared<std::promise<void>>();
        auto future = done->get_future();
        boost::asio::post(*io, [iface, property, value, done]() {
            iface->set_property(property, value);
            done->set_value();
        });
        future.wait();
    }

    template <typename PropertyType>
    void addInterface(
        const std::string& object, const std::string& interface,
        const std::string& property, const PropertyType& value,
        std::shared_ptr<sdbusplus::asio::dbus_interface>* out = nullptr)
    {
        auto done = std::make_shared<std::promise<void>>();
        auto future = done->get_future();
        boost::asio::post(*io, [this, object, interface, property, value, out,
                                done]() {
            auto iface = server->add_interface(object, interface);
            iface->register_property(
                property, value,
                sdbusplus::asio::PropertyPermission::readWrite);
            iface->initialize();
            if (out != nullptr)
            {
                *out = iface;
            }
            dynamicIfaces.push_back(iface);
            done->set_value();
        });
        future.wait();
    }

    void run(std::promise<bool>* ready)
    {
        try
        {
            io = std::make_shared<boost::asio::io_context>();
            conn = std::make_shared<sdbusplus::asio::connection>(*io);
            conn->request_name(hostService);
            conn->request_name(miscService);
            conn->request_name(osService);
            server = std::make_shared<sdbusplus::asio::object_server>(conn);

            hostIface = server->add_interface(
                hostObject, sdbusplus::server::xyz::openbmc_project::state::
                                Host::interface);
            hostIface->register_property(
                "CurrentHostState", std::string{hostStateRunning},
                sdbusplus::asio::PropertyPermission::readWrite);
            hostIface->initialize();

            miscIface = server->add_interface(miscObject, miscInterface);
            miscIface->register_property(
                "CoreBiosDone", false,
                sdbusplus::asio::PropertyPermission::readWrite);
            miscIface->initialize();

            ready->set_value(true);
        }
        catch (const std::exception&)
        {
            ready->set_value(false);
            return;
        }
        io->run();
    }

    std::shared_ptr<boost::asio::io_context> io;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::shared_ptr<sdbusplus::asio::object_server> server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> hostIface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> miscIface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> osIface;
    std::vector<std::shared_ptr<sdbusplus::asio::dbus_interface>> dynamicIfaces;
    std::thread worker;
    bool started{false};
};

} // namespace

TEST(CpuinfoUtilsBit, ZeroReturnsOne)
{
    EXPECT_EQ(bit(0), 1u);
}

TEST(CpuinfoUtilsBit, IndexNReturnsTwoToN)
{
    EXPECT_EQ(bit(1), 2u);
    EXPECT_EQ(bit(2), 4u);
    EXPECT_EQ(bit(7), 128u);
    EXPECT_EQ(bit(63), (1ull << 63));
}

TEST(CpuinfoUtilsMask, SingleBitExtraction)
{
    EXPECT_EQ(mask(10u, 1, 1), 1u);
    EXPECT_EQ(mask(10u, 2, 2), 0u);
    EXPECT_EQ(mask(10u, 0, 0), 0u);
    EXPECT_EQ(mask(10u, 3, 3), 1u);
}

TEST(CpuinfoUtilsMask, MultiBitField)
{
    EXPECT_EQ(mask(218u, 1, 4), 13u);
    EXPECT_EQ(mask(0xFFu, 0, 7), 255u);
}

TEST(CpuinfoUtilsMask, WithDestType)
{
    uint32_t val = 0x1234;
    EXPECT_EQ(mask<uint8_t>(val, 0, 7), 0x34u);
    EXPECT_EQ(mask<uint16_t>(val, 8, 23), 0x12u);
}

TEST(CpuinfoUtilsMask, SameLoAndHiBit)
{
    EXPECT_EQ(mask(0xFu, 2, 2), 1u);
    EXPECT_EQ(mask(0xFu, 0, 0), 1u);
    EXPECT_EQ(mask(0xFu, 3, 3), 1u);
}

TEST(CpuinfoUtilsMask, DifferentSourceTypes)
{
    uint16_t u16 = 0xABCD;
    EXPECT_EQ(mask(u16, 4, 11), 0xBCu);
    int32_t i32 = -1;
    EXPECT_EQ(mask(i32, 0, 7), 0xFFu);
}

TEST(CpuinfoUtilsMask, ZeroData)
{
    EXPECT_EQ(mask(0u, 0, 31), 0u);
    EXPECT_EQ(mask(0u, 5, 10), 0u);
}

TEST(CpuinfoUtilsHostState, EnumValues)
{
    EXPECT_EQ(static_cast<int>(HostState::off), 0);
    EXPECT_EQ(static_cast<int>(HostState::postInProgress), 1);
    EXPECT_EQ(static_cast<int>(HostState::postComplete), 2);
}

TEST(CpuinfoUtilsCpp, AddHostStateCallbackRegisters)
{
    addHostStateCallback([](HostState, HostState) {});
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, HostStateInitialValue)
{
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, DbusGetIOContextAndConnection)
{
    boost::asio::io_context& ioc = dbus::getIOContext();
    std::shared_ptr<sdbusplus::asio::connection> conn = dbus::getConnection();
    EXPECT_NE(conn.get(), nullptr);
    EXPECT_EQ(&conn->get_io_context(), &ioc);
}

TEST(CpuinfoUtilsCpp, MultipleCallbacksCanBeRegistered)
{
    addHostStateCallback([](HostState, HostState) {});
    addHostStateCallback([](HostState, HostState) {});
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, HostStateSetupConsumesInitialPropertiesAndSignals)
{
    FakeHostStateServices services;
    ASSERT_TRUE(services.ok()) << "fake host state services failed to start";

    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updateBiosDone(false);
    hostStateSetup(dbus::getConnection());
    pumpCpuInfoIo(std::chrono::milliseconds(250));
    EXPECT_EQ(hostState, HostState::postInProgress);

    services.addWrongObjectOsInterface();
    services.addWrongNamedInterface();
    services.addWrongTypeOsInterface();
    pumpCpuInfoIo(std::chrono::milliseconds(250));
    EXPECT_EQ(hostState, HostState::postInProgress);

    services.replaceOsInterface("Standby");
    pumpCpuInfoIo(std::chrono::milliseconds(250));
    EXPECT_EQ(hostState, HostState::postComplete);

    services.setOsState("Inactive");
    pumpCpuInfoIo(std::chrono::milliseconds(250));
    EXPECT_EQ(hostState, HostState::postInProgress);

    services.setBiosDone(true);
    pumpCpuInfoIo(std::chrono::milliseconds(250));
    EXPECT_EQ(hostState, HostState::postComplete);

    services.setHostState("xyz.openbmc_project.State.Host.HostState.Off");
    pumpCpuInfoIo(std::chrono::milliseconds(250));
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, HostStateSetupIdempotent)
{
    auto conn = dbus::getConnection();
    ASSERT_NE(conn.get(), nullptr);
    hostStateSetup(conn);
    hostStateSetup(conn);
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, DbusGettersReturnSameInstance)
{
    boost::asio::io_context& a = dbus::getIOContext();
    boost::asio::io_context& b = dbus::getIOContext();
    EXPECT_EQ(&a, &b);
    std::shared_ptr<sdbusplus::asio::connection> c1 = dbus::getConnection();
    std::shared_ptr<sdbusplus::asio::connection> c2 = dbus::getConnection();
    EXPECT_EQ(c1.get(), c2.get());
}

TEST(CpuinfoUtilsCpp, UpdatePowerStateOffKeepsHostOff)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, UpdatePowerStateOnWithoutBiosGivesPostInProgress)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updateBiosDone(false);
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    EXPECT_EQ(hostState, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, UpdateBiosDoneTrueWithPowerOnGivesPostComplete)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState(
        "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Inactive");
    EXPECT_EQ(hostState, HostState::postInProgress);
    updateBiosDone(true);
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateShortString)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("Standby");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateFullPath)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState(
        "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.BootComplete");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateInvalidEnumFallsBackToInactive)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("NoSuchStatus");
    EXPECT_EQ(hostState, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateEmptyStringPrependThenInvalidEnum)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("");
    EXPECT_EQ(hostState, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, StateChangeInvokesCallbacks)
{
    static HostState oldSeen = HostState::off, newSeen = HostState::off;
    oldSeen = HostState::off;
    newSeen = HostState::off;
    addHostStateCallback([&](HostState oldS, HostState newS) {
        oldSeen = oldS;
        newSeen = newS;
    });
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    EXPECT_EQ(oldSeen, HostState::off);
    EXPECT_EQ(newSeen, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, PowerOffResetsBiosDoneAndOsState)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(true);
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    EXPECT_EQ(hostState, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, NoCallbackWhenStateUnchanged)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    static int calls = 0;
    calls = 0;
    addHostStateCallback([&](HostState, HostState) { ++calls; });
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    EXPECT_EQ(hostState, HostState::off);
    EXPECT_EQ(calls, 0);
}

TEST(CpuinfoUtilsCpp, PostCompleteViaOsStateOnly)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState(
        "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.BootComplete");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, PostCompleteViaOsStateStandby)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(true);
    updateOsState("Standby");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, PostCompleteViaOsStateDiagBoot)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("DiagBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, HostStateReadAfterUpdates)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    EXPECT_TRUE(hostState == HostState::off ||
                hostState == HostState::postInProgress ||
                hostState == HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateInvalidEnum)
{
    EXPECT_NO_THROW(updateOsState("InvalidStatusThatDoesNotMatchAnyEnum"));
}

TEST(CpuinfoUtilsCpp, UpdatePowerStateInvalidStringThrows)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    EXPECT_EQ(hostState, HostState::off);
    EXPECT_THROW(updatePowerState("NotAValidHostState"),
                 sdbusplus::exception::InvalidEnumString);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateCbootAndPxeBoot)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("CBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("PXEBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateRomBootAndCdromBoot)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("ROMBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("CDROMBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
}

} // namespace cpu_info
