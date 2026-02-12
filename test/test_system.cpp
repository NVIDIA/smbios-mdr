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
#include "smbios_test_tables.hpp"
#include "system.hpp"
#include "test_mock_helpers.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::smbios;
using namespace phosphor::smbios::test;

class SystemTest : public TestFixtureBase
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

    std::string testFilePath = "";
};

TEST_F(SystemTest, ConstructorNoThrow)
{
    uint8_t storage[64] = {0};
    EXPECT_NO_THROW({
        System system(conn, "/xyz/openbmc_project/test/inventory/system",
                      storage, testFilePath);
    });
}

TEST_F(SystemTest, UuidNullStorage)
{
    uint8_t* storage = nullptr;
    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);

    std::string value = "";

    EXPECT_NO_THROW({
        auto result = system.uuid(value);
        EXPECT_FALSE(result.empty());
    });
}

TEST_F(SystemTest, UuidReturnsDefaultStringWhenNoType1)
{
    uint8_t storage[100] = {0};
    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);
    std::string result = system.uuid("");
    EXPECT_EQ(result, "00000000-0000-0000-0000-000000000000");
}

TEST_F(SystemTest, UuidEmptyStorage)
{
    uint8_t storage[100] = {0};
    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);

    std::string value = "";

    EXPECT_NO_THROW({
        auto result = system.uuid(value);
        EXPECT_FALSE(result.empty());
    });
}

TEST_F(SystemTest, UuidWithValidData)
{
    uint8_t storage[512] = {0};

    storage[0] = systemType;
    storage[1] = 27;
    storage[2] = 0x01;
    storage[3] = 0x00;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 3;
    storage[7] = 4;

    uint8_t uuid[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    std::memcpy(storage + 8, uuid, 16);

    storage[24] = 0x01;
    storage[25] = 5;
    storage[26] = 6;

    storage[27] = 0;
    storage[28] = 0;

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);

    std::string value = "";

    EXPECT_NO_THROW({
        auto result = system.uuid(value);
        EXPECT_FALSE(result.empty());
        EXPECT_NE(result, "00000000-0000-0000-0000-000000000000");
    });
}

TEST_F(SystemTest, UuidFormatWithKnownBytes)
{
    uint8_t storage[512] = {0};
    storage[0] = systemType;
    storage[1] = 27;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 3;
    storage[7] = 4;
    uint8_t uuid[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    std::memcpy(storage + 8, uuid, 16);
    storage[27] = 0;
    storage[28] = 0;

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);
    std::string result = system.uuid("");
    EXPECT_NE(result, "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(result.size(), 36u);
    EXPECT_EQ(result[8], '-');
    EXPECT_EQ(result[13], '-');
    EXPECT_EQ(result[18], '-');
    EXPECT_EQ(result[23], '-');
}

TEST_F(SystemTest, VersionNullStorage)
{
    uint8_t* storage = nullptr;
    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);

    EXPECT_NO_THROW({
        auto result = system.version("");
        EXPECT_FALSE(result.empty());
    });
}

TEST_F(SystemTest, VersionReturnsNoBiosVersionWhenNoType0)
{
    uint8_t storage[100] = {0};
    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);
    std::string result = system.version("");
    EXPECT_EQ(result, "No BIOS Version");
}

TEST_F(SystemTest, VersionEmptyStorage)
{
    uint8_t storage[100] = {0};
    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);

    EXPECT_NO_THROW({
        auto result = system.version("");
        EXPECT_FALSE(result.empty());
    });
}

TEST_F(SystemTest, VersionWithValidBIOSData)
{
    uint8_t storage[512] = {0};

    storage[0] = biosType;
    storage[1] = 24;
    storage[2] = 0x00;
    storage[3] = 0x00;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 0xE8;
    storage[7] = 0x00;
    storage[8] = 3;
    storage[9] = 0x00;

    storage[20] = 0x00;
    storage[21] = 0x00;
    storage[22] = 0xFF;
    storage[23] = 0xFF;

    storage[24] = 0;
    storage[25] = 0;

    const char strings[] = "Vendor\0v1.0.0\0Date\0\0";
    std::memcpy(storage + 26, strings, sizeof(strings));

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);

    EXPECT_NO_THROW({ auto result = system.version(""); });
}

TEST_F(SystemTest, VersionReturnsBiosVersionStringWhenValid)
{
    uint8_t storage[512] = {0};
    storage[0] = biosType;
    storage[1] = 24;
    storage[4] = 1;
    storage[5] = 2;
    const char strings[] = "Vendor\0v1.2.3\0Date\0\0";
    std::memcpy(storage + 24, strings, sizeof(strings));

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);
    std::string result = system.version("");
    EXPECT_EQ(result, "v1.2.3");
}

TEST_F(SystemTest, VersionWithNonPrintableChars)
{
    uint8_t storage[512] = {0};

    storage[0] = biosType;
    storage[1] = 24;
    storage[2] = 0x00;
    storage[3] = 0x00;
    storage[4] = 1;
    storage[5] = 2;

    storage[24] = 0;
    storage[25] = 0;

    const char strings[] = "Vendor\0v1.0\x01\0Date\0\0";
    std::memcpy(storage + 26, strings, sizeof(strings));

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePath);

    EXPECT_NO_THROW({ auto result = system.version(""); });
}

TEST_F(SystemTest, VersionWithFileOperationFailure)
{
    uint8_t storage[512] = {0};

    storage[0] = biosType;
    storage[1] = 24;
    storage[4] = 1;
    storage[5] = 2;

    storage[24] = 0;
    storage[25] = 0;

    std::string invalidPath = "/invalid/path/that/does/not/exist/smbios2";
    const char strings[] = "Vendor\0v1.0\x01\0Date\0\0";
    std::memcpy(storage + 26, strings, sizeof(strings));

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  invalidPath);

    std::string result = system.version("");
    EXPECT_TRUE(result == "No BIOS Version" || result.empty());
}

TEST_F(SystemTest, UuidWithValidSystemInfo)
{
    uint8_t storage[512] = {0};

    storage[0] = systemType;
    storage[1] = 27;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 3;
    storage[7] = 4;
    storage[8] = 0x01;

    *reinterpret_cast<uint32_t*>(&storage[8]) = 0x12345678;
    *reinterpret_cast<uint16_t*>(&storage[12]) = 0xABCD;
    *reinterpret_cast<uint16_t*>(&storage[14]) = 0xEF01;
    storage[16] = 0x23;
    storage[17] = 0x45;
    storage[18] = 0x67;
    storage[19] = 0x89;
    storage[20] = 0xAB;
    storage[21] = 0xCD;
    storage[22] = 0xEF;
    storage[23] = 0x01;

    storage[27] = 0;
    storage[28] = 0;

    uint8_t* strStart = storage + 27;
    std::string manufacturer = "TestManufacturer";
    std::memcpy(strStart, manufacturer.c_str(), manufacturer.length());
    strStart += manufacturer.length() + 1;
    std::string product = "TestProduct";
    std::memcpy(strStart, product.c_str(), product.length());
    strStart += product.length() + 1;
    std::string version = "v1.0";
    std::memcpy(strStart, version.c_str(), version.length());
    strStart += version.length() + 1;
    std::string serial = "SN123456";
    std::memcpy(strStart, serial.c_str(), serial.length());
    strStart += serial.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  "");

    EXPECT_NO_THROW({
        auto result = system.uuid("");
        EXPECT_FALSE(result.empty());
    });
}

TEST_F(SystemTest, UuidWithNullSystemInfo)
{
    uint8_t storage[512] = {0};

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  "");

    EXPECT_NO_THROW({
        auto result = system.uuid("");
        EXPECT_FALSE(result.empty());
    });
}

TEST_F(SystemTest, VersionWithPrintableCharacters)
{
    uint8_t storage[512] = {0};

    storage[0] = biosType;
    storage[1] = 24;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 3;

    storage[24] = 0;
    storage[25] = 0;

    uint8_t* strStart = storage + 26;
    std::string vendor = "TestVendor";
    std::memcpy(strStart, vendor.c_str(), vendor.length());
    strStart += vendor.length() + 1;
    std::string version = "v1.0.0";
    std::memcpy(strStart, version.c_str(), version.length());
    strStart += version.length() + 1;
    std::string date = "01/01/2024";
    std::memcpy(strStart, date.c_str(), date.length());
    strStart += date.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  "");

    EXPECT_NO_THROW({ auto result = system.version(""); });
}

TEST_F(SystemTest, VersionWithSetProperty)
{
    uint8_t storage[512] = {0};

    storage[0] = biosType;
    storage[1] = 24;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 3;

    storage[24] = 0;
    storage[25] = 0;

    uint8_t* strStart = storage + 26;
    std::string vendor = "TestVendor";
    std::memcpy(strStart, vendor.c_str(), vendor.length());
    strStart += vendor.length() + 1;
    std::string version = "v1.0.0";
    std::memcpy(strStart, version.c_str(), version.length());
    strStart += version.length() + 1;
    std::string date = "01/01/2024";
    std::memcpy(strStart, date.c_str(), date.length());
    strStart += date.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  "");

    EXPECT_NO_THROW({ auto result = system.version(""); });
}

TEST_F(SystemTest, VersionWithNonPrintableCharsFileOpenSuccess)
{
    uint8_t storage[512] = {0};

    storage[0] = biosType;
    storage[1] = 24;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 3;

    storage[24] = 0;
    storage[25] = 0;

    uint8_t* strStart = storage + 26;
    std::string vendor = "TestVendor";
    std::memcpy(strStart, vendor.c_str(), vendor.length());
    strStart += vendor.length() + 1;
    std::string version = "v1.0\x01\x02";
    std::memcpy(strStart, version.c_str(), version.length());
    strStart += version.length() + 1;
    std::string date = "01/01/2024";
    std::memcpy(strStart, date.c_str(), date.length());
    strStart += date.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    std::string testFilePathLocal = "/tmp/test_smbios_file";
    std::ofstream testFile(testFilePathLocal);
    testFile.close();

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  testFilePathLocal);

    EXPECT_NO_THROW({ auto result = system.version(""); });
    std::remove(testFilePathLocal.c_str());
}

TEST_F(SystemTest, UuidWithAllZeroValues)
{
    uint8_t storage[512] = {0};

    storage[0] = systemType;
    storage[1] = 27;
    storage[2] = 0x00;
    storage[3] = 0x00;
    storage[4] = 0x00;
    storage[5] = 0x00;
    storage[6] = 0x00;
    storage[7] = 0x00;
    storage[8] = 0x00;
    storage[9] = 0x00;
    storage[10] = 0x00;
    storage[11] = 0x00;
    storage[12] = 0x00;
    storage[13] = 0x00;
    storage[14] = 0x00;
    storage[15] = 0x00;
    storage[16] = 0x00;
    storage[17] = 0x00;
    storage[18] = 0x00;
    storage[19] = 0x00;
    storage[20] = 0x00;
    storage[21] = 0x00;
    storage[22] = 0x00;
    storage[23] = 0x00;
    storage[24] = 0x00;
    storage[25] = 0x00;
    storage[26] = 0x00;
    storage[27] = 0x00;

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  "");

    EXPECT_NO_THROW({ auto result = system.uuid(""); });
}

TEST_F(SystemTest, GetServiceException)
{
    uint8_t storage[512] = {0};

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  "");

    EXPECT_NO_THROW({ auto result = system.version(""); });
}

TEST_F(SystemTest, SetPropertyEmptyService)
{
    uint8_t storage[512] = {0};

    storage[0] = biosType;
    storage[1] = 24;
    storage[4] = 1;
    storage[5] = 2;
    storage[6] = 3;

    storage[24] = 0;
    storage[25] = 0;

    uint8_t* strStart = storage + 26;
    std::string vendor = "TestVendor";
    std::memcpy(strStart, vendor.c_str(), vendor.length());
    strStart += vendor.length() + 1;
    std::string version = "v1.0.0";
    std::memcpy(strStart, version.c_str(), version.length());
    strStart += version.length() + 1;
    std::string date = "01/01/2024";
    std::memcpy(strStart, date.c_str(), date.length());
    strStart += date.length() + 1;
    *strStart = 0;
    *(strStart + 1) = 0;

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  "");

    EXPECT_NO_THROW({ auto result = system.version(""); });
}

TEST_F(SystemTest, UuidWithAllZeroBytes)
{
    uint8_t storage[512] = {0};
    storage[0] = systemType;
    storage[1] = 27;
    storage[2] = 0x00;
    storage[3] = 0x00;
    for (int i = 4; i < 24; i++)
        storage[i] = 0;
    storage[27] = 0;
    storage[28] = 0;

    System system(conn, "/xyz/openbmc_project/test/inventory/system", storage,
                  "");
    std::string result = system.uuid("");
    EXPECT_EQ(result, "00000000-0000-0000-0000-000000000000");
}

TEST_F(SystemTest, UuidWithMakeSystemTable)
{
    std::vector<uint8_t> table = makeSystemTable(
        0x12345678, 0x1234, 0x5678, 0xab, 0xcd, {1, 2, 3, 4, 5, 6});
    System system(conn, "/xyz/openbmc_project/test/inventory/system",
                  table.data(), testFilePath);
    std::string result = system.uuid("");
    EXPECT_EQ(result.size(), 36u);
    EXPECT_NE(result, "00000000-0000-0000-0000-000000000000");
}
