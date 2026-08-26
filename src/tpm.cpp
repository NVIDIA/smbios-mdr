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
#include "tpm.hpp"

#include "mdrv2.hpp"

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
namespace phosphor
{
namespace smbios
{

void Tpm::tpmInfoUpdate(uint8_t* smbiosTableStorage,
                        const std::string& motherboard)
{
    storage = smbiosTableStorage;
    motherboardPath = motherboard;

    uint8_t* dataIn = storage;
    // Require only the fixed fields we read; trailing OEM bytes are optional.
    dataIn = getSMBIOSTypePtr(dataIn, tpmDeviceType, offsetof(TPMInfo, oem),
                              storage + smbiosTableStorageSize);
    if (dataIn == nullptr)
    {
        return;
    }
    for (uint8_t index = 0; index < tpmId; index++)
    {
        dataIn = smbiosNextPtr(dataIn, storage + smbiosTableStorageSize);
        if (dataIn == nullptr)
        {
            return;
        }
        dataIn = getSMBIOSTypePtr(dataIn, tpmDeviceType, offsetof(TPMInfo, oem),
                                  storage + smbiosTableStorageSize);
        if (dataIn == nullptr)
        {
            return;
        }
    }
    auto tpmInfo = reinterpret_cast<struct TPMInfo*>(dataIn);

    present(true);
    purpose(softwareversion::VersionPurpose::Other);
    tpmVendor(tpmInfo);
    tpmFirmwareVersion(tpmInfo);
    tpmDescription(tpmInfo->description, tpmInfo->length, dataIn);
    trustedComponentType(trustedComponent::ComponentAttachType::Discrete);
}

void Tpm::tpmVendor(const struct TPMInfo* tpmInfo)
{
    constexpr int vendorIdLength = 4;
    // Specified as four ASCII characters, as defined by TCG Vendor ID
    char vendorId[vendorIdLength + 1];
    int i;
    for (i = 0; i < vendorIdLength && tpmInfo->vendor[i] != '\0'; i++)
    {
        if (std::isprint(tpmInfo->vendor[i]))
        {
            vendorId[i] = tpmInfo->vendor[i];
        }
        else
        {
            vendorId[i] = '.';
        }
    }
    vendorId[i] = '\0';
    manufacturer(vendorId);
}

void Tpm::tpmFirmwareVersion(const struct TPMInfo* tpmInfo)
{
    std::stringstream stream;

    if (tpmInfo->specMajor == tpmMajorVersion1)
    {
        auto ver = reinterpret_cast<const struct TPMVersionSpec1*>(
            &tpmInfo->firmwareVersion1);
        stream << ver->revMajor << "." << ver->revMinor;
    }
    else if (tpmInfo->specMajor == tpmMajorVersion2)
    {
        auto ver = reinterpret_cast<const struct TPMVersionSpec2*>(
            &tpmInfo->firmwareVersion1);
        stream << ver->revMajor << "." << ver->revMinor;
    }
    version(stream.str());
}

void Tpm::tpmDescription(const uint8_t positionNum, const uint8_t structLen,
                         uint8_t* dataIn)
{
    std::string result = positionToString(positionNum, structLen, dataIn,
                                          storage + smbiosTableStorageSize);
    prettyName(result);
}
} // namespace smbios
} // namespace phosphor
