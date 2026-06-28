#pragma once

#include "handler.hpp"

#include <ipmid/api.h>
#include <systemd/sd-bus.h>

#include <sdbusplus/exception.hpp>

#include <cerrno>
#include <stdexcept>

#include <gtest/gtest.h>

namespace blobs
{
namespace test
{

enum class IpmiBusBehavior
{
    returnBus,
    throwSdbus,
    throwStandard,
};

inline sd_bus* ipmiBus = nullptr;
inline IpmiBusBehavior ipmiBusBehavior = IpmiBusBehavior::returnBus;

inline void setIpmiBus(sd_bus* bus)
{
    ipmiBus = bus;
}

inline void setIpmiBusBehavior(IpmiBusBehavior behavior)
{
    ipmiBusBehavior = behavior;
}

} // namespace test
} // namespace blobs

sd_bus* ipmid_get_sd_bus_connection()
{
    if (blobs::test::ipmiBusBehavior ==
        blobs::test::IpmiBusBehavior::throwSdbus)
    {
        throw sdbusplus::exception::SdBusError(EIO, "forced IPMI bus error");
    }
    if (blobs::test::ipmiBusBehavior ==
        blobs::test::IpmiBusBehavior::throwStandard)
    {
        throw std::runtime_error("forced IPMI bus error");
    }
    return blobs::test::ipmiBus;
}

namespace blobs
{

class SmbiosBlobHandlerTest : public ::testing::Test
{
  protected:
    SmbiosBlobHandlerTest() = default;

    SmbiosBlobHandler handler;

    const uint16_t session = 0;
    const std::string expectedBlobId = "/smbios";
    const std::vector<std::string> expectedBlobIdList = {"/smbios"};
    const uint32_t handlerMaxBufferSize = 64 * 1024;
};
} // namespace blobs
