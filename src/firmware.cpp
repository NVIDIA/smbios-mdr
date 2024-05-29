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
#include "firmware.hpp"

#include "mdrv2.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
namespace phosphor
{
namespace smbios
{

void Firmware::firmwareInfoUpdate(void)
{
    uint8_t* dataIn = storage;
    dataIn = getSMBIOSTypeIndexPtr(dataIn, firmwareInventoryInformationType,
                                   index);
    if (dataIn == nullptr)
    {
        return;
    }

    auto firmwareInfo = reinterpret_cast<struct FirmwareInfo*>(dataIn);

    firmwareComponentName(firmwareInfo->componentName, firmwareInfo->length,
                          dataIn);
#ifdef EXPOSE_FW_INVENTORY
    firmwareVersion(firmwareInfo->Version, firmwareInfo->length, dataIn);
    firmwareId(firmwareInfo->Id, firmwareInfo->length, dataIn);
#endif
    firmwareReleaseDate(firmwareInfo->releaseDate, firmwareInfo->length,
                        dataIn);
    firmwareManufacturer(firmwareInfo->manufacturer, firmwareInfo->length,
                         dataIn);

    present(true);
#ifdef EXPOSE_FW_INVENTORY
    purpose(softwareversionIntf::VersionPurpose::Other);
#endif
    std::vector<std::tuple<std::string, std::string, std::string>> association =
        {{"software_version", "functional", "/xyz/openbmc_project/software"}};
    associationIntf::associations(association);
}

std::tuple<std::string, std::string> Firmware::getFirmwareName(uint8_t* dataIn,
                                                               int targetIndex)
{
    std::tuple<std::string, std::string> ret;
    auto& name = std::get<0>(ret);
    auto& id = std::get<1>(ret);

    auto firmwarePtr = getSMBIOSTypeIndexPtr(
        dataIn, firmwareInventoryInformationType, targetIndex);
    if (firmwarePtr == nullptr)
    {
        return ret;
    }
    auto firmwareInfo = reinterpret_cast<struct FirmwareInfo*>(firmwarePtr);
    name = positionToString(firmwareInfo->componentName, firmwareInfo->length,
                            firmwarePtr);
    id = positionToString(firmwareInfo->Id, firmwareInfo->length, firmwarePtr);

    // append designation or location to the id
    if (firmwareInfo->numOfAssociatedComponents > 0)
    {
        for (int i = 0; i < firmwareInfo->numOfAssociatedComponents; i++)
        {
            auto component = smbiosHandlePtr(
                dataIn, firmwareInfo->associatedComponentHandles[i]);
            if (component == nullptr)
            {
                continue;
            }

            auto header = reinterpret_cast<struct StructureHeader*>(component);
            switch (header->type)
            {
                case processorsType:
                case systemSlots:
                case onboardDevicesExtended:
                {
                    auto designation = positionToString(
                        component[4], header->length, component);
                    if (!designation.empty())
                    {
                        id.append("_").append(designation);
                    }
                    break;
                }
                case systemPowerSupply:
                {
                    auto location = positionToString(component[5],
                                                     header->length, component);
                    if (!location.empty())
                    {
                        id.append("_").append(location);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return ret;
}

void Firmware::firmwareComponentName(const uint8_t positionNum,
                                     const uint8_t structLen, uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);
    prettyName(result);
}
#ifdef EXPOSE_FW_INVENTORY
void Firmware::firmwareVersion(const uint8_t positionNum,
                               const uint8_t structLen, uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);
    version(result);
}

void Firmware::firmwareId(const uint8_t positionNum, const uint8_t structLen,
                          uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);
    softwareId(result);
}
#endif

void Firmware::firmwareReleaseDate(const uint8_t positionNum,
                                   const uint8_t structLen, uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);
    buildDate(result);
}

void Firmware::firmwareManufacturer(const uint8_t positionNum,
                                    const uint8_t structLen, uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);
    manufacturer(result);
}

} // namespace smbios
} // namespace phosphor
