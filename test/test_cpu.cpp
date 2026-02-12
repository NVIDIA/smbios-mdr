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
    auto [found, socket,
          chip] = Cpu::socketChipNumber(static_cast<uint8_t*>(nullptr));
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
