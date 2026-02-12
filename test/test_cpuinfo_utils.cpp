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
#include "cpuinfo_utils.hpp"

#include <sdbusplus/exception.hpp>

#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

namespace cpu_info
{
extern void updatePowerState(const std::string& newState);
extern void updateBiosDone(bool newState);
extern void updateOsState(const std::string& newState);
} // namespace cpu_info

namespace cpu_info
{

TEST(CpuinfoUtilsBit, ZeroReturnsOne)
{
    EXPECT_EQ(bit(0), 1u);
}

TEST(CpuinfoUtilsBit, IndexNReturnsTwoToN)
{
    EXPECT_EQ(bit(1), 2u);
    EXPECT_EQ(bit(2), 4u);
    EXPECT_EQ(bit(7), 128u);
    EXPECT_EQ(bit(63), (1ull << 63));
}

TEST(CpuinfoUtilsMask, SingleBitExtraction)
{
    EXPECT_EQ(mask(10u, 1, 1), 1u);
    EXPECT_EQ(mask(10u, 2, 2), 0u);
    EXPECT_EQ(mask(10u, 0, 0), 0u);
    EXPECT_EQ(mask(10u, 3, 3), 1u);
}

TEST(CpuinfoUtilsMask, MultiBitField)
{
    EXPECT_EQ(mask(218u, 1, 4), 13u);
    EXPECT_EQ(mask(0xFFu, 0, 7), 255u);
}

TEST(CpuinfoUtilsMask, WithDestType)
{
    uint32_t val = 0x1234;
    EXPECT_EQ(mask<uint8_t>(val, 0, 7), 0x34u);
    EXPECT_EQ(mask<uint16_t>(val, 8, 23), 0x12u);
}

TEST(CpuinfoUtilsMask, SameLoAndHiBit)
{
    EXPECT_EQ(mask(0xFu, 2, 2), 1u);
    EXPECT_EQ(mask(0xFu, 0, 0), 1u);
    EXPECT_EQ(mask(0xFu, 3, 3), 1u);
}

TEST(CpuinfoUtilsMask, DifferentSourceTypes)
{
    uint16_t u16 = 0xABCD;
    EXPECT_EQ(mask(u16, 4, 11), 0xBCu);
    int32_t i32 = -1;
    EXPECT_EQ(mask(i32, 0, 7), 0xFFu);
}

TEST(CpuinfoUtilsMask, ZeroData)
{
    EXPECT_EQ(mask(0u, 0, 31), 0u);
    EXPECT_EQ(mask(0u, 5, 10), 0u);
}

TEST(CpuinfoUtilsHostState, EnumValues)
{
    EXPECT_EQ(static_cast<int>(HostState::off), 0);
    EXPECT_EQ(static_cast<int>(HostState::postInProgress), 1);
    EXPECT_EQ(static_cast<int>(HostState::postComplete), 2);
}

TEST(CpuinfoUtilsCpp, AddHostStateCallbackRegisters)
{
    static int callCount = 0;
    callCount = 0;
    addHostStateCallback([&](HostState, HostState) { ++callCount; });
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, HostStateInitialValue)
{
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, DbusGetIOContextAndConnection)
{
    boost::asio::io_context& ioc = dbus::getIOContext();
    std::shared_ptr<sdbusplus::asio::connection> conn = dbus::getConnection();
    EXPECT_NE(conn.get(), nullptr);
    EXPECT_EQ(&conn->get_io_context(), &ioc);
}

TEST(CpuinfoUtilsCpp, MultipleCallbacksCanBeRegistered)
{
    static int first = 0, second = 0;
    first = 0;
    second = 0;
    addHostStateCallback([&](HostState, HostState) { ++first; });
    addHostStateCallback([&](HostState, HostState) { ++second; });
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, HostStateSetupIdempotent)
{
    auto conn = dbus::getConnection();
    ASSERT_NE(conn.get(), nullptr);
    hostStateSetup(conn);
    hostStateSetup(conn);
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, DbusGettersReturnSameInstance)
{
    boost::asio::io_context& a = dbus::getIOContext();
    boost::asio::io_context& b = dbus::getIOContext();
    EXPECT_EQ(&a, &b);
    std::shared_ptr<sdbusplus::asio::connection> c1 = dbus::getConnection();
    std::shared_ptr<sdbusplus::asio::connection> c2 = dbus::getConnection();
    EXPECT_EQ(c1.get(), c2.get());
}

TEST(CpuinfoUtilsCpp, UpdatePowerStateOffKeepsHostOff)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    EXPECT_EQ(hostState, HostState::off);
}

TEST(CpuinfoUtilsCpp, UpdatePowerStateOnWithoutBiosGivesPostInProgress)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updateBiosDone(false);
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    EXPECT_EQ(hostState, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, UpdateBiosDoneTrueWithPowerOnGivesPostComplete)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState(
        "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Inactive");
    EXPECT_EQ(hostState, HostState::postInProgress);
    updateBiosDone(true);
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateShortString)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("Standby");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateFullPath)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState(
        "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.BootComplete");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateInvalidEnumFallsBackToInactive)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("NoSuchStatus");
    EXPECT_EQ(hostState, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, StateChangeInvokesCallbacks)
{
    static HostState oldSeen = HostState::off, newSeen = HostState::off;
    oldSeen = HostState::off;
    newSeen = HostState::off;
    addHostStateCallback([&](HostState oldS, HostState newS) {
        oldSeen = oldS;
        newSeen = newS;
    });
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    EXPECT_EQ(oldSeen, HostState::off);
    EXPECT_EQ(newSeen, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, PowerOffResetsBiosDoneAndOsState)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(true);
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    EXPECT_EQ(hostState, HostState::postInProgress);
}

TEST(CpuinfoUtilsCpp, NoCallbackWhenStateUnchanged)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    static int calls = 0;
    calls = 0;
    addHostStateCallback([&](HostState, HostState) { ++calls; });
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    EXPECT_EQ(hostState, HostState::off);
    EXPECT_EQ(calls, 0);
}

TEST(CpuinfoUtilsCpp, PostCompleteViaOsStateOnly)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState(
        "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.BootComplete");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, PostCompleteViaOsStateStandby)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(true);
    updateOsState("Standby");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, PostCompleteViaOsStateDiagBoot)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("DiagBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, HostStateReadAfterUpdates)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    EXPECT_TRUE(hostState == HostState::off ||
                hostState == HostState::postInProgress ||
                hostState == HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateInvalidEnum)
{
    EXPECT_NO_THROW(updateOsState("InvalidStatusThatDoesNotMatchAnyEnum"));
}

TEST(CpuinfoUtilsCpp, UpdatePowerStateInvalidStringThrows)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    EXPECT_EQ(hostState, HostState::off);
    EXPECT_THROW(updatePowerState("NotAValidHostState"),
                 sdbusplus::exception::InvalidEnumString);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateCbootAndPxeBoot)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("CBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("PXEBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
}

TEST(CpuinfoUtilsCpp, UpdateOsStateRomBootAndCdromBoot)
{
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("ROMBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Running");
    updateBiosDone(false);
    updateOsState("CDROMBoot");
    EXPECT_EQ(hostState, HostState::postComplete);
}

} // namespace cpu_info

#ifdef PHOSPHOR_SMBIOS_MDR_UNIT_TEST
class CpuinfoUtilsTestEnv : public ::testing::Environment
{
  public:
    void TearDown() override
    {
        cpu_info::dbus::resetConnectionForTest();
    }
};

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new CpuinfoUtilsTestEnv);
    int ret = RUN_ALL_TESTS();
    /* Destroy connection before static destructors so Valgrind sees no
     * use-after-free */
    cpu_info::dbus::resetConnectionForTest();
    return ret;
}
#endif
