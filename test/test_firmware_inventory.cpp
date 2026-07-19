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
#include "firmware_inventory.hpp"
#include "nvidia_firmware_inventory.hpp"
#include "smbios_mdrv2.hpp"
#include "test_mock_helpers.hpp"

#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::smbios;
using namespace phosphor::smbios::test;

static size_t setupFirmwareInfoStructure(
    uint8_t* storage, size_t offset, const char* componentName,
    const char* version, const char* id, const char* releaseDate,
    const char* manufacturer, uint8_t numComponents = 0,
    uint16_t* componentHandles = nullptr)
{
    storage[offset + 0] = firmwareInventoryInformationType;
    storage[offset + 1] = 30;
    storage[offset + 2] = 0x2D;
    storage[offset + 3] = 0x00;
    storage[offset + 4] = 1;
    storage[offset + 5] = 2;
    storage[offset + 6] = 0;
    storage[offset + 7] = 3;
    storage[offset + 8] = 0;
    storage[offset + 9] = 4;
    storage[offset + 10] = 5;
    storage[offset + 11] = 0;
    storage[offset + 22] = 0;
    storage[offset + 23] = numComponents;

    if (numComponents > 0 && componentHandles != nullptr)
    {
        for (uint8_t i = 0; i < numComponents; i++)
        {
            storage[offset + 24 + i * 2] = componentHandles[i] & 0xFF;
            storage[offset + 25 + i * 2] = (componentHandles[i] >> 8) & 0xFF;
        }
    }

    uint8_t* strPtr = storage + offset + 30;
    std::memcpy(strPtr, componentName, std::strlen(componentName));
    strPtr += std::strlen(componentName);
    *strPtr++ = 0;
    std::memcpy(strPtr, version, std::strlen(version));
    strPtr += std::strlen(version);
    *strPtr++ = 0;
    std::memcpy(strPtr, id, std::strlen(id));
    strPtr += std::strlen(id);
    *strPtr++ = 0;
    std::memcpy(strPtr, releaseDate, std::strlen(releaseDate));
    strPtr += std::strlen(releaseDate);
    *strPtr++ = 0;
    std::memcpy(strPtr, manufacturer, std::strlen(manufacturer));
    strPtr += std::strlen(manufacturer);
    *strPtr++ = 0;
    *strPtr++ = 0;

    return static_cast<size_t>(strPtr - storage);
}

static size_t setupComponentStructure(
    uint8_t* storage, size_t offset, uint8_t type, uint16_t handle,
    uint8_t stringIndex, const char* stringValue)
{
    storage[offset + 0] = type;
    storage[offset + 1] = 8;
    storage[offset + 2] = handle & 0xFF;
    storage[offset + 3] = (handle >> 8) & 0xFF;

    if (type == systemPowerSupply)
    {
        storage[offset + 4] = 0;
        storage[offset + 5] = stringIndex;
    }
    else
    {
        storage[offset + 4] = stringIndex;
    }

    size_t stringOffset = offset + 8;
    storage[stringOffset] = 0;
    storage[stringOffset + 1] = 0;
    if (stringIndex > 0 && stringValue != nullptr)
    {
        std::memcpy(storage + stringOffset + 2, stringValue,
                    std::strlen(stringValue));
        stringOffset += 2 + std::strlen(stringValue);
    }
    else
    {
        stringOffset += 2;
    }
    storage[stringOffset] = 0;
    storage[stringOffset + 1] = 0;

    return stringOffset + 2;
}

class FirmwareInventoryTest : public TestFixtureBase
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
};

TEST_F(FirmwareInventoryTest, FirmwareInfoUpdateNullStorage)
{
    uint8_t index = 0;
    uint8_t* storage = nullptr;

    EXPECT_NO_THROW({
        FirmwareInventory firmware(
            *bus, "/xyz/openbmc_project/test/inventory/system/firmware0", index,
            storage);
    });
}

TEST_F(FirmwareInventoryTest, FirmwareInfoUpdateEmptyStorage)
{
    uint8_t index = 0;
    uint8_t storage[100] = {0};

    EXPECT_NO_THROW({
        FirmwareInventory firmware(
            *bus, "/xyz/openbmc_project/test/inventory/system/firmware0", index,
            storage);
    });
}

TEST_F(FirmwareInventoryTest, FirmwareInfoUpdateWithValidData)
{
    uint8_t index = 0;
    uint8_t storage[512] = {0};

    storage[0] = firmwareInventoryInformationType;
    storage[1] = 30;
    storage[2] = 0x2D;
    storage[3] = 0x00;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 0;
    storage[7] = 3;
    storage[8] = 0;
    storage[9] = 4;
    storage[10] = 5;
    storage[11] = 0;

    storage[22] = 0;
    storage[23] = 0;

    storage[30] = 0;
    storage[31] = 0;

    const char* part1 = "\0BMC\0v1.0.0\0FW001\0";
    const char* part2 = "2024-01-01\0Manufacturer\0\0";
    std::memcpy(storage + 32, part1, 18);
    std::memcpy(storage + 50, part2, 24);

    EXPECT_NO_THROW({
        FirmwareInventory firmware(
            *bus, "/xyz/openbmc_project/test/inventory/system/firmware0", index,
            storage);
    });
}

TEST_F(FirmwareInventoryTest, FirmwareInfoUpdateMultipleEntries)
{
    uint8_t index = 1;
    uint8_t storage[1024] = {0};

    storage[0] = firmwareInventoryInformationType;
    storage[1] = 30;
    storage[4] = 1;
    storage[30] = 0;
    storage[31] = 0;

    storage[100] = firmwareInventoryInformationType;
    storage[101] = 30;
    storage[104] = 1;
    storage[130] = 0;
    storage[131] = 0;

    EXPECT_NO_THROW({
        FirmwareInventory firmware(
            *bus, "/xyz/openbmc_project/test/inventory/system/firmware1", index,
            storage);
    });
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathInvalidData)
{
    uint8_t storage[512] = {0};
    std::vector<std::string> existingPaths;

    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_TRUE(result.empty());
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathValidData)
{
    uint8_t storage[512] = {0};
    size_t endOff =
        setupFirmwareInfoStructure(storage, 0, "CustomFW", "v1.0.0", "FW001",
                                   "2024-01-01", "Manufacturer");
    (void)endOff;

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("/xyz/openbmc_project/software") == 0);
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathPathAlreadyExists)
{
    uint8_t storage[512] = {0};
    setupFirmwareInfoStructure(storage, 0, "CustomFW", "v1.0.0", "FW001",
                               "2024-01-01", "Manufacturer");

    std::vector<std::string> existingPaths;
    std::string first = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    ASSERT_FALSE(first.empty());
    existingPaths.push_back(first);
    std::string second = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_TRUE(second.empty());
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathIndexOutOfRange)
{
    uint8_t storage[512] = {0};
    setupFirmwareInfoStructure(storage, 0, "CustomFW", "v1.0.0", "FW001",
                               "2024-01-01", "Manufacturer");

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 1, existingPaths);
    EXPECT_TRUE(result.empty());
}

TEST_F(FirmwareInventoryTest, FirmwareInfoUpdateWithComponentsNoHandles)
{
    uint8_t storage[1024] = {0};
    uint16_t handles[] = {0x9999};
    setupFirmwareInfoStructure(storage, 0, "BMC", "v1.0.0", "FW001",
                               "2024-01-01", "Manufacturer", 1, handles);

    EXPECT_NO_THROW({
        FirmwareInventory firmware(
            *bus, "/xyz/openbmc_project/test/inventory/system/firmware0", 0,
            storage);
    });
}

TEST_F(FirmwareInventoryTest, FirmwareInfoUpdateWithComponentStructures)
{
    uint8_t storage[1024] = {0};
    uint16_t handles[] = {0x0100};
    size_t endOff =
        setupFirmwareInfoStructure(storage, 0, "BMC", "v1.0.0", "FW001",
                                   "2024-01-01", "Manufacturer", 1, handles);
    setupComponentStructure(storage, endOff, processorsType, 0x0100, 1, "CPU0");

    EXPECT_NO_THROW({
        FirmwareInventory firmware(
            *bus, "/xyz/openbmc_project/test/inventory/system/firmware0", 0,
            storage);
    });
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathNullData)
{
    uint8_t* storage = nullptr;
    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_TRUE(result.empty());
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathWithSlotComponent)
{
    uint8_t storage[2048] = {0};
    uint16_t compHandle = 0x0009;
    size_t endOff = setupFirmwareInfoStructure(
        storage, 0, "CustomFW", "v1.0", "FW001", "2024-01-01", "Manufacturer",
        1, &compHandle);
    setupComponentStructure(storage, endOff, systemSlots, compHandle, 1,
                            "Slot0");

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("/xyz/openbmc_project/software") !=
                std::string::npos);
}

TEST_F(FirmwareInventoryTest,
       CheckAndCreateFirmwarePathWithOnboardDevicesExtended)
{
    uint8_t storage[2048] = {0};
    uint16_t compHandle = 0x0029;
    size_t endOff = setupFirmwareInfoStructure(
        storage, 0, "CustomFW", "v1.0", "FW001", "2024-01-01", "Manufacturer",
        1, &compHandle);
    setupComponentStructure(storage, endOff, onboardDevicesExtended, compHandle,
                            1, "OnboardDevice");

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("/xyz/openbmc_project/software") !=
                std::string::npos);
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathWithPowerSupplyLocation)
{
    uint8_t storage[2048] = {0};
    uint16_t compHandle = 0x002A;
    size_t endOff = setupFirmwareInfoStructure(
        storage, 0, "CustomFW", "v1.0", "FW001", "2024-01-01", "Manufacturer",
        1, &compHandle);
    setupComponentStructure(storage, endOff, systemPowerSupply, compHandle, 1,
                            "PSU0");

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("/xyz/openbmc_project/software") !=
                std::string::npos);
}

TEST_F(FirmwareInventoryTest,
       CheckAndCreateFirmwarePathProcessorEmptyDesignation)
{
    uint8_t storage[2048] = {0};
    uint16_t compHandle = 0x0064;
    size_t endOff = setupFirmwareInfoStructure(
        storage, 0, "CustomFW", "v1.0", "FW001", "2024-01-01", "Manufacturer",
        1, &compHandle);
    setupComponentStructure(storage, endOff, processorsType, compHandle, 0,
                            nullptr);

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("/xyz/openbmc_project/software") !=
                std::string::npos);
}

TEST_F(FirmwareInventoryTest,
       CheckAndCreateFirmwarePathIndexOutOfRangeReturnsEmpty)
{
    uint8_t storage[512] = {0};
    storage[0] = firmwareInventoryInformationType;
    storage[1] = 30;
    storage[4] = 0;
    storage[7] = 0;
    storage[23] = 0;
    storage[30] = 0;
    storage[31] = 0;
    const char* emptyStr = "\0\0\0\0";
    std::memcpy(storage + 32, emptyStr, 4);

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 5, existingPaths);
    EXPECT_TRUE(result.empty());
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathSpecialCharsInName)
{
    uint8_t storage[512] = {0};
    setupFirmwareInfoStructure(storage, 0, "TestFW@v1.0#test", "v1.0", "FW001",
                               "2024-01-01", "Manufacturer");

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.find("@"), std::string::npos);
    EXPECT_EQ(result.find("#"), std::string::npos);
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathUnknownComponentType)
{
    uint8_t storage[2048] = {0};
    uint16_t compHandle = 0x0064;
    size_t endOff = setupFirmwareInfoStructure(
        storage, 0, "CustomFW", "v1.0", "FW001", "2024-01-01", "Manufacturer",
        1, &compHandle);
    storage[endOff + 0] = 0xFF;
    storage[endOff + 1] = 8;
    storage[endOff + 2] = compHandle & 0xFF;
    storage[endOff + 3] = (compHandle >> 8) & 0xFF;
    storage[endOff + 4] = 1;
    storage[endOff + 8] = 0;
    storage[endOff + 9] = 0;
    std::memcpy(storage + endOff + 10, "Unknown\0\0", 9);

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("/xyz/openbmc_project/software") !=
                std::string::npos);
}

TEST_F(FirmwareInventoryTest, GetExistingVersionPathsNoThrow)
{
    EXPECT_NO_THROW({
        std::vector<std::string> paths =
            phosphor::smbios::utils::getExistingVersionPaths(*bus);
        EXPECT_GE(paths.size(), 0u);
    });
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathEmptyNameAfterFilter)
{
    uint8_t storage[512] = {0};
    setupFirmwareInfoStructure(storage, 0, "BMC", "v1.0.0", "FW001",
                               "2024-01-01", "Manufacturer");
    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_EQ(result.empty(), filterFirmwareName("BMC").empty());
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathFallbackWhenIdEmpty)
{
    uint8_t storage[512] = {0};
    storage[0] = firmwareInventoryInformationType;
    storage[1] = 30;
    storage[4] = 2;
    storage[5] = 3;
    storage[7] = 4;
    storage[9] = 5;
    storage[10] = 6;
    storage[22] = 0;
    storage[23] = 0;
    size_t o = 30;
    storage[o++] = 0;
    std::memcpy(storage + o, "SomeFW", 6);
    o += 6;
    storage[o++] = 0;
    std::memcpy(storage + o, "v1", 2);
    o += 2;
    storage[o++] = 0;
    storage[o++] = 0;
    std::memcpy(storage + o, "date", 4);
    o += 4;
    storage[o++] = 0;
    std::memcpy(storage + o, "mfr", 3);
    o += 3;
    storage[o++] = 0;
    storage[o++] = 0;
    storage[o++] = 0;

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("firmware0"), std::string::npos);
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathTrimRight)
{
    uint8_t storage[2048] = {0};
    uint16_t compHandle = 0x0009;
    size_t endOff = setupFirmwareInfoStructure(
        storage, 0, "CustomFW", "v1.0", "FW001", "2024-01-01", "Manufacturer",
        1, &compHandle);
    setupComponentStructure(storage, endOff, systemSlots, compHandle, 1,
                            "Slot0  ");

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("/xyz/openbmc_project/software"), std::string::npos);
    EXPECT_TRUE(result.back() != ' ');
}

TEST_F(FirmwareInventoryTest,
       CheckAndCreateFirmwarePathPowerSupplyEmptyLocation)
{
    uint8_t storage[2048] = {0};
    uint16_t compHandle = 0x002A;
    size_t endOff = setupFirmwareInfoStructure(
        storage, 0, "CustomFW", "v1.0", "FW001", "2024-01-01", "Manufacturer",
        1, &compHandle);
    setupComponentStructure(storage, endOff, systemPowerSupply, compHandle, 1,
                            "");

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("/xyz/openbmc_project/software") !=
                std::string::npos);
}

TEST_F(FirmwareInventoryTest,
       CheckAndCreateFirmwarePathTwoComponentsFirstNullSecondValid)
{
    uint8_t storage[2048] = {0};
    uint16_t handles[] = {0x9999, 0x0100};
    size_t endOff =
        setupFirmwareInfoStructure(storage, 0, "CustomFW", "v1.0", "FW001",
                                   "2024-01-01", "Manufacturer", 2, handles);
    setupComponentStructure(storage, endOff, processorsType, 0x0100, 1, "CPU0");

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(
        result.find("CPU0") != std::string::npos ||
        result.find("/xyz/openbmc_project/software") != std::string::npos);
}

TEST_F(FirmwareInventoryTest, FirmwareInfoUpdateAllFieldsPopulated)
{
    uint8_t storage[512] = {0};
    size_t endOff =
        setupFirmwareInfoStructure(storage, 0, "ComponentName", "2.0.0",
                                   "ID123", "2025-06-15", "VendorInc");
    (void)endOff;

    EXPECT_NO_THROW({
        FirmwareInventory firmware(
            *bus, "/xyz/openbmc_project/test/inventory/system/firmware_all", 0,
            storage);
    });
}

TEST_F(FirmwareInventoryTest, GetFirmwareInventoryDataIndexBeyondAvailable)
{
    uint8_t storage[512] = {0};
    setupFirmwareInfoStructure(storage, 0, "OnlyOne", "v1", "id1", "2024-01-01",
                               "Mfr");
    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 1, existingPaths);
    EXPECT_TRUE(result.empty());
}

TEST_F(FirmwareInventoryTest,
       CheckAndCreateFirmwarePathFallbackWhenIdTrimmedEmpty)
{
    uint8_t storage[512] = {0};
    setupFirmwareInfoStructure(storage, 0, "FW", "v1", "   ", "2024-01-01",
                               "Mfr");
    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("/xyz/openbmc_project/software") !=
                std::string::npos);
}

TEST_F(FirmwareInventoryTest, CheckAndCreateFirmwarePathTypeLengthTooSmall)
{
    uint8_t storage[512] = {0};
    // Declared length (30) cannot hold 4 associated-component handles; the
    // record is rejected before the handle loop reads past it.
    setupFirmwareInfoStructure(storage, 0, "CustomFW", "v1.0", "FW001",
                               "2024-01-01", "Manufacturer", 4);

    std::vector<std::string> existingPaths;
    std::string result = FirmwareInventory::checkAndCreateFirmwarePath(
        storage, 0, existingPaths);
    EXPECT_TRUE(result.empty());
}
