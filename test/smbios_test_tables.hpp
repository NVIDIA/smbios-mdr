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
#pragma once

#include "smbios_mdrv2.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace phosphor
{
namespace smbios
{
namespace test
{

struct SmbiosFixture
{
    std::vector<uint8_t> buffer;
    uint8_t* tablePtr{nullptr};
    bool loaded() const
    {
        return tablePtr != nullptr;
    }
};

inline SmbiosFixture loadSmbiosFromFile(const std::string& path)
{
    SmbiosFixture out;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return out;
    auto size = f.tellg();
    f.seekg(0);
    out.buffer.resize(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(out.buffer.data()), size))
        return out;
    const uint8_t* base = out.buffer.data();
    const size_t len = out.buffer.size();
    static const char anchor30[] = "_SM3_";
    const size_t anchorLen = sizeof(anchor30) - 1;
    if (len < anchorLen)
    {
        out.tablePtr = const_cast<uint8_t*>(base);
        return out;
    }
    for (size_t i = 0; i <= len - anchorLen; i++)
    {
        if (std::memcmp(base + i, anchor30, anchorLen) != 0)
            continue;
        const size_t offAddr = i + 5 + 1 + 1 + 2 + 1 + 1 + 1 + 4;
        if (offAddr + sizeof(uint64_t) > len)
            break;
        uint64_t structTableAddr;
        std::memcpy(&structTableAddr, base + offAddr, sizeof(uint64_t));
        size_t tableOffset = static_cast<size_t>(structTableAddr);
        if (tableOffset >= len)
            break;
        out.tablePtr = const_cast<uint8_t*>(base) + tableOffset;
        return out;
    }
    out.tablePtr = const_cast<uint8_t*>(base);
    return out;
}

inline std::string getDefaultSmbiosFixturePath()
{
    const char* env = std::getenv("SMBIOS_TEST_FIXTURE");
    if (env && env[0])
        return env;
    return "test/fixtures/smbios2.bin";
}

inline std::vector<uint8_t> makeProcessorTable(
    uint8_t status = 0x41, const char* socket = "Socket0",
    const char* manufacturer = "TestVendor", const char* version = "1.0",
    const char* serial = "SN", const char* assetTag = "Tag",
    const char* partNum = "Part")
{
    std::vector<uint8_t> buf;
    buf.push_back(processorsType);
    buf.push_back(50);
    buf.push_back(0x04);
    buf.push_back(0x00);
    buf.push_back(1);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(2);
    buf.insert(buf.end(), 8, 0);
    buf.push_back(3);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(status);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(4);
    buf.push_back(5);
    buf.push_back(6);
    buf.push_back(1);
    buf.push_back(1);
    buf.push_back(1);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    while (buf.size() < 50)
        buf.push_back(0);
    for (const char* s :
         {socket, manufacturer, version, serial, assetTag, partNum})
    {
        while (*s)
            buf.push_back(static_cast<uint8_t>(*s++));
        buf.push_back(0);
    }
    buf.push_back(0);
    buf.push_back(0);
    return buf;
}

inline std::vector<uint8_t> makeSystemTable(
    uint32_t uuidTimeLow, uint16_t uuidTimeMid, uint16_t uuidTimeHi,
    uint8_t uuidSeqHi, uint8_t uuidSeqLo,
    const std::array<uint8_t, 6>& uuidNode)
{
    std::vector<uint8_t> buf;
    buf.push_back(systemType);
    buf.push_back(27);
    buf.push_back(0x01);
    buf.push_back(0x00);
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    buf.push_back(4);
    buf.push_back((uuidTimeLow >> 0) & 0xff);
    buf.push_back((uuidTimeLow >> 8) & 0xff);
    buf.push_back((uuidTimeLow >> 16) & 0xff);
    buf.push_back((uuidTimeLow >> 24) & 0xff);
    buf.push_back((uuidTimeMid >> 0) & 0xff);
    buf.push_back((uuidTimeMid >> 8) & 0xff);
    buf.push_back((uuidTimeHi >> 0) & 0xff);
    buf.push_back((uuidTimeHi >> 8) & 0xff);
    buf.push_back(uuidSeqHi);
    buf.push_back(uuidSeqLo);
    for (size_t i = 0; i < 6; i++)
        buf.push_back(uuidNode[i]);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    while (buf.size() < 27)
        buf.push_back(0);
    for (const char* s : {"Mfr", "Product", "Ver", "Serial"})
    {
        while (*s)
            buf.push_back(static_cast<uint8_t>(*s++));
        buf.push_back(0);
    }
    buf.push_back(0);
    buf.push_back(0);
    /* Pad so getSMBIOSTypePtr never reads past buffer when walking past type 1
     */
    while (buf.size() < 55)
        buf.push_back(0);
    return buf;
}

inline std::vector<uint8_t> makeBiosTable(const char* version = "BiosVer1.0")
{
    std::vector<uint8_t> buf;
    buf.push_back(biosType);
    buf.push_back(26);
    buf.push_back(0x00);
    buf.push_back(0x00);
    buf.push_back(1);
    buf.push_back(2);
    buf.insert(buf.end(), 20, 0);
    while (buf.size() < 26)
        buf.push_back(0);
    buf.push_back('V');
    buf.push_back(0);
    while (*version)
    {
        buf.push_back(static_cast<uint8_t>(*version++));
    }
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    return buf;
}

inline std::vector<uint8_t> makeFirmwareInventoryTable(
    const char* componentName = "BIOS", const char* versionStr = "1.0",
    const char* idStr = "id1", const char* releaseDate = "01/01/2020",
    const char* manufacturer = "Mfr")
{
    std::vector<uint8_t> buf;
    buf.push_back(firmwareInventoryInformationType);
    buf.push_back(35);
    buf.push_back(0x2d);
    buf.push_back(0x00);
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(0);
    buf.push_back(3);
    buf.push_back(0);
    buf.push_back(4);
    buf.push_back(5);
    buf.push_back(0);
    buf.insert(buf.end(), 8, 0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    while (buf.size() < 35)
        buf.push_back(0);
    for (const char* s :
         {componentName, versionStr, idStr, releaseDate, manufacturer})
    {
        while (*s)
            buf.push_back(static_cast<uint8_t>(*s++));
        buf.push_back(0);
    }
    buf.push_back(0);
    buf.push_back(0);
    return buf;
}

inline std::vector<uint8_t> makeSystemAndBiosTable(
    const char* biosVersion = "BiosVer1.0")
{
    const std::array<uint8_t, 6> defaultNode = {1, 2, 3, 4, 5, 6};
    auto t1 =
        makeSystemTable(0x12345678, 0x1234, 0x5678, 0xab, 0xcd, defaultNode);
    auto t0 = makeBiosTable(biosVersion);
    t1.insert(t1.end(), t0.begin(), t0.end());
    return t1;
}

} // namespace test
} // namespace smbios
} // namespace phosphor
