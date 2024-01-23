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
    dataIn =
        getSMBIOSTypeIndexPtr(dataIn, firmwareInventoryInformationType, index);
    if (dataIn == nullptr)
    {
        return;
    }

    auto firmwareInfo = reinterpret_cast<struct FirmwareInfo*>(dataIn);

    firmwareComponentName(firmwareInfo->componentName, firmwareInfo->length,
                          dataIn);
    firmwareVersion(firmwareInfo->Version, firmwareInfo->length, dataIn);
    firmwareId(firmwareInfo->Id, firmwareInfo->length, dataIn);
    firmwareReleaseDate(firmwareInfo->releaseDate, firmwareInfo->length,
                        dataIn);
    firmwareManufacturer(firmwareInfo->manufacturer, firmwareInfo->length,
                         dataIn);

    present(true);
    purpose(softwareversionIntf::VersionPurpose::Other);
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