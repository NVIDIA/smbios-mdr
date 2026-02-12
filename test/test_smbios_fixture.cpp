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
