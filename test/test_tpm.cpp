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
#include "smbios_mdrv2.hpp"
#include "test_mock_helpers.hpp"
#include "tpm.hpp"

#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::smbios;
using namespace phosphor::smbios::test;

TEST(TpmConstants, MajorVersionValues)
{
    EXPECT_EQ(tpmMajorVersion1, 0x01);
    EXPECT_EQ(tpmMajorVersion2, 0x02);
}

class TpmTest : public TestFixtureBase
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

TEST_F(TpmTest, ConstructorNoThrow)
{
    uint8_t storage[64] = {0};
    storage[0] = tpmDeviceType;
    storage[1] = 28;
    storage[18] = 0;
    storage[28] = 0;
    storage[29] = 0;
    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", 0,
                storage, "/xyz/openbmc_project/test/inventory/system");
    });
}

TEST_F(TpmTest, TpmInfoUpdateNullStorage)
{
    uint8_t tpmID = 0;
    uint8_t* storage = nullptr;
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmInfoUpdateEmptyStorage)
{
    uint8_t tpmID = 0;
    uint8_t storage[100] = {0};
    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmInfoUpdateWithValidDataTPM12)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[2] = 0x2B;
    storage[3] = 0x00;
    storage[4] = 'I';
    storage[5] = 'N';
    storage[6] = 'T';
    storage[7] = 'C';
    storage[8] = 0x01;
    storage[9] = 0x02;

    storage[10] = 0x01;
    storage[11] = 0x02;
    storage[12] = 0x01;
    storage[13] = 0x02;

    storage[14] = 0x00;
    storage[15] = 0x00;
    storage[16] = 0x00;
    storage[17] = 0x00;
    storage[18] = 1;

    storage[0x1C] = 0;
    storage[0x1D] = 0;

    const char strings[] = "\0TPM Device\0\0";
    std::memcpy(storage + 0x1E, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmInfoUpdateWithTPM20Data)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[4] = 'I';
    storage[5] = 'N';
    storage[6] = 'T';
    storage[7] = 'C';
    storage[8] = 0x02;
    storage[9] = 0x00;

    storage[10] = 0x02;
    storage[11] = 0x00;
    storage[12] = 0x01;
    storage[13] = 0x00;
    storage[18] = 1;
    storage[0x1C] = 0;
    storage[0x1D] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmInfoUpdateMultipleTPMs)
{
    uint8_t tpmID = 1;
    uint8_t storage[1024] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[8] = 0x01;
    storage[0x1C] = 0;
    storage[0x1D] = 0;

    storage[50] = tpmDeviceType;
    storage[51] = 0x1C;
    storage[58] = 0x01;
    storage[0x1C + 50] = 0;
    storage[0x1D + 50] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm1", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmInfoUpdateNonPrintableVendorID)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[4] = '\x01';
    storage[5] = 'N';
    storage[6] = 'T';
    storage[7] = 'C';
    storage[8] = 0x01;
    storage[0x1C] = 0;
    storage[0x1D] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmInfoUpdateEmptyMotherboard)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[8] = 0x01;
    storage[0x1C] = 0;
    storage[0x1D] = 0;

    std::string motherboard = "";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmInfoUpdateNullDuringIteration)
{
    uint8_t tpmID = 1;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[8] = 0x01;
    storage[0x1C] = 0;
    storage[0x1D] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm1", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmInfoUpdateNullDuringIterationLine47)
{
    uint8_t tpmID = 1;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[8] = 0x01;
    storage[0x1C] = 0;
    storage[0x1D] = 0;

    storage[50] = 0x01;
    storage[51] = 0x1C;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm1", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmVendorNonPrintable)
{
    uint8_t storage[512] = {0};
    uint8_t tpmId = 0;

    storage[0] = tpmDeviceType;
    storage[1] = 20;
    storage[4] = 0x01;
    storage[5] = 0x00;
    storage[6] = 0x00;
    storage[7] = 0x00;
    storage[8] = 1;

    storage[9] = 0x01;
    storage[10] = 0x41;
    storage[11] = 0x42;
    storage[12] = 0x00;

    storage[20] = 0;
    storage[21] = 0;

    uint8_t* strStart = storage + 22;
    std::string description = "TPM Device";
    std::memcpy(strStart, description.c_str(), description.length());
    strStart += description.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmId,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmFirmwareVersionTPM12)
{
    uint8_t storage[512] = {0};
    uint8_t tpmId = 0;

    storage[0] = tpmDeviceType;
    storage[1] = 20;
    storage[4] = 0x01;
    storage[5] = 0x00;
    storage[6] = 0x05;
    storage[7] = 0x10;
    storage[8] = 1;
    storage[9] = 0x41;
    storage[10] = 0x42;
    storage[11] = 0x43;
    storage[12] = 0x44;

    storage[20] = 0;
    storage[21] = 0;

    uint8_t* strStart = storage + 22;
    std::string description = "TPM Device";
    std::memcpy(strStart, description.c_str(), description.length());
    strStart += description.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmId,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmFirmwareVersionTPM20)
{
    uint8_t storage[512] = {0};
    uint8_t tpmId = 0;

    storage[0] = tpmDeviceType;
    storage[1] = 20;
    storage[4] = 0x02;
    storage[5] = 0x00;
    storage[6] = 0x03;
    storage[7] = 0x20;
    storage[8] = 1;
    storage[9] = 0x49;
    storage[10] = 0x4E;
    storage[11] = 0x54;
    storage[12] = 0x43;

    storage[20] = 0;
    storage[21] = 0;

    uint8_t* strStart = storage + 22;
    std::string description = "TPM Device";
    std::memcpy(strStart, description.c_str(), description.length());
    strStart += description.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmId,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmFirmwareVersionUnknown)
{
    uint8_t storage[512] = {0};
    uint8_t tpmId = 0;

    storage[0] = tpmDeviceType;
    storage[1] = 20;
    storage[4] = 0xFF;
    storage[5] = 0x00;
    storage[6] = 0x00;
    storage[7] = 0x00;
    storage[8] = 1;
    storage[9] = 0x41;
    storage[10] = 0x42;
    storage[11] = 0x43;
    storage[12] = 0x44;

    storage[20] = 0;
    storage[21] = 0;

    uint8_t* strStart = storage + 22;
    std::string description = "TPM Device";
    std::memcpy(strStart, description.c_str(), description.length());
    strStart += description.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmId,
                storage, motherboard);
    });
}

TEST_F(TpmTest, EmptyMotherboardPath)
{
    uint8_t storage[512] = {0};
    uint8_t tpmId = 0;

    storage[0] = tpmDeviceType;
    storage[1] = 20;
    storage[4] = 0x02;
    storage[5] = 0x00;
    storage[6] = 0x01;
    storage[7] = 0x10;
    storage[8] = 1;
    storage[9] = 0x49;
    storage[10] = 0x4E;
    storage[11] = 0x54;
    storage[12] = 0x43;

    storage[20] = 0;
    storage[21] = 0;

    uint8_t* strStart = storage + 22;
    std::string description = "TPM Device";
    std::memcpy(strStart, description.c_str(), description.length());
    strStart += description.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string motherboard = "";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmId,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmVendorNullTerminatedEarly)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[2] = 0x2B;
    storage[3] = 0x00;
    storage[4] = 'I';
    storage[5] = '\0';
    storage[6] = 'T';
    storage[7] = 'C';
    storage[8] = 0x01;
    storage[9] = 0x02;

    storage[10] = 0x01;
    storage[11] = 0x02;
    storage[12] = 0x01;
    storage[13] = 0x02;

    storage[14] = 0x00;
    storage[15] = 0x00;
    storage[16] = 0x00;
    storage[17] = 0x00;
    storage[18] = 1;

    storage[0x1C] = 0;
    storage[0x1D] = 0;

    const char strings[] = "\0TPM Device\0\0";
    std::memcpy(storage + 0x1E, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmFirmwareVersionOtherSpec)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[2] = 0x2B;
    storage[3] = 0x00;
    storage[4] = 'I';
    storage[5] = 'N';
    storage[6] = 'T';
    storage[7] = 'C';
    storage[8] = 0x03;
    storage[9] = 0x02;

    storage[10] = 0x01;
    storage[11] = 0x02;
    storage[12] = 0x01;
    storage[13] = 0x02;

    storage[14] = 0x00;
    storage[15] = 0x00;
    storage[16] = 0x00;
    storage[17] = 0x00;
    storage[18] = 1;

    storage[0x1C] = 0;
    storage[0x1D] = 0;

    const char strings[] = "\0TPM Device\0\0";
    std::memcpy(storage + 0x1E, strings, sizeof(strings));

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmDescriptionEmpty)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[2] = 0x2B;
    storage[3] = 0x00;
    storage[4] = 'I';
    storage[5] = 'N';
    storage[6] = 'T';
    storage[7] = 'C';
    storage[8] = 0x01;
    storage[9] = 0x02;
    storage[10] = 0x01;
    storage[11] = 0x02;
    storage[12] = 0x01;
    storage[13] = 0x02;
    storage[14] = 0x00;
    storage[15] = 0x00;
    storage[16] = 0x00;
    storage[17] = 0x00;
    storage[18] = 0;

    storage[0x1C] = 0;
    storage[0x1D] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmVendorAllPrintable)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[4] = 'A';
    storage[5] = 'B';
    storage[6] = 'C';
    storage[7] = 'D';
    storage[8] = 0x01;
    storage[9] = 0x00;
    storage[10] = 0x01;
    storage[11] = 0x00;
    storage[12] = 0x00;
    storage[13] = 0x00;
    storage[14] = 0x00;
    storage[15] = 0x00;
    storage[16] = 0x00;
    storage[17] = 0x00;
    storage[18] = 0;

    storage[0x1C] = 0;
    storage[0x1D] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmFirmwareVersionSpec1Format)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[4] = 'I';
    storage[5] = 'N';
    storage[6] = 'T';
    storage[7] = 'C';
    storage[8] = tpmMajorVersion1;
    storage[9] = 0x00;
    storage[10] = 5;
    storage[11] = 0;
    storage[12] = 3;
    storage[13] = 0;
    storage[18] = 0;
    storage[0x1C] = 0;
    storage[0x1D] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}

TEST_F(TpmTest, TpmFirmwareVersionSpec2Format)
{
    uint8_t tpmID = 0;
    uint8_t storage[512] = {0};

    storage[0] = tpmDeviceType;
    storage[1] = 0x1C;
    storage[4] = 'I';
    storage[5] = 'N';
    storage[6] = 'T';
    storage[7] = 'C';
    storage[8] = tpmMajorVersion2;
    storage[9] = 0x00;
    storage[10] = 0x02;
    storage[11] = 0x00;
    storage[12] = 0x01;
    storage[13] = 0x00;
    storage[18] = 0;
    storage[0x1C] = 0;
    storage[0x1D] = 0;

    std::string motherboard = "/xyz/openbmc_project/test/inventory/system";

    EXPECT_NO_THROW({
        Tpm tpm(*bus, "/xyz/openbmc_project/test/inventory/system/tpm0", tpmID,
                storage, motherboard);
    });
}
