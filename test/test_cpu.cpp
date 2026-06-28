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
#include "cpu.hpp"
#include "smbios_mdrv2.hpp"
#include "test_mock_helpers.hpp"

#include <cstring>
#include <tuple>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::smbios;
using namespace phosphor::smbios::test;

class CpuTest : public TestFixtureBase
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

TEST_F(CpuTest, ConstructorNullStorage)
{
    uint8_t cpuId = 0;
    uint8_t* storage = nullptr;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, DuplicateObjectPathRegistrationThrows)
{
    uint8_t* storage = nullptr;
    std::string path = "/xyz/openbmc_project/test/inventory/system/cpu0";
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = path;

    Cpu cpu(*bus, path, 0, storage, motherboard, assocPath);
    EXPECT_ANY_THROW({
        Cpu duplicate(*bus, path, 1, storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, ConstructorEmptyStorage)
{
    uint8_t cpuId = 0;
    uint8_t storage[100] = {0};
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateNullStorage)
{
    uint8_t cpuId = 0;
    uint8_t* storage = nullptr;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
            storage, motherboard, assocPath);

    EXPECT_NO_THROW({ cpu.infoUpdate(storage, motherboard); });
}

TEST_F(CpuTest, InfoUpdateEmptyStorage)
{
    uint8_t cpuId = 0;
    uint8_t storage[100] = {0};
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
            storage, motherboard, assocPath);

    EXPECT_NO_THROW({ cpu.infoUpdate(storage, motherboard); });
}

TEST_F(CpuTest, InfoUpdateWithValidData)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[2] = 0x04;
    storage[3] = 0x00;
    storage[4] = 1;
    storage[5] = 0x03;
    storage[6] = 0x0B;
    storage[7] = 2;

    storage[8] = 0x78;
    storage[9] = 0x06;
    storage[10] = 0x01;
    storage[11] = 0x00;
    storage[12] = 0xFF;
    storage[13] = 0xFB;
    storage[14] = 0xEB;
    storage[15] = 0xBF;
    storage[16] = 3;
    storage[17] = 0x80;
    storage[18] = 100;
    storage[19] = 0;
    storage[20] = 0x80;
    storage[21] = 0x0C;
    storage[22] = 0x80;
    storage[23] = 0x0C;
    storage[24] = 0x41;
    storage[25] = 0x01;
    storage[26] = 0xFF;
    storage[27] = 0xFF;
    storage[28] = 0xFF;
    storage[29] = 0xFF;
    storage[30] = 0xFF;
    storage[31] = 0xFF;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[35] = 8;
    storage[36] = 8;
    storage[37] = 16;
    storage[38] = 0x01;
    storage[39] = 0x00;
    storage[40] = 0x00;
    storage[41] = 0x00;
    storage[42] = 0x00;
    storage[43] = 0x00;
    storage[44] = 0x00;
    storage[45] = 0x00;
    storage[46] = 0x00;
    storage[47] = 0x00;

    storage[50] = 0;
    storage[51] = 0;

    const char strings[] =
        "\0CPU Socket 0\0Intel\0Intel(R) Xeon(R) CPU\0SN123\0TAG\0PN123\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
        cpu.infoUpdate(storage, motherboard);
    });
}

TEST_F(CpuTest, InfoUpdateCpuNotPopulated)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x01;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
        cpu.infoUpdate(storage, motherboard);
    });
}

TEST_F(CpuTest, InfoUpdateCpuDisabled)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x40;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
        cpu.infoUpdate(storage, motherboard);
    });
}

TEST_F(CpuTest, InfoUpdateNullDuringIteration)
{
    uint8_t cpuId = 1;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu1";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu1", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateFamily2Unknown)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;
    storage[28] = 0xFE;
    storage[29] = 0xFF;
    storage[30] = 0xFF;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdFamilyF)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;

    storage[32] = 0x0F;
    storage[33] = 0xF0;
    storage[34] = 0x00;
    storage[35] = 0x00;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateFamily2Known)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;
    storage[28] = 0xFE;
    storage[29] = 0x01;
    storage[30] = 0x00;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateFamily2FromTable)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0xFE;  /* Processor Family 2 Indicator */
    storage[24] = 0x41;
    storage[40] = 0x00; /* family2 = 0x0100 (ARMv7) - in family2Table */
    storage[41] = 0x01;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateFamilyUnknown)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0xFD; /* not in familyTable -> "Unknown Processor Family" */
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateFunctionalFalse)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0x0B;
    storage[24] = 0x40; /* socket populated (bit 6) but (status & 0x07) != 1 */
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCoreCountFromExtended)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0x0B;
    storage[24] = 0x41;
    storage[35] = 0xFF; /* coreCount >= 0xff -> use coreCount2 */
    storage[42] = 16;
    storage[43] = 0;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCharacteristicsWithCapability)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0x0B;
    storage[24] = 0x41;
    storage[38] = 0x04; /* bit 2 set -> Capable64bit in characteristicsTable */
    storage[39] = 0x00;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCoreAndThreadNonExtended)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0x0B;
    storage[24] = 0x41;
    storage[35] = 4; /* coreCount < 0xFF -> use coreCount path */
    storage[37] = 8; /* threadCount < 0xFF -> use threadCount path */
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateIntelFamilyIdCpuFamily6)
{
    /* family 0x0B = "Intel Pentium processor" (has " Intel ") ->
     * step/effectiveFamily/effectiveModel */
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0x0B;
    storage[24] = 0x41;
    storage[8] = 0x21; /* id: step=1, model=2, family from [9] */
    storage[9] = 0x06; /* cpuFamily=0x6 -> effectiveFamily(cpuFamily),
                          effectiveModel((xModel<<4)|model) */
    storage[10] = 0;
    storage[11] = 0;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateIntelFamilyIdCpuFamilyF)
{
    /* family 0x0B = "Intel Pentium processor"; id with cpuFamily=0xf */
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0x0B;
    storage[24] = 0x41;
    storage[8] = 0x00;
    storage[9] =
        0x0f; /* cpuFamily=0xf -> effectiveFamily(cpuXFamily+cpuFamily) */
    storage[10] = 0;
    storage[11] = 0;
    storage[12] = 0x01; /* cpuXFamily for effectiveFamily */
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateIntelFamilyIdEffectiveModelElse)
{
    /* family in table with " Intel "; cpuFamily neither 0x6 nor 0xf ->
     * effectiveModel(cpuModel) */
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0x0B;
    storage[24] = 0x41;
    storage[8] = 0x53; /* step=3, model=5 */
    storage[9] = 0x01; /* cpuFamily=0x1 -> else effectiveModel(cpuModel) */
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateThreadCountFromExtended)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0x0B;
    storage[24] = 0x41;
    storage[37] = 0xFF; /* threadCount >= 0xff -> use threadCount2 */
    storage[46] = 32;
    storage[47] = 0;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdFamilyNotF)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0x60;
    storage[34] = 0x00;
    storage[35] = 0x00;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateWithCharacteristics)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;
    storage[30] = 0xFF;
    storage[31] = 0xFF;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdFamilyFExtended)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0xF0;
    storage[34] = 0x12;
    storage[35] = 0x34;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, FamilyUnknown)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0xFF;
    storage[7] = 1;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, FamilyProcessorFamily2Indicator)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0xFE;
    storage[7] = 1;
    storage[24] = 0x41;
    storage[28] = 0x01;
    storage[29] = 0x00;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, Family2Unknown)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0xFE;
    storage[7] = 1;
    storage[24] = 0x41;
    storage[28] = 0xFF;
    storage[29] = 0xFF;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdXeonFamily)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x10;
    storage[7] = 1;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0xF0;
    storage[34] = 0x00;
    storage[35] = 0x00;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdIntelFamily)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x16;
    storage[7] = 1;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0x60;
    storage[34] = 0x00;
    storage[35] = 0x00;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCoreAndThreadCountExtended)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[6] = 0x0B;
    storage[7] = 2;
    storage[16] = 3;
    storage[18] = 100;
    storage[20] = 100;
    storage[24] = 0x41;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[35] = 0xFF;
    storage[36] = 0;
    storage[37] = 0xFF;
    storage[38] = 0x00;
    storage[39] = 0x00;
    storage[42] = 0x10;
    storage[43] = 0x00;
    storage[46] = 0x20;
    storage[47] = 0x00;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] =
        "\0CPU Socket 0\0Intel\0Version\0SN123\0TAG\0PN123\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateEmptyMotherboardSkipsAssociations)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[6] = 0x0B;
    storage[7] = 2;
    storage[24] = 0x41;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] =
        "\0CPU Socket 0\0Intel\0Version\0SN123\0TAG\0PN123\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdFamilyFWithXFamily)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x10;
    storage[7] = 1;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0xF0;
    storage[34] = 0x00;
    storage[35] = 0x80;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, CharacteristicsMultipleBits)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x16;
    storage[7] = 1;
    storage[24] = 0x41;
    storage[26] = 0x03;
    storage[27] = 0x00;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, CharacteristicsUnmappedBits)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x16;
    storage[7] = 1;
    storage[24] = 0x41;
    storage[26] = 0xFF;
    storage[27] = 0xFF;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, CpuNotPopulated)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x16;
    storage[7] = 1;
    storage[24] = 0x01;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, CpuFunctional)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x16;
    storage[7] = 1;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, CpuNotFunctional)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x16;
    storage[7] = 1;
    storage[24] = 0x43;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 50;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCoreCountExtended)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x16;
    storage[7] = 1;
    storage[24] = 0x41;
    storage[35] = 0xFF;
    storage[42] = 16;
    storage[43] = 0xFF;
    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 52;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdFamily6)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x10;
    storage[7] = 1;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0x60;
    storage[34] = 0x00;
    storage[35] = 0x00;

    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 52;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdFamilyOther)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x10;
    storage[7] = 1;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0x50;
    storage[34] = 0x00;
    storage[35] = 0x00;

    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 52;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdXeonFamilyNotF)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x10;
    storage[7] = 1;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0x60;
    storage[34] = 0x00;
    storage[35] = 0x00;

    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 52;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuIdZenFamily)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0x6B;
    storage[7] = 1;
    storage[24] = 0x41;

    storage[32] = 0x05;
    storage[33] = 0xF0;
    storage[34] = 0x00;
    storage[35] = 0x00;

    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 52;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateCpuFamilyNotInTable)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[5] = 0x01;
    storage[6] = 0xFF;
    storage[7] = 1;
    storage[24] = 0x41;
    storage[32] = 0x05;
    storage[33] = 0x50;
    storage[34] = 0x00;
    storage[35] = 0x00;

    storage[50] = 0;
    storage[51] = 0;

    uint8_t* strStart = storage + 52;
    std::string socket = "CPU Socket 0";
    std::memcpy(strStart, socket.c_str(), socket.length());
    strStart += socket.length() + 1;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, SocketChipNumber_String_CPUPattern)
{
    auto [found, socket, chip] = Cpu::socketChipNumber("CPU0");
    if (found)
    {
        EXPECT_EQ(socket, 0u);
        EXPECT_EQ(chip, 0u);
    }

    std::tie(found, socket, chip) = Cpu::socketChipNumber("CPU1");
    if (found)
    {
        EXPECT_EQ(socket, 1u);
        EXPECT_EQ(chip, 0u);
    }

    std::tie(found, socket, chip) = Cpu::socketChipNumber("CPU12");
    if (found)
    {
        EXPECT_EQ(socket, 12u);
        EXPECT_EQ(chip, 0u);
    }
}

TEST_F(CpuTest, SocketChipNumber_String_NoMatch)
{
    auto [found, socket, chip] = Cpu::socketChipNumber("Socket0");
    EXPECT_FALSE(found);
    EXPECT_EQ(socket, 0u);
    EXPECT_EQ(chip, 0u);

    std::tie(found, socket, chip) = Cpu::socketChipNumber("INVALID");
    EXPECT_FALSE(found);
}

TEST_F(CpuTest, SocketChipNumber_DataInNull)
{
    uint8_t* volatile runtimeNull = nullptr;
    auto [found, socket, chip] = Cpu::socketChipNumber(runtimeNull);
    EXPECT_FALSE(found);
    EXPECT_EQ(socket, 0u);
    EXPECT_EQ(chip, 0u);
}

TEST_F(CpuTest, SocketChipNumber_DataInValid)
{
    uint8_t storage[64] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[50] = 0;
    std::memcpy(storage + 51, "CPU2", 4);
    storage[55] = 0;
    storage[56] = 0;

    auto [found, socket, chip] = Cpu::socketChipNumber(storage);
    if (found)
    {
        EXPECT_EQ(socket, 2u);
        EXPECT_EQ(chip, 0u);
    }
}

TEST_F(CpuTest, InfoUpdate_GetSmbiosTypePtrReturnsNull)
{
    uint8_t cpuId = 0;
    uint8_t storage[128] = {0};
    storage[0] = 0x09;
    storage[1] = 13;
    storage[13] = 0;
    storage[14] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdate_SmbiosNextPtrReturnsNullDuringIteration)
{
    uint8_t cpuId = 1;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu1";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu1", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, Socket_CalledFromInfoUpdate)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[7] = 2;
    storage[16] = 3;
    storage[24] = 0x41;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] = "\0MySocket\0Intel\0Version\0SN\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest,
       Manufacturer_SerialNumber_PartNumber_AssetTag_CalledFromInfoUpdate)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[6] = 0x0B;
    storage[7] = 2;
    storage[16] = 3;
    storage[24] = 0x41;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] =
        "\0Socket\0AMD\0MyVersion\0SerialNum\0AssetTag\0PartNum\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, Version_CalledFromInfoUpdate)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[6] = 0x0B;
    storage[7] = 2;
    storage[16] = 3;
    storage[24] = 0x41;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] = "\0Socket\0Vendor\0VersionString\0SN\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, Family_UnknownProcessorFamily)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[6] = 0xFF;
    storage[7] = 2;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] = "\0Socket\0Vendor\0Version\0SN\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, Family_FromTableWithEffectiveFamily)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[6] = 0x0B;
    storage[7] = 2;
    storage[16] = 3;
    storage[24] = 0x41;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] = "\0Socket\0Intel\0Version\0SN\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, Characteristics_WithMappedCapabilities)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[6] = 0x0B;
    storage[7] = 2;
    storage[24] = 0x41;
    storage[26] = 0x1C;
    storage[27] = 0x00;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] = "\0Socket\0Vendor\0Version\0SN\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath = "/xyz/openbmc_project/test/inventory/system/cpu0";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, motherboard, assocPath);
    });
}

// family 0xa1 = "Quad-Core Intel Xeon processor 3200 Series" contains both
// " Intel " and " Xeon " (with surrounding spaces), so it enters the Intel
// step/family/model decode block (cpu.cpp lines 200-241). The existing tests
// used 0x0b ("Intel Pentium processor") whose name has no LEADING space and so
// never matched. id with cpuFamily nibble 0xf covers the 0xf branches
// (224->226, 232->234).
TEST_F(CpuTest, InfoUpdateXeonFamilyCpuFamilyF)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0xa1; // " Intel " + " Xeon "
    storage[24] = 0x41;
    storage[8] = 0x00;
    storage[9] = 0x0f; // cpuFamily = 0xf
    storage[10] = 0x10;
    storage[11] = 0x01;
    storage[50] = 0;
    storage[51] = 0;
    std::string mb = "/xyz/openbmc_project/test/inventory/system";
    std::string assoc = "/xyz/openbmc_project/test/inventory/system/cpu0";
    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, mb, assoc);
    });
}

// Same Xeon family but cpuFamily neither 0x6 nor 0xf -> effectiveFamily(else,
// 230) and effectiveModel(else, 238) branches.
TEST_F(CpuTest, InfoUpdateXeonFamilyCpuFamilyOther)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0xa1;
    storage[24] = 0x41;
    storage[8] = 0x21;
    storage[9] = 0x02; // cpuFamily = 2 (neither 0x6 nor 0xf)
    storage[10] = 0;
    storage[11] = 0;
    storage[50] = 0;
    storage[51] = 0;
    std::string mb = "/xyz/openbmc_project/test/inventory/system";
    std::string assoc = "/xyz/openbmc_project/test/inventory/system/cpu0";
    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, mb, assoc);
    });
}

// cpuId index 1 but only one processor record present: the iteration loop calls
// getSMBIOSTypePtr on the trailing zero padding and returns null (cpu.cpp 163).
TEST_F(CpuTest, InfoUpdateIndexBeyondAvailableRecords)
{
    uint8_t cpuId = 1;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0xa1;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;
    std::string mb = "/xyz/openbmc_project/test/inventory/system";
    std::string assoc = "/xyz/openbmc_project/test/inventory/system/cpu1";
    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu1", cpuId,
                storage, mb, assoc);
    });
}

// Xeon family with cpuFamily == 0x6 covers the 0x6 operand of the
// effectiveModel condition (cpu.cpp line 232).
TEST_F(CpuTest, InfoUpdateXeonFamilyCpuFamily6)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[6] = 0xa1; // " Intel " + " Xeon "
    storage[24] = 0x41;
    storage[8] = 0x60; // model nibble
    storage[9] = 0x06; // cpuFamily = 0x6
    storage[50] = 0;
    storage[51] = 0;
    std::string mb = "/xyz/openbmc_project/test/inventory/system";
    std::string assoc = "/xyz/openbmc_project/test/inventory/system/cpu0";
    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu0", cpuId,
                storage, mb, assoc);
    });
}

// cpuId == 1 with a single processor record whose string area never terminates:
// the per-index loop's smbiosNextPtr hits the size limit and returns nullptr,
// exercising the early-return branch inside the loop (cpu.cpp ~L158).
TEST_F(CpuTest, CpuId1NextPtrNullReturnsEarly)
{
    std::vector<uint8_t> buf(static_cast<size_t>(mdrSMBIOSSize) + 64, 0x01);
    buf[0] = processorsType;
    buf[1] = 50; // formatted length
    buf[2] = 0;
    buf[3] = 0;
    std::string mb = "/xyz/openbmc_project/test/inventory/system";
    std::string assoc = "/xyz/openbmc_project/test/inventory/system/cpu1";
    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu1", 1,
                buf.data(), mb, assoc);
    });
}

#ifdef CPU_DBUS
namespace
{
void setWellFormedProcessorRecord(uint8_t* storage, uint8_t family,
                                  uint16_t characteristics,
                                  uint64_t processorId = 0x000306a9)
{
    std::memset(storage, 0, 512);
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1; // socket designation string index
    storage[5] = 0x03;
    storage[6] = family;
    storage[7] = 2;  // manufacturer string index
    std::memcpy(storage + 8, &processorId, sizeof(processorId));
    storage[16] = 3; // version string index
    storage[20] = 0x80;
    storage[21] = 0x0c;
    storage[24] = 0x41; // populated and enabled
    storage[32] = 4;    // serial number string index
    storage[33] = 5;    // asset tag string index
    storage[34] = 6;    // part number string index
    storage[35] = 0xff;
    storage[37] = 0xff;
    storage[38] = characteristics & 0xff;
    storage[39] = (characteristics >> 8) & 0xff;
    storage[42] = 4;
    storage[46] = 8;

    const char strings[] =
        "CPU0\0TestVendor\0Model-X\0Serial123\0Asset123\0Part123   \0\0";
    std::memcpy(storage + 50, strings, sizeof(strings));
}
} // namespace

TEST_F(CpuTest, InfoUpdateWellFormedStringsDriveSettersAndCharacteristics)
{
    uint8_t storage[512] = {0};
    setWellFormedProcessorRecord(storage, 0xb1, 0x0005);

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/cpu_well_formed";

    EXPECT_NO_THROW({
        Cpu cpu(*bus,
                "/xyz/openbmc_project/test/inventory/system/cpu_well_formed", 0,
                storage, motherboard, assocPath);
    });
}

TEST_F(CpuTest, InfoUpdateWellFormedXeonAndZenFamiliesDriveOrOperands)
{
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    {
        uint8_t storage[512] = {0};
        setWellFormedProcessorRecord(storage, 0x10, 0x0004);
        std::string assocPath =
            "/xyz/openbmc_project/test/inventory/system/cpu_xeon_operand";
        EXPECT_NO_THROW({
            Cpu cpu(
                *bus,
                "/xyz/openbmc_project/test/inventory/system/cpu_xeon_operand",
                0, storage, motherboard, assocPath);
        });
    }

    {
        uint8_t storage[512] = {0};
        setWellFormedProcessorRecord(storage, 0x6b, 0x0004);
        std::string assocPath =
            "/xyz/openbmc_project/test/inventory/system/cpu_zen_operand";
        EXPECT_NO_THROW({
            Cpu cpu(
                *bus,
                "/xyz/openbmc_project/test/inventory/system/cpu_zen_operand", 0,
                storage, motherboard, assocPath);
        });
    }
}

TEST_F(CpuTest, SocketChipNumberDataWellFormedDesignation)
{
    uint8_t storage[512] = {0};
    setWellFormedProcessorRecord(storage, 0xb1, 0x0004);
    const char strings[] = "CPU2\0\0";
    std::memcpy(storage + 50, strings, sizeof(strings));

    auto [found, socket, chip] = Cpu::socketChipNumber(storage);
    if (found)
    {
        EXPECT_EQ(socket, 2u);
        EXPECT_EQ(chip, 0u);
    }
}
#endif

#ifdef CPU_DBUS
TEST_F(CpuTest, InfoUpdateSecondProcessorRecordContinuesPastIndexLoop)
{
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;
    storage[52] = processorsType;
    storage[53] = 50;
    storage[76] = 0x41;
    storage[102] = 0;
    storage[103] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/cpu_second";

    EXPECT_NO_THROW({
        Cpu cpu(*bus, "/xyz/openbmc_project/test/inventory/system/cpu_second",
                1, storage, motherboard, assocPath);
    });
}
#endif

#ifdef NVIDIA
TEST_F(CpuTest, SocketChipNumberNvidiaPatternsAndOverflow)
{
    auto [found, socket, chip] = Cpu::socketChipNumber("GPU:3.7");
    EXPECT_TRUE(found);
    EXPECT_EQ(socket, 3u);
    EXPECT_EQ(chip, 7u);

    std::tie(found, socket, chip) = Cpu::socketChipNumber("CPU4 ");
    EXPECT_TRUE(found);
    EXPECT_EQ(socket, 4u);
    EXPECT_EQ(chip, 0u);

    std::tie(found, socket, chip) =
        Cpu::socketChipNumber("GPU:999999999999999999999999999999999999.1");
    EXPECT_FALSE(found);

    std::tie(found, socket, chip) =
        Cpu::socketChipNumber("CPU999999999999999999999999999999999999");
    EXPECT_FALSE(found);
}
#endif
