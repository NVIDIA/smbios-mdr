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
#include "dimm.hpp"
#include "smbios_mdrv2.hpp"
#include "test_mock_helpers.hpp"

#include <unistd.h>

#include <sdbusplus/exception.hpp>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::smbios;
using namespace phosphor::smbios::test;

class DimmTest : public TestFixtureBase
{
  protected:
    void SetUp() override
    {
        TestFixtureBase::SetUp();
    }

    void TearDown() override
    {
        TestFixtureBase::TearDown();
    }

    std::vector<uint8_t> createSMBIOSMemoryDevice(uint8_t dimmIndex = 0)
    {
        std::vector<uint8_t> data;

        struct MemoryInfo memInfo{};
        memInfo.type = memoryDeviceType;
        memInfo.length = sizeof(MemoryInfo);
        memInfo.handle = 0x1000 + dimmIndex;
        memInfo.phyArrayHandle = 0x0001;
        memInfo.errInfoHandle = 0xFFFE;
        memInfo.totalWidth = 72;
        memInfo.dataWidth = 64;
        memInfo.size = 8192;
        memInfo.formFactor = 0x09;
        memInfo.deviceSet = 0;
        memInfo.deviceLocator = 1;
        memInfo.bankLocator = 2;
        memInfo.memoryType = 0x1A;
        memInfo.typeDetail = 0x0080;
        memInfo.speed = 3200;
        memInfo.manufacturer = 3;
        memInfo.serialNum = 4;
        memInfo.assetTag = 5;
        memInfo.partNum = 6;
        memInfo.attributes = 0;
        memInfo.extendedSize = 0;
        memInfo.confClockSpeed = 3200;
        memInfo.memoryTechnology = 0x03;

        uint8_t* ptr = reinterpret_cast<uint8_t*>(&memInfo);
        data.insert(data.end(), ptr, ptr + sizeof(MemoryInfo));

        std::string strings[] = {"DIMM0", "BANK0", "Manufacturer",
                                 "SN123", "TAG",   "PN123"};
        for (const auto& str : strings)
        {
            data.insert(data.end(), str.begin(), str.end());
            data.push_back(0);
        }
        data.push_back(0);
        data.push_back(0); /* SMBIOS string table double-null terminator */
        /* Pad so getSMBIOSTypePtr never reads past buffer when walking tables
         */
        while (data.size() < 128)
            data.push_back(0);

        return data;
    }

    void createType16PhysicalMemoryArray(uint8_t* buffer, uint16_t handle,
                                         uint8_t errorCorrection)
    {
        PhysicalMemoryArrayInfo type16{};
        type16.type = physicalMemoryArrayType;
        type16.length = 23;
        type16.handle = handle;
        type16.location = 0x01;
        type16.use = 0x03;
        type16.memoryErrorCorrection = errorCorrection;
        type16.maximumCapacity = 0;
        type16.memoryErrorInformationHandle = 0xFFFE;
        type16.numberOfMemoryDevices = 1;
        type16.extendedMaximumCapacity = 0;

        std::memcpy(buffer, &type16, sizeof(PhysicalMemoryArrayInfo));
        buffer[23] = 0;
        buffer[24] = 0;
    }
};

TEST_F(DimmTest, MemoryInfoUpdateNullStorage)
{
    uint8_t dimmId = 0;
    uint8_t* storage = nullptr;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateEmptyStorage)
{
    uint8_t dimmId = 0;
    uint8_t storage[100] = {0};
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateNoType17)
{
    uint8_t dimmId = 0;
    uint8_t storage[128] = {0};
    storage[0] = 0x09;
    storage[1] = 13;
    storage[13] = 0;
    storage[14] = 0;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateWithValidData)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, DuplicateObjectPathRegistrationThrows)
{
    auto smbiosData = createSMBIOSMemoryDevice(0);
    std::string path = "/xyz/openbmc_project/test/inventory/system/dimm0";
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    Dimm dimm(*bus, path, 0, smbiosData.data(), motherboard);
    EXPECT_ANY_THROW({
        Dimm duplicate(*bus, path, 1, smbiosData.data(), motherboard);
    });
}

TEST_F(DimmTest, ManufacturerNoDimm)
{
    auto storage = createSMBIOSMemoryDevice();
    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    size_t off = 1 + 6 + 1 + 6 + 1;
    const char* noDimm = "NO DIMM";
    std::memcpy(strStart + off, noDimm, 8);
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, UpdateEccTypeEccNotInMapSetsNoEcc)
{
    auto data = createSMBIOSMemoryDevice(0);
    uint8_t type16Buf[32] = {0};
    createType16PhysicalMemoryArray(type16Buf, 0x0001, 0xFF);
    data.insert(data.end(), type16Buf, type16Buf + 25);
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), motherboard);
    });
}

TEST_F(DimmTest, UpdateEccTypeType16MatchingHandleSetsEccFromMap)
{
    auto data = createSMBIOSMemoryDevice(0);
    uint8_t type16Buf[32] = {0};
    createType16PhysicalMemoryArray(type16Buf, 0x0001, 0x05);
    data.insert(data.end(), type16Buf, type16Buf + 25);
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateSizeMaxOldUsesExtendedSize)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->size = 0x7fff; /* maxOldDimmSize -> dimmSizeExt branch */
    memInfo->extendedSize = 32768;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateExtendedSize)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->size = 0x7fff;
    memInfo->extendedSize = 16384;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateSizeZero)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->size = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateFormFactorKeyNotInMapUsesRDIMM)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->formFactor =
        0xFF; /* not in dimmFormFactorMap -> defaults to RDIMM */

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateDimmSizeNewVersionEncoding)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    /* size with bit 0x8000 set: uses else branch (no * 1024) in dimmSize() */
    memInfo->size = 0x8001;
    memInfo->extendedSize = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateMemoryTypeUnknown)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->memoryType = 0xFF;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateMemoryTechnologyUnknown)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->memoryTechnology =
        0xFF; /* not in dimmMemoryTechTypeMap -> MemoryTechType::Unknown */

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateTypeDetailMultipleBits)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->typeDetail = 0x0003; /* multiple bits set -> dimmTypeDetail loop */

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateDeviceLocatorCpuAndDimmFallbackNoConfig)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));
    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->deviceLocator = 1; /* SMBIOS string 1 = "CPU0" */
    memInfo->bankLocator = 2;   /* SMBIOS string 2 = "DIMM_A" */
    /* Overwrite string table: pos 1 "CPU0", pos 2 "DIMM_A", then rest */
    uint8_t* strStart = storage + sizeof(MemoryInfo);
    const char newStrings[] =
        "\0CPU0\0DIMM_A\0Manufacturer\0SN123\0TAG\0PN123\0\0";
    std::memcpy(strStart, newStrings, sizeof(newStrings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateDeviceLocatorBankPlusDeviceSingleLetterSlot)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));
    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;
    uint8_t* strStart = storage + sizeof(MemoryInfo);
    const char newStrings[] =
        "\0DIMM_C\0BANK1\0Manufacturer\0SN123\0TAG\0PN123\0\0";
    std::memcpy(strStart, newStrings, sizeof(newStrings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

// Config file present and the device locator matches an entry: drives the
// non-empty parse path and the matching-entry branch (socket/controller/slot/
// channel setters) in dimmDeviceLocator().
TEST_F(DimmTest, MemoryInfoUpdateDeviceLocatorConfigFileMatch)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));
    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->deviceLocator = 1; /* "DIMM_A" */
    memInfo->bankLocator = 0;   /* empty -> result == deviceLocator */
    uint8_t* strStart = storage + sizeof(MemoryInfo);
    const char newStrings[] = "\0DIMM_A\0Manufacturer\0SN123\0TAG\0PN123\0\0";
    std::memcpy(strStart, newStrings, sizeof(newStrings));

    std::string cfg =
        "/tmp/dimm_cfg_match_" + std::to_string(getpid()) + ".json";
    {
        std::ofstream f(cfg);
        f << R"({"DIMM_A":{"MemoryController":1,"Socket":0,"Slot":2,)"
          << R"("Channel":3}})";
    }

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard, cfg);
    });
    {
        std::error_code removeEc;
        std::filesystem::remove(cfg, removeEc);
    }
}

// Config file present but the device locator is not in it: drives the
// no-matching-entry (else) branch in dimmDeviceLocator().
TEST_F(DimmTest, MemoryInfoUpdateDeviceLocatorConfigFileNoMatch)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));
    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->deviceLocator = 1; /* "DIMM_Z" (absent from config) */
    memInfo->bankLocator = 0;
    uint8_t* strStart = storage + sizeof(MemoryInfo);
    const char newStrings[] = "\0DIMM_Z\0Manufacturer\0SN123\0TAG\0PN123\0\0";
    std::memcpy(strStart, newStrings, sizeof(newStrings));

    std::string cfg =
        "/tmp/dimm_cfg_nomatch_" + std::to_string(getpid()) + ".json";
    {
        std::ofstream f(cfg);
        f << R"({"DIMM_A":{"MemoryController":1,"Socket":0,"Slot":2,)"
          << R"("Channel":3}})";
    }

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard, cfg);
    });
    {
        std::error_code removeEc;
        std::filesystem::remove(cfg, removeEc);
    }
}

// Device locator with a multi-letter slot suffix ("DIMM_AB"): the slot regex
// matches alphabetic but length != 1, taking the false side of that check.
TEST_F(DimmTest, MemoryInfoUpdateDeviceLocatorMultiLetterSlot)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));
    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->deviceLocator = 1; /* "DIMM_AB" */
    memInfo->bankLocator = 0;
    uint8_t* strStart = storage + sizeof(MemoryInfo);
    const char newStrings[] = "\0DIMM_AB\0Manufacturer\0SN123\0TAG\0PN123\0\0";
    std::memcpy(strStart, newStrings, sizeof(newStrings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateMultipleDIMMs)
{
    uint8_t dimmId = 1;
    auto smbiosData1 = createSMBIOSMemoryDevice(0);
    auto smbiosData2 = createSMBIOSMemoryDevice(1);

    uint8_t storage[4096] = {0};
    std::memcpy(storage, smbiosData1.data(), smbiosData1.size());
    std::memcpy(storage + smbiosData1.size(), smbiosData2.data(),
                smbiosData2.size());

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm1",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, FormFactorSetterVarious)
{
    uint8_t dimmId = 0;
    uint8_t storage[100] = {0};
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", dimmId,
              storage, motherboard);

    using FormFactor = sdbusplus::server::xyz::openbmc_project::inventory::
        item::Dimm::FormFactor;

    EXPECT_NO_THROW({
        dimm.formFactor(FormFactor::RDIMM);
        dimm.formFactor(FormFactor::UDIMM);
        dimm.formFactor(FormFactor::SO_DIMM);
        dimm.formFactor(FormFactor::LRDIMM);
    });
}

TEST_F(DimmTest, PublicMethodsComprehensive)
{
    uint8_t dimmId = 0;
    uint8_t storage[100] = {0};
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", dimmId,
              storage, motherboard);

    EXPECT_NO_THROW({
        dimm.memoryDataWidth(64);
        dimm.memoryTotalWidth(72);
        dimm.memorySizeInKB(8388608);
        dimm.memoryDeviceLocator("DIMM0");
        dimm.memoryType(sdbusplus::server::xyz::openbmc_project::inventory::
                            item::Dimm::DeviceType::DDR4);
        dimm.memoryTypeDetail("Synchronous");
        dimm.maxMemorySpeedInMhz(3200);
        dimm.manufacturer("TestManufacturer");
        dimm.present(true);
        dimm.serialNumber("SN123456");
        dimm.partNumber("PN123456");
        dimm.memoryAttributes(0x01);
        dimm.memoryMedia(sdbusplus::server::xyz::openbmc_project::inventory::
                             item::Dimm::MemoryTech::DRAM);
        dimm.slot(0);
        dimm.socket(0);
        dimm.memoryController(0);
        dimm.channel(0);
        dimm.memoryConfiguredSpeedInMhz(3200);
        dimm.functional(true);
        dimm.ecc(sdbusplus::server::xyz::openbmc_project::inventory::item::
                     Dimm::Ecc::NoECC);
        dimm.formFactor(sdbusplus::server::xyz::openbmc_project::inventory::
                            item::Dimm::FormFactor::RDIMM);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateEmptyMotherboard)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    uint8_t storage[2048] = {0};
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));
    std::string motherboard = "";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, MemoryInfoUpdateNullDuringIteration)
{
    uint8_t dimmId = 1;
    uint8_t storage[512] = {0};

    storage[0] = memoryDeviceType;
    storage[1] = 50;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm1",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, UpdateFormFactorKnownValue)
{
    uint8_t dimmId = 0;
    uint8_t storage[512] = {0};

    auto smbiosData = createSMBIOSMemoryDevice(0);
    std::memcpy(storage, smbiosData.data(),
                std::min(smbiosData.size(), sizeof(storage)));

    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage);
    memInfo->formFactor = 0x09;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, UpdateEccTypeWithPhysicalMemoryArray)
{
    uint8_t dimmId = 0;
    uint8_t storage[1000] = {0};

    createType16PhysicalMemoryArray(storage, 0x0001, 0x03);

    auto smbiosData = createSMBIOSMemoryDevice(0);
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(smbiosData.data());
    memInfo->phyArrayHandle = 0x0001;

    size_t type17Offset = 30;
    std::memcpy(storage + type17Offset, smbiosData.data(), smbiosData.size());

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, FormFactorUnknownDefaultsToRDIMM)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->formFactor = 0xFF;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, FormFactorDie)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->formFactor = 0x10;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DimmSizeExtended)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->size = 0x7FFF;
    memInfo->extendedSize = 16384;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DimmSizeNewVersionFormat)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->size = 0x8000 | 8192;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DimmSizeZeroNotPresent)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->size = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, PartNumberTrimming)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string partNum = "ABCD123  ";
    std::memcpy(strStart, partNum.c_str(), partNum.length());
    strStart[partNum.length()] = 0;
    memInfo->partNum = 1;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, MemoryTypeUnknown)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->memoryType = 0xFF;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, MemoryTechnologyUnknown)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->memoryTechnology = 0xFF;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, TypeDetailMultipleBits)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->typeDetail = 0x0083;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, EmptyMotherboardPath)
{
    auto storage = createSMBIOSMemoryDevice();
    std::string motherboard = "";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorOnlyDimmLocationCode)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "DIMM_A";
    std::memcpy(strStart, deviceLoc.c_str(), deviceLoc.length());
    strStart += deviceLoc.length() + 1;
    std::string bankLoc = "";
    std::memcpy(strStart, bankLoc.c_str(), bankLoc.length());
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorWithConfigFile)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "DIMM_A";
    std::memcpy(strStart, deviceLoc.c_str(), deviceLoc.length());
    strStart += deviceLoc.length() + 1;
    std::string bankLoc = "BANK0";
    std::memcpy(strStart, bankLoc.c_str(), bankLoc.length());
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorNoCpuNoConfig)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "DIMM_A";
    std::memcpy(strStart, deviceLoc.c_str(), deviceLoc.length());
    strStart += deviceLoc.length() + 1;
    std::string bankLoc = "BANK0";
    std::memcpy(strStart, bankLoc.c_str(), bankLoc.length());
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorDimmNotSingleLetter)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "DIMM_AB";
    std::memcpy(strStart, deviceLoc.c_str(), deviceLoc.length());
    strStart += deviceLoc.length() + 1;
    std::string bankLoc = "BANK0";
    std::memcpy(strStart, bankLoc.c_str(), bankLoc.length());
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, UpdateEccTypeNotFound)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(smbiosData.data());
    memInfo->phyArrayHandle = 0x9999;

    uint8_t storage[1000] = {0};
    std::memcpy(storage, smbiosData.data(), smbiosData.size());

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, UpdateEccTypeNoMatchingHandle)
{
    uint8_t dimmId = 0;
    uint8_t storage[1000] = {0};
    createType16PhysicalMemoryArray(storage, 0x0001, 0x03);
    createType16PhysicalMemoryArray(storage + 25, 0x0002, 0x03);

    auto smbiosData = createSMBIOSMemoryDevice(0);
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(smbiosData.data());
    memInfo->phyArrayHandle = 0x9999;

    size_t type17Offset = 50;
    std::memcpy(storage + type17Offset, smbiosData.data(), smbiosData.size());

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, UpdateEccTypeNoPhysicalMemoryArray)
{
    uint8_t dimmId = 0;
    auto smbiosData = createSMBIOSMemoryDevice(0);
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(smbiosData.data());
    memInfo->phyArrayHandle = 0x0001;

    uint8_t storage[1000] = {0};
    std::memcpy(storage, smbiosData.data(), smbiosData.size());

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                  dimmId, storage, motherboard);
    });
}

TEST_F(DimmTest, UpdateEccTypeAllMappings)
{
    uint8_t eccTypes[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    for (auto eccType : eccTypes)
    {
        uint8_t dimmId = 0;
        uint8_t storage[1000] = {0};
        createType16PhysicalMemoryArray(storage, 0x0001, eccType);

        auto smbiosData = createSMBIOSMemoryDevice(0);
        MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(smbiosData.data());
        memInfo->phyArrayHandle = 0x0001;

        size_t type17Offset = 30;
        std::memcpy(storage + type17Offset, smbiosData.data(),
                    smbiosData.size());

        std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

        EXPECT_NO_THROW({
            Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                      dimmId, storage, motherboard);
        });
    }
}

TEST_F(DimmTest, DeviceLocatorWithBankLocator)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "DIMM_A";
    std::memcpy(strStart, deviceLoc.c_str(), deviceLoc.length());
    strStart += deviceLoc.length() + 1;
    std::string bankLoc = "BANK0";
    std::memcpy(strStart, bankLoc.c_str(), bankLoc.length());
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorWithCpuString)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "CPU0_DIMM_A";
    std::memcpy(strStart, deviceLoc.c_str(), deviceLoc.length());
    strStart += deviceLoc.length() + 1;
    std::string bankLoc = "";
    std::memcpy(strStart, bankLoc.c_str(), bankLoc.length());
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorCpuStringInvalidNumber)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "CPUX_DIMM_A";
    std::memcpy(strStart, deviceLoc.c_str(), deviceLoc.length());
    strStart += deviceLoc.length() + 1;
    std::string bankLoc = "";
    std::memcpy(strStart, bankLoc.c_str(), bankLoc.length());
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    // A non-numeric CPU number in the device locator must be handled
    // gracefully (std::stoi's std::invalid_argument is caught) instead of
    // propagating out of the constructor and crashing the daemon.
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, MemoryDeviceShortLengthRejected)
{
    auto storage = createSMBIOSMemoryDevice();
    // Declared record length shorter than the parsed MemoryInfo struct: the
    // size guard rejects it and the DIMM is left unpopulated (no crash).
    storage[1] = 8;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorStringUnterminatedBounded)
{
    // Full-size backing buffer with no string terminator anywhere after the
    // record: every walk and string read is bounded by the real buffer end
    // instead of running past it, and returns empty without crashing.
    std::vector<uint8_t> storage(smbiosTableStorageSize, 0xFF);
    struct MemoryInfo memInfo{};
    memInfo.type = memoryDeviceType;
    memInfo.length = sizeof(MemoryInfo);
    memInfo.handle = 0x1000;
    memInfo.deviceLocator = 1;
    memInfo.bankLocator = 0;
    memInfo.manufacturer = 0;
    memInfo.serialNum = 0;
    memInfo.assetTag = 0;
    memInfo.partNum = 0;
    std::memcpy(storage.data(), &memInfo, sizeof(MemoryInfo));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorDimmSingleLetter)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());

    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "DIMM_A";
    std::memcpy(strStart, deviceLoc.c_str(), deviceLoc.length());
    strStart += deviceLoc.length() + 1;
    std::string bankLoc = "";
    std::memcpy(strStart, bankLoc.c_str(), bankLoc.length());
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DimmSizeOldVersion)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->size = 0x7FFF;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DimmSizeNewVersionWithBase)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->size = 0x8000 | 4096;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, FormFactorDieCoversMapLookup)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->formFactor = 0x10;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

static std::string makeTempConfigPath()
{
    static int counter = 0;
    std::filesystem::path p = std::filesystem::temp_directory_path();
    p /= "smbios_dimm_test_" + std::to_string(static_cast<long>(getpid())) +
         "_" + std::to_string(counter++) + ".json";
    return p.string();
}

TEST_F(DimmTest, ParseConfigFile_NonexistentPath_ReturnsEmpty)
{
    auto storage = createSMBIOSMemoryDevice();
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
              storage.data(), motherboard);

    Json result = dimm.parseConfigFile("/nonexistent/memoryLocationTable.json");
    EXPECT_TRUE(result.empty());
}

TEST_F(DimmTest, ParseConfigFile_ValidJson_ReturnsParsedData)
{
    std::string path = makeTempConfigPath();
    std::ofstream f(path);
    f << R"({"DIMM0":{"MemoryController":1,"Socket":2,"Slot":3,"Channel":4}})";
    f.close();

    auto storage = createSMBIOSMemoryDevice();
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
              storage.data(), motherboard);

    Json result = dimm.parseConfigFile(path);
    std::filesystem::remove(path);

    ASSERT_FALSE(result.empty());
    EXPECT_TRUE(result.contains("DIMM0"));
    EXPECT_EQ(result["DIMM0"]["MemoryController"].get<uint8_t>(), 1);
    EXPECT_EQ(result["DIMM0"]["Socket"].get<uint8_t>(), 2);
    EXPECT_EQ(result["DIMM0"]["Slot"].get<uint8_t>(), 3);
    EXPECT_EQ(result["DIMM0"]["Channel"].get<uint8_t>(), 4);
}

TEST_F(DimmTest, ParseConfigFile_InvalidJson_ReturnsEmpty)
{
    std::string path = makeTempConfigPath();
    std::ofstream f(path);
    f << "{ invalid json }";
    f.close();

    auto storage = createSMBIOSMemoryDevice();
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
              storage.data(), motherboard);

    Json result = dimm.parseConfigFile(path);
    std::filesystem::remove(path);

    EXPECT_TRUE(result.empty());
}

TEST_F(DimmTest, ConfigFileWithMatchingDeviceLocator_SetsSocketSlotChannel)
{
    std::string path = makeTempConfigPath();
    std::ofstream f(path);
    f << R"({"DIMM0":{"MemoryController":5,"Socket":2,"Slot":1,"Channel":3}})";
    f.close();

    auto storage = createSMBIOSMemoryDevice();
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard, path);
    });
    std::filesystem::remove(path);
}

TEST_F(DimmTest, ConfigFileWithNonMatchingDeviceLocator_LogsAndZerosLocation)
{
    std::string path = makeTempConfigPath();
    std::ofstream f(path);
    f << R"({"OtherDIMM":{"MemoryController":1,"Socket":1,"Slot":1,"Channel":1}})";
    f.close();

    auto storage = createSMBIOSMemoryDevice();
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard, path);
    });
    std::filesystem::remove(path);
}

TEST_F(DimmTest, PublicSetters_ExerciseAllFromMemoryInfoUpdate)
{
    uint8_t storage[2048] = {0};
    createType16PhysicalMemoryArray(storage, 0x0001, 0x05);
    auto smbiosData = createSMBIOSMemoryDevice(0);
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(smbiosData.data());
    memInfo->phyArrayHandle = 0x0001;
    memInfo->size = 8192;
    memInfo->formFactor = 0x09;
    memInfo->memoryType = 0x1A;
    memInfo->typeDetail = 0x0080;
    memInfo->speed = 3200;
    memInfo->confClockSpeed = 3200;
    memInfo->memoryTechnology = 0x03;
    std::memcpy(storage + 50, smbiosData.data(), smbiosData.size());

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage, motherboard);
    });
}

TEST_F(DimmTest, BankLocatorNonEmpty_ConcatenatesWithDeviceLocator)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    size_t off = 1 + 6 + 1;
    std::string bankLoc = "BANK1";
    std::memcpy(strStart + off, bankLoc.c_str(), bankLoc.length() + 1);
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorCpuDigit_ParsesSocket)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "CPU0_DIMM_A";
    std::memcpy(strStart + 1, deviceLoc.c_str(), deviceLoc.length() + 1);
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST_F(DimmTest, DeviceLocatorSingleLetterSlot_SetsSlotFromLetter)
{
    auto storage = createSMBIOSMemoryDevice();
    MemoryInfo* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    uint8_t* strStart = storage.data() + sizeof(MemoryInfo);
    std::string deviceLoc = "DIMM_B";
    std::memcpy(strStart + 1, deviceLoc.c_str(), deviceLoc.length() + 1);
    memInfo->deviceLocator = 1;
    memInfo->bankLocator = 2;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

TEST(DimmHeader, DimmTypeTableLookup)
{
    EXPECT_EQ(dimmTypeTable.at(0x1), DeviceType::Other);
    EXPECT_EQ(dimmTypeTable.at(0x3), DeviceType::DRAM);
    EXPECT_EQ(dimmTypeTable.at(0x1a), DeviceType::DDR4);
    EXPECT_EQ(dimmTypeTable.at(0x22), DeviceType::DDR5);
}

TEST(DimmHeader, DetailTableIndexZeroReserved)
{
    EXPECT_EQ(detailTable[0], "Reserved");
    EXPECT_EQ(detailTable.size(), 16u);
}

TEST(DimmHeader, DimmEccTypeMapLookup)
{
    EXPECT_EQ(dimmEccTypeMap.at(0x1), EccType::NoECC);
    EXPECT_EQ(dimmEccTypeMap.at(0x5), EccType::SingleBitECC);
}

TEST(DimmHeader, DimmFormFactorMapLookup)
{
    EXPECT_EQ(dimmFormFactorMap.at(0x10), FormFactor::Die);
}

// dimmId=1 with two MemoryInfo records exercises the for-loop body in
// memoryInfoUpdate (lines 65-77 in dimm.cpp: smbiosNextPtr + getSMBIOSTypePtr
// iteration).
TEST_F(DimmTest, MemoryInfoUpdateDimmId1TwoDevicesProcessesSecond)
{
    auto dev0 = createSMBIOSMemoryDevice(0);
    auto dev1 = createSMBIOSMemoryDevice(1);
    uint8_t* dev0End = smbiosNextPtr(dev0.data());
    ASSERT_NE(dev0End, nullptr);
    dev0.resize(static_cast<size_t>(dev0End - dev0.data()));
    std::vector<uint8_t> storage;
    storage.insert(storage.end(), dev0.begin(), dev0.end());
    storage.insert(storage.end(), dev1.begin(), dev1.end());

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm1",
                  1, // dimmId=1 → loop runs once to reach second record
                  storage.data(), motherboard);
    });
}

// dimmId=1 with only ONE MemoryInfo record: smbiosNextPtr advances past it,
// getSMBIOSTypePtr returns null → early return at line 73.
TEST_F(DimmTest, MemoryInfoUpdateDimmId1SingleDeviceReturnsEarly)
{
    auto storage = createSMBIOSMemoryDevice(0);
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm1",
                  1, // dimmId=1 → tries to advance past the only record
                  storage.data(), motherboard);
    });
}

// updateEccType with no type-16 in storage: getSMBIOSTypePtr returns null on
// the first search, covering the early-return at line 153.
TEST_F(DimmTest, UpdateEccTypeNoType16InStorageReturnsEarlyWithError)
{
    // Storage contains only a MemoryInfo (type 17), no PhysicalMemoryArray
    // (type 16).  phyArrayHandle=0x1234 → no match possible.
    auto storage = createSMBIOSMemoryDevice(0);
    auto* memInfo = reinterpret_cast<MemoryInfo*>(storage.data());
    memInfo->phyArrayHandle = 0x1234;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  storage.data(), motherboard);
    });
}

// updateEccType with a type-16 whose handle does NOT match phyArrayHandle:
// exercises the handle-mismatch branch (line 157 false) and smbiosNextPtr call
// at line 172, then the while loop iterates and getSMBIOSTypePtr returns null.
TEST_F(DimmTest, UpdateEccTypeType16HandleMismatchAdvancesLoop)
{
    auto data = createSMBIOSMemoryDevice(0);
    auto* memInfo = reinterpret_cast<MemoryInfo*>(data.data());
    memInfo->phyArrayHandle = 0x9999; // won't match the type-16 handle below

    uint8_t type16Buf[32] = {0};
    createType16PhysicalMemoryArray(type16Buf, 0x0001 /*handle*/, 0x05);
    data.insert(data.end(), type16Buf, type16Buf + 25);

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), motherboard);
    });
}

// ---------------------------------------------------------------------------
// Additional tests with carefully framed, contiguous SMBIOS records (no zero
// padding between records, which would otherwise halt getSMBIOSTypePtr before
// reaching a trailing type-16 record) targeting previously-uncovered branches
// in dimm.cpp.
// ---------------------------------------------------------------------------
namespace
{
// Build a type-17 memory device followed (optionally) by a contiguous type-16
// physical-memory-array record, with full control over the string set.
std::vector<uint8_t> buildDimmBlob(
    const std::vector<std::string>& strings, uint8_t bankLocatorIdx,
    uint16_t phyHandle, bool withType16, uint16_t type16Handle, uint8_t eccType)
{
    std::vector<uint8_t> data;
    struct MemoryInfo mem{};
    mem.type = memoryDeviceType;
    mem.length = sizeof(MemoryInfo);
    mem.handle = 0x1000;
    mem.phyArrayHandle = phyHandle;
    mem.errInfoHandle = 0xFFFE;
    mem.totalWidth = 72;
    mem.dataWidth = 64;
    mem.size = 8192;
    mem.formFactor = 0x09;
    mem.deviceLocator = 1;
    mem.bankLocator = bankLocatorIdx;
    mem.memoryType = 0x1A;
    mem.manufacturer = 3;
    mem.serialNum = 4;
    mem.assetTag = 5;
    mem.partNum = 6;
    mem.memoryTechnology = 0x03;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&mem);
    data.insert(data.end(), p, p + sizeof(MemoryInfo));
    for (const auto& s : strings)
    {
        data.insert(data.end(), s.begin(), s.end());
        data.push_back(0);
    }
    data.push_back(0); // end of string set

    if (withType16)
    {
        PhysicalMemoryArrayInfo t16{};
        t16.type = physicalMemoryArrayType;
        t16.length = 23;
        t16.handle = type16Handle;
        t16.location = 0x01;
        t16.use = 0x03;
        t16.memoryErrorCorrection = eccType;
        t16.numberOfMemoryDevices = 1;
        const uint8_t* q = reinterpret_cast<const uint8_t*>(&t16);
        data.insert(data.end(), q, q + sizeof(PhysicalMemoryArrayInfo));
        data.push_back(0);
        data.push_back(0);
    }
    // Trailing zero padding so SMBIOS table walks terminate cleanly instead of
    // reading past the end of the buffer.
    data.resize(data.size() + 512, 0);
    return data;
}

class ThrowingSocketDimm : public Dimm
{
  public:
    using Dimm::Dimm;

    uint8_t socket(uint8_t) override
    {
        throw sdbusplus::exception::SdBusError(EIO, "forced dimm socket");
    }
};
} // namespace

// bankLocator empty -> result == deviceLocator (line 233); deviceLocator
// "DIMM_A" -> single-letter slot parsing (line 308 true).
TEST_F(DimmTest, DeviceLocatorBankEmptyAndSingleLetterSlot)
{
    auto data = buildDimmBlob({"DIMM_A", "BANK", "Mfr", "SN", "TAG", "PN"},
                              /*bankLocatorIdx*/ 0, 0x0001, false, 0, 0);
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}

TEST_F(DimmTest, DeviceLocatorCpuSocketSdbusExceptionIsCaught)
{
    const std::string motherboard =
        "/xyz/openbmc_project/test/inventory/system";
    auto initial = buildDimmBlob({"DIMM0", "BANK0", "Mfr", "SN", "TAG", "PN"},
                                 2, 0x0001, false, 0, 0);
    ThrowingSocketDimm dimm(
        *bus, "/xyz/openbmc_project/test/inventory/system/dimm_throw", 0,
        initial.data(), motherboard);

    auto update = buildDimmBlob({"CPU0", "BANK0", "Mfr", "SN", "TAG", "PN"}, 0,
                                0x0001, false, 0, 0);
    EXPECT_NO_THROW(dimm.memoryInfoUpdate(update.data(), motherboard));
}

// manufacturer string is exactly "NO DIMM" -> blanked (lines 392-397).
TEST_F(DimmTest, ManufacturerExactlyNoDimmIsBlanked)
{
    auto data = buildDimmBlob({"DIMM0", "BANK0", "NO DIMM", "SN", "TAG", "PN"},
                              2, 0x0001, false, 0, 0);
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}

// type-16 matching handle, ECC type present in map -> ecc(it->second) (167).
TEST_F(DimmTest, EccContiguousMatchingHandleMappedType)
{
    auto data = buildDimmBlob({"DIMM0", "BANK0", "Mfr", "SN", "TAG", "PN"}, 2,
                              0x0042, true, 0x0042, 0x05 /*SingleBitECC*/);
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}

// type-16 matching handle, ECC type NOT in map -> ecc(NoECC) (lines 161,163).
TEST_F(DimmTest, EccContiguousMatchingHandleUnmappedType)
{
    auto data = buildDimmBlob({"DIMM0", "BANK0", "Mfr", "SN", "TAG", "PN"}, 2,
                              0x0042, true, 0x0042, 0xFF /*not in map*/);
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}

// type-16 present but handle never matches -> loops, then "not found" (176).
TEST_F(DimmTest, EccContiguousNoMatchingHandleLogsError)
{
    auto data = buildDimmBlob({"DIMM0", "BANK0", "Mfr", "SN", "TAG", "PN"}, 2,
                              0x9999 /*phys handle*/, true,
                              0x0001 /*type16 handle*/, 0x05);
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}

// configFilePath non-empty with a matching deviceLocator entry exercises the
// ternary (line 249) and the JSON-found branch (253-266).
TEST_F(DimmTest, DeviceLocatorWithConfigFileMatchedEntry)
{
    std::string cfg = "/tmp/dimm_cfg_" + std::to_string(getpid()) + ".json";
    {
        std::ofstream f(cfg);
        f << R"({"DIMM0":{"MemoryController":1,"Socket":2,"Slot":3,"Channel":4}})";
    }
    auto data = buildDimmBlob({"DIMM0", "BANK0", "Mfr", "SN", "TAG", "PN"}, 2,
                              0x0001, false, 0, 0);
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system",
                  cfg);
    });
    {
        std::error_code removeEc;
        std::filesystem::remove(cfg, removeEc);
    }
}

// configFilePath non-empty but deviceLocator NOT in the JSON -> else branch
// (lines 268-276) zeroes the location fields.
TEST_F(DimmTest, DeviceLocatorWithConfigFileUnmatchedEntry)
{
    std::string cfg = "/tmp/dimm_cfg2_" + std::to_string(getpid()) + ".json";
    {
        std::ofstream f(cfg);
        f << R"({"OTHER":{"MemoryController":1,"Socket":2,"Slot":3,"Channel":4}})";
    }
    auto data = buildDimmBlob({"DIMM0", "BANK0", "Mfr", "SN", "TAG", "PN"}, 2,
                              0x0001, false, 0, 0);
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system",
                  cfg);
    });
    {
        std::error_code removeEc;
        std::filesystem::remove(cfg, removeEc);
    }
}

// dimmId == 1 with a single memory record whose string area never terminates:
// the per-index loop calls smbiosNextPtr, which hits the size limit and returns
// nullptr, exercising the early-return branch inside the loop (dimm.cpp ~L68).
TEST_F(DimmTest, DimmId1NextPtrNullReturnsEarly)
{
    // smbiosNextPtr scans up to mdrSMBIOSSize bytes starting past the record's
    // formatted area before its endless-loop guard returns nullptr, so the
    // buffer must be large enough to hold that whole scan (record offset +
    // mdrSMBIOSSize + the two bytes read per iteration). Size generously to
    // keep the scan in-bounds; otherwise it reads past the end (ASan abort).
    std::vector<uint8_t> buf(2 * static_cast<size_t>(mdrSMBIOSSize), 0x01);
    buf[0] = memoryDeviceType;
    buf[1] = sizeof(MemoryInfo); // formatted length
    buf[2] = 0;                  // handle lo
    buf[3] = 0;                  // handle hi
    // No double-null after the record -> smbiosNextPtr never terminates and
    // returns nullptr at the limit.
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm1", 1,
                  buf.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}

#ifdef DIMM_DBUS
TEST_F(DimmTest, PublicSettersCoverChangedAndUnchangedValues)
{
    auto data = buildDimmBlob({"DIMM0", "BANK0", "Mfr", "SN", "TAG", "PN"}, 2,
                              0x0042, true, 0x0042, 0x05);
    Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm_setters",
              0, data.data(), "/xyz/openbmc_project/test/inventory/system");

    dimm.memoryDataWidth(64);
    dimm.memoryDataWidth(64);
    dimm.memoryTotalWidth(72);
    dimm.memoryTotalWidth(72);
    dimm.memorySizeInKB(8192);
    dimm.memorySizeInKB(8192);
    dimm.memoryDeviceLocator("BANK0 DIMM0");
    dimm.memoryDeviceLocator("BANK0 DIMM0");
    dimm.memoryType(DeviceType::DDR4);
    dimm.memoryType(DeviceType::DDR4);
    dimm.memoryTypeDetail("Synchronous");
    dimm.memoryTypeDetail("Synchronous");
    dimm.maxMemorySpeedInMhz(3200);
    dimm.maxMemorySpeedInMhz(3200);
    dimm.manufacturer("Mfr");
    dimm.manufacturer("Mfr");
    dimm.present(true);
    dimm.present(true);
    dimm.serialNumber("SN");
    dimm.serialNumber("SN");
    dimm.partNumber("PN");
    dimm.partNumber("PN");
#ifdef DIMM_LOCATION_CODE
    dimm.locationCode("BANK0 DIMM0");
    dimm.locationCode("BANK0 DIMM0");
#endif
    dimm.memoryAttributes(1);
    dimm.memoryAttributes(1);
    dimm.memoryMedia(MemoryTechType::DRAM);
    dimm.memoryMedia(MemoryTechType::DRAM);
    dimm.slot(1);
    dimm.slot(1);
    dimm.socket(1);
    dimm.socket(1);
    dimm.memoryController(1);
    dimm.memoryController(1);
    dimm.channel(1);
    dimm.channel(1);
    dimm.memoryConfiguredSpeedInMhz(3200);
    dimm.memoryConfiguredSpeedInMhz(3200);
    dimm.functional(true);
    dimm.functional(true);
    dimm.ecc(EccType::SingleBitECC);
    dimm.ecc(EccType::SingleBitECC);
    dimm.formFactor(FormFactor::RDIMM);
    dimm.formFactor(FormFactor::RDIMM);
}
#endif

#ifdef DIMM_DBUS
TEST_F(DimmTest, UpdateEccTypeMismatchedType16WithUnterminatedTailFallsOut)
{
    auto data = buildDimmBlob({"DIMM0", "BANK0", "Mfr", "SN", "TAG", "PN"}, 2,
                              0x9999, false, 0, 0);
    data.resize(data.size() - 512);

    PhysicalMemoryArrayInfo type16{};
    type16.type = physicalMemoryArrayType;
    type16.length = 23;
    type16.handle = 0x0001;
    type16.location = 0x01;
    type16.use = 0x03;
    type16.memoryErrorCorrection = 0x05;
    type16.numberOfMemoryDevices = 1;
    const auto* begin = reinterpret_cast<const uint8_t*>(&type16);
    data.insert(data.end(), begin, begin + sizeof(type16));
    data.insert(data.end(), static_cast<size_t>(mdrSMBIOSSize) + 32, 0x01);

    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}
#endif

#ifdef DIMM_DBUS
namespace phosphor
{
namespace smbios
{
extern bool onlyDimmLocationCode;
}
} // namespace phosphor

TEST_F(DimmTest, OnlyDimmLocationCodeUsesDeviceLocatorWithNonEmptyBank)
{
    auto data = buildDimmBlob({"DIMM_B", "BANK_B", "Mfr", "SN", "TAG", "PN"}, 2,
                              0x0001, false, 0, 0);
    const bool original = phosphor::smbios::onlyDimmLocationCode;
    phosphor::smbios::onlyDimmLocationCode = true;
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system");
    });
    phosphor::smbios::onlyDimmLocationCode = original;
}

TEST_F(DimmTest, DeviceLocatorMultiLetterDimmSuffixSkipsSlotSet)
{
    auto data = buildDimmBlob({"DIMM_AB", "BANK_AB", "Mfr", "SN", "TAG", "PN"},
                              2, 0x0001, false, 0, 0);
    EXPECT_NO_THROW({
        Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0", 0,
                  data.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}
#endif
