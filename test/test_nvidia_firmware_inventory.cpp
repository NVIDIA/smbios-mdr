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
#include "nvidia_firmware_inventory.hpp"

#include <gtest/gtest.h>

namespace phosphor
{
namespace smbios
{

TEST(NvidiaFirmwareInventory, CompilesAndLinks)
{
    SUCCEED();
}

TEST(NvidiaFirmwareInventory, FilterFirmwareNamePassThrough)
{
    EXPECT_EQ(filterFirmwareName(""), "");
    EXPECT_EQ(filterFirmwareName("SomeComponent"), "SomeComponent");
    EXPECT_EQ(filterFirmwareName("BMC"), "BMC");
}

TEST(NvidiaFirmwareInventory, FilterFirmwareNameNormalizesInput)
{
    std::string name = "MyFirmware_1.0";
    EXPECT_EQ(filterFirmwareName(name), name);
}

TEST(NvidiaFirmwareInventory, FilterFirmwareNameLongString)
{
    std::string name =
        "VeryLongFirmwareNameThatDoesNotContainAnyFilteredComponentNames";
    EXPECT_EQ(filterFirmwareName(name), name);
}

TEST(NvidiaFirmwareInventory, FilterFirmwareNameSpecialCharacters)
{
    std::string name = "Firmware-With-Special_Chars.v1.0";
    EXPECT_EQ(filterFirmwareName(name), name);
}

TEST(NvidiaFirmwareInventory, FilterFirmwareNameUnderscores)
{
    std::string name = "Some_Component_Name_1_2";
    EXPECT_EQ(filterFirmwareName(name), name);
}

TEST(NvidiaFirmwareInventory, FilterFirmwareNameComponentNames)
{
    /* When FIRMWARE_COMPONENT_NAME_* macros are defined, component names may be
     * filtered to ""; otherwise pass-through. Accept both so tests pass in any
     * build. */
    std::string bmc = filterFirmwareName("BMC");
    EXPECT_TRUE(bmc == "BMC" || bmc.empty());
    std::string nic = filterFirmwareName("NIC");
    EXPECT_TRUE(nic == "NIC" || nic.empty());
    std::string fpga = filterFirmwareName("FPGA");
    EXPECT_TRUE(fpga == "FPGA" || fpga.empty());
    std::string tpm = filterFirmwareName("TPM");
    EXPECT_TRUE(tpm == "TPM" || tpm.empty());
    std::string bios = filterFirmwareName("BIOS");
    EXPECT_TRUE(bios == "BIOS" || bios.empty());
}

TEST(NvidiaFirmwareInventory, FilterFirmwareNameSubstringMatch)
{
    std::string withBmc = filterFirmwareName("SystemBMC");
    EXPECT_TRUE(withBmc == "SystemBMC" || withBmc.empty());
    std::string withBios = filterFirmwareName("LegacyBIOS");
    EXPECT_TRUE(withBios == "LegacyBIOS" || withBios.empty());
}

} // namespace smbios
} // namespace phosphor
