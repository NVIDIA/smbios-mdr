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
#include "config.h"

#include "smbios_mdrv2.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/AssetTag/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Instance/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Chassis/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <regex>

namespace phosphor
{

namespace smbios
{

using asset =
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::Asset;
using assetTagType =
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::AssetTag;
using location =
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::LocationCode;
using chassis =
    sdbusplus::server::xyz::openbmc_project::inventory::item::Chassis;
using Item = sdbusplus::server::xyz::openbmc_project::inventory::Item;
using association =
    sdbusplus::server::xyz::openbmc_project::association::Definitions;
using operationalStatus = sdbusplus::xyz::openbmc_project::State::Decorator::
    server::OperationalStatus;

class chassisCpu :
    sdbusplus::server::object_t<asset, assetTagType, location, chassis, Item,
                                association, operationalStatus>
{
  public:
    chassisCpu() = delete;
    chassisCpu(const chassisCpu&) = delete;
    chassisCpu& operator=(const chassisCpu&) = delete;
    chassisCpu(chassisCpu&&) = delete;
    chassisCpu& operator=(chassisCpu&&) = delete;
    ~chassisCpu() = default;
    chassisCpu(sdbusplus::bus_t& bus, const std::string& objPath,
               const uint8_t& cpuId, uint8_t* smbiosTableStorage,
               const std::string& motherboard, const std::string& assocPath) :
        sdbusplus::server::object_t<asset, assetTagType, location, chassis,
                                    Item, association, operationalStatus>(
            bus, objPath.c_str()),
        cpuNum(cpuId), storage(smbiosTableStorage),
        motherboardPath(motherboard), objPath(assocPath)
    {
        infoUpdate(smbiosTableStorage, motherboard);

        // the default value is unknown, set to Component when CPU exists
        chassis::type(chassis::ChassisType::Component);
    }

    void infoUpdate(uint8_t* smbiosTableStorage,
                    const std::string& motherboard);

  private:
    uint8_t cpuNum;

    uint8_t* storage;

    std::string motherboardPath;

    std::string objPath;

    struct ProcessorInfo
    {
        uint8_t type;
        uint8_t length;
        uint16_t handle;
        uint8_t socketDesignation;
        uint8_t processorType;
        uint8_t family;
        uint8_t manufacturer;
        uint64_t id;
        uint8_t version;
        uint8_t voltage;
        uint16_t exClock;
        uint16_t maxSpeed;
        uint16_t currSpeed;
        uint8_t status;
        uint8_t upgrade;
        uint16_t l1Handle;
        uint16_t l2Handle;
        uint16_t l3Handle;
        uint8_t serialNum;
        uint8_t assetTag;
        uint8_t partNum;
        uint8_t coreCount;
        uint8_t coreEnable;
        uint8_t threadCount;
        uint16_t characteristics;
        uint16_t family2;
        uint16_t coreCount2;
        uint16_t coreEnable2;
        uint16_t threadCount2;
    } __attribute__((packed));

    void locationString(const uint8_t positionNum, const uint8_t structLen,
                        uint8_t* dataIn);
    void manufacturer(const uint8_t positionNum, const uint8_t structLen,
                      uint8_t* dataIn);
    void serialNumber(const uint8_t positionNum, const uint8_t structLen,
                      uint8_t* dataIn);
    void assetTagString(const uint8_t positionNum, const uint8_t structLen,
                        uint8_t* dataIn);
    void partNumber(const uint8_t positionNum, const uint8_t structLen,
                    uint8_t* dataIn);
    void model(const uint8_t positionNum, const uint8_t structLen,
               uint8_t* dataIn);
};

} // namespace smbios

} // namespace phosphor
