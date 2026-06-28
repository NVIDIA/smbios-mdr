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

#define private public
#include "mdrv2.hpp"
#undef private

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using Mdrv2InvalidParameter =
    sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::InvalidParameter;
using Mdrv2InvalidId =
    sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::InvalidId;
using Mdrv2UpdateInProgress =
    sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::UpdateInProgress;

ACTION_P(SetReadString, value)
{
    *static_cast<const char**>(arg2) = value;
    return 0;
}

TEST(Mdrv2, CompilesAndLinks)
{
    SUCCEED();
}

TEST(Mdrv2, SmbiosMdrv2Constants)
{
    EXPECT_EQ(std::string_view(mdrDefaultFile), "/var/lib/smbios/smbios2");
    EXPECT_EQ(mdrSMBIOSSize, 32 * 1024);
    EXPECT_EQ(smbiosAgentId, 0x0101);
    EXPECT_EQ(firstAgentIndex, 1);
    EXPECT_EQ(maxDirEntries, 4);
    EXPECT_EQ(mdrDirVersion, 1);
    EXPECT_EQ(mdr2Version, 2);
    EXPECT_EQ(smbiosTableStorageSize, 64 * 1024u);
    EXPECT_EQ(anchorString21, "_SM_");
    EXPECT_EQ(anchorString30, "_SM3_");
}

TEST(Mdrv2, Mdr2SmbiosStatusEnumValues)
{
    EXPECT_EQ(static_cast<int>(MDR2SMBIOSStatusEnum::mdr2Init), 0);
    EXPECT_EQ(static_cast<int>(MDR2SMBIOSStatusEnum::mdr2Loaded), 1);
    EXPECT_EQ(static_cast<int>(MDR2SMBIOSStatusEnum::mdr2Updated), 2);
}

TEST(Mdrv2, Mdr2DirLockEnumValues)
{
    EXPECT_EQ(static_cast<int>(MDR2DirLockEnum::mdr2DirUnlock), 0);
    EXPECT_EQ(static_cast<int>(MDR2DirLockEnum::mdr2DirLock), 1);
}

TEST(Mdrv2, SmbiosTypeEnumValues)
{
    EXPECT_EQ(static_cast<int>(memoryDeviceType), 17);
    EXPECT_EQ(static_cast<int>(systemSlots), 9);
    EXPECT_EQ(static_cast<int>(tpmDeviceType), 43);
    EXPECT_EQ(static_cast<int>(firmwareInventoryInformationType), 45);
}

class Mdrv2Fixture : public phosphor::smbios::test::TestFixtureBase
{};

static bool writeMinimalSmbiosStub(const std::string& path)
{
    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.timestamp = 0;
    hdr.dataSize = sizeof(EntryPointStructure30);

    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.epChecksum = 0;
    ep30.epLength = sizeof(EntryPointStructure30);
    ep30.smbiosVersion.majorVersion = 3;
    ep30.smbiosVersion.minorVersion = 0;
    ep30.smbiosDocRev = 0;
    ep30.epRevision = 0;
    ep30.reserved = 0;
    ep30.structTableMaxSize = 0;
    ep30.structTableAddr = 0;

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(&ep30), sizeof(ep30));
    return f.good();
}

static bool writeTooSmallStub(const std::string& path)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write("\x01\x02\x00\x00\x00", 5);
    return f.good();
}

static bool writeOversizedDataSizeStub(const std::string& path)
{
    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.timestamp = 0;
    hdr.dataSize = smbiosTableStorageSize + 1;
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write("x", 1);
    return f.good();
}

// Header says dataSize 64; payload is only sizeof(ep30) so readDataFromFlash
// partial-read path is taken
static bool writePartialReadStub(const std::string& path)
{
    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.timestamp = 0;
    hdr.dataSize = 64;
    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.epChecksum = 0;
    ep30.epLength = sizeof(EntryPointStructure30);
    ep30.smbiosVersion.majorVersion = 3;
    ep30.smbiosVersion.minorVersion = 0;
    ep30.smbiosDocRev = 0;
    ep30.epRevision = 0;
    ep30.reserved = 0;
    ep30.structTableMaxSize = 0;
    ep30.structTableAddr = 0;
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(&ep30), sizeof(ep30));
    return f.good();
}

static bool writeNoAnchorStub(const std::string& path)
{
    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.timestamp = 0;
    hdr.dataSize = 24;
    std::vector<uint8_t> payload(24, 0);
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    return f.good();
}

TEST_F(Mdrv2Fixture, ConstructorWithMissingFileRunsMdrv2CodePaths)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
}

TEST_F(Mdrv2Fixture, ConstructorWithCustomObjectPathRunsPlaceGetRecordTypePath)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                    "/xyz/openbmc_project/Smbios/MDR_V2/Custom",
                                    phosphor::smbios::defaultInventoryPath);
    });
}

TEST_F(Mdrv2Fixture, GetDirectoryInformationMissingFileThrows)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_THROW(mdr.getDirectoryInformation(0), Mdrv2InvalidParameter);
}

TEST_F(Mdrv2Fixture, StubFileInTmpGetDirectoryInformationSucceeds)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    std::vector<uint8_t> dirInfo;
    EXPECT_NO_THROW(dirInfo = mdr.getDirectoryInformation(0));
    EXPECT_GE(dirInfo.size(), 4u);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFileGetDirectoryInformationInvalidIndexThrows)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    EXPECT_THROW(mdr.getDirectoryInformation(2), Mdrv2InvalidParameter);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFileDirectoryEntriesReturnsOne)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    uint8_t v = mdr.directoryEntries(0);
    EXPECT_EQ(v, 1u);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFileGetDataInformationAndOfferSucceed)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    std::vector<uint8_t> info = mdr.getDataInformation(0);
    EXPECT_GE(info.size(), 5u);
    std::vector<uint8_t> offer = mdr.getDataOffer();
    EXPECT_EQ(offer.size(), 16u);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFileSendDirectoryInformationDifferentVersionUpdates)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    std::vector<uint8_t> entry(16, 0xab);
    bool terminate = mdr.sendDirectoryInformation(2, 0, 1, 0, entry);
    EXPECT_TRUE(terminate);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFileSendDirectoryInformationRemainingReturnsFalse)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    std::vector<uint8_t> entry(16, 0);
    bool terminate = mdr.sendDirectoryInformation(2, 0, 1, 1, entry);
    EXPECT_FALSE(terminate);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFileSendDataInformationChangedReturnsTrue)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    bool changed = mdr.sendDataInformation(0, 0, 100, 2, 1);
    EXPECT_TRUE(changed);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFileSendDataInformationUnchangedReturnsFalse)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    mdr.sendDataInformation(0, 0, 24, 1, 0);
    bool changed = mdr.sendDataInformation(0, 0, 24, 1, 0);
    EXPECT_FALSE(changed);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFileSendDataInformationSingleFieldChanges)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    mdr.sendDataInformation(0, 0, 24, 1, 0);
    bool changedLen = mdr.sendDataInformation(0, 0, 25, 1, 0);
    EXPECT_TRUE(changedLen);
    bool changedVer = mdr.sendDataInformation(0, 0, 25, 2, 0);
    EXPECT_TRUE(changedVer);
    bool changedTs = mdr.sendDataInformation(0, 0, 25, 2, 1);
    EXPECT_TRUE(changedTs);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, BadFileTooSmallConstructorCompletesReadFails)
{
    std::string path =
        "/tmp/smbios2_mdrv2_bad_small_" + std::to_string(getpid());
    ASSERT_TRUE(writeTooSmallStub(path)) << "write too-small stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, BadFileOversizedDataSizeConstructorCompletesReadFails)
{
    std::string path =
        "/tmp/smbios2_mdrv2_bad_oversize_" + std::to_string(getpid());
    ASSERT_TRUE(writeOversizedDataSizeStub(path))
        << "write oversized stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, StubFilePartialReadConstructorSucceeds)
{
    std::string path = "/tmp/smbios2_mdrv2_partial_" + std::to_string(getpid());
    ASSERT_TRUE(writePartialReadStub(path)) << "write partial-read stub";

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, SendDirectoryInformationSameVersionReturnsTrue)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    std::vector<uint8_t> entry(16, 0);
    bool terminate = mdr.sendDirectoryInformation(1, 0, 1, 0, entry);
    EXPECT_TRUE(terminate);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture,
       StubFileWithCustomInventoryPathRunsSystemInfoUpdateBranches)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    const std::string customInventory =
        "/xyz/openbmc_project/inventory/system/custom";
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    customInventory);
    });

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, FakeObjectMapperGetSubTreePathsRunsSystemInfoUpdate)
{
    const std::string motherboardPath =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard";

    int pipefd[2];
    if (pipe(pipefd) != 0)
        GTEST_SKIP() << "pipe() failed";

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        GTEST_SKIP() << "fork() failed";
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        std::shared_ptr<boost::asio::io_context> ioMapper =
            std::make_shared<boost::asio::io_context>();
        std::shared_ptr<sdbusplus::asio::connection> connMapper;
        try
        {
            connMapper = std::make_shared<sdbusplus::asio::connection>(
                *ioMapper, sdbusplus::bus::new_bus());
            connMapper->request_name(phosphor::smbios::mapperBusName);
        }
        catch (const std::exception&)
        {
            ssize_t w = write(pipefd[1], "E", 1);
            (void)w;
            close(pipefd[1]);
            _exit(1);
        }
        auto mapperServer =
            std::make_shared<sdbusplus::asio::object_server>(connMapper);
        auto mapperIface = mapperServer->add_interface(
            phosphor::smbios::mapperPath, phosphor::smbios::mapperInterface);
        mapperIface->register_method(
            "GetSubTreePaths",
            [motherboardPath](const std::string&, int32_t,
                              const std::vector<std::string>&) {
                return std::vector<std::string>{motherboardPath};
            });
        mapperIface->initialize();
        ssize_t w = write(pipefd[1], "R", 1);
        (void)w;
        close(pipefd[1]);
        ioMapper->run();
        _exit(0);
    }

    close(pipefd[1]);
    char ready = 0;
    ssize_t n = read(pipefd[0], &ready, 1);
    close(pipefd[0]);
    if (n != 1 || ready == 'E')
    {
        int status = 0;
        waitpid(pid, &status, 0);
        GTEST_SKIP() << "Child could not own "
                     << phosphor::smbios::mapperBusName;
    }
    if (ready != 'R')
    {
        int status = 0;
        waitpid(pid, &status, 0);
        GTEST_SKIP() << "Child did not signal ready";
    }

    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });

    kill(pid, SIGTERM);
    int status = 0;
    waitpid(pid, &status, 0);
    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, BadFileNoAnchorConstructorCompletesCheckVersionFails)
{
    std::string path =
        "/tmp/smbios2_mdrv2_bad_noanchor_" + std::to_string(getpid());
    ASSERT_TRUE(writeNoAnchorStub(path)) << "write no-anchor stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, GetDataInformationInvalidIndexThrows)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_THROW(mdr.getDataInformation(maxDirEntries), Mdrv2InvalidParameter);
    EXPECT_THROW(mdr.getDataInformation(maxDirEntries + 1),
                 Mdrv2InvalidParameter);
}

TEST_F(Mdrv2Fixture, GetDataOfferReturnsOfferWhenAvailForUpdate)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    std::vector<uint8_t> offer = mdr.getDataOffer();
    EXPECT_EQ(offer.size(), 16u);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, SendDirectoryInformationInvalidDirIndexThrows)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    std::vector<uint8_t> entry(16, 0);
    EXPECT_THROW(mdr.sendDirectoryInformation(0, maxDirEntries, 1, 0, entry),
                 Mdrv2InvalidParameter);
}

TEST_F(Mdrv2Fixture, SendDirectoryInformationReturnedEntriesZeroThrows)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_THROW(
        mdr.sendDirectoryInformation(0, 0, 0, 0, std::vector<uint8_t>()),
        Mdrv2InvalidParameter);
}

TEST_F(Mdrv2Fixture, SendDirectoryInformationDirEntrySizeMismatchThrows)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    std::vector<uint8_t> entry(8, 0);
    EXPECT_THROW(mdr.sendDirectoryInformation(0, 0, 1, 0, entry),
                 Mdrv2InvalidParameter);
}

TEST_F(Mdrv2Fixture, SendDataInformationInvalidIdIndexThrows)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_THROW(mdr.sendDataInformation(maxDirEntries, 0, 0, 0, 0),
                 Mdrv2InvalidParameter);
}

TEST_F(Mdrv2Fixture, FindIdIndexWrongSizeThrowsInvalidId)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_THROW(mdr.findIdIndex(std::vector<uint8_t>(15, 0)), Mdrv2InvalidId);
    EXPECT_THROW(mdr.findIdIndex(std::vector<uint8_t>(17, 0)), Mdrv2InvalidId);
}

TEST_F(Mdrv2Fixture, FindIdIndexNoMatchThrowsInvalidId)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    std::vector<uint8_t> wrongId(16, 0);
    EXPECT_THROW(mdr.findIdIndex(wrongId), Mdrv2InvalidId);
}

TEST_F(Mdrv2Fixture, FindIdIndexMatchReturnsIndex)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    std::vector<uint8_t> id{40, 41, 42, 43, 44, 45, 46, 47,
                            48, 49, 50, 51, 52, 53, 54, 0x42};
    int idx = mdr.findIdIndex(id);
    EXPECT_EQ(idx, 0);
}

TEST_F(Mdrv2Fixture, DirectoryEntriesWithMissingFileReturnsZero)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    uint8_t v = mdr.directoryEntries(1);
    EXPECT_EQ(v, 0u);
}

TEST_F(Mdrv2Fixture, GetRecordTypeInvalidTypeThrows)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_THROW(mdr.getRecordType(0), std::invalid_argument);
    EXPECT_THROW(mdr.getRecordType(99), std::invalid_argument);
}

TEST_F(Mdrv2Fixture, GetRecordTypeMemoryDeviceReturnsVector)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    auto records = mdr.getRecordType(memoryDeviceType);
    EXPECT_TRUE(records.empty());
}

TEST_F(Mdrv2Fixture, SynchronizeDirectoryCommonDataReturnsThreeValues)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    auto result = mdr.synchronizeDirectoryCommonData(0, 1024);
    EXPECT_EQ(result.size(), 3u);
}

TEST_F(Mdrv2Fixture, GetDataInformationReturnsInfoForValidIndex)
{
    std::string path = "/tmp/smbios2_mdrv2_test_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path)) << "write stub to " << path;

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    std::vector<uint8_t> info = mdr.getDataInformation(0);
    EXPECT_GE(info.size(), 5u);

    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, SendDirectoryInformationSameVersionTerminatesTrue)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    std::vector<uint8_t> entry(16, 0);
    bool terminate =
        mdr.sendDirectoryInformation(smbiosDirVersion, 0, 1, 0, entry);
    EXPECT_TRUE(terminate);
}

// Build a SMBIOS file: MDRSMBIOSHeader + EP30 + N MemoryInfo records + strings.
// structTableAddr = sizeof(EP30) = 24 so getSMBIOSTypePtr skips the EP
// correctly.
static bool writeSmbiosWithMemoryDevices(const std::string& path,
                                         int numDevices)
{
    // Guard against negative counts before any unsigned arithmetic below, so a
    // negative argument cannot wrap when computing sizes.
    if (numDevices < 0)
    {
        return false;
    }
    // Strings for each MemoryInfo (6 strings: dev-locator, bank-locator,
    // manufacturer, serial, asset-tag, part-number), terminated with
    // double-null.
    static const char strings[] =
        "DIMM_A1\0"
        "BANK_A\0"
        "Samsung\0"
        "SN1234\0"
        "TAG1\0"
        "PN1234\0";
    // sizeof(strings) includes the implicit null terminator of the C literal,
    // giving the SMBIOS required double-null at the end of the string section.

    phosphor::smbios::MemoryInfo mem{};
    mem.type = static_cast<uint8_t>(memoryDeviceType);
    mem.length = static_cast<uint8_t>(sizeof(phosphor::smbios::MemoryInfo));
    mem.errInfoHandle = 0xFFFE;
    mem.totalWidth = 72;
    mem.dataWidth = 64;
    mem.size = 8192;
    mem.formFactor = 0x09;
    mem.deviceLocator = 1;
    mem.bankLocator = 2;
    mem.memoryType = 0x1A;
    mem.typeDetail = 0x0080;
    mem.speed = 3200;
    mem.manufacturer = 3;
    mem.serialNum = 4;
    mem.assetTag = 5;
    mem.partNum = 6;
    mem.attributes = 1;
    mem.confClockSpeed = 3200;
    mem.minimumVoltage = 1200;
    mem.maximumVoltage = 1200;
    mem.configuredVoltage = 1200;
    mem.memoryTechnology = 0x03;
    mem.memoryOperatingModeCap = 0x0004;
    mem.modelManufId = 0x80CE;

    uint32_t structsSize =
        static_cast<uint32_t>(numDevices) *
        (static_cast<uint32_t>(sizeof(phosphor::smbios::MemoryInfo)) +
         sizeof(strings));

    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.epLength = sizeof(EntryPointStructure30);
    ep30.smbiosVersion.majorVersion = 3;
    ep30.smbiosVersion.minorVersion = 0;
    ep30.structTableMaxSize = structsSize;
    ep30.structTableAddr = sizeof(EntryPointStructure30); // = 24

    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = sizeof(EntryPointStructure30) + structsSize;

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(&ep30), sizeof(ep30));
    for (int i = 0; i < numDevices; i++)
    {
        mem.handle = static_cast<uint16_t>(0x0100 + i);
        mem.phyArrayHandle = static_cast<uint16_t>(i);
        f.write(reinterpret_cast<const char*>(&mem), sizeof(mem));
        f.write(strings, sizeof(strings));
    }
    return f.good();
}

// Write a file with a SMBIOS 2.1 (_SM_) entry point header carrying
// version 3.0. checkSMBIOSVersion will take the smbios21Found branch, read the
// version, and succeed because 3.0 is in supportedSMBIOSVersions.
static bool writeSmbios21Stub(const std::string& path)
{
    EntryPointStructure21 ep21{};
    std::memcpy(&ep21.anchorString, "_SM_", 4); // write 4 bytes into uint32_t
    ep21.epLength = sizeof(EntryPointStructure21);
    ep21.smbiosVersion.majorVersion = 3;
    ep21.smbiosVersion.minorVersion = 0;

    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = sizeof(EntryPointStructure21);

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(&ep21), sizeof(ep21));
    return f.good();
}

// Write a file with _SM3_ anchor but version 99.9 (not in
// supportedSMBIOSVersions).
static bool writeSmbiosUnsupportedVersionStub(const std::string& path)
{
    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.epLength = sizeof(EntryPointStructure30);
    ep30.smbiosVersion.majorVersion = 99;
    ep30.smbiosVersion.minorVersion = 9;
    ep30.structTableAddr = sizeof(EntryPointStructure30);

    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = sizeof(EntryPointStructure30);

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(&ep30), sizeof(ep30));
    return f.good();
}

// getRecordType with a real MemoryInfo record exercises the full do-while body
// (lines 1012-1062 in mdrv2.cpp).
TEST_F(Mdrv2Fixture, GetRecordTypeMemoryDeviceWithFileDataFindsOneRecord)
{
    std::string path = "/tmp/smbios2_mdrv2_memdev1_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    auto records = mdr.getRecordType(memoryDeviceType);
    EXPECT_EQ(records.size(), 1u);
    if (!records.empty())
    {
        EXPECT_EQ(std::get<uint8_t>(records[0].at("Type")),
                  static_cast<uint8_t>(memoryDeviceType));
        EXPECT_EQ(std::get<uint8_t>(records[0].at("Length")),
                  static_cast<uint8_t>(sizeof(phosphor::smbios::MemoryInfo)));
    }

    std::remove(path.c_str());
}

// Two MemoryInfo records exercise the smbiosNextPtr do-while continuation.
TEST_F(Mdrv2Fixture, GetRecordTypeMemoryDeviceTwoRecordsFindsTwo)
{
    std::string path = "/tmp/smbios2_mdrv2_memdev2_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 2));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    auto records = mdr.getRecordType(memoryDeviceType);
    EXPECT_EQ(records.size(), 2u);

    std::remove(path.c_str());
}

// SMBIOS 2.1 anchor exercises the smbios21Found=true branch in
// checkSMBIOSVersion.
TEST_F(Mdrv2Fixture, ConstructorSmbios21AnchorSupportedVersionSucceeds)
{
    std::string path = "/tmp/smbios2_mdrv2_sm21_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbios21Stub(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });

    std::remove(path.c_str());
}

// Unsupported SMBIOS version exercises the itr==end branch in
// checkSMBIOSVersion.
TEST_F(Mdrv2Fixture, ConstructorUnsupportedSmbiosVersionDoesNotThrow)
{
    std::string path = "/tmp/smbios2_mdrv2_badver_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosUnsupportedVersionStub(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });

    std::remove(path.c_str());
}

// Version 3.1 is not in supportedSMBIOSVersions: major=3 matches some entries
// so both sides of the && operator in the find_if lambda are evaluated
// (left TRUE, right FALSE), exercising the second operand branch.
static bool writeSmbiosVersion31Stub(const std::string& path)
{
    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.epLength = sizeof(EntryPointStructure30);
    ep30.smbiosVersion.majorVersion = 3;
    ep30.smbiosVersion.minorVersion = 1; // not in supportedSMBIOSVersions
    ep30.structTableAddr = sizeof(EntryPointStructure30);

    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = sizeof(EntryPointStructure30);

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(&ep30), sizeof(ep30));
    return f.good();
}

TEST_F(Mdrv2Fixture, ConstructorSmbios31VersionExercisesLambdaAndBothOperands)
{
    std::string path = "/tmp/smbios2_mdrv2_v31_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosVersion31Stub(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });

    std::remove(path.c_str());
}

// getDirectoryInformation with a non-existent file exercises the file-open
// failure branch (lines 39-45 in mdrv2.cpp).
TEST_F(Mdrv2Fixture, GetDirectoryInformationNonExistentFileThrows)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_THROW(mdr.getDirectoryInformation(0), Mdrv2InvalidParameter);
}

// readDataFromFlash: file shorter than MDRSMBIOSHeader covers the fileLength
// < sizeof(MDRSMBIOSHeader) branch (mdrv2.cpp line 227-231).
static bool writeTooSmallSmbiosFile(const std::string& path)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    uint8_t stub[] = {0x01, 0x02}; // only 2 bytes, far below MDRSMBIOSHeader
    f.write(reinterpret_cast<const char*>(stub), sizeof(stub));
    return f.good();
}

TEST_F(Mdrv2Fixture, ConstructorTooSmallFileHandlesReadFailure)
{
    std::string path = "/tmp/smbios2_mdrv2_tiny_" + std::to_string(getpid());
    ASSERT_TRUE(writeTooSmallSmbiosFile(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

// readDataFromFlash: dataSize > smbiosTableStorageSize covers the
// "Data size out of limitation" branch (mdrv2.cpp lines 233-237).
static bool writeSmbiosOversizedDataSize(const std::string& path)
{
    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = smbiosTableStorageSize + 1; // exceeds 64 KB limit

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    // Write a few padding bytes so fileLength >= sizeof(MDRSMBIOSHeader)
    uint8_t pad[32] = {0};
    f.write(reinterpret_cast<const char*>(pad), sizeof(pad));
    return f.good();
}

TEST_F(Mdrv2Fixture, ConstructorOversizedDataSizeHandlesReadFailure)
{
    std::string path =
        "/tmp/smbios2_mdrv2_oversize_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosOversizedDataSize(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

// POD mirror structs for building multi-type SMBIOS test data.  Defined here
// to avoid pulling in the full class headers and their D-Bus template
// machinery into the test binary.
struct TestBaseboardInfo
{
    uint8_t type;
    uint8_t length;
    uint16_t handle;
    uint8_t manufacturer;
    uint8_t product;
    uint8_t version;
    uint8_t serialNumber;
    uint8_t assetTag;
    uint8_t featureFlags;
    uint8_t locationInChassis;
    uint16_t chassisHandle;
    uint8_t boardType;
    uint8_t numOfContainedObject;
    uint16_t containedObjectHandles[1];
} __attribute__((packed));

struct TestProcessorInfo
{
    uint8_t type;
    uint8_t length;
    uint16_t handle;
    uint8_t socketDesignation;
    uint8_t processorType;
    uint8_t family;
    uint8_t manufacturer;
    uint64_t id;
    uint8_t version;
    uint8_t voltage;
    uint16_t exClock;
    uint16_t maxSpeed;
    uint16_t currSpeed;
    uint8_t status;
    uint8_t upgrade;
    uint16_t l1Handle;
    uint16_t l2Handle;
    uint16_t l3Handle;
    uint8_t serialNum;
    uint8_t assetTag;
    uint8_t partNum;
    uint8_t coreCount;
    uint8_t coreEnable;
    uint8_t threadCount;
    uint16_t characteristics;
    uint16_t family2;
    uint16_t coreCount2;
    uint16_t coreEnable2;
    uint16_t threadCount2;
} __attribute__((packed));

struct TestSystemSlotInfo
{
    uint8_t type;
    uint8_t length;
    uint16_t handle;
    uint8_t slotDesignation;
    uint8_t slotType; // byte 5 — checked by getTotalSmbiosEntries
    uint8_t slotDataBusWidth;
    uint8_t currUsage;
    uint8_t slotLength;
    uint16_t slotID;
    uint8_t characteristics1;
    uint8_t characteristics2;
    uint16_t segGroupNum;
    uint8_t busNum;
    uint8_t deviceNum;
} __attribute__((packed));

// Write a SMBIOS file with baseboard (type 2), processor (type 4), memory
// device (type 17), and TWO system-slot (type 9) records.  One slot has a
// PCIe slot type (0xa5) so pcieSmbiosType.find() returns end() != true for
// that record, and one has a non-PCIe type (0x01 = ISA) so
// pcieSmbiosType.find() returns end() for the other, exercising both branches
// of that conditional inside getTotalSmbiosEntries.
static bool writeSmbiosWithAllTypes(const std::string& path)
{
    // Baseboard record
    static const char bbStr[] =
        "Mfr\0Product\0Ver\0Serial\0Asset\0Location\0\0";
    TestBaseboardInfo bb{};
    bb.type = static_cast<uint8_t>(baseboardType);
    bb.length = static_cast<uint8_t>(sizeof(TestBaseboardInfo));
    bb.handle = 0x0001;
    bb.boardType = 0x0A; // Interconnect board (not Processor* type)
    bb.numOfContainedObject = 1;
    bb.containedObjectHandles[0] = 0x0100; // will be the DIMM handle

    // CPU record
    static const char cpuStr[] = "CPU0\0\0";
    TestProcessorInfo cpu{};
    cpu.type = static_cast<uint8_t>(processorsType);
    cpu.length = static_cast<uint8_t>(sizeof(TestProcessorInfo));
    cpu.handle = 0x0004;
    cpu.processorType = 3;
    cpu.status = 0x41;

    // Memory device record — handle matches baseboard's contained object
    static const char memStr[] = "DIMM_A0\0BANK_A\0\0";
    phosphor::smbios::MemoryInfo mem{};
    mem.type = static_cast<uint8_t>(memoryDeviceType);
    mem.length = static_cast<uint8_t>(sizeof(phosphor::smbios::MemoryInfo));
    mem.handle = 0x0100;
    mem.errInfoHandle = 0xFFFE;
    mem.totalWidth = 72;
    mem.dataWidth = 64;
    mem.size = 8192;
    mem.memoryType = 0x1A;
    mem.deviceLocator = 1;
    mem.bankLocator = 2;

    // PCIe slot (slotType=0xa5 IS in pcieSmbiosType → counted)
    static const char slotPcieStr[] = "PCIe_x16\0\0";
    TestSystemSlotInfo slotPcie{};
    slotPcie.type = static_cast<uint8_t>(systemSlots);
    slotPcie.length = static_cast<uint8_t>(sizeof(TestSystemSlotInfo));
    slotPcie.handle = 0x0009;
    slotPcie.slotDesignation = 1;
    slotPcie.slotType = 0xa5; // in pcieSmbiosType

    // ISA slot (slotType=0x01 NOT in pcieSmbiosType → not counted)
    static const char slotIsaStr[] = "ISA_Slot\0\0";
    TestSystemSlotInfo slotIsa{};
    slotIsa.type = static_cast<uint8_t>(systemSlots);
    slotIsa.length = static_cast<uint8_t>(sizeof(TestSystemSlotInfo));
    slotIsa.handle = 0x000A;
    slotIsa.slotDesignation = 1;
    slotIsa.slotType = 0x01; // ISA — NOT in pcieSmbiosType

    uint32_t structsSize =
        static_cast<uint32_t>(sizeof(TestBaseboardInfo)) + sizeof(bbStr) +
        static_cast<uint32_t>(sizeof(TestProcessorInfo)) + sizeof(cpuStr) +
        static_cast<uint32_t>(sizeof(phosphor::smbios::MemoryInfo)) +
        sizeof(memStr) + static_cast<uint32_t>(sizeof(TestSystemSlotInfo)) +
        sizeof(slotPcieStr) +
        static_cast<uint32_t>(sizeof(TestSystemSlotInfo)) + sizeof(slotIsaStr);

    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.epLength = sizeof(EntryPointStructure30);
    ep30.smbiosVersion.majorVersion = 3;
    ep30.smbiosVersion.minorVersion = 0;
    ep30.structTableMaxSize = structsSize;
    ep30.structTableAddr = sizeof(EntryPointStructure30);

    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = sizeof(EntryPointStructure30) + structsSize;

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(&ep30), sizeof(ep30));
    f.write(reinterpret_cast<const char*>(&bb), sizeof(bb));
    f.write(bbStr, sizeof(bbStr));
    f.write(reinterpret_cast<const char*>(&cpu), sizeof(cpu));
    f.write(cpuStr, sizeof(cpuStr));
    f.write(reinterpret_cast<const char*>(&mem), sizeof(mem));
    f.write(memStr, sizeof(memStr));
    f.write(reinterpret_cast<const char*>(&slotPcie), sizeof(slotPcie));
    f.write(slotPcieStr, sizeof(slotPcieStr));
    f.write(reinterpret_cast<const char*>(&slotIsa), sizeof(slotIsa));
    f.write(slotIsaStr, sizeof(slotIsaStr));
    return f.good();
}

// This test exercises:
// - getTotalSmbiosEntries with systemSlots type (smbiosType == systemSlots
// branch)
// - pcieSmbiosType.find() TRUE and FALSE branches
// - systemInfoUpdate PCIe creation loop
// - baseboard / CPU / DIMM for-loop bodies in systemInfoUpdate
// - getObjectPath inline function
TEST_F(Mdrv2Fixture, ConstructorAllTypesSmbiosExercisesSystemInfoUpdate)
{
    std::string path =
        "/tmp/smbios2_mdrv2_alltypes_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithAllTypes(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

// Custom inventoryPath exercises the smbiosInventoryPath !=
// defaultInventoryPath TRUE branch and requireExactMatch == true in
// systemInfoUpdate.
TEST_F(Mdrv2Fixture, ConstructorCustomInventoryPathExercisesRequireExactMatch)
{
    std::string path =
        "/tmp/smbios2_mdrv2_custpath_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    const std::string customInventoryPath =
        "/xyz/openbmc_project/inventory/custom/board";
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    customInventoryPath);
    });
    std::remove(path.c_str());
}

// synchronizeDirectoryCommonData schedules a 2-second timer.  Destroying the
// MDRV2 object before the timer fires cancels it; running the io_context then
// processes the cancelled-timer callback, covering the if(ec) TRUE branch in
// the async_wait lambda (mdrv2.cpp line 976).
TEST_F(Mdrv2Fixture, SynchronizeDirectoryTimerCancelledOnDestruction)
{
    std::string path =
        "/tmp/smbios2_mdrv2_tmrcancel_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    {
        auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
        auto result = mdr.synchronizeDirectoryCommonData(0, 1024);
        EXPECT_EQ(result.size(), 3u);
        // mdr destroyed here → pending timer cancelled by boost::asio
    }
    // The cancelled-timer callback is now in the ready queue.
    // restart() un-stops the io_context in case sdbusplus stopped it during
    // connection setup, then poll() dispatches all ready handlers immediately.
    io->restart();
    io->poll();
    std::remove(path.c_str());
}

// synchronizeDirectoryCommonData timer fires normally (ec == 0) → covers the
// if(ec) FALSE branch and the subsequent agentSynchronizeData() call.
// We use a stop-timer (3 s) with io->run() so the event loop reliably waits
// for the 2-second synchronize timer to fire before returning.
TEST_F(Mdrv2Fixture, SynchronizeDirectoryTimerFiresCoversFalseBranch)
{
    std::string path = "/tmp/smbios2_mdrv2_tmrfire_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.synchronizeDirectoryCommonData(0, 1024);

    // Stop the io_context 3 s from now (> defaultTimeout = 2 s) so io->run()
    // blocks long enough for the synchronize timer to fire naturally.
    boost::asio::steady_timer stopTimer(*io, std::chrono::seconds(3));
    stopTimer.async_wait(
        [this_io = io.get()](const boost::system::error_code&) {
            this_io->stop();
        });

    io->restart(); // un-stop in case sdbusplus stopped it during construction
    io->run();     // blocks until stop() is called at t≈3 s
    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// Direct unit tests for the inline helper functions in smbios_mdrv2.hpp.
// These are free functions operating on raw byte buffers, so they can be
// exercised directly without a D-Bus connection or an MDRV2 instance.
// ---------------------------------------------------------------------------

// smbiosNextPtr(nullptr) returns nullptr (smbios_mdrv2.hpp line 232).
TEST(SmbiosHelpers, SmbiosNextPtrNullReturnsNull)
{
    uint8_t* volatile runtimeNull = nullptr;
    EXPECT_EQ(smbiosNextPtr(runtimeNull), nullptr);
}

// smbiosNextPtr endless-loop guard: a buffer whose structure never reaches a
// double-null within mdrSMBIOSSize returns nullptr (line 242).
TEST(SmbiosHelpers, SmbiosNextPtrEndlessLoopGuardReturnsNull)
{
    std::vector<uint8_t> buf(static_cast<size_t>(mdrSMBIOSSize) + 16, 0x01);
    buf[0] = 17; // type
    buf[1] = 2;  // length -> smbiosData starts at buf+2
    // From buf+2 onward all 0x01, so (a | b) is never 0 -> len hits the cap.
    EXPECT_EQ(smbiosNextPtr(buf.data()), nullptr);
}

// smbiosNextPtr normal traversal: stops at the double-null and returns the
// pointer just past it (separateLen).
TEST(SmbiosHelpers, SmbiosNextPtrNormalTraversal)
{
    // [type,len, h,h] then string-area "A\0" then terminator; smbiosNextPtr
    // returns the byte two past the closing null (separateLen), index 7 here.
    std::vector<uint8_t> buf = {17, 4, 0, 0, 'A', 0, 0, 0xAB};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    uint8_t* next = smbiosNextPtr(buf.data());
    ASSERT_NE(next, nullptr);
    EXPECT_EQ(*next, 0xAB);
}

// smbiosSkipEntryPoint(nullptr) returns nullptr (line 253).
TEST(SmbiosHelpers, SmbiosSkipEntryPointNullReturnsNull)
{
    EXPECT_EQ(smbiosSkipEntryPoint(nullptr), nullptr);
}

// _SM3_ anchor with a small structTableAddr advances the pointer (line 265
// TRUE branch).
TEST(SmbiosHelpers, SmbiosSkipEntryPointSmallAddrAdvances)
{
    std::vector<uint8_t> buf(64, 0);
    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.structTableAddr = 24; // < mdrSMBIOSSize
    std::memcpy(buf.data(), &ep30, sizeof(ep30));
    uint8_t* p = smbiosSkipEntryPoint(buf.data());
    EXPECT_EQ(p, buf.data() + 24);
}

// _SM3_ anchor with structTableAddr >= mdrSMBIOSSize leaves pointer unchanged
// (line 265 FALSE branch).
TEST(SmbiosHelpers, SmbiosSkipEntryPointLargeAddrUnchanged)
{
    std::vector<uint8_t> buf(64, 0);
    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.structTableAddr = mdrSMBIOSSize; // not < mdrSMBIOSSize
    std::memcpy(buf.data(), &ep30, sizeof(ep30));
    EXPECT_EQ(smbiosSkipEntryPoint(buf.data()), buf.data());
}

// No anchor -> pointer returned unchanged.
TEST(SmbiosHelpers, SmbiosSkipEntryPointNoAnchorUnchanged)
{
    std::vector<uint8_t> buf = {17, 4, 0, 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(smbiosSkipEntryPoint(buf.data()), buf.data());
}

// getSMBIOSTypePtr(nullptr,...) returns nullptr.
TEST(SmbiosHelpers, GetSmbiosTypePtrNullReturnsNull)
{
    EXPECT_EQ(getSMBIOSTypePtr(nullptr, memoryDeviceType), nullptr);
}

// getSMBIOSTypePtr finds a matching record.
TEST(SmbiosHelpers, GetSmbiosTypePtrFindsRecord)
{
    std::vector<uint8_t> buf = {17, 8, 0, 0, 1, 2, 3, 4, 'A', 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    uint8_t* p = getSMBIOSTypePtr(buf.data(), 17);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 17);
}

// getSMBIOSTypePtr with size larger than record length returns nullptr
// (line 304 TRUE branch).
TEST(SmbiosHelpers, GetSmbiosTypePtrSizeMismatchReturnsNull)
{
    std::vector<uint8_t> buf = {17, 8, 0, 0, 1, 2, 3, 4, 'A', 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(getSMBIOSTypePtr(buf.data(), 17, 100), nullptr);
}

// getSMBIOSTypePtr skips a non-matching record then finds the target.
TEST(SmbiosHelpers, GetSmbiosTypePtrSkipsNonMatching)
{
    // First record type 4 (len 4) with string "X\0", then type 17 record.
    std::vector<uint8_t> buf = {4,  4, 0, 0, 'X', 0, 0, // end of rec 1
                                17, 8, 0, 0, 1,   2, 3, 4, 'B', 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    uint8_t* p = getSMBIOSTypePtr(buf.data(), 17);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 17);
}

// getSMBIOSTypePtr inner endless-loop guard: a non-matching record whose
// string area never terminates returns nullptr (line 296).
TEST(SmbiosHelpers, GetSmbiosTypePtrInnerEndlessLoopReturnsNull)
{
    std::vector<uint8_t> buf(static_cast<size_t>(mdrSMBIOSSize) + 16, 0x01);
    buf[0] = 4; // type 4, != 17
    buf[1] = 4; // length -> smbiosData jumps to buf+4, then all 0x01
    EXPECT_EQ(getSMBIOSTypePtr(buf.data(), 17), nullptr);
}

// getSMBIOSTypeIndexPtr index 0 returns the first match.
TEST(SmbiosHelpers, GetSmbiosTypeIndexPtrIndexZero)
{
    std::vector<uint8_t> buf = {17, 8, 0, 0, 1, 2, 3, 4, 'A', 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    uint8_t* p = getSMBIOSTypeIndexPtr(buf.data(), 17, 0);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 17);
}

// getSMBIOSTypeIndexPtr null input returns nullptr.
TEST(SmbiosHelpers, GetSmbiosTypeIndexPtrNullReturnsNull)
{
    EXPECT_EQ(getSMBIOSTypeIndexPtr(nullptr, 17, 0), nullptr);
}

// getSMBIOSTypeIndexPtr asking for a second record when only one exists
// returns nullptr via the getSMBIOSTypePtr==null path (line 335).
TEST(SmbiosHelpers, GetSmbiosTypeIndexPtrSecondMissingReturnsNull)
{
    std::vector<uint8_t> buf = {17, 8, 0, 0, 1, 2, 3, 4, 'A', 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(getSMBIOSTypeIndexPtr(buf.data(), 17, 1), nullptr);
}

// getSMBIOSTypeIndexPtr finds the 2nd record of a type.
TEST(SmbiosHelpers, GetSmbiosTypeIndexPtrSecondRecord)
{
    // Each record is 11 bytes: 8 formatted + "A\0" string + "\0" terminator.
    // smbiosNextPtr lands exactly on the next record start.
    std::vector<uint8_t> buf = {
        17, 8, 0x01, 0, 1, 2, 3, 4, 'A', 0, 0, // record 0 (handle 0x0001)
        17, 8, 0x02, 0, 1, 2, 3, 4, 'B', 0, 0  // record 1 (handle 0x0002)
    };
    buf.resize(buf.size() + 512, 0);           // ASAN: helpers read ahead
    uint8_t* p = getSMBIOSTypeIndexPtr(buf.data(), 17, 1);
    ASSERT_NE(p, nullptr);
    // handle of second record is 0x0002
    auto* hdr = reinterpret_cast<StructureHeader*>(p);
    EXPECT_EQ(hdr->handle, 0x0002);
}

// smbiosHandlePtr finds a record by handle.
TEST(SmbiosHelpers, SmbiosHandlePtrFindsHandle)
{
    std::vector<uint8_t> buf = {17, 8, 0x55, 0x00, 1, 2, 3, 4, 'A', 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    uint8_t* p = smbiosHandlePtr(buf.data(), 0x0055);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 17);
}

// smbiosHandlePtr with a header length below sizeof(StructureHeader) returns
// nullptr (line 351 TRUE branch).
TEST(SmbiosHelpers, SmbiosHandlePtrShortHeaderReturnsNull)
{
    std::vector<uint8_t> buf = {17, 2, 0x55, 0x00, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(smbiosHandlePtr(buf.data(), 0x0055), nullptr);
}

// smbiosHandlePtr with no matching handle traverses to the end and returns
// nullptr.
TEST(SmbiosHelpers, SmbiosHandlePtrNoMatchReturnsNull)
{
    std::vector<uint8_t> buf = {17, 8, 0x55, 0x00, 1, 2, 3, 4, 'A', 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(smbiosHandlePtr(buf.data(), 0x9999), nullptr);
}

// positionToString: positionNum == 0 returns "" (line 368).
TEST(SmbiosHelpers, PositionToStringZeroPositionReturnsEmpty)
{
    std::vector<uint8_t> buf = {0, 0, 0, 0, 'H', 'i', 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(positionToString(0, 4, buf.data()), "");
}

// positionToString: null data returns "" (line 368).
TEST(SmbiosHelpers, PositionToStringNullDataReturnsEmpty)
{
    EXPECT_EQ(positionToString(1, 4, nullptr), "");
}

// positionToString: positionNum 1 returns the first string.
TEST(SmbiosHelpers, PositionToStringFirstStringReturnsValue)
{
    std::vector<uint8_t> buf = {0, 0, 0, 0, 'H', 'e', 'l', 'l', 'o', 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(positionToString(1, 4, buf.data()), "Hello");
}

// positionToString: requesting a string past the end-of-entry double-null
// returns "" (line 392 *target == '\0' branch).
TEST(SmbiosHelpers, PositionToStringPastEndReturnsEmpty)
{
    // structLen 4, then "A\0" then immediate terminator.
    std::vector<uint8_t> buf = {0, 0, 0, 0, 'A', 0, 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(positionToString(2, 4, buf.data()), "");
}

// positionToString: second string returned correctly.
TEST(SmbiosHelpers, PositionToStringSecondStringReturnsValue)
{
    std::vector<uint8_t> buf = {0, 0, 0, 0, 'A', 'B', 0, 'C', 'D', 0, 0};
    buf.resize(buf.size() + 512, 0); // ASAN: helpers read ahead
    EXPECT_EQ(positionToString(2, 4, buf.data()), "CD");
}

// positionToString: a string that never terminates within mdrSMBIOSSize hits
// the limit guard and returns "" (line 386 limit < 1 branch).
TEST(SmbiosHelpers, PositionToStringLimitGuardReturnsEmpty)
{
    std::vector<uint8_t> buf(static_cast<size_t>(mdrSMBIOSSize) + 16, 'A');
    // structLen 4; from buf+4 onward all 'A' (never null) -> limit underflows.
    EXPECT_EQ(positionToString(2, 4, buf.data()), "");
}

// getObjectPath builds inventoryPath + suffix + index.
TEST(SmbiosHelpers, GetObjectPathBuildsPath)
{
    std::string path = getObjectPath("/xyz/openbmc_project/inventory/system",
                                     "/motherboard", "/cpu", 3);
    EXPECT_EQ(path, "/xyz/openbmc_project/inventory/system/cpu3");
}

// decorateName without PLATFORM_PREFIX returns the path unmodified.
TEST(SmbiosHelpers, DecorateNameReturnsUnmodified)
{
    EXPECT_EQ(decorateName("/some/path/cpu0"), "/some/path/cpu0");
}

// ---------------------------------------------------------------------------
// In-process fake ObjectMapper running on a background thread. Unlike the
// fork()-based fake earlier in this file (which cannot own a bus name in this
// environment), an in-process second connection on its own io_context/thread
// can own the mapper name and answer the synchronous calls MDRV2 / System make
// during systemInfoUpdate(). This lets us drive the D-Bus dependent branches.
// ---------------------------------------------------------------------------
namespace
{
using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

class FakeObjectMapper
{
  public:
    FakeObjectMapper(std::vector<std::string> subTreePaths,
                     GetSubTreeType procModuleSubTree,
                     uint64_t instanceNumber) :
        paths(std::move(subTreePaths)),
        procModules(std::move(procModuleSubTree)), instance(instanceNumber)
    {
        std::promise<bool> readyPromise;
        auto readyFuture = readyPromise.get_future();
        worker = std::thread([this, &readyPromise]() { run(&readyPromise); });
        started = readyFuture.get();
    }

    ~FakeObjectMapper()
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

    // Add an inventory interface at runtime, which emits an InterfacesAdded
    // signal that MDRV2's match rules subscribe to.
    void emitInterface(const std::string& path, const std::string& iface)
    {
        auto emittedPromise = std::make_shared<std::promise<void>>();
        auto emittedFuture = emittedPromise->get_future();
        boost::asio::post(*io, [this, path, iface, emittedPromise]() {
            auto i = server->add_interface(path, iface);
            i->initialize(); // emits org.freedesktop.DBus.ObjectManager signal
            emitted.push_back(i);
            emittedPromise->set_value();
        });
        emittedFuture.wait();
    }

  private:
    void run(std::promise<bool>* readyPromise)
    {
        try
        {
            io = std::make_shared<boost::asio::io_context>();
            conn = std::make_shared<sdbusplus::asio::connection>(*io);
            conn->request_name("xyz.openbmc_project.ObjectMapper");
            // Also own the inventory service that the ProcessorModule subtree
            // points at, so Properties.Get(InstanceNumber) is answerable.
            conn->request_name("xyz.openbmc_project.FakeInventory");

            server = std::make_shared<sdbusplus::asio::object_server>(conn);

            auto iface =
                server->add_interface("/xyz/openbmc_project/object_mapper",
                                      "xyz.openbmc_project.ObjectMapper");
            interfaces.push_back(iface);

            auto localPaths = paths;
            iface->register_method(
                "GetSubTreePaths",
                [localPaths](const std::string&, int32_t,
                             const std::vector<std::string>&) {
                    return localPaths;
                });

            auto localTree = procModules;
            iface->register_method(
                "GetSubTree", [localTree](const std::string&, int32_t,
                                          const std::vector<std::string>&) {
                    return localTree;
                });

            iface->register_method(
                "GetObject",
                [](const std::string&, const std::vector<std::string>&) {
                    std::vector<
                        std::pair<std::string, std::vector<std::string>>>
                        ret{{"xyz.openbmc_project.FakeInventory",
                             {"xyz.openbmc_project.Software.Version"}}};
                    return ret;
                });
            iface->initialize();

            // Expose a ProcessorModule object with an InstanceNumber property,
            // matching the path advertised in procModules (if any).
            for (const auto& [modPath, services] : procModules)
            {
                auto modIface = server->add_interface(
                    modPath,
                    "xyz.openbmc_project.Inventory.Decorator.Instance");
                modIface->register_property<uint64_t>("InstanceNumber",
                                                      instance);
                modIface->initialize();
                interfaces.push_back(modIface);
            }

            readyPromise->set_value(true);
        }
        catch (const std::exception&)
        {
            readyPromise->set_value(false);
            return;
        }
        io->run();
        emitted.clear();
        interfaces.clear();
        server.reset();
        conn.reset();
    }

    std::vector<std::string> paths;
    GetSubTreeType procModules;
    uint64_t instance;
    std::shared_ptr<boost::asio::io_context> io;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::shared_ptr<sdbusplus::asio::object_server> server;
    std::thread worker;
    bool started{false};
    std::vector<std::shared_ptr<sdbusplus::asio::dbus_interface>> interfaces;
    std::vector<std::shared_ptr<sdbusplus::asio::dbus_interface>> emitted;
};
} // namespace

TEST_F(Mdrv2Fixture, InProcessFakeMapperThreadCanOwnNameAndAnswer)
{
    FakeObjectMapper mapper({"/xyz/openbmc_project/inventory/system/chassis/"
                             "motherboard"},
                            {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path = "/tmp/smbios2_mdrv2_inproc_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, InventoryInterfacesAddedSignalUpdatesInventory)
{
    FakeObjectMapper mapper({"/xyz/openbmc_project/inventory/system/chassis/"
                             "motherboard"},
                            {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path =
        "/tmp/smbios2_mdrv2_iface_added_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    mapper.emitInterface(
        "/xyz/openbmc_project/inventory/system/processor_module_added",
        phosphor::smbios::processorModuleInterface);

    boost::asio::steady_timer stopTimer(*io, std::chrono::milliseconds(500));
    stopTimer.async_wait(
        [this_io = io.get()](const boost::system::error_code&) {
            this_io->stop();
        });
    io->restart();
    io->run();
    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, MotherboardConfigInterfacesAddedSignalUpdatesInventory)
{
    FakeObjectMapper mapper({}, {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path =
        "/tmp/smbios2_mdrv2_board_added_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_NO_THROW(mdr.systemInfoUpdate());
    ASSERT_NE(mdr.motherboardConfigMatch, nullptr);

    mapper.emitInterface(
        "/xyz/openbmc_project/inventory/system/board/not_motherboard",
        "xyz.openbmc_project.Inventory.Item.Chassis");

    mapper.emitInterface(
        "/xyz/openbmc_project/inventory/system/board/motherboard",
        phosphor::smbios::systemInterface);

    boost::asio::steady_timer stopTimer(*io, std::chrono::seconds(3));
    stopTimer.async_wait(
        [this_io = io.get()](const boost::system::error_code&) {
            this_io->stop();
        });
    io->restart();
    io->run();
    std::remove(path.c_str());
}

TEST(Mdrv2SignalPredicates,
     InventoryInterfaceAddedMatchesOnlyInterestedInterfaces)
{
    phosphor::smbios::MDRV2::InventoryAddedInterfaceMap interfaces;
    EXPECT_FALSE(
        phosphor::smbios::MDRV2::inventoryInterfaceAddedMatches(interfaces));

    interfaces.emplace(phosphor::smbios::chassisInterface,
                       phosphor::smbios::MDRV2::InventoryAddedPropertyMap{});
    EXPECT_FALSE(
        phosphor::smbios::MDRV2::inventoryInterfaceAddedMatches(interfaces));

    interfaces.emplace(phosphor::smbios::processorModuleInterface,
                       phosphor::smbios::MDRV2::InventoryAddedPropertyMap{});
    EXPECT_TRUE(
        phosphor::smbios::MDRV2::inventoryInterfaceAddedMatches(interfaces));

    interfaces.clear();
    interfaces.emplace(phosphor::smbios::systemInterface,
                       phosphor::smbios::MDRV2::InventoryAddedPropertyMap{});
    EXPECT_TRUE(
        phosphor::smbios::MDRV2::inventoryInterfaceAddedMatches(interfaces));
}

TEST(Mdrv2SignalPredicates, MotherboardConfigMatchesSystemAndExactBoard)
{
    phosphor::smbios::MDRV2::MotherboardConfigData msgData;
    for (bool requireExactMatch : {false, true})
    {
        EXPECT_FALSE(phosphor::smbios::MDRV2::motherboardConfigMatches(
            msgData, requireExactMatch));
    }

    msgData.emplace(phosphor::smbios::boardInterface,
                    phosphor::smbios::MDRV2::MotherboardConfigProperties{});
    EXPECT_FALSE(
        phosphor::smbios::MDRV2::motherboardConfigMatches(msgData, false));
    EXPECT_TRUE(
        phosphor::smbios::MDRV2::motherboardConfigMatches(msgData, true));

    msgData.clear();
    msgData.emplace(phosphor::smbios::systemInterface,
                    phosphor::smbios::MDRV2::MotherboardConfigProperties{});
    EXPECT_TRUE(
        phosphor::smbios::MDRV2::motherboardConfigMatches(msgData, false));
}

TEST(Mdrv2Helpers, FindProcessorModuleForCpuCoversSelectionCases)
{
    using Module = phosphor::smbios::MDRV2::ProcessorModule;

    std::vector<Module> modules;
    EXPECT_EQ(phosphor::smbios::MDRV2::findProcessorModuleForCpu(modules, 7),
              nullptr);

    modules.push_back({"/module0", false, 0});
    const Module* module =
        phosphor::smbios::MDRV2::findProcessorModuleForCpu(modules, 7);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->path, "/module0");

    modules = {
        {"/module0", false, 0}, {"/module1", true, 8}, {"/module2", true, 7}};
    module = phosphor::smbios::MDRV2::findProcessorModuleForCpu(modules, 7);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->path, "/module2");

    EXPECT_EQ(phosphor::smbios::MDRV2::findProcessorModuleForCpu(modules, 9),
              nullptr);
}

TEST(Mdrv2Helpers, IndexedPathHelpersBuildExpectedNames)
{
    EXPECT_EQ(phosphor::smbios::MDRV2::indexedName("CPU_", 0), "CPU_0");
    EXPECT_EQ(
        phosphor::smbios::MDRV2::indexedName("ProcessorModule", "_Memory_", 12),
        "ProcessorModule_Memory_12");
    EXPECT_EQ(phosphor::smbios::MDRV2::indexedChildPath("/module0", "CPU_", 3),
              "/module0/CPU_3");
    EXPECT_EQ(phosphor::smbios::MDRV2::childPath("/inventory", "Memory_0"),
              "/inventory/Memory_0");
    EXPECT_EQ(phosphor::smbios::MDRV2::pcieObjectPath(
                  "/xyz/openbmc_project/inventory/system", "/motherboard", 2),
              "/xyz/openbmc_project/inventory/system/chassis/motherboard/"
              "pcieslot2");
    EXPECT_EQ(phosphor::smbios::MDRV2::systemObjectPath(
                  "/xyz/openbmc_project/inventory/system"),
              "/xyz/openbmc_project/inventory/system/chassis/motherboard/bios");
}

TEST(Mdrv2Helpers, ApplyProcessorModulePathBuildsCpuPathAndContainer)
{
    phosphor::smbios::MDRV2::ProcessorModule module{"/module0", true, 0};
    std::string path = "/xyz/openbmc_project/inventory/system/chassis/CPU0";
    std::string container = "/xyz/openbmc_project/inventory/system/chassis";

    phosphor::smbios::MDRV2::applyProcessorModulePath(module, path, container,
                                                      3);

#ifdef NVIDIA
    EXPECT_EQ(path, "/module0/CPU_3");
#else
    EXPECT_EQ(path, "/module0/cpu3");
#endif
    EXPECT_EQ(container, "/module0");
}

// ProcessorModule subtree with multiple service/interface combinations drives
// the GetSubTree result loop in systemInfoUpdate (mdrv2.cpp lines 547-583):
//  - module0: InstanceNumber Get succeeds (lines 567-570)
//  - module1: a non-matching interface precedes Decorator.Instance (line 555)
//  - module2: service name nobody owns -> Properties.Get throws (catch 573-577)
TEST_F(Mdrv2Fixture, FakeMapperProcessorModuleSubTreeDrivesInstanceLoop)
{
    const std::string base = "/xyz/openbmc_project/inventory/system/module";
    GetSubTreeType tree = {
        {base + "0",
         {{"xyz.openbmc_project.FakeInventory",
           {"xyz.openbmc_project.Inventory.Decorator.Instance"}}}},
        {base + "1",
         {{"xyz.openbmc_project.FakeInventory",
           {"xyz.openbmc_project.Inventory.Item.Board",
            "xyz.openbmc_project.Inventory.Decorator.Instance"}}}},
        {base + "2",
         {{"xyz.openbmc_project.NobodyOwnsThis",
           {"xyz.openbmc_project.Inventory.Decorator.Instance"}}}},
        {base + "3", {{"xyz.openbmc_project.FakeInventory", {}}}},
    };
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, tree, 7);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path = "/tmp/smbios2_mdrv2_procmod_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    EXPECT_NO_THROW(mdr.systemInfoUpdate());
    std::remove(path.c_str());
}

// Custom inventory path makes requireExactMatch == true.  GetSubTreePaths
// returns a non-matching path first (exercising the `continue` at line 434)
// then the exact custom path (matched and used).
TEST_F(Mdrv2Fixture, FakeMapperCustomPathRequireExactMatch)
{
    const std::string customInventory =
        "/xyz/openbmc_project/inventory/system/custom";
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/other", customInventory}, {},
        0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path =
        "/tmp/smbios2_mdrv2_custexact_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    customInventory);
    });
    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, FakeMapperCustomPathExactOnlyMatches)
{
    const std::string customInventory =
        "/xyz/openbmc_project/inventory/system/custom_exact_only";
    FakeObjectMapper mapper({customInventory}, {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path =
        "/tmp/smbios2_mdrv2_custonly_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbiosWithMemoryDevices(path, 1));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    customInventory);
    });
    std::remove(path.c_str());
}

// Build a fully-framed SMBIOS blob: a ProcessorModule baseboard (type 2,
// boardType 6) that contains the memory device handle, a processor (type 4),
// a memory device (type 17), and a PCIe slot (type 9).  Framing is verified
// in-test with the global SMBIOS helpers before use.
static std::vector<uint8_t> buildFullInventoryBlob()
{
    std::vector<uint8_t> data;
    auto appendStruct = [&](const void* p, size_t n) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(p);
        data.insert(data.end(), b, b + n);
    };
    auto appendStrings = [&](std::initializer_list<const char*> strs) {
        for (const char* s : strs)
        {
            data.insert(data.end(), s, s + std::strlen(s));
            data.push_back(0);
        }
        data.push_back(0); // end-of-strings terminator
    };

    EntryPointStructure30 ep30{};
    std::memcpy(ep30.anchorString, "_SM3_", 5);
    ep30.epLength = sizeof(EntryPointStructure30);
    ep30.smbiosVersion.majorVersion = 3;
    ep30.smbiosVersion.minorVersion = 0;
    ep30.structTableAddr = sizeof(EntryPointStructure30);
    appendStruct(&ep30, sizeof(ep30));

    constexpr uint16_t kMemHandle = 0x0100;

    TestBaseboardInfo bb{};
    bb.type = static_cast<uint8_t>(baseboardType);
    bb.length = static_cast<uint8_t>(sizeof(TestBaseboardInfo));
    bb.handle = 0x0001;
    bb.manufacturer = 1;
    bb.boardType = 6; // Baseboard::BoardType::ProcessorModule
    bb.numOfContainedObject = 1;
    bb.containedObjectHandles[0] = kMemHandle;
    appendStruct(&bb, sizeof(bb));
    appendStrings({"Nvidia"});

    TestProcessorInfo cpu{};
    cpu.type = static_cast<uint8_t>(processorsType);
    cpu.length = static_cast<uint8_t>(sizeof(TestProcessorInfo));
    cpu.handle = 0x0004;
    cpu.socketDesignation = 1;
    cpu.processorType = 3;
    cpu.status = 0x41;   // socket populated + enabled
    cpu.family = 0xb3;   // Xeon-family entry in familyTable (Intel decode)
    cpu.id = 0x000206d7; // family nibble 0xd? ensures decode path runs
    cpu.coreCount = 4;
    cpu.threadCount = 8;
    appendStruct(&cpu, sizeof(cpu));
    appendStrings({"CPU0"});

    phosphor::smbios::MemoryInfo mem{};
    mem.type = static_cast<uint8_t>(memoryDeviceType);
    mem.length = static_cast<uint8_t>(sizeof(phosphor::smbios::MemoryInfo));
    mem.handle = kMemHandle;
    mem.phyArrayHandle = 0x1000;
    mem.errInfoHandle = 0xFFFE;
    mem.totalWidth = 72;
    mem.dataWidth = 64;
    mem.size = 8192;
    mem.deviceLocator = 1;
    mem.bankLocator = 2;
    mem.memoryType = 0x1A;
    appendStruct(&mem, sizeof(mem));
    appendStrings({"DIMM_A1", "BANK_A"});

    TestSystemSlotInfo slot{};
    slot.type = static_cast<uint8_t>(systemSlots);
    slot.length = static_cast<uint8_t>(sizeof(TestSystemSlotInfo));
    slot.handle = 0x0009;
    slot.slotDesignation = 1;
    slot.slotType = 0xa5; // PCIe -> counted by getTotalSmbiosEntries
    appendStruct(&slot, sizeof(slot));
    appendStrings({"PCIe_x16"});

    // A second, non-PCIe slot (slotType not in pcieSmbiosType) so
    // getTotalSmbiosEntries exercises the find()==end() direction too.
    TestSystemSlotInfo isaSlot{};
    isaSlot.type = static_cast<uint8_t>(systemSlots);
    isaSlot.length = static_cast<uint8_t>(sizeof(TestSystemSlotInfo));
    isaSlot.handle = 0x000a;
    isaSlot.slotDesignation = 1;
    isaSlot.slotType = 0x01; // ISA -> not in pcieSmbiosType
    appendStruct(&isaSlot, sizeof(isaSlot));
    appendStrings({"ISA"});

    // Trailing zero padding so SMBIOS table walks terminate within bounds
    // (the helpers read ahead assuming a large storage buffer).
    data.resize(data.size() + 512, 0);
    return data;
}

static bool writeBlobToFile(const std::string& path,
                            const std::vector<uint8_t>& payload)
{
    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = static_cast<uint32_t>(payload.size());
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    return f.good();
}

TEST(Mdrv2Helpers, MemoryObjectNameUsesContainingBaseboard)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();
    std::vector<std::unique_ptr<phosphor::smbios::Baseboard>> baseboards;
    auto baseboard =
        std::make_unique<phosphor::smbios::Baseboard>(0, blob.data());
    baseboard->setName("ProcessorModule_0");
    baseboards.emplace_back(std::move(baseboard));

    EXPECT_EQ(phosphor::smbios::MDRV2::memoryObjectName(baseboards, 0x0100, 0),
              "ProcessorModule_0_Memory_0");
    EXPECT_EQ(phosphor::smbios::MDRV2::memoryObjectName(baseboards, 0x2222, 1),
              "Memory_1");
}

// Drives the CPU/DIMM/baseboard/PCIe creation loops in systemInfoUpdate while a
// fake mapper supplies a motherboard anchor and a ProcessorModule subtree.
TEST_F(Mdrv2Fixture, ConstructorFullInventoryUnderMapperDrivesAllLoops)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();

    // Verify the blob framing: every type must be locatable from the EP.
    uint8_t* base = blob.data();
    ASSERT_NE(getSMBIOSTypePtr(base, baseboardType), nullptr);
    ASSERT_NE(getSMBIOSTypePtr(base, processorsType), nullptr);
    ASSERT_NE(getSMBIOSTypePtr(base, memoryDeviceType), nullptr);
    ASSERT_NE(getSMBIOSTypePtr(base, systemSlots), nullptr);

    const std::string modPath = "/xyz/openbmc_project/inventory/system/module0";
    GetSubTreeType tree = {
        {modPath,
         {{"xyz.openbmc_project.FakeInventory",
           {"xyz.openbmc_project.Inventory.Decorator.Instance"}}}}};
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, tree, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path = "/tmp/smbios2_mdrv2_full_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(path, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

// getSMBIOSTypeIndexPtr: when smbiosNextPtr hits its endless-loop guard and
// returns null, the index loop returns null (smbios_mdrv2.hpp line 330).
TEST(SmbiosHelpers, GetSmbiosTypeIndexPtrNextPtrNullReturnsNull)
{
    std::vector<uint8_t> buf(static_cast<size_t>(mdrSMBIOSSize) + 32, 0x01);
    buf[0] = 17; // type matches
    buf[1] = 4;  // length; smbiosNextPtr starts at buf+4 -> all 0x01 -> null
    EXPECT_EQ(getSMBIOSTypeIndexPtr(buf.data(), 17, 1), nullptr);
}

// smbiosHandlePtr: a non-matching record followed by a region with no
// double-null makes smbiosNextPtr return null, exiting the while loop via its
// false branch (smbios_mdrv2.hpp line 348).
TEST(SmbiosHelpers, SmbiosHandlePtrNextPtrNullExitsLoop)
{
    std::vector<uint8_t> buf(static_cast<size_t>(mdrSMBIOSSize) + 32, 0x01);
    buf[0] = 17;   // type
    buf[1] = 4;    // length
    buf[2] = 0x11; // handle low
    buf[3] = 0x00; // handle high (0x0011, won't match query)
    EXPECT_EQ(smbiosHandlePtr(buf.data(), 0x9999), nullptr);
}

// checkSMBIOSVersion: place the "_SM_" anchor in the final bytes of the 64K
// storage buffer so fewer than sizeof(EntryPointStructure21) bytes remain.
// This takes the "Invalid entry point structure for SMBIOS 2.1" branch
// (mdrv2.cpp lines 887-889) that the usual anchor-at-start stubs never hit.
static bool writeSmbios21AnchorAtEnd(const std::string& path)
{
    std::vector<uint8_t> payload(smbiosTableStorageSize, 0);
    const char anchor[4] = {'_', 'S', 'M', '_'};
    std::copy(anchor, anchor + 4,
              payload.begin() + (smbiosTableStorageSize - 4));

    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = static_cast<uint32_t>(payload.size());

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    return f.good();
}

TEST_F(Mdrv2Fixture, ConstructorSmbios21AnchorAtEndTooShortHandled)
{
    std::string path = "/tmp/smbios2_mdrv2_sm21end_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbios21AnchorAtEnd(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

// checkSMBIOSVersion: only a "_SM3_" anchor near the buffer end (no "_SM_"
// anywhere) drives the SMBIOS-3.0 path, where fewer than
// sizeof(EntryPointStructure30) bytes remain -> "Invalid entry point structure
// for SMBIOS 3.0" branch (mdrv2.cpp lines 900-902).
static bool writeSmbios30AnchorAtEnd(const std::string& path)
{
    std::vector<uint8_t> payload(smbiosTableStorageSize, 0);
    const char anchor[5] = {'_', 'S', 'M', '3', '_'};
    std::copy(anchor, anchor + 5,
              payload.begin() + (smbiosTableStorageSize - 5));

    MDRSMBIOSHeader hdr{};
    hdr.dirVer = 1;
    hdr.mdrType = 2;
    hdr.dataSize = static_cast<uint32_t>(payload.size());

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    return f.good();
}

TEST_F(Mdrv2Fixture, ConstructorSmbios30AnchorAtEndTooShortHandled)
{
    std::string path = "/tmp/smbios2_mdrv2_sm30end_" + std::to_string(getpid());
    ASSERT_TRUE(writeSmbios30AnchorAtEnd(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

static std::vector<uint8_t> buildMinimalRecords(uint8_t type, size_t count)
{
    std::vector<uint8_t> data;
    data.reserve(count * 6 + 512);
    for (size_t index = 0; index < count; ++index)
    {
        data.push_back(type);
        data.push_back(sizeof(StructureHeader));
        data.push_back(static_cast<uint8_t>(index));
        data.push_back(static_cast<uint8_t>(index >> 8));
        data.push_back(0);
        data.push_back(0);
    }
    data.resize(data.size() + 512, 0);
    return data;
}

static uint8_t getDirectoryFlag(phosphor::smbios::MDRV2& mdr)
{
    auto dataInfo = mdr.getDataInformation(smbiosDirIndex);
    return dataInfo.at(sizeof(DataIdStruct) + 1);
}

TEST_F(Mdrv2Fixture, PrivateReadDataFromFlashRejectsNullArguments)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    MDRSMBIOSHeader header{};
    uint8_t data[8]{};
    EXPECT_FALSE(mdr.readDataFromFlash(nullptr, data));
    EXPECT_FALSE(mdr.readDataFromFlash(&header, nullptr));
}

TEST_F(Mdrv2Fixture, PrivateAvailabilityAndValidFlagStates)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(maxDirEntries));

    auto& entry = mdr.smbiosDir.dir[smbiosDirIndex];
    entry.stage = MDR2SMBIOSStatusEnum::mdr2Updating;
    entry.lock = MDR2DirLockEnum::mdr2DirUnlock;
    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(smbiosDirIndex));
    EXPECT_EQ(getDirectoryFlag(mdr),
              static_cast<uint8_t>(FlagStatus::flagIsInvalid));

    entry.stage = MDR2SMBIOSStatusEnum::mdr2Init;
    entry.lock = MDR2DirLockEnum::mdr2DirUnlock;
    EXPECT_TRUE(mdr.smbiosIsAvailForUpdate(smbiosDirIndex));
    EXPECT_EQ(getDirectoryFlag(mdr),
              static_cast<uint8_t>(FlagStatus::flagIsInvalid));

    entry.stage = MDR2SMBIOSStatusEnum::mdr2Loaded;
    entry.lock = MDR2DirLockEnum::mdr2DirLock;
    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(smbiosDirIndex));
    EXPECT_EQ(getDirectoryFlag(mdr),
              static_cast<uint8_t>(FlagStatus::flagIsLocked));

    entry.lock = MDR2DirLockEnum::mdr2DirUnlock;
    EXPECT_TRUE(mdr.smbiosIsAvailForUpdate(smbiosDirIndex));
    EXPECT_EQ(getDirectoryFlag(mdr),
              static_cast<uint8_t>(FlagStatus::flagIsValid));

    entry.stage = MDR2SMBIOSStatusEnum::mdr2Updated;
    EXPECT_TRUE(mdr.smbiosIsAvailForUpdate(smbiosDirIndex));
    EXPECT_EQ(getDirectoryFlag(mdr),
              static_cast<uint8_t>(FlagStatus::flagIsValid));

    entry.stage = static_cast<MDR2SMBIOSStatusEnum>(0xff);
    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(smbiosDirIndex));
    EXPECT_EQ(getDirectoryFlag(mdr),
              static_cast<uint8_t>(FlagStatus::flagIsInvalid));
}

TEST_F(Mdrv2Fixture, GetDataOfferThrowsWhenDirectoryLocked)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    auto& entry = mdr.smbiosDir.dir[smbiosDirIndex];
    entry.stage = MDR2SMBIOSStatusEnum::mdr2Loaded;
    entry.lock = MDR2DirLockEnum::mdr2DirLock;

    EXPECT_THROW(mdr.getDataOffer(), Mdrv2UpdateInProgress);
}

TEST_F(Mdrv2Fixture, DestructorHandlesNullInterfaceAndNullObjectServer)
{
    {
        auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
        mdr.smbiosInterface = nullptr;
    }

    {
        auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
        mdr.objServer = nullptr;
    }
}

TEST_F(Mdrv2Fixture, PrivateGetTotalSmbiosEntriesNullStorageReturnsNullopt)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = nullptr;
    EXPECT_FALSE(mdr.getTotalSmbiosEntries(memoryDeviceType).has_value());
}

TEST_F(Mdrv2Fixture, PrivateGetTotalSmbiosEntriesStopsAtLimit)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    auto data = buildMinimalRecords(memoryDeviceType,
                                    phosphor::smbios::limitEntryLen + 2);

    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = data.data();
    auto total = mdr.getTotalSmbiosEntries(memoryDeviceType);

    ASSERT_TRUE(total.has_value());
    EXPECT_EQ(*total, phosphor::smbios::limitEntryLen);
}

TEST_F(Mdrv2Fixture, PrivateGetTotalSmbiosEntriesNextPtrNullBreaks)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    std::vector<uint8_t> data(static_cast<size_t>(mdrSMBIOSSize) + 32, 0x01);
    data[0] = memoryDeviceType;
    data[1] = sizeof(StructureHeader);
    data[2] = 0;
    data[3] = 0;

    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = data.data();
    auto total = mdr.getTotalSmbiosEntries(memoryDeviceType);

    ASSERT_TRUE(total.has_value());
    EXPECT_EQ(*total, 1u);
}

TEST_F(Mdrv2Fixture, GetRecordTypeThrowsWhenStorageNull)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = nullptr;
    EXPECT_THROW(mdr.getRecordType(memoryDeviceType), std::runtime_error);
}

TEST_F(Mdrv2Fixture, GetRecordTypeStopsWhenNextPtrReturnsNull)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    std::vector<uint8_t> data(sizeof(phosphor::smbios::MemoryInfo) +
                                  static_cast<size_t>(mdrSMBIOSSize) + 32,
                              0x01);
    phosphor::smbios::MemoryInfo memoryInfo{};
    memoryInfo.type = memoryDeviceType;
    memoryInfo.length = sizeof(phosphor::smbios::MemoryInfo);
    std::memcpy(data.data(), &memoryInfo, sizeof(memoryInfo));

    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = data.data();
    auto records = mdr.getRecordType(memoryDeviceType);

    EXPECT_EQ(records.size(), 1u);
}

TEST_F(Mdrv2Fixture,
       AgentSynchronizeRestoresDirectoryEntriesWhenPropertyWasZero)
{
    std::string path = "/tmp/smbios2_mdrv2_dirzero_" + std::to_string(getpid());
    ASSERT_TRUE(writeMinimalSmbiosStub(path));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_EQ(mdr.directoryEntries(1), 0u);

    mdr.smbiosFilePath = path;
    EXPECT_TRUE(mdr.agentSynchronizeData());
    std::remove(path.c_str());
}

#ifdef PUBLISH_INVENTORY
TEST_F(Mdrv2Fixture, PrivateSystemInfoUpdateReturnsWhenStorageNull)
{
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = nullptr;

    EXPECT_NO_THROW(mdr.systemInfoUpdate());
    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = mdr.smbiosTableStorage;
}

TEST_F(Mdrv2Fixture,
       ConstructorFullInventoryWithUncontainedDimmCoversNoBoardMatch)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();
    auto* baseboard = reinterpret_cast<TestBaseboardInfo*>(
        blob.data() + sizeof(EntryPointStructure30));
    baseboard->containedObjectHandles[0] = 0x2222;

    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path =
        "/tmp/smbios2_mdrv2_uncontained_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(path, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture, PrivateSystemInfoUpdateCustomPathRunsExactMatchBranch)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();
    const std::string customInventory =
        "/xyz/openbmc_project/inventory/system/custom_exact";

    const std::string modPath =
        "/xyz/openbmc_project/inventory/system/custom_module0";
    GetSubTreeType tree = {
        {modPath,
         {{"xyz.openbmc_project.FakeInventory",
           {"xyz.openbmc_project.Inventory.Decorator.Instance"}}}}};
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/not_the_custom_path",
         customInventory},
        tree, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string smbiosPath =
        "/tmp/smbios2_mdrv2_update_custom_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(smbiosPath, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, smbiosPath,
                                phosphor::smbios::defaultObjectPath,
                                customInventory);

    EXPECT_NO_THROW(mdr.systemInfoUpdate());
    std::remove(smbiosPath.c_str());
}

TEST_F(Mdrv2Fixture, PrivateSystemInfoUpdateReusesMotherboardMatchRule)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();
    FakeObjectMapper mapper({}, {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string smbiosPath =
        "/tmp/smbios2_mdrv2_update_match_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(smbiosPath, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, smbiosPath,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    EXPECT_NO_THROW(mdr.systemInfoUpdate());
    if (mdr.motherboardConfigMatch == nullptr)
    {
        std::remove(smbiosPath.c_str());
        GTEST_SKIP()
            << "shared fake ObjectMapper was answered by another parallel test";
    }
    EXPECT_NO_THROW(mdr.systemInfoUpdate());
    std::remove(smbiosPath.c_str());
}

TEST_F(Mdrv2Fixture, SystemRegistrationCollisionPropagates)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    const std::string path =
        "/tmp/smbios2_mdrv2_system_collision_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(path, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);

    using UUIDObject = sdbusplus::server::object_t<
        sdbusplus::xyz::openbmc_project::Common::server::UUID>;
    const std::string systemPath = phosphor::smbios::MDRV2::systemObjectPath(
        phosphor::smbios::defaultInventoryPath);
    UUIDObject blocker(*conn, systemPath.c_str());

    EXPECT_THROW(mdr.systemInfoUpdate(), sdbusplus::exception::SdBusError);
    std::remove(path.c_str());
}

TEST(Mdrv2MockBus, MotherboardMatchRegistrationErrorPropagates)
{
    testing::NiceMock<sdbusplus::SdBusMock> mock;
    ON_CALL(mock, sd_bus_call(testing::_, testing::_, testing::_, testing::_,
                              testing::_))
        .WillByDefault(testing::Return(-EIO));

    auto mockIo = std::make_shared<boost::asio::io_context>();
    auto mockBus = sdbusplus::get_mocked_new(&mock);
    auto mockConn = std::make_shared<sdbusplus::asio::connection>(
        *mockIo, std::move(mockBus));
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(mockConn);
    phosphor::smbios::MDRV2 mdr(
        mockIo, mockConn, objServer, "/nonexistent/smbios2",
        phosphor::smbios::defaultObjectPath,
        phosphor::smbios::defaultInventoryPath);

    EXPECT_CALL(mock, sd_bus_add_match(testing::_, testing::_, testing::_,
                                       testing::_, testing::_))
        .WillOnce(testing::Return(-EIO));
    EXPECT_THROW(mdr.systemInfoUpdate(), sdbusplus::exception::SdBusError);
}

#ifdef PROCMOD_DBUS
TEST_F(Mdrv2Fixture, DefaultBaseboardTypeKeepsGenericName)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();
    auto* baseboard = reinterpret_cast<TestBaseboardInfo*>(
        blob.data() + sizeof(EntryPointStructure30));
    baseboard->boardType = static_cast<uint8_t>(
        phosphor::smbios::Baseboard::BoardType::InterconnectBoard);

    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, {}, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    const std::string path =
        "/tmp/smbios2_mdrv2_default_board_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(path, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    {
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
        EXPECT_NO_THROW(mdr.systemInfoUpdate());
        ASSERT_EQ(mdr.baseboards.size(), 1u);
        EXPECT_EQ(mdr.baseboards.front()->getName(), "Board_0");
    }
    std::remove(path.c_str());
}
#endif

#endif

#ifdef NVIDIA
TEST_F(Mdrv2Fixture,
       ConstructorFullInventoryNvidiaMatchesProcessorModuleByInstance)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();

    const std::string modBase =
        "/xyz/openbmc_project/inventory/system/nvidia_module";
    GetSubTreeType tree = {
        {modBase + "0", {{"xyz.openbmc_project.FakeInventory", {}}}},
        {modBase + "1",
         {{"xyz.openbmc_project.FakeInventory",
           {"xyz.openbmc_project.Inventory.Decorator.Instance"}}}},
    };
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, tree, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path = "/tmp/smbios2_mdrv2_nvidia_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(path, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}

TEST_F(Mdrv2Fixture,
       ConstructorFullInventoryNvidiaSingleProcessorModuleShortCircuits)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();

    const std::string modPath =
        "/xyz/openbmc_project/inventory/system/nvidia_single";
    GetSubTreeType tree = {
        {modPath, {{"xyz.openbmc_project.FakeInventory", {}}}},
    };
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, tree, 0);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path =
        "/tmp/smbios2_mdrv2_nvidia_single_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(path, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}
#endif

#ifdef NVIDIA
TEST_F(Mdrv2Fixture,
       ConstructorFullInventoryNvidiaNoProcessorModuleInstanceMatch)
{
    std::vector<uint8_t> blob = buildFullInventoryBlob();

    const std::string modPath =
        "/xyz/openbmc_project/inventory/system/nvidia_nomatch";
    GetSubTreeType tree = {
        {modPath + "0",
         {{"xyz.openbmc_project.FakeInventory",
           {"xyz.openbmc_project.Inventory.Decorator.Instance"}}}},
        {modPath + "1",
         {{"xyz.openbmc_project.FakeInventory",
           {"xyz.openbmc_project.Inventory.Decorator.Instance"}}}},
    };
    FakeObjectMapper mapper(
        {"/xyz/openbmc_project/inventory/system/chassis/motherboard"}, tree,
        99);
    if (!mapper.ok())
    {
        GTEST_SKIP() << "fake ObjectMapper unavailable in this environment";
    }

    std::string path =
        "/tmp/smbios2_mdrv2_nvidia_nomatch_" + std::to_string(getpid());
    ASSERT_TRUE(writeBlobToFile(path, blob));

    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    EXPECT_NO_THROW({
        phosphor::smbios::MDRV2 mdr(io, conn, objServer, path,
                                    phosphor::smbios::defaultObjectPath,
                                    phosphor::smbios::defaultInventoryPath);
    });
    std::remove(path.c_str());
}
#endif

// ---------------------------------------------------------------------------
// Direct unit tests for private MDRV2 methods (accessible via
// #define private public above).  These cover branches that are
// unreachable through the public API because they require specific internal
// state (stage/lock fields, null pointers, etc.).
// ---------------------------------------------------------------------------

TEST_F(Mdrv2Fixture, SmbiosIsAvailForUpdateOutOfBoundsReturnsFalse)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(maxDirEntries));
    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(maxDirEntries + 5));
}

TEST_F(Mdrv2Fixture, SmbiosIsAvailForUpdateUpdatingStageReturnsFalse)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[0].stage = MDR2SMBIOSStatusEnum::mdr2Updating;
    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(0));
}

TEST_F(Mdrv2Fixture, SmbiosIsAvailForUpdateLockedLoadedReturnsFalse)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[0].stage = MDR2SMBIOSStatusEnum::mdr2Loaded;
    mdr.smbiosDir.dir[0].lock = MDR2DirLockEnum::mdr2DirLock;
    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(0));
}

TEST_F(Mdrv2Fixture, SmbiosIsAvailForUpdateLockedUpdatedReturnsFalse)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[0].stage = MDR2SMBIOSStatusEnum::mdr2Updated;
    mdr.smbiosDir.dir[0].lock = MDR2DirLockEnum::mdr2DirLock;
    EXPECT_FALSE(mdr.smbiosIsAvailForUpdate(0));
}

TEST_F(Mdrv2Fixture, GetDataOfferWhenUpdatingThrowsUpdateInProgress)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[0].stage = MDR2SMBIOSStatusEnum::mdr2Updating;
    EXPECT_THROW(mdr.getDataOffer(), Mdrv2UpdateInProgress);
}

TEST_F(Mdrv2Fixture, SmbiosValidFlagUpdatingStageFlagInvalid)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[0].stage = MDR2SMBIOSStatusEnum::mdr2Updating;
    EXPECT_EQ(mdr.smbiosValidFlag(0),
              static_cast<uint8_t>(FlagStatus::flagIsInvalid));
}

TEST_F(Mdrv2Fixture, SmbiosValidFlagInitStageFlagInvalid)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[0].stage = MDR2SMBIOSStatusEnum::mdr2Init;
    mdr.smbiosDir.dir[0].lock = MDR2DirLockEnum::mdr2DirUnlock;
    EXPECT_EQ(mdr.smbiosValidFlag(0),
              static_cast<uint8_t>(FlagStatus::flagIsInvalid));
}

TEST_F(Mdrv2Fixture, SmbiosValidFlagLockedLoadedFlagLocked)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[0].stage = MDR2SMBIOSStatusEnum::mdr2Loaded;
    mdr.smbiosDir.dir[0].lock = MDR2DirLockEnum::mdr2DirLock;
    EXPECT_EQ(mdr.smbiosValidFlag(0),
              static_cast<uint8_t>(FlagStatus::flagIsLocked));
}

TEST_F(Mdrv2Fixture, ReadDataFromFlashNullMdrHdrReturnsFalse)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    uint8_t data[64] = {};
    EXPECT_FALSE(mdr.readDataFromFlash(nullptr, data));
}

TEST_F(Mdrv2Fixture, ReadDataFromFlashNullDataReturnsFalse)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    MDRSMBIOSHeader hdr{};
    EXPECT_FALSE(mdr.readDataFromFlash(&hdr, nullptr));
}

TEST_F(Mdrv2Fixture, GetTotalSmbiosEntriesNullDataStorageReturnsNullopt)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = nullptr;
    auto result = mdr.getTotalSmbiosEntries(memoryDeviceType);
    EXPECT_FALSE(result.has_value());
    auto result2 = mdr.getTotalSmbiosEntries(processorsType);
    EXPECT_FALSE(result2.has_value());
    auto result3 = mdr.getTotalSmbiosEntries(systemSlots);
    EXPECT_FALSE(result3.has_value());
}

TEST_F(Mdrv2Fixture, SystemInfoUpdateNullDataStorageExitsEarlyWithoutCrash)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[smbiosDirIndex].dataStorage = nullptr;
    EXPECT_NO_THROW(mdr.systemInfoUpdate());
}

TEST_F(Mdrv2Fixture, SmbiosIsAvailForUpdateInitStageUnlockedReturnsTrue)
{
    auto objServer = std::make_shared<sdbusplus::asio::object_server>(conn);
    phosphor::smbios::MDRV2 mdr(io, conn, objServer, "/nonexistent/smbios2",
                                phosphor::smbios::defaultObjectPath,
                                phosphor::smbios::defaultInventoryPath);
    mdr.smbiosDir.dir[0].stage = MDR2SMBIOSStatusEnum::mdr2Init;
    mdr.smbiosDir.dir[0].lock = MDR2DirLockEnum::mdr2DirUnlock;
    EXPECT_TRUE(mdr.smbiosIsAvailForUpdate(0));
}
