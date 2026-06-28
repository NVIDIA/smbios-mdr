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
#include "pcieslot.hpp"
#include "smbios_mdrv2.hpp"
#include "smbios_test_tables.hpp"
#include "test_mock_helpers.hpp"

#include <cstring>

#include <gtest/gtest.h>

using namespace phosphor::smbios::test;

namespace phosphor
{
namespace smbios
{

TEST(PcieSlot, CompilesAndLinks)
{
    SUCCEED();
}

TEST(PcieSlot, PcieSmbiosTypeContainsSlotTypes)
{
    EXPECT_NE(pcieSmbiosType.find(0x09), pcieSmbiosType.end());
    EXPECT_NE(pcieSmbiosType.find(0x14), pcieSmbiosType.end());
    EXPECT_NE(pcieSmbiosType.find(0xa5), pcieSmbiosType.end());
    EXPECT_EQ(pcieSmbiosType.count(0x09), 1u);
}

TEST(PcieSlot, PcieGenerationTableLookup)
{
    EXPECT_EQ(pcieGenerationTable.at(0x09), PCIeGeneration::Unknown);
    EXPECT_EQ(pcieGenerationTable.at(0x14), PCIeGeneration::Gen3);
    EXPECT_EQ(pcieGenerationTable.at(0x18), PCIeGeneration::Gen1);
    EXPECT_EQ(pcieGenerationTable.at(0x25), PCIeGeneration::Gen5);
}

TEST(PcieSlot, PcieLanesTableLookup)
{
    EXPECT_EQ(pcieLanesTable.at(0x08), 1u);
    EXPECT_EQ(pcieLanesTable.at(0x09), 2u);
    EXPECT_EQ(pcieLanesTable.at(0xd), 16u);
    EXPECT_EQ(pcieLanesTable.at(0xe), 32u);
}

TEST(PcieSlot, AvailabilityEnumValues)
{
    EXPECT_EQ(static_cast<uint8_t>(Availability::Other), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(Availability::Available), 0x03);
    EXPECT_EQ(static_cast<uint8_t>(Availability::Unavailable), 0x05);
}

class PcieSlotFixture : public TestFixtureBase
{};

TEST_F(PcieSlotFixture, PcieInfoUpdateWithStubSmbiosFile)
{
    auto smbiosFixture = loadSmbiosFromFile(getDefaultSmbiosFixturePath());
    if (!smbiosFixture.loaded())
    {
        GTEST_SKIP() << "No stub SMBIOS file (set SMBIOS_TEST_FIXTURE or add "
                        "test/fixtures/smbios2.bin)";
    }
    phosphor::smbios::Pcie pcie(
        *bus, "/xyz/openbmc_project/inventory/chassis/motherboard/pcieslot0", 0,
        smbiosFixture.tablePtr, "");
    SUCCEED();
}

TEST_F(PcieSlotFixture, PcieInfoUpdateNullStorage)
{
    uint8_t* storage = nullptr;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateEmptyStorage)
{
    uint8_t storage[100] = {0};
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateValidType9Data)
{
    uint8_t storage[512] = {0};
    storage[0] = systemSlots;
    storage[1] = 17;
    storage[2] = 0x09;
    storage[3] = 0x00;
    storage[4] = 1;
    storage[5] = 0x12;
    storage[6] = 0x0A;
    storage[7] = 0x01;
    storage[8] = 0x03;
    storage[17] = 0;
    storage[18] = 0;
    const char strings[] = "\0PCIe Slot 0\0\0";
    std::memcpy(storage + 19, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateGen3Slot)
{
    uint8_t storage[512] = {0};
    storage[0] = systemSlots;
    storage[1] = 17;
    storage[4] = 1;
    storage[5] = 0x14;
    storage[6] = 0x0A;
    storage[17] = 0;
    storage[18] = 0;
    const char strings[] = "Slot\0\0";
    std::memcpy(storage + 19, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

static void setSystemSlot(
    uint8_t* storage, size_t base, uint8_t slotDesignation, uint8_t slotType,
    uint8_t slotDataBusWidth, uint8_t currUsage, uint8_t slotLength,
    uint8_t characteristics2, const char* designationStr)
{
    storage[base + 0] = systemSlots;
    storage[base + 1] = 17;
    storage[base + 2] = 0;
    storage[base + 3] = 0;
    storage[base + 4] = slotDesignation;
    storage[base + 5] = slotType;
    storage[base + 6] = slotDataBusWidth;
    storage[base + 7] = currUsage;
    storage[base + 8] = slotLength;
    storage[base + 12] = characteristics2;
    size_t strOff = base + 17;
    storage[strOff] = 0;
    storage[strOff + 1] = 0;
    if (designationStr != nullptr && *designationStr != '\0')
    {
        std::memcpy(storage + strOff + 2, designationStr,
                    std::strlen(designationStr) + 1);
        strOff += 2 + std::strlen(designationStr) + 1;
    }
    else
    {
        strOff += 2;
    }
    storage[strOff] = 0;
    storage[strOff + 1] = 0;
}

TEST_F(PcieSlotFixture, PcieInfoUpdateEmptyMotherboard)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0x14, 0x09, 0x01, 0x04, 0x00, "Slot0");
    std::string motherboard = "";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateGenerationUnknown)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0xb7, 0x09, 0x01, 0x04, 0x00, "M2");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateTypeFromLengthHalfLength)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0x18, 0x09, 0x01, 0x03, 0x00, "Slot");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateTypeFromLengthFullLength)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0xa5, 0x09, 0x01, 0x04, 0x00, "Slot");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateLaneWidthUnknown)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0x14, 0xFF, 0x01, 0x04, 0x00, "Slot");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateTypeUnknown)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0xb7, 0x09, 0x01, 0x00, 0x00, "Slot");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateLanesNotInTable)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0x14, 0x00, 0x01, 0x04, 0x00, "Slot");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateLanesInTable)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0x14, 0x0d, 0x01, 0x04, 0x00, "Slot");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateHotPluggableTrue)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0x14, 0x09, 0x01, 0x04, 0x02, "Slot");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateHotPluggableFalse)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0x14, 0x09, 0x01, 0x04, 0x00, "Slot");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateLocationEmptyDesignation)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 0, 0x14, 0x09, 0x01, 0x04, 0x00, "");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateSecondSlotMissing)
{
    uint8_t storage[256] = {0};
    setSystemSlot(storage, 0, 1, 0x14, 0x09, 0x01, 0x04, 0x00, "Slot0");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot1", 1,
            storage, motherboard);
    });
}

TEST_F(PcieSlotFixture, PcieInfoUpdateSkipsNonPcieSlotType)
{
    uint8_t storage[512] = {0};
    setSystemSlot(storage, 0, 1, 0x00, 0x09, 0x01, 0x04, 0x00, "Legacy");
    size_t next = 17 + 2 + 7 + 1 + 2;
    setSystemSlot(storage, next, 1, 0x14, 0x09, 0x01, 0x04, 0x00, "PCIe");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot0", 0,
            storage, motherboard);
    });
}

TEST(PcieSlot, PcieTypeTableLookup)
{
    EXPECT_EQ(pcieTypeTable.at(0x14), PCIeType::M_2);
    EXPECT_EQ(pcieTypeTable.at(0x1F), PCIeType::U_2);
    EXPECT_EQ(pcieTypeTable.at(0x26), PCIeType::OCP3Small);
    EXPECT_EQ(pcieTypeTable.at(0x27), PCIeType::OCP3Large);
}

TEST(PcieSlot, PCIeTypeByLengthLookup)
{
    EXPECT_EQ(PCIeTypeByLength.at(0x03), PCIeType::HalfLength);
    EXPECT_EQ(PCIeTypeByLength.at(0x04), PCIeType::FullLength);
}

// pcieId index 1 with only one PCIe slot record: the iteration loop's second
// getSMBIOSTypePtr returns null on the trailing padding (pcieslot.cpp 35).
TEST_F(PcieSlotFixture, PcieInfoUpdateIndexBeyondAvailableRecords)
{
    uint8_t storage[512] = {0};
    storage[0] = systemSlots;
    storage[1] = 17;
    storage[5] = 0x09; // PCIe slot type
    storage[17] = 0;
    storage[18] = 0;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot1", 1,
            storage, motherboard);
    });
}

// Two PCIe slot records, index 1: the loop finds the 2nd slot, covering the
// getSMBIOSTypePtr-non-null direction (pcieslot.cpp 35 false) and index++ (39).
TEST_F(PcieSlotFixture, PcieInfoUpdateSecondRecordFound)
{
    uint8_t storage[512] = {0};
    storage[0] = systemSlots;
    storage[1] = 17;
    storage[5] = 0x09;
    storage[17] = 0;
    storage[18] = 0;
    storage[19] = systemSlots;
    storage[20] = 17;
    storage[24] = 0x09;
    storage[36] = 0;
    storage[37] = 0;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot1", 1,
            storage, motherboard);
    });
}

// pcieId == 1 with a single slot record whose string area never terminates:
// the loop's smbiosNextPtr hits the size limit and returns nullptr, exercising
// the early-return branch inside the loop (pcieslot.cpp ~L30).
TEST_F(PcieSlotFixture, PcieId1NextPtrNullReturnsEarly)
{
    std::vector<uint8_t> buf(static_cast<size_t>(mdrSMBIOSSize) + 64, 0x01);
    buf[0] = systemSlots;
    buf[1] = 17; // formatted length
    buf[2] = 0;
    buf[3] = 0;
    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot1", 1,
            buf.data(), "/xyz/openbmc_project/test/inventory/system");
    });
}

} // namespace smbios
} // namespace phosphor

namespace phosphor
{
namespace smbios
{

TEST_F(PcieSlotFixture, PcieInfoUpdateLoopSeesNonPcieRecordBeforeTarget)
{
    uint8_t storage[512] = {0};

    storage[0] = systemSlots;
    storage[1] = 17;
    storage[5] = 0x14;
    storage[17] = 0;
    storage[18] = 0;

    storage[19] = systemSlots;
    storage[20] = 17;
    storage[24] = 0x01;
    storage[36] = 0;
    storage[37] = 0;

    storage[38] = systemSlots;
    storage[39] = 17;
    storage[43] = 0x14;
    storage[55] = 0;
    storage[56] = 0;

    EXPECT_NO_THROW({
        phosphor::smbios::Pcie pcie(
            *bus, "/xyz/openbmc_project/test/inventory/system/pcieslot1", 1,
            storage, "/xyz/openbmc_project/test/inventory/system");
    });
}

} // namespace smbios
} // namespace phosphor
