#pragma once
#include "smbios_mdrv2.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor
{

namespace smbios
{

class Baseboard
{
  public:
    enum class BoardType : uint8_t
    {
        Reserved,
        Unknown,
        Other,
        ServerBlade,
        ConnectivitySwitch,
        SystemManagementModule,
        ProcessorModule,
        IoModule,
        MemoryModule,
        DaughterBoard,
        Motherboard,
        ProcesssorMemoryModule,
        ProcessorIoModule,
        InterconnectBoard,
    };

    struct BaseboardInfo
    {
        uint8_t type;
        uint8_t length;
        uint16_t handle;
        uint8_t manufacturer;
        uint8_t product;
        uint8_t version;
        uint8_t serialNumber;
        uint8_t assetTag;
        uint8_t featureFlags;
        uint8_t locationInChassis;
        uint16_t chassisHandle;
        uint8_t boardType;
        uint8_t numOfContainedObject;
        uint16_t containedObjectHandles[1];
    } __attribute__((packed));

    Baseboard() = delete;
    ~Baseboard() = default;
    Baseboard(const Baseboard&) = delete;
    Baseboard& operator=(const Baseboard&) = delete;
    Baseboard(Baseboard&&) = default;
    Baseboard& operator=(Baseboard&&) = default;

    Baseboard(int index, uint8_t* smbiosTableStorage) :
        storage(smbiosTableStorage), index(index)
    {
        // Default name
        name = "Board_" + std::to_string(index);

        uint8_t* dataIn = storage;
        dataIn = getSMBIOSTypeIndexPtr(dataIn, baseboardType, index);
        if (dataIn == nullptr)
        {
            lg2::error("Failed to find baseboard info. index={INDEX}", "INDEX",
                       index);
            return;
        }

        raw = reinterpret_cast<struct BaseboardInfo*>(dataIn);
        for (int i = 0; i < raw->numOfContainedObject; i++)
        {
            auto objectHanle = raw->containedObjectHandles[i];
            uint8_t* objectData = smbiosHandlePtr(storage, objectHanle);
            if (objectData == nullptr)
            {
                lg2::error("Failed to find handle. Handle={HANDLE}", "HANDLE",
                           objectHanle);
                continue;
            }
            containedObjects.push_back(
                {objectHanle,
                 reinterpret_cast<struct StructureHeader*>(objectData)});
        }
    }

    /** @brief find the index number of the same structure type inside this
     *  board.
     *  @param handle the handle of searched structure.
     *  @return a pair of bool and int. bool is true if the handle is found,
     *  false if the handle is not found, the int is the index number of the
     *  same structure type inside this board.
     */
    std::pair<bool, int> findIndexOfType(const uint16_t& handle)
    {
        std::map<int, int> typeCount;
        for (const auto& [objHandle, objHeader] : containedObjects)
        {
            if (objHeader == nullptr)
            {
                continue;
            }
            if (handle == objHandle)
            {
                return {true, typeCount[objHeader->type]};
            }
            typeCount[objHeader->type]++;
        }
        return {false, 0};
    }

    void setName(const std::string& newName)
    {
        name = newName;
    }

    std::string getName()
    {
        return name;
    }

    BoardType getType()
    {
        if (raw == nullptr)
        {
            return BoardType::Reserved;
        }
        return BoardType(raw->boardType);
    }

  private:
    uint8_t* storage;

    int index;

    std::string name;

    struct BaseboardInfo* raw;

    std::vector<std::pair<uint16_t, struct StructureHeader*>> containedObjects;
};

} // namespace smbios

} // namespace phosphor
