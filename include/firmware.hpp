#pragma once
#include "smbios_mdrv2.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

namespace phosphor
{

namespace smbios
{

using associationIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using assetIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::Asset>;
using itemIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::Item>;
#ifdef EXPOSE_FW_INVENTORY
using softwareversionIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::software::Version>;
class Firmware : associationIntf, assetIntf, itemIntf, softwareversionIntf
#else
class Firmware : associationIntf, assetIntf, itemIntf
#endif
{
  public:
    Firmware() = delete;
    ~Firmware() = default;
    Firmware(const Firmware&) = delete;
    Firmware& operator=(const Firmware&) = delete;
    Firmware(Firmware&&) = default;
    Firmware& operator=(Firmware&&) = default;

    Firmware(std::shared_ptr<sdbusplus::asio::connection> bus, const std::string& objPath, int index,
             uint8_t* smbiosTableStorage) :
        associationIntf(*bus, objPath.c_str()),
        assetIntf(*bus, objPath.c_str()), itemIntf(*bus, objPath.c_str()),
#ifdef EXPOSE_FW_INVENTORY
        softwareversionIntf(*bus, objPath.c_str()), path(objPath),
#endif
        storage(smbiosTableStorage), index(index)
    {
        firmwareInfoUpdate();
    }

    static std::tuple<std::string, std::string>
        getFirmwareName(uint8_t* dataIn, int targetIndex = 0);

    void firmwareInfoUpdate(void);

  private:
    /** @brief Path of the group instance */
    std::string path;

    uint8_t* storage;

    int index;

    struct FirmwareInfo
    {
        uint8_t type;
        uint8_t length;
        uint16_t handle;
        uint8_t componentName;
        uint8_t Version;
        uint8_t VersionFormat;
        uint8_t Id;
        uint8_t IdFormat;
        uint8_t releaseDate;
        uint8_t manufacturer;
        uint8_t lowestSupportedVersion;
        uint64_t imageSize;
        uint16_t characteristics;
        uint8_t state;
        uint8_t numOfAssociatedComponents;
        uint16_t associatedComponentHandles[1];
    } __attribute__((packed));

    void firmwareComponentName(const uint8_t positionNum,
                               const uint8_t structLen, uint8_t* dataIn);
#ifdef EXPOSE_FW_INVENTORY
    void firmwareVersion(const uint8_t positionNum, const uint8_t structLen,
                         uint8_t* dataIn);
    void firmwareId(const uint8_t positionNum, const uint8_t structLen,
                    uint8_t* dataIn);
#endif
    void firmwareReleaseDate(const uint8_t positionNum, const uint8_t structLen,
                             uint8_t* dataIn);
    void firmwareManufacturer(const uint8_t positionNum,
                              const uint8_t structLen, uint8_t* dataIn);
};

} // namespace smbios

} // namespace phosphor
