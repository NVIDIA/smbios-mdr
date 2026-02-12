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

    EXPECT_THROW(
        {
            Dimm dimm(*bus, "/xyz/openbmc_project/test/inventory/system/dimm0",
                      0, storage.data(), motherboard);
        },
        std::exception);
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
