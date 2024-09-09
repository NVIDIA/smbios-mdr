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

#include "cpu.hpp"

#include <bitset>
#include <map>

namespace phosphor
{
namespace smbios
{

void chassisCpu::locationString(const uint8_t positionNum,
                                const uint8_t structLen, uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);

    location::locationCode(result);
}

void chassisCpu::manufacturer(const uint8_t positionNum,
                              const uint8_t structLen, uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);

    asset::manufacturer(result);
}

void chassisCpu::partNumber(const uint8_t positionNum, const uint8_t structLen,
                            uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);

    asset::partNumber(result);
}

void chassisCpu::serialNumber(const uint8_t positionNum,
                              const uint8_t structLen, uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);

    asset::serialNumber(result);
}

void chassisCpu::assetTagString(const uint8_t positionNum,
                                const uint8_t structLen, uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn);

    assetTagType::assetTag(result);
}

void chassisCpu::model(const uint8_t positionNum, const uint8_t structLen,
                       uint8_t* dataIn)
{
    std::string result;

    result = positionToString(positionNum, structLen, dataIn);

    if (IS_COPY_CPU_VERSION_TO_MODEL == true)
    {
        // populate the version to Model property for Redfish
        asset::model(result);
    }
}

void chassisCpu::infoUpdate(uint8_t* smbiosTableStorage,
                            const std::string& motherboard)
{
    storage = smbiosTableStorage;
    motherboardPath = motherboard;

    uint8_t* dataIn = storage;

    dataIn = getSMBIOSTypePtr(dataIn, processorsType);
    if (dataIn == nullptr)
    {
        return;
    }

    for (uint8_t index = 0; index < cpuNum; index++)
    {
        dataIn = smbiosNextPtr(dataIn);
        if (dataIn == nullptr)
        {
            return;
        }
        dataIn = getSMBIOSTypePtr(dataIn, processorsType);
        if (dataIn == nullptr)
        {
            return;
        }
    }

    auto cpuInfo = reinterpret_cast<struct chassisCpu::ProcessorInfo*>(dataIn);

    // the default value is unknown, set to Component when CPU exists
    chassis::type(Chassis::ChassisType::Component);

    locationString(cpuInfo->socketDesignation, cpuInfo->length,
                   dataIn); // offset 4h

    constexpr uint32_t socketPopulatedMask = 1 << 6;
    constexpr uint32_t statusMask = 0x07;
    if ((cpuInfo->status & socketPopulatedMask) == 0)
    {
        // Don't attempt to fill in any other details if the CPU is not present.
        present(false);
        functional(false);
        return;
    }
    present(true);

    if ((cpuInfo->status & statusMask) == 1)
    {
        functional(true);
    }
    else
    {
        functional(false);
    }

    manufacturer(cpuInfo->manufacturer, cpuInfo->length,
                 dataIn);                             // offset 7h

    model(cpuInfo->version, cpuInfo->length, dataIn); // offset 10h
    serialNumber(cpuInfo->serialNum, cpuInfo->length,
                 dataIn);                             // offset 20h
    assetTagString(cpuInfo->assetTag, cpuInfo->length,
                   dataIn);                           // offset 21h
    partNumber(cpuInfo->partNum, cpuInfo->length,
               dataIn);                               // offset 22h

    if (!motherboardPath.empty())
    {
        std::vector<std::tuple<std::string, std::string, std::string>> assocs;
        assocs.emplace_back("processors", "all_processors", objPath);
        assocs.emplace_back("parent_chassis", "all_chassis", motherboardPath);
        association::associations(assocs);
    }
}

} // namespace smbios
} // namespace phosphor
