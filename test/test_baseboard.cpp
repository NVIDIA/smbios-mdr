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
#include "baseboard.hpp"
#include "smbios_mdrv2.hpp"

#include <cstring>

#include <gtest/gtest.h>

using namespace phosphor::smbios;

static void setBaseboardTable(uint8_t* storage, size_t base, uint8_t boardType,
                              uint8_t numContained, uint16_t handle0 = 0)
{
    storage[base + 0] = baseboardType;
    storage[base + 1] = 17;
    storage[base + 2] = 0x2B;
    storage[base + 3] = 0x00;
    for (int i = 4; i <= 12; i++)
        storage[base + i] = 0;
    storage[base + 13] = boardType;
    storage[base + 14] = numContained;
    storage[base + 15] = handle0 & 0xFF;
    storage[base + 16] = (handle0 >> 8) & 0xFF;
    storage[base + 17] = 0;
    storage[base + 18] = 0;
}

static void setStructureWithHandle(uint8_t* storage, size_t base,
                                   uint16_t handle)
{
    storage[base + 0] = chassisType;
    storage[base + 1] = 5;
    storage[base + 2] = handle & 0xFF;
    storage[base + 3] = (handle >> 8) & 0xFF;
    storage[base + 4] = 0;
    storage[base + 5] = 0;
    storage[base + 6] = 0;
}

TEST(Baseboard, CompilesAndLinks)
{
    SUCCEED();
}

TEST(Baseboard, ConstructorNullStorage)
{
    uint8_t* storage = nullptr;
    Baseboard bb(0, storage);
    EXPECT_EQ(bb.getType(), Baseboard::BoardType::Reserved);
    EXPECT_EQ(bb.getName(), "Board_0");
}

TEST(Baseboard, ConstructorNoType2InTable)
{
    uint8_t storage[64] = {0};
    storage[0] = 0x01;
    storage[1] = 4;
    storage[2] = 0;
    storage[3] = 0;
    storage[4] = 0;
    storage[5] = 0;
    Baseboard bb(0, storage);
    EXPECT_EQ(bb.getType(), Baseboard::BoardType::Reserved);
    EXPECT_EQ(bb.getName(), "Board_0");
}

TEST(Baseboard, ConstructorValidType2)
{
    uint8_t storage[256] = {0};
    setBaseboardTable(storage, 0, 10, 0);

    Baseboard bb(0, storage);
    EXPECT_EQ(static_cast<uint8_t>(bb.getType()), 10u);
    EXPECT_EQ(bb.getName(), "Board_0");
}

TEST(Baseboard, SetNameGetName)
{
    uint8_t storage[256] = {0};
    setBaseboardTable(storage, 0, 5, 0);

    Baseboard bb(0, storage);
    bb.setName("CustomBoard");
    EXPECT_EQ(bb.getName(), "CustomBoard");
}

TEST(Baseboard, FindIndexOfTypeNoMatch)
{
    uint8_t storage[256] = {0};
    setBaseboardTable(storage, 0, 5, 0);

    Baseboard bb(0, storage);
    auto [found, idx] = bb.findIndexOfType(0x1234);
    EXPECT_FALSE(found);
    EXPECT_EQ(idx, 0);
}

TEST(Baseboard, FindIndexOfTypeNullHeader)
{
    uint8_t storage[256] = {0};
    setBaseboardTable(storage, 0, 5, 1, 0x9999);

    Baseboard bb(0, storage);
    auto [found, idx] = bb.findIndexOfType(0x9999);
    EXPECT_FALSE(found);
}

TEST(Baseboard, FindIndexOfTypeMatchWithResolvedHandle)
{
    uint8_t storage[256] = {0};
    const uint16_t containedHandle = 0x0040;
    setBaseboardTable(storage, 0, 5, 1, containedHandle);
    setStructureWithHandle(storage, 19, containedHandle);

    Baseboard bb(0, storage);
    auto [found, idx] = bb.findIndexOfType(containedHandle);
    EXPECT_TRUE(found);
    EXPECT_EQ(idx, 0);
}

TEST(Baseboard, GetTypeReservedWhenRawNull)
{
    uint8_t storage[64] = {0};
    Baseboard bb(0, storage);
    EXPECT_EQ(bb.getType(), Baseboard::BoardType::Reserved);
}

TEST(Baseboard, BoardTypeEnumValues)
{
    EXPECT_EQ(static_cast<uint8_t>(Baseboard::BoardType::Motherboard), 10);
    EXPECT_EQ(static_cast<uint8_t>(Baseboard::BoardType::ProcessorModule), 6);
}
