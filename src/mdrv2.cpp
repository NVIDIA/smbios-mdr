/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "mdrv2.hpp"

#include "pcieslot.hpp"

#include <sys/mman.h>

#include <phosphor-logging/elog-errors.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Smbios/MDR_V2/error.hpp>

#include <fstream>

namespace phosphor
{
namespace smbios
{

std::vector<uint8_t> MDRV2::getDirectoryInformation(uint8_t dirIndex)
{
    std::vector<uint8_t> responseDir;

    std::ifstream smbiosFile(smbiosFilePath, std::ios_base::binary);
    if (!smbiosFile.good())
    {
        lg2::error(
            "Read data from flash error - Open MDRV2 table file failure");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            InvalidParameter();
    }
    if (dirIndex > smbiosDir.dirEntries)
    {
        responseDir.push_back(0);
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            InvalidParameter();
    }
    responseDir.push_back(mdr2Version);
    responseDir.push_back(smbiosDir.dirVersion);
    uint8_t returnedEntries = smbiosDir.dirEntries - dirIndex;
    responseDir.push_back(returnedEntries);
    if ((dirIndex + returnedEntries) >= smbiosDir.dirEntries)
    {
        responseDir.push_back(0);
    }
    else
    {
        responseDir.push_back(
            smbiosDir.dirEntries - dirIndex - returnedEntries);
    }
    for (uint8_t index = dirIndex; index < smbiosDir.dirEntries; index++)
    {
        for (uint8_t indexId = 0; indexId < sizeof(DataIdStruct); indexId++)
        {
            responseDir.push_back(
                smbiosDir.dir[index].common.id.dataInfo[indexId]);
        }
    }

    return responseDir;
}

bool MDRV2::smbiosIsAvailForUpdate(uint8_t index)
{
    bool ret = false;
    if (index >= maxDirEntries)
    {
        return ret;
    }

    switch (smbiosDir.dir[index].stage)
    {
        case MDR2SMBIOSStatusEnum::mdr2Updating:
            ret = false;
            break;

        case MDR2SMBIOSStatusEnum::mdr2Init:
            // This *looks* like there should be a break statement here,
            // as the effects of the previous statement are a noop given
            // the following code that this falls through to.
            // We've elected not to change it, though, since it's been
            // this way since the old generation, and it would affect
            // the way the code functions.
            // If it ain't broke, don't fix it.

        case MDR2SMBIOSStatusEnum::mdr2Loaded:
        case MDR2SMBIOSStatusEnum::mdr2Updated:
            if (smbiosDir.dir[index].lock == MDR2DirLockEnum::mdr2DirLock)
            {
                ret = false;
            }
            else
            {
                ret = true;
            }
            break;

        default:
            break;
    }

    return ret;
}

std::vector<uint8_t> MDRV2::getDataOffer()
{
    std::vector<uint8_t> offer(sizeof(DataIdStruct));
    if (smbiosIsAvailForUpdate(0))
    {
        std::copy(smbiosDir.dir[0].common.id.dataInfo,
                  &smbiosDir.dir[0].common.id.dataInfo[16], offer.data());
    }
    else
    {
        lg2::error("smbios is not ready for update");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            UpdateInProgress();
    }
    return offer;
}

inline uint8_t MDRV2::smbiosValidFlag(uint8_t index)
{
    FlagStatus ret = FlagStatus::flagIsInvalid;
    MDR2SMBIOSStatusEnum stage = smbiosDir.dir[index].stage;
    MDR2DirLockEnum lock = smbiosDir.dir[index].lock;

    switch (stage)
    {
        case MDR2SMBIOSStatusEnum::mdr2Loaded:
        case MDR2SMBIOSStatusEnum::mdr2Updated:
            if (lock == MDR2DirLockEnum::mdr2DirLock)
            {
                ret = FlagStatus::flagIsLocked; // locked
            }
            else
            {
                ret = FlagStatus::flagIsValid; // valid
            }
            break;

        case MDR2SMBIOSStatusEnum::mdr2Updating:
        case MDR2SMBIOSStatusEnum::mdr2Init:
            ret = FlagStatus::flagIsInvalid; // invalid
            break;

        default:
            break;
    }

    return static_cast<uint8_t>(ret);
}

// If source variable size is 4 bytes (uint32_t) and destination is Vector type
// is 1 byte (uint8_t), then by using this API can copy data byte by byte. For
// Example copying data from smbiosDir.dir[idIndex].common.size and
// smbiosDir.dir[idIndex].common.timestamp to vector variable responseInfo
template <typename T>
void appendReversed(std::vector<uint8_t>& vector, const T& value)
{
    auto data = reinterpret_cast<const uint8_t*>(&value);
    std::reverse_copy(data, data + sizeof(value), std::back_inserter(vector));
}

std::vector<uint8_t> MDRV2::getDataInformation(uint8_t idIndex)
{
    std::vector<uint8_t> responseInfo;
    responseInfo.push_back(mdr2Version);

    if (idIndex >= maxDirEntries)
    {
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            InvalidParameter();
    }

    for (uint8_t index = 0; index < sizeof(DataIdStruct); index++)
    {
        responseInfo.push_back(
            smbiosDir.dir[idIndex].common.id.dataInfo[index]);
    }

    responseInfo.push_back(smbiosValidFlag(idIndex));
    appendReversed(responseInfo, smbiosDir.dir[idIndex].common.size);
    responseInfo.push_back(smbiosDir.dir[idIndex].common.dataVersion);
    appendReversed(responseInfo, smbiosDir.dir[idIndex].common.timestamp);

    return responseInfo;
}

bool MDRV2::readDataFromFlash(MDRSMBIOSHeader* mdrHdr, uint8_t* data)
{
    if (mdrHdr == nullptr)
    {
        lg2::error("Read data from flash error - Invalid mdr header");
        return false;
    }
    if (data == nullptr)
    {
        lg2::error("Read data from flash error - Invalid data point");
        return false;
    }
    std::ifstream smbiosFile(smbiosFilePath, std::ios_base::binary);
    if (!smbiosFile.good())
    {
        lg2::error(
            "Read data from flash error - Open MDRV2 table file failure");
        return false;
    }
    smbiosFile.clear();
    smbiosFile.seekg(0, std::ios_base::end);
    size_t fileLength = smbiosFile.tellg();
    smbiosFile.seekg(0, std::ios_base::beg);
    if (fileLength < sizeof(MDRSMBIOSHeader))
    {
        lg2::error("MDR V2 file size is smaller than mdr header");
        return false;
    }
    smbiosFile.read(reinterpret_cast<char*>(mdrHdr), sizeof(MDRSMBIOSHeader));
    if (mdrHdr->dataSize > smbiosTableStorageSize)
    {
        lg2::error("Data size out of limitation");
        smbiosFile.close();
        return false;
    }
    fileLength -= sizeof(MDRSMBIOSHeader);
    if (fileLength < mdrHdr->dataSize)
    {
        smbiosFile.read(reinterpret_cast<char*>(data), fileLength);
    }
    else
    {
        smbiosFile.read(reinterpret_cast<char*>(data), mdrHdr->dataSize);
    }
    smbiosFile.close();
    return true;
}

bool MDRV2::sendDirectoryInformation(
    uint8_t dirVersion, uint8_t dirIndex, uint8_t returnedEntries,
    uint8_t remainingEntries, std::vector<uint8_t> dirEntry)
{
    bool terminate = false;
    if ((dirIndex >= maxDirEntries) || (returnedEntries < 1))
    {
        lg2::error("Send Dir info failed - input parameter invalid");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            InvalidParameter();
    }
    // Reject writes that would overrun the fixed dir[] array (OOB write).
    if (static_cast<size_t>(dirIndex) + returnedEntries > maxDirEntries)
    {
        lg2::error("Send Dir info failed - entries exceed directory capacity");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            InvalidParameter();
    }
    if ((static_cast<size_t>(returnedEntries) * sizeof(DataIdStruct)) !=
        dirEntry.size())
    {
        lg2::error("Directory size invalid");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            InvalidParameter();
    }
    if (dirVersion == smbiosDir.dirVersion)
    {
        terminate = true;
    }
    else
    {
        if (remainingEntries > 0)
        {
            terminate = false;
        }
        else
        {
            terminate = true;
            smbiosDir.dirVersion = dirVersion;
        }
        uint8_t idIndex = dirIndex;
        smbiosDir.dirEntries = returnedEntries;

        uint8_t* pData = dirEntry.data();
        if (pData == nullptr)
        {
            return false;
        }
        for (uint8_t index = 0; index < returnedEntries; index++)
        {
            auto data = reinterpret_cast<const DataIdStruct*>(pData);
            std::copy(data->dataInfo, data->dataInfo + sizeof(DataIdStruct),
                      smbiosDir.dir[idIndex + index].common.id.dataInfo);
            pData += sizeof(DataIdStruct);
        }
    }
    return terminate;
}

bool MDRV2::sendDataInformation(uint8_t idIndex, uint8_t /* flag */,
                                uint32_t dataLen, uint32_t dataVer,
                                uint32_t timeStamp)
{
    if (idIndex >= maxDirEntries)
    {
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            InvalidParameter();
    }
    static_assert((maxDirEntries & (maxDirEntries - 1)) == 0,
                  "maxDirEntries must be a power of two for the index mask");
    idIndex &= static_cast<uint8_t>(maxDirEntries - 1);

    auto& entry = smbiosDir.dir[idIndex].common;

    int entryChanged = 0;
    if (entry.dataSetSize != dataLen)
    {
        entryChanged++;
        entry.dataSetSize = dataLen;
    }

    if (entry.dataVersion != dataVer)
    {
        entryChanged++;
        entry.dataVersion = dataVer;
    }

    if (entry.timestamp != timeStamp)
    {
        entryChanged++;
        entry.timestamp = timeStamp;
    }
    if (entryChanged == 0)
    {
        return false;
    }
    return true;
}

int MDRV2::findIdIndex(std::vector<uint8_t> dataInfo)
{
    if (dataInfo.size() != sizeof(DataIdStruct))
    {
        lg2::error("Length of dataInfo invalid");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::
            InvalidId();
    }
    std::array<uint8_t, 16> arrayDataInfo;

    std::copy(dataInfo.begin(), dataInfo.end(), arrayDataInfo.begin());

    for (size_t index = 0; index < smbiosDir.dirEntries; index++)
    {
        size_t info = 0;
        for (; info < arrayDataInfo.size(); info++)
        {
            if (arrayDataInfo[info] !=
                smbiosDir.dir[index].common.id.dataInfo[info])
            {
                break;
            }
        }
        if (info == arrayDataInfo.size())
        {
            return index;
        }
    }
    throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::InvalidId();
}

uint8_t MDRV2::directoryEntries(uint8_t value)
{
    std::ifstream smbiosFile(smbiosFilePath, std::ios_base::binary);
    if (!smbiosFile.good())
    {
        lg2::error(
            "Read data from flash error - Open MDRV2 table file failure");
        value = 0;
    }
    else
    {
        value = smbiosDir.dirEntries;
    }
    return sdbusplus::server::xyz::openbmc_project::smbios::MDRV2::
        directoryEntries(value);
}

void MDRV2::systemInfoUpdate()
{
#ifndef PUBLISH_INVENTORY
    // Inventory publication disabled at build time. Records are still
    // received/persisted and served via xyz.openbmc_project.Smbios.MDR_V2,
    // but no inventory objects are created under
    // /xyz/openbmc_project/inventory. Another service owns that subtree.
    return;
#else
    // By default, look for System interface on any system/board/* object
    std::string mapperAncestorPath = smbiosInventoryPath;
    std::string matchParentPath = smbiosInventoryPath + "/board/";
    bool requireExactMatch = false;

    // If customized, look for System on only that custom object
    if (smbiosInventoryPath != defaultInventoryPath)
    {
        std::filesystem::path path(smbiosInventoryPath);

        // Search under parent to find exact match for self
        mapperAncestorPath = path.parent_path().string();
        matchParentPath = mapperAncestorPath;
        requireExactMatch = true;
    }

    std::string motherboardPath;
    auto method = bus->new_method_call(mapperBusName, mapperPath,
                                       mapperInterface, "GetSubTreePaths");
    method.append(mapperAncestorPath);
    method.append(0);

    // If customized, also accept Board as anchor, not just System
    std::vector<std::string> desiredInterfaces{systemInterface};
    if (requireExactMatch)
    {
        desiredInterfaces.emplace_back(boardInterface);
    }
    method.append(desiredInterfaces);

    try
    {
        std::vector<std::string> paths;
        sdbusplus::message_t reply = bus->call(method);
        reply.read(paths);

        size_t pathsCount = paths.size();
        for (size_t i = 0; i < pathsCount; ++i)
        {
            if (requireExactMatch && (paths[i] != smbiosInventoryPath))
            {
                continue;
            }

            motherboardPath = std::move(paths[i]);
            break;
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Exception while trying to find Inventory anchor object for SMBIOS content {I}: {E}",
            "I", smbiosInventoryPath, "E", e.what());
        lg2::error("Failed to query system motherboard: {ERROR}", "ERROR",
                   e.what());
    }

    if (motherboardPath.empty())
    {
        lg2::error(
            "Failed to get system motherboard dbus path. Setting up a match rule");

        if (motherboardConfigMatch)
        {
            lg2::info("Motherboard match rule already exists");
        }
        else
        {
            motherboardConfigMatch = std::make_unique<sdbusplus::bus::match_t>(
                *bus,
                sdbusplus::bus::match::rules::interfacesAdded() +
                    sdbusplus::bus::match::rules::argNpath(0, matchParentPath),
                [this, requireExactMatch](sdbusplus::message_t& msg) {
                    sdbusplus::message::object_path objectName;
                    boost::container::flat_map<
                        std::string,
                        boost::container::flat_map<
                            std::string, std::variant<std::string, uint64_t>>>
                        msgData;
                    msg.read(objectName, msgData);
                    bool gotMatch = false;

                    if (msgData.contains(systemInterface))
                    {
                        lg2::info("Successful match on system interface");
                        gotMatch = true;
                    }

                    // If customized, also accept Board as anchor, not just
                    // System
                    if (requireExactMatch && msgData.contains(boardInterface))
                    {
                        lg2::info("Successful match on board interface");
                        gotMatch = true;
                    }

                    if (gotMatch)
                    {
                        // There is a race condition here: our desired interface
                        // has just been created, triggering the D-Bus callback,
                        // but Object Mapper has not been told of it yet. The
                        // mapper must also add it. Stall for time, so it can.
                        sleep(2);
                        systemInfoUpdate();
                    }
                });
        }
    }
    else
    {
#ifdef ASSOC_TRIM_PATH
        // When enabled, chop off last component of motherboardPath, to trim one
        // layer, so that associations are built to the underlying chassis
        // itself, not the system boards in the chassis. This is for
        // compatibility with traditional systems which only have one
        // motherboard per chassis.
        std::filesystem::path foundPath(motherboardPath);
        motherboardPath = foundPath.parent_path().string();
#endif

        lg2::info("Found Inventory anchor object for SMBIOS content {I}: {M}",
                  "I", smbiosInventoryPath, "M", motherboardPath);
    }

    lg2::info("Using Inventory anchor object for SMBIOS content {I}: {M}", "I",
              smbiosInventoryPath, "M", motherboardPath);

    // Get ProcessorModule inventories
    std::vector<std::pair<std::string, std::optional<size_t>>> modules;
    std::vector<std::pair<
        std::string,
        std::vector<std::pair<std::string, std::vector<std::string>>>>>
        response;
    sdbusplus::message_t findProcModuleMethod =
        bus->new_method_call("xyz.openbmc_project.ObjectMapper",
                             "/xyz/openbmc_project/object_mapper",
                             "xyz.openbmc_project.ObjectMapper", "GetSubTree");
    findProcModuleMethod.append(
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.ProcessorModule"});
    try
    {
        sdbusplus::message_t reply = bus->call(findProcModuleMethod);
        reply.read(response);
    }
    catch (const sdbusplus::exception_t& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Failed to query ProcessorModule",
            phosphor::logging::entry("ERROR=%s", e.what()));
    }
    for (auto& [path, services] : response)
    {
        // instert the path with a empty instance number first
        modules.push_back({path, {}});
        for (auto& [service, interfaces] : services)
        {
            for (auto& interface : interfaces)
            {
                if (interface ==
                    "xyz.openbmc_project.Inventory.Decorator.Instance")
                {
                    std::variant<uint64_t> instanceNumber;
                    sdbusplus::message_t getInstanceMethod =
                        bus->new_method_call(service.c_str(), path.c_str(),
                                             "org.freedesktop.DBus.Properties",
                                             "Get");
                    getInstanceMethod.append(interface, "InstanceNumber");
                    try
                    {
                        sdbusplus::message_t reply =
                            bus->call(getInstanceMethod);
                        reply.read(instanceNumber);
                        // Update the instance number
                        modules.back().second =
                            std::get<uint64_t>(instanceNumber);
                    }
                    catch (const sdbusplus::exception_t& e)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "Failed to query instanceNumber",
                            phosphor::logging::entry("ERROR=%s", e.what()));
                    }
                    break;
                }
            }
        }
    }

    std::optional<size_t> num;

#ifdef PROCMOD_DBUS
    int processorModuleIndex = 0;
    baseboards.clear();
    num = getTotalSmbiosEntries(baseboardType);
    if (!num)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "get total num failed");
        return;
    }
    for (size_t index = 0; index < *num; index++)
    {
        using enum phosphor::smbios::Baseboard::BoardType;
        baseboards.emplace_back(std::make_unique<phosphor::smbios::Baseboard>(
            index, smbiosDir.dir[smbiosDirIndex].dataStorage));
        auto& baseboard = baseboards.back();
        switch (baseboard->getType())
        {
            case ProcessorModule:
            case ProcesssorMemoryModule:
            case ProcessorIoModule:
                baseboard->setName("ProcessorModule_" +
                                   std::to_string(processorModuleIndex++));
                break;
            default:
                break;
        }
    }
#endif

#ifdef CPU_DBUS
    cpus.clear();
    num = getTotalSmbiosEntries(processorsType);
    if (!num)
    {
        lg2::error("get cpu total slot failed");
        return;
    }

    // In case the new size is smaller than old, trim the vector
    if (*num < cpus.size())
    {
        cpus.resize(*num);
    }

    for (unsigned int index = 0; index < *num; index++)
    {
        std::string path = cpuPath + std::to_string(index);
        std::string cpuContainerPath = motherboardPath;

        // customize path if we know the socket number
        auto dataPtr = getSMBIOSTypeIndexPtr(
            smbiosDir.dir[smbiosDirIndex].dataStorage, processorsType, index, 0,
            smbiosDir.dir[smbiosDirIndex].dataStorage + smbiosTableStorageSize);
        auto [found, socket, chip] = Cpu::socketChipNumber(
            dataPtr,
            smbiosDir.dir[smbiosDirIndex].dataStorage + smbiosTableStorageSize);
        if (found && modules.size())
        {
            for (auto& [modulePath, moduleIntanceOpt] : modules)
            {
                if (modules.size() == 1 || (moduleIntanceOpt.has_value() &&
                                            *moduleIntanceOpt == socket))
                {
                    // make the cpu under socket path
#ifndef PLATFORM_PREFIX
                    std::filesystem::path filePath(path);
                    path.assign(modulePath)
                        .append("/")
                        .append(filePath.filename().string());
#endif
                    cpuContainerPath = modulePath;
                    break;
                }
            }
        }

        std::string decoratePath = decorateName(path);
        cpus.emplace_back(std::make_unique<phosphor::smbios::Cpu>(
            *bus, path, index, smbiosDir.dir[smbiosDirIndex].dataStorage,
            cpuContainerPath, decoratePath));
    }
#endif

#ifdef DIMM_DBUS
    dimms.clear();
    num = getTotalSmbiosEntries(memoryDeviceType);
    if (!num)
    {
        lg2::error("get dimm total slot failed");
        return;
    }

    // In case the new size is smaller than old, trim the vector
    if (*num < dimms.size())
    {
        dimms.resize(*num);
    }

    for (unsigned int index = 0; index < *num; index++)
    {
        std::string objName = "Memory_" + std::to_string(index);

        // Rename the object if it's contaned by a board
        uint8_t* dataIn = smbiosDir.dir[smbiosDirIndex].dataStorage;
        dataIn = getSMBIOSTypeIndexPtr(dataIn, memoryDeviceType, index,
                                       sizeof(struct StructureHeader),
                                       dataIn + smbiosTableStorageSize);
        if (dataIn == nullptr)
        {
            continue;
        }
        auto memoryHeader = reinterpret_cast<struct StructureHeader*>(dataIn);
        for (const auto& baseboard : baseboards)
        {
            auto [found, indexOfType] =
                baseboard->findIndexOfType(memoryHeader->handle);
            if (found == true)
            {
                objName = baseboard->getName() + "_" + "Memory_" +
                          std::to_string(indexOfType);
                break;
            }
        }

        std::string path(dimmPath);
        path += "/" + objName;
        dimms.emplace_back(std::make_unique<phosphor::smbios::Dimm>(
            *bus, path, index, smbiosDir.dir[smbiosDirIndex].dataStorage,
            motherboardPath));
    }
#endif

    pcies.clear();
    num = getTotalSmbiosEntries(systemSlots);
    if (!num)
    {
        lg2::error("get pcie total slot failed");
        return;
    }

    // In case the new size is smaller than old, trim the vector
    if (*num < pcies.size())
    {
        pcies.resize(*num);
    }

    for (unsigned int index = 0; index < *num; index++)
    {
        std::string path = getObjectPath(smbiosInventoryPath, motherboardPath,
                                         pcieSuffix, index);
        if (index + 1 > pcies.size())
        {
            pcies.emplace_back(std::make_unique<phosphor::smbios::Pcie>(
                *bus, path, index, smbiosDir.dir[smbiosDirIndex].dataStorage,
                motherboardPath));
        }
        else
        {
            pcies[index]->pcieInfoUpdate(
                smbiosDir.dir[smbiosDirIndex].dataStorage, motherboardPath);
        }
    }

#ifdef TPM_DBUS

    num = getTotalSmbiosEntries(tpmDeviceType);
    if (!num)
    {
        lg2::error("get tpm failed");
        return;
    }
    // In case the new size is smaller than old, trim the vector
    if (*num < tpms.size())
    {
        tpms.resize(*num);
    }

    for (unsigned int index = 0; index < *num; index++)
    {
        std::string path = getObjectPath(smbiosInventoryPath, motherboardPath,
                                         tpmSuffix, index);
        if (index + 1 > tpms.size())
        {
            tpms.emplace_back(std::make_unique<phosphor::smbios::Tpm>(
                *bus, path, index, smbiosDir.dir[smbiosDirIndex].dataStorage,
                motherboardPath));
        }
        else
        {
            tpms[index]->tpmInfoUpdate(
                smbiosDir.dir[smbiosDirIndex].dataStorage, motherboardPath);
        }
    }
#endif

#ifdef FIRMWARE_INVENTORY_DBUS

    auto existingVersionPaths = utils::getExistingVersionPaths(*bus);
    num = getTotalSmbiosEntries(firmwareInventoryInformationType);
    if (!num)
    {
        lg2::error("get firmware inventory failed");
        existingVersionPaths.clear();
        return;
    }

    // In case the new size is smaller than old, trim the vector
    if (*num < firmwareCollection.size())
    {
        firmwareCollection.resize(*num);
    }
    for (unsigned int index = 0; index < *num; index++)
    {
        auto path = FirmwareInventory::checkAndCreateFirmwarePath(
            smbiosDir.dir[smbiosDirIndex].dataStorage, index,
            existingVersionPaths);
        if (path.empty())
        {
            continue;
        }
        firmwareCollection.emplace_back(
            std::make_unique<phosphor::smbios::FirmwareInventory>(
                *bus, path, index, smbiosDir.dir[smbiosDirIndex].dataStorage));
    }

#endif

    system.reset();
    system = std::make_unique<System>(bus, smbiosInventoryPath + systemSuffix,
                                      smbiosDir.dir[smbiosDirIndex].dataStorage,
                                      smbiosFilePath);
#endif // PUBLISH_INVENTORY
}

std::optional<size_t> MDRV2::getTotalSmbiosEntries(uint8_t smbiosType)
{
    uint8_t* dataIn = smbiosDir.dir[smbiosDirIndex].dataStorage;
    size_t num = 0;

    if (dataIn == nullptr)
    {
        lg2::error("Failed to get SMBIOS entries for type {TYPE}", "TYPE",
                   smbiosType);
        return std::nullopt;
    }

    const uint8_t* dataEnd = dataIn + smbiosTableStorageSize;

    while (1)
    {
        dataIn = getSMBIOSTypePtr(dataIn, smbiosType, 0, dataEnd);
        if (dataIn == nullptr)
        {
            break;
        }

        // Special handling for PCIe slots
        if (smbiosType == systemSlots)
        {
            if (dataIn + 5 >= dataEnd)
            {
                break;
            }
            if (pcieSmbiosType.find(*(dataIn + 5)) != pcieSmbiosType.end())
            {
                num++;
            }
        }
        else
        {
            num++;
        }
        if (num >= limitEntryLen)
        {
            break;
        }
        dataIn = smbiosNextPtr(dataIn, dataEnd);
        if (dataIn == nullptr)
        {
            break;
        }
    }
    return num;
}

bool MDRV2::checkSMBIOSVersion(uint8_t* dataIn)
{
    const std::string anchorString21 = "_SM_";
    const std::string anchorString30 = "_SM3_";
    std::string buffer(reinterpret_cast<const char*>(dataIn),
                       smbiosTableStorageSize);

    auto it = std::search(std::begin(buffer), std::end(buffer),
                          std::begin(anchorString21), std::end(anchorString21));
    bool smbios21Found = it != std::end(buffer);
    if (!smbios21Found)
    {
        lg2::info("SMBIOS 2.1 Anchor String not found. Looking for SMBIOS 3.0");
        it = std::search(std::begin(buffer), std::end(buffer),
                         std::begin(anchorString30), std::end(anchorString30));
        if (it == std::end(buffer))
        {
            lg2::error("SMBIOS 2.1 and 3.0 Anchor Strings not found");
            return false;
        }
    }

    auto pos = std::distance(std::begin(buffer), it);
    size_t length = smbiosTableStorageSize - pos;
    uint8_t foundMajorVersion;
    uint8_t foundMinorVersion;

    if (smbios21Found)
    {
        if (length < sizeof(EntryPointStructure21))
        {
            lg2::error("Invalid entry point structure for SMBIOS 2.1");
            return false;
        }

        auto epStructure =
            reinterpret_cast<const EntryPointStructure21*>(&dataIn[pos]);
        foundMajorVersion = epStructure->smbiosVersion.majorVersion;
        foundMinorVersion = epStructure->smbiosVersion.minorVersion;
    }
    else
    {
        if (length < sizeof(EntryPointStructure30))
        {
            lg2::error("Invalid entry point structure for SMBIOS 3.0");
            return false;
        }

        auto epStructure =
            reinterpret_cast<const EntryPointStructure30*>(&dataIn[pos]);
        foundMajorVersion = epStructure->smbiosVersion.majorVersion;
        foundMinorVersion = epStructure->smbiosVersion.minorVersion;
    }
    lg2::info("SMBIOS VERSION - {MAJOR}.{MINOR}", "MAJOR", foundMajorVersion,
              "MINOR", foundMinorVersion);

    auto itr = std::find_if(
        std::begin(supportedSMBIOSVersions), std::end(supportedSMBIOSVersions),
        [&](SMBIOSVersion versionItr) {
            return versionItr.majorVersion == foundMajorVersion &&
                   versionItr.minorVersion == foundMinorVersion;
        });
    if (itr == std::end(supportedSMBIOSVersions))
    {
        lg2::error("Unsupported SMBIOS table version");
        return false;
    }
    return true;
}

bool MDRV2::agentSynchronizeData()
{
    struct MDRSMBIOSHeader mdr2SMBIOS;
    std::fill_n(smbiosDir.dir[smbiosDirIndex].dataStorage,
                smbiosTableStorageSize, 0);
    bool status = readDataFromFlash(&mdr2SMBIOS,
                                    smbiosDir.dir[smbiosDirIndex].dataStorage);
    if (!status)
    {
        lg2::error("agent data sync failed - read data from flash failed");
        return false;
    }

    if (!checkSMBIOSVersion(smbiosDir.dir[smbiosDirIndex].dataStorage))
    {
        lg2::error("Unsupported SMBIOS table version");
        return false;
    }

    // Reject a declared data extent larger than the backing storage.
    if (mdr2SMBIOS.dataSize > smbiosTableStorageSize)
    {
        lg2::error("SMBIOS data size exceeds storage capacity");
        return false;
    }

    if (0 == static_cast<uint8_t>(sdbusplus::server::xyz::openbmc_project::
                                      smbios::MDRV2::directoryEntries()))
    {
        directoryEntries(smbiosDir.dirEntries);
    }

    systemInfoUpdate();
    smbiosDir.dir[smbiosDirIndex].common.dataVersion = mdr2SMBIOS.dirVer;
    smbiosDir.dir[smbiosDirIndex].common.timestamp = mdr2SMBIOS.timestamp;
    smbiosDir.dir[smbiosDirIndex].common.size = mdr2SMBIOS.dataSize;
    smbiosDir.dir[smbiosDirIndex].stage = MDR2SMBIOSStatusEnum::mdr2Loaded;
    smbiosDir.dir[smbiosDirIndex].lock = MDR2DirLockEnum::mdr2DirUnlock;

    return true;
}

std::vector<uint32_t> MDRV2::synchronizeDirectoryCommonData(uint8_t idIndex,
                                                            uint32_t size)
{
    std::chrono::microseconds usec(
        defaultTimeout); // default lock time out is 2s
    std::vector<uint32_t> result;
    smbiosDir.dir[idIndex].common.size = size;
    result.push_back(smbiosDir.dir[idIndex].common.dataSetSize);
    result.push_back(smbiosDir.dir[idIndex].common.dataVersion);
    result.push_back(smbiosDir.dir[idIndex].common.timestamp);

    timer.expires_after(usec);
    timer.async_wait([this](boost::system::error_code ec) {
        if (ec)
        {
            lg2::error("Timer Error!");
            return;
        }
        agentSynchronizeData();
    });
    return result;
}

std::vector<boost::container::flat_map<std::string, RecordVariant>>
    MDRV2::getRecordType(size_t type)
{
    std::vector<boost::container::flat_map<std::string, RecordVariant>> ret;
    if (type == memoryDeviceType)
    {
        uint8_t* dataIn = smbiosDir.dir[smbiosDirIndex].dataStorage;

        if (dataIn == nullptr)
        {
            throw std::runtime_error("Data not populated");
        }

        // Real end of the fixed backing buffer; bounds the reads below.
        const uint8_t* dataEnd = dataIn + smbiosTableStorageSize;

        do
        {
            dataIn = getSMBIOSTypePtr(dataIn, memoryDeviceType,
                                      sizeof(MemoryInfo), dataEnd);
            if (dataIn == nullptr)
            {
                break;
            }
            boost::container::flat_map<std::string, RecordVariant>& record =
                ret.emplace_back();

            auto memoryInfo = reinterpret_cast<MemoryInfo*>(dataIn);

            record["Type"] = memoryInfo->type;
            record["Length"] = memoryInfo->length;
            record["Handle"] = uint16_t(memoryInfo->handle);
            record["Physical Memory Array Handle"] =
                uint16_t(memoryInfo->phyArrayHandle);
            record["Memory Error Information Handle"] =
                uint16_t(memoryInfo->errInfoHandle);
            record["Total Width"] = uint16_t(memoryInfo->totalWidth);
            record["Data Width"] = uint16_t(memoryInfo->dataWidth);
            record["Size"] = uint16_t(memoryInfo->size);
            record["Form Factor"] = memoryInfo->formFactor;
            record["Device Set"] = memoryInfo->deviceSet;
            record["Device Locator"] = positionToString(
                memoryInfo->deviceLocator, memoryInfo->length, dataIn, dataEnd);
            record["Bank Locator"] = positionToString(
                memoryInfo->bankLocator, memoryInfo->length, dataIn, dataEnd);
            record["Memory Type"] = memoryInfo->memoryType;
            record["Type Detail"] = uint16_t(memoryInfo->typeDetail);
            record["Speed"] = uint16_t(memoryInfo->speed);
            record["Manufacturer"] = positionToString(
                memoryInfo->manufacturer, memoryInfo->length, dataIn, dataEnd);
            record["Serial Number"] = positionToString(
                memoryInfo->serialNum, memoryInfo->length, dataIn, dataEnd);
            record["Asset Tag"] = positionToString(
                memoryInfo->assetTag, memoryInfo->length, dataIn, dataEnd);
            record["Part Number"] = positionToString(
                memoryInfo->partNum, memoryInfo->length, dataIn, dataEnd);
            record["Attributes"] = uint32_t(memoryInfo->attributes);
            record["Extended Size"] = uint32_t(memoryInfo->extendedSize);
            record["Configured Memory Speed"] =
                uint32_t(memoryInfo->confClockSpeed);
            record["Minimum voltage"] = uint16_t(memoryInfo->minimumVoltage);
            record["Maximum voltage"] = uint16_t(memoryInfo->maximumVoltage);
            record["Configured voltage"] =
                uint16_t(memoryInfo->configuredVoltage);
            record["Memory Technology"] = memoryInfo->memoryTechnology;
            record["Memory Operating Mode Capability"] =
                uint16_t(memoryInfo->memoryOperatingModeCap);
            record["Firmware Version"] = memoryInfo->firwareVersion;
            record["Module Manufacturer ID"] =
                uint16_t(memoryInfo->modelManufId);
            record["Module Product ID"] = uint16_t(memoryInfo->modelProdId);
            record["Memory Subsystem Controller Manufacturer ID"] =
                uint16_t(memoryInfo->memSubConManufId);
            record["Memory Subsystem Controller Product Id"] =
                uint16_t(memoryInfo->memSubConProdId);
            record["Non-volatile Size"] = uint64_t(memoryInfo->nvSize);
            record["Volatile Size"] = uint64_t(memoryInfo->volatileSize);
            record["Cache Size"] = uint64_t(memoryInfo->cacheSize);
            record["Logical Size"] = uint64_t(memoryInfo->logicalSize);
        } while ((dataIn = smbiosNextPtr(dataIn, dataEnd)) != nullptr);

        return ret;
    }

    throw std::invalid_argument("Invalid record type");
    return ret;
}

} // namespace smbios
} // namespace phosphor
