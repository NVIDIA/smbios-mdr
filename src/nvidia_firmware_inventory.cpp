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

namespace phosphor
{
namespace smbios
{
std::string filterFirmwareName(const std::string& firmwareName)
{
#ifdef FIRMWARE_COMPONENT_NAME_BMC
    std::string bmcComponentName(FIRMWARE_COMPONENT_NAME_BMC);
    if (firmwareName.rfind(bmcComponentName) != std::string::npos)
    {
        return "";
    }
#endif
#ifdef FIRMWARE_COMPONENT_NAME_NIC
    std::string nicComponentName(FIRMWARE_COMPONENT_NAME_NIC);
    if (firmwareName.rfind(nicComponentName) != std::string::npos)
    {
        return "";
    }
#endif
#ifdef FIRMWARE_COMPONENT_NAME_FPGA
    std::string fpgaComponentName(FIRMWARE_COMPONENT_NAME_FPGA);
    if (firmwareName.rfind(fpgaComponentName) != std::string::npos)
    {
        return "";
    }
#endif
#ifdef FIRMWARE_COMPONENT_NAME_TPM
    std::string tpmComponentName(FIRMWARE_COMPONENT_NAME_TPM);
    if (firmwareName.rfind(tpmComponentName) != std::string::npos)
    {
        return "";
    }
#endif
#ifdef FIRMWARE_COMPONENT_NAME_BIOS
    std::string biosComponentName(FIRMWARE_COMPONENT_NAME_BIOS);
    if (firmwareName.rfind(biosComponentName) != std::string::npos)
    {
        return "";
    }
#endif
    return firmwareName;
}
} // namespace smbios
} // namespace phosphor
