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
#include "mdrv2.hpp"
#include "smbios_mdrv2.hpp"
#include "test_mock_helpers.hpp"

#include <unistd.h>

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Smbios/MDR_V2/error.hpp>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

using Mdrv2InvalidParameter =
    sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::InvalidParameter;
using Mdrv2InvalidId =
    sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::InvalidId;

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
            connMapper =
                std::make_shared<sdbusplus::asio::connection>(*ioMapper);
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
