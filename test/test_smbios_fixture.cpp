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

#include <cstring>

#include <gtest/gtest.h>

using namespace phosphor::smbios::test;

TEST(SmbiosFixture, LoadRealDumpWhenAvailable)
{
    auto path = getDefaultSmbiosFixturePath();
    auto fixture = loadSmbiosFromFile(path);
    if (!fixture.loaded())
    {
        GTEST_SKIP()
            << "No fixture at " << path
            << " (set SMBIOS_TEST_FIXTURE or add test/fixtures/smbios2.bin)";
    }
    uint8_t* table = fixture.tablePtr;
    EXPECT_NE(table, nullptr);

    uint8_t* proc = getSMBIOSTypePtr(table, processorsType);
    if (proc)
    {
        EXPECT_EQ(*proc, processorsType);
    }

    uint8_t* cache = getSMBIOSTypePtr(table, cacheType);
    if (cache)
    {
        EXPECT_EQ(*cache, cacheType);
    }

    uint8_t* pcie = getSMBIOSTypePtr(table, systemSlots);
    if (pcie)
    {
        EXPECT_EQ(*pcie, systemSlots);
    }

    uint8_t* dimm = getSMBIOSTypePtr(table, memoryDeviceType);
    if (dimm)
    {
        EXPECT_EQ(*dimm, memoryDeviceType);
    }

    uint8_t* tpm = getSMBIOSTypePtr(table, tpmDeviceType);
    if (tpm)
    {
        EXPECT_EQ(*tpm, tpmDeviceType);
    }

    uint8_t* fw = getSMBIOSTypePtr(table, firmwareInventoryInformationType);
    if (fw)
    {
        EXPECT_EQ(*fw, firmwareInventoryInformationType);
    }
}

TEST(SmbiosPrimitives, PositionToStringBasics)
{
    uint8_t buf[64] = {0};
    const char strs[] = "AB\0CD\0";
    std::memcpy(buf + 4, strs, sizeof(strs));
    const uint8_t* end = buf + sizeof(buf);

    EXPECT_EQ(positionToString(0, 4, buf, end), "");     // positionNum == 0
    EXPECT_EQ(positionToString(65, 4, buf, end), "");    // exceeds index cap
    EXPECT_EQ(positionToString(1, 4, nullptr, end), ""); // null dataIn
    EXPECT_EQ(positionToString(1, 4, buf, end), "AB");
    EXPECT_EQ(positionToString(2, 4, buf, end), "CD");
}

TEST(SmbiosPrimitives, PositionToStringBoundedByDataEnd)
{
    uint8_t buf[64];
    std::memset(buf, 'X', sizeof(buf)); // no terminator anywhere
    const uint8_t* end = buf + sizeof(buf);

    // No NUL before the buffer end -> bounded read returns empty.
    EXPECT_EQ(positionToString(1, 4, buf, end), "");
    // A second string that never terminates before the end.
    EXPECT_EQ(positionToString(2, 4, buf, end), "");
    // structLen pushes the start pointer at/after dataEnd -> empty.
    EXPECT_EQ(positionToString(1, 4, buf, buf + 2), "");
}

TEST(SmbiosPrimitives, PositionToStringLegacyNoDataEnd)
{
    uint8_t buf[64] = {0};
    const char strs[] = "hello\0";
    std::memcpy(buf + 4, strs, sizeof(strs));
    // dataEnd defaulted (nullptr): legacy limit-bounded path.
    EXPECT_EQ(positionToString(1, 4, buf), "hello");
}

TEST(SmbiosPrimitives, GetTypePtrSizeGuardAndBounds)
{
    uint8_t buf[64] = {0};
    buf[0] = memoryDeviceType;
    buf[1] = 8;
    buf[8] = 0;
    buf[9] = 0;
    const uint8_t* end = buf + sizeof(buf);

    EXPECT_EQ(getSMBIOSTypePtr(buf, memoryDeviceType, 0, end), buf);
    EXPECT_EQ(getSMBIOSTypePtr(buf, memoryDeviceType, 16, end),
              nullptr);                                               // short
    EXPECT_EQ(getSMBIOSTypePtr(buf, tpmDeviceType, 0, end), nullptr); // absent
    EXPECT_EQ(getSMBIOSTypePtr(nullptr, memoryDeviceType, 0, end), nullptr);
    // Declared struct does not fit before dataEnd.
    EXPECT_EQ(getSMBIOSTypePtr(buf, memoryDeviceType, 8, buf + 4), nullptr);
}

TEST(SmbiosPrimitives, NextPtrBounds)
{
    uint8_t buf[64] = {0};
    buf[0] = memoryDeviceType;
    buf[1] = 8;
    buf[8] = 0;
    buf[9] = 0;
    buf[10] = tpmDeviceType;
    buf[11] = 4;
    const uint8_t* end = buf + sizeof(buf);

    EXPECT_EQ(smbiosNextPtr(buf, end), buf + 10);
    EXPECT_EQ(smbiosNextPtr(nullptr, end), nullptr);
    EXPECT_EQ(smbiosNextPtr(buf, buf + 1), nullptr); // dataEnd too tight

    // Terminator sits within one separator of dataEnd -> bounded to nullptr.
    uint8_t bufn[16] = {0};
    bufn[0] = memoryDeviceType;
    bufn[1] = 4;
    EXPECT_EQ(smbiosNextPtr(bufn, bufn + 5), nullptr);
}

TEST(SmbiosPrimitives, SkipEntryPointBounds)
{
    uint8_t buf[64] = {0};
    EXPECT_EQ(smbiosSkipEntryPoint(buf, buf + sizeof(buf)), buf); // no anchor
    EXPECT_EQ(smbiosSkipEntryPoint(nullptr, buf + sizeof(buf)), nullptr);

    uint8_t buf2[128] = {0};
    std::memcpy(buf2, "_SM3_", 5);
    buf2[16] = 8; // structTableAddr (offset 16) advances by 8
    EXPECT_EQ(smbiosSkipEntryPoint(buf2, buf2 + sizeof(buf2)), buf2 + 8);
    // dataEnd too small to hold a full entry point -> unchanged.
    EXPECT_EQ(smbiosSkipEntryPoint(buf2, buf2 + 4), buf2);

    // structTableAddr not below mdrSMBIOSSize -> no advance.
    uint8_t buf3[128] = {0};
    std::memcpy(buf3, "_SM3_", 5);
    buf3[16] = 0x00;
    buf3[17] = 0x80; // 0x8000 == mdrSMBIOSSize
    EXPECT_EQ(smbiosSkipEntryPoint(buf3, buf3 + sizeof(buf3)), buf3);

    // structTableAddr in range but the advance would cross dataEnd ->
    // unchanged.
    uint8_t buf4[128] = {0};
    std::memcpy(buf4, "_SM3_", 5);
    buf4[16] = 100;
    EXPECT_EQ(smbiosSkipEntryPoint(buf4, buf4 + 50), buf4);
}

TEST(SmbiosPrimitives, HandlePtrBounds)
{
    uint8_t buf[64] = {0};
    buf[0] = memoryDeviceType;
    buf[1] = 8;
    buf[2] = 0x34;
    buf[3] = 0x12; // handle 0x1234
    buf[8] = 0;
    buf[9] = 0;
    const uint8_t* end = buf + sizeof(buf);

    EXPECT_EQ(smbiosHandlePtr(buf, 0x1234, end), buf);
    EXPECT_EQ(smbiosHandlePtr(buf, 0x9999, end), nullptr);
}

TEST(SmbiosPrimitives, TypeIndexPtr)
{
    uint8_t buf[64] = {0};
    buf[0] = memoryDeviceType;
    buf[1] = 8;
    buf[8] = 0;
    buf[9] = 0;
    const uint8_t* end = buf + sizeof(buf);

    EXPECT_EQ(getSMBIOSTypeIndexPtr(buf, memoryDeviceType, 0, 0, end), buf);
    EXPECT_EQ(getSMBIOSTypeIndexPtr(buf, memoryDeviceType, 1, 0, end), nullptr);
}

TEST(SmbiosPrimitives, GetTypePtrSkipsNonMatching)
{
    uint8_t buf[64] = {0};
    buf[0] = tpmDeviceType; // first record, not the one we want
    buf[1] = 4;
    buf[4] = 0;
    buf[5] = 0;
    buf[6] = memoryDeviceType; // second record
    buf[7] = 8;
    buf[14] = 0;
    buf[15] = 0;
    const uint8_t* end = buf + sizeof(buf);

    EXPECT_EQ(getSMBIOSTypePtr(buf, memoryDeviceType, 0, end), buf + 6);
}

TEST(SmbiosPrimitives, PositionToStringEndOfEntry)
{
    uint8_t buf[64] = {0};
    const char strs[] = "AB\0"; // string "AB", then two NULs (end of entry)
    std::memcpy(buf + 4, strs, sizeof(strs));
    const uint8_t* end = buf + sizeof(buf);

    // Walking to a second string hits the 0x00 0x00 end-of-entry marker.
    EXPECT_EQ(positionToString(2, 4, buf, end), "");
}
