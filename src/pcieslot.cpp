#include "pcieslot.hpp"

#include <cstdint>
#include <map>

namespace phosphor
{
namespace smbios
{

void Pcie::pcieInfoUpdate(uint8_t* smbiosTableStorage,
                          const std::string& motherboard)
{
    storage = smbiosTableStorage;
    motherboardPath = motherboard;

    uint8_t* dataIn = getSMBIOSTypePtr(storage, systemSlots);

    if (dataIn == nullptr)
    {
        return;
    }

    /* offset 5 points to the slot type */
    for (uint8_t index = 0;
         index < pcieNum ||
         pcieSmbiosType.find(*(dataIn + 5)) == pcieSmbiosType.end();)
    {
        dataIn = smbiosNextPtr(dataIn);
        if (dataIn == nullptr)
        {
            return;
        }
        dataIn = getSMBIOSTypePtr(dataIn, systemSlots);
        if (dataIn == nullptr)
        {
            return;
        }
        if (pcieSmbiosType.find(*(dataIn + 5)) != pcieSmbiosType.end())
        {
            index++;
        }
    }

    auto pcieInfo = reinterpret_cast<struct SystemSlotInfo*>(dataIn);

    uint8_t slotHight = slotHeightNotApplicable;
    // check if the data inside the valid length, some data might not exist in older SMBIOS version
    auto isValid = [pcieInfo](void* varAddr, size_t length = 1){
        char* base = reinterpret_cast<char*>(pcieInfo);
        char* data = reinterpret_cast<char*>(varAddr);
        size_t offset = data - base;
        if ((data >= base) &&
            (offset + length - 1) < pcieInfo->length)
        {
            return true;
        }
        return false;
    };
    if (isValid(&pcieInfo->peerGorupingCount))
    {
        auto pcieInfoAfterPeerGroups = reinterpret_cast<struct SystemSlotInfoAfterPeerGroups*>(&(pcieInfo->peerGroups[pcieInfo->peerGorupingCount]));
        if (isValid(&pcieInfoAfterPeerGroups->slotHeight))
        {
            slotHight = pcieInfoAfterPeerGroups->slotHeight;
        }
    }

    pcieGeneration(pcieInfo->slotType);
    pcieType(pcieInfo->slotType, pcieInfo->slotLength, slotHight);
    pcieLaneSize(pcieInfo->slotDataBusWidth);
    pcieIsHotPluggable(pcieInfo->characteristics2);
    pcieLocation(pcieInfo->slotDesignation, pcieInfo->length, dataIn);

    /* Pcie slot is embedded on the board. Always be true */
    Item::present(true);

    if (!motherboardPath.empty())
    {
        std::vector<std::tuple<std::string, std::string, std::string>> assocs;
        assocs.emplace_back("chassis", "pcie_slots", motherboardPath);
        association::associations(assocs);
    }
}

void Pcie::pcieGeneration(const uint8_t type)
{
    std::map<uint8_t, PCIeGeneration>::const_iterator it =
        pcieGenerationTable.find(type);
    if (it == pcieGenerationTable.end())
    {
        PCIeSlot::generation(PCIeGeneration::Unknown);
    }
    else
    {
        PCIeSlot::generation(it->second);
    }
}

void Pcie::pcieType(const uint8_t type, const uint8_t length, const uint8_t height)
{
    PCIeType dbusPcieType = PCIeType::Unknown;
    std::map<uint8_t, PCIeType>::const_iterator it = pcieTypeTable.find(type);
    if (it != pcieTypeTable.end() && it->second != PCIeType::Unknown)
    {
        dbusPcieType = it->second;
    }
    else if (height == slotHeightLowProfile)
    {
        dbusPcieType = PCIeType::LowProfile;
    }
    else if (length == slotLengthLong)
    {
        dbusPcieType = PCIeType::FullLength;
    }
    else if (length == slotLengthShort)
    {
        dbusPcieType = PCIeType::HalfLength;
    }

    PCIeSlot::slotType(dbusPcieType);
}

void Pcie::pcieLaneSize(const uint8_t width)
{
    std::map<uint8_t, size_t>::const_iterator it = pcieLanesTable.find(width);
    if (it == pcieLanesTable.end())
    {
        PCIeSlot::lanes(0);
    }
    else
    {
        PCIeSlot::lanes(it->second);
    }
}

void Pcie::pcieIsHotPluggable(const uint8_t characteristics)
{
    /*  Bit 1 of slot characteristics 2 indicates if slot supports hot-plug
     *  devices
     */
    PCIeSlot::hotPluggable(characteristics & 0x2);
}

void Pcie::pcieLocation(const uint8_t slotDesignation, const uint8_t structLen,
                        uint8_t* dataIn)
{
    location::locationCode(
        positionToString(slotDesignation, structLen, dataIn));
}

} // namespace smbios
} // namespace phosphor
