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
#include "chassisCpu.hpp"
#include "smbios_mdrv2.hpp"
#include "smbios_test_tables.hpp"
#include "test_mock_helpers.hpp"

#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::smbios;
using namespace phosphor::smbios::test;

class ChassisCpuTest : public TestFixtureBase
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

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateNullStorage)
{
    uint8_t cpuId = 0;
    uint8_t* storage = nullptr;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateEmptyStorage)
{
    uint8_t cpuId = 0;
    uint8_t storage[100] = {0};
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateWithValidData)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[24] = 0x41;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[16] = 3;

    storage[50] = 0;
    storage[51] = 0;
    /* SMBIOS string table: index 1 = first string (no leading null). */
    const char strings[] =
        "CPU Socket 0\0Intel\0Intel(R) Xeon(R) CPU\0SN123\0TAG\0PN123\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateCpuNotPopulated)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x01;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateCpuDisabled)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x40;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateFunctionalFalse)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x40; /* socket populated (bit 6) but (status & 0x07) != 1 */

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateWithMakeProcessorTable)
{
    std::vector<uint8_t> table =
        makeProcessorTable(0x41, "Socket0", "Intel", "1.0");
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0", 0,
            table.data(), motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateEmptyMotherboard)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;

    std::string motherboard = "";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateNullDuringIteration)
{
    uint8_t cpuId = 1;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu1";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu1",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateNullAfterIteration)
{
    uint8_t cpuId = 1;
    uint8_t storage[1024] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;

    storage[100] = 0x01;
    storage[101] = 50;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu1";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu1",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, ChassisCpuInfoUpdateNullDuringIterationLine103)
{
    uint8_t cpuId = 1;
    uint8_t storage[1024] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[24] = 0x41;
    storage[50] = 0;
    storage[51] = 0;

    storage[100] = 0x01;
    storage[101] = 50;
    storage[150] = 0x00;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu1";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu1",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, InfoUpdateCpuNotPopulated)
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
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu cpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, InfoUpdateCpuDisabled)
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
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu cpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, EmptyMotherboardPath)
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

    std::string motherboard = "";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu cpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, CpuPopulatedButNotFunctional)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};

    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[24] = 0x42;
    storage[32] = 4;
    storage[33] = 5;
    storage[34] = 6;
    storage[16] = 3;
    storage[50] = 0;
    storage[51] = 0;
    const char strings[] =
        "\0CPU Socket 0\0Intel\0Version\0SN123\0TAG\0PN123\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, Constructor_SetsChassisTypeAndInstanceNumber)
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
    const char strings[] =
        "CPU Socket 0\0Intel\0Version\0SN123\0TAG\0PN123\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, InfoUpdate_GetSmbiosTypePtrReturnsNull)
{
    uint8_t cpuId = 0;
    uint8_t storage[128] = {0};
    storage[0] = 0x09;
    storage[1] = 13;
    storage[13] = 0;
    storage[14] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, InfoUpdate_SmbiosNextPtrReturnsNullDuringIteration)
{
    uint8_t cpuId = 1;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[50] = 0;
    storage[51] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu1";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu1",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, InfoUpdate_SecondGetSmbiosTypePtrReturnsNull)
{
    uint8_t cpuId = 1;
    uint8_t storage[1024] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[50] = 0;
    storage[51] = 0;
    storage[100] = 0x01;
    storage[101] = 50;
    storage[150] = 0;
    storage[151] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu1";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu1",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, LocationString_CalledFromInfoUpdate)
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
    const char strings[] = "MySocket\0Intel\0MyVersion\0SN\0AT\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, Manufacturer_CalledFromInfoUpdate)
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
    const char strings[] = "Socket\0AMD\0Version\0Serial\0Tag\0PartNum\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, Model_CalledFromInfoUpdate)
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
    const char strings[] = "Socket\0Vendor\0ModelString\0SN\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, SerialNumber_CalledFromInfoUpdate)
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
    const char strings[] = "Socket\0Vendor\0Version\0MY_SERIAL\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, AssetTagString_CalledFromInfoUpdate)
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
    const char strings[] = "Socket\0Vendor\0Version\0SN\0ASSET_TAG\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, PartNumber_CalledFromInfoUpdate)
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
    const char strings[] = "Socket\0Vendor\0Version\0SN\0Tag\0PART_NUM\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, InfoUpdate_NonEmptyMotherboard_SetsAssociations)
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
    const char strings[] = "Socket\0Vendor\0Version\0SN\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, InfoUpdate_SocketPopulated_FunctionalTrue)
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
    const char strings[] = "Socket\0Vendor\0Version\0SN\0Tag\0PN\0\0";
    std::memcpy(storage + 52, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}

TEST_F(ChassisCpuTest, InfoUpdate_SocketNotPopulated_PresentAndFunctionalFalse)
{
    uint8_t cpuId = 0;
    uint8_t storage[512] = {0};
    storage[0] = processorsType;
    storage[1] = 50;
    storage[4] = 1;
    storage[24] = 0x01;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";
    std::string assocPath =
        "/xyz/openbmc_project/test/inventory/system/chassis_cpu0";

    EXPECT_NO_THROW({
        chassisCpu ccpu(
            *bus, "/xyz/openbmc_project/test/inventory/system/chassis_cpu0",
            cpuId, storage, motherboard, assocPath);
    });
}
