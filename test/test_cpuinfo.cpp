/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
#include "cpuinfo.hpp"
#include "test_mock_helpers.hpp"

#include <string>

#include <gtest/gtest.h>

using namespace cpu_info;
using namespace phosphor::smbios::test;

class CpuInfoTest : public TestFixtureBase
{};

TEST_F(CpuInfoTest, ConstructorStoresFields)
{
    CPUInfo info(1, 0x30, 2, 0x40);
    EXPECT_EQ(info.id, 1);
    EXPECT_EQ(info.peciAddr, 0x30);
    EXPECT_EQ(info.i2cBus, 2);
    EXPECT_EQ(info.i2cDevice, 0x40);
}

TEST_F(CpuInfoTest, PublishUUIDCreatesInterface)
{
    // publishUUID() emplaces a UniqueIdentifier D-Bus object at the cpu path
    // derived from the id and sets the uuid property. Exercises cpuinfo.hpp
    // lines 49/51.
    CPUInfo info(1, 0x30, 0, 0);

    EXPECT_NO_THROW({
        info.publishUUID(*bus, "12345678-1234-1234-1234-123456789abc");
    });

    EXPECT_TRUE(info.uuidInterface.has_value());
}

TEST_F(CpuInfoTest, PublishUUIDOverwritesExistingInterface)
{
    // Calling publishUUID twice re-emplaces the optional interface, covering
    // the path where the optional already holds a value.
    CPUInfo info(2, 0x31, 0, 0);

    EXPECT_NO_THROW({
        info.publishUUID(*bus, "11111111-1111-1111-1111-111111111111");
    });
    EXPECT_NO_THROW({
        info.publishUUID(*bus, "22222222-2222-2222-2222-222222222222");
    });

    EXPECT_TRUE(info.uuidInterface.has_value());
}
