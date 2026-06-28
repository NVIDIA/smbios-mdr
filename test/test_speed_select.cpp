#ifdef PECI_ENABLED

#include "cpuinfo_utils.hpp"
#include "speed_select.hpp"

#include <peci.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/message/types.hpp>

#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../src/speed_select.cpp"
#include "../src/sst_mailbox.cpp"

#include <gtest/gtest.h>

namespace cpu_info
{
void updatePowerState(const std::string& newState);
}

namespace
{

using cpu_info::HostState;
using cpu_info::sst::PECIError;
using cpu_info::sst::SSTInterface;
using cpu_info::sst::SSTMailbox;
using cpu_info::sst::TurboEntry;
using cpu_info::sst::WakePolicy;

// Keep the protocol model values explicit so these tests are platform-neutral.
constexpr CPUModel model50650 = static_cast<CPUModel>(0x00050650);
constexpr CPUModel model606A0 = static_cast<CPUModel>(0x000606A0);
constexpr CPUModel model606C0 = static_cast<CPUModel>(0x000606C0);
constexpr CPUModel model806F0 = static_cast<CPUModel>(0x000806F0);
constexpr CPUModel modelC06F0 = static_cast<CPUModel>(0x000C06F0);
constexpr CPUModel modelA06D0 = static_cast<CPUModel>(0x000A06D0);
constexpr CPUModel modelA06E0 = static_cast<CPUModel>(0x000A06E0);
constexpr CPUModel modelA06F0 = static_cast<CPUModel>(0x000A06F0);
constexpr int model606A0MailboxBus = 14;

struct IoResult
{
    EPECIStatus status{PECI_CC_SUCCESS};
    uint8_t completionCode{PECI_DEV_CC_SUCCESS};
};

struct MailboxResponse
{
    uint8_t status{0};
    uint32_t data{0};
};

struct CpuResponse
{
    EPECIStatus status{PECI_CC_CPU_NOT_PRESENT};
    CPUModel model{modelA06D0};
    uint8_t stepping{0};
    uint8_t completionCode{PECI_DEV_CC_SUCCESS};
};

struct PeciFake
{
    std::deque<IoResult> reads;
    std::deque<IoResult> writes;
    std::deque<IoResult> wakeWrites;
    std::deque<IoResult> msrReads;
    std::deque<uint32_t> interfaceReads;
    std::deque<uint32_t> dataReads;
    std::map<std::pair<uint8_t, uint32_t>, MailboxResponse> exactResponses;
    std::map<uint8_t, MailboxResponse> responses;
    std::map<uint8_t, CpuResponse> cpus;
    std::vector<std::pair<uint8_t, uint32_t>> commands;
    std::vector<bool> wakeValues;
    uint32_t interfaceValue{0};
    uint32_t outputData{0};
    uint32_t inputData{0};
    uint64_t msrValue{0};
    uint8_t lastBus{0};
    unsigned readCalls{0};
    unsigned writeCalls{0};
};

PeciFake peci;

IoResult popResult(std::deque<IoResult>& results)
{
    if (results.empty())
    {
        return {};
    }
    IoResult result = results.front();
    results.pop_front();
    return result;
}

void resetPeci()
{
    peci = {};
}

MailboxResponse responseFor(uint8_t subcommand, uint32_t input)
{
    auto exact = peci.exactResponses.find({subcommand, input});
    if (exact != peci.exactResponses.end())
    {
        return exact->second;
    }
    auto generic = peci.responses.find(subcommand);
    if (generic != peci.responses.end())
    {
        return generic->second;
    }
    return {};
}

template <typename Command>
void expectMailboxCommandStatuses(cpu_info::sst::PECIManager& manager,
                                  uint8_t subcommand)
{
    using Policy = typename Command::ErrorPolicy;
    for (const uint8_t status : {uint8_t{0}, uint8_t{1}})
    {
        peci.responses[subcommand] = {status, 0};
        Command command(manager, Policy::NoThrow);
        EXPECT_EQ(command.success(), status == 0);
    }
}

struct FakeSSTState
{
    bool readyValue{true};
    bool controlValue{true};
    bool ppEnabledValue{true};
    bool bfSupportedValue{true};
    bool tfSupportedValue{true};
    bool bfEnabledValue{false};
    bool tfEnabledValue{false};
    bool throwCurrentLevel{false};
    bool throwCurrentLevelRuntime{false};
    bool throwBfEnabled{false};
    bool throwBfEnabledRuntime{false};
    bool throwSetCurrentLevel{false};
    unsigned int currentLevelValue{0};
    unsigned int maxLevelValue{0};
    unsigned int powerLimitValue{250};
    unsigned int p1Value{2200};
    unsigned int p0Value{3600};
    unsigned int prochotValue{95};
    unsigned int highPriorityFreq{3000};
    unsigned int lowPriorityFreq{1800};
    std::set<unsigned int> supportedLevels{0};
    std::vector<unsigned int> enabledCores{0, 1, 2, 3};
    std::vector<unsigned int> highPriorityCores{1, 3};
    std::vector<TurboEntry> turboProfile{{3600, 1}, {3400, 4}};
    std::vector<bool> setBfValues;
    std::vector<bool> setTfValues;
    std::vector<unsigned int> setLevels;
};

class FakeSST : public SSTMailbox
{
  public:
    explicit FakeSST(std::shared_ptr<FakeSSTState> state_) :
        SSTMailbox(0x30, model806F0, cpu_info::sst::dontWake),
        state(std::move(state_))
    {
        peci.responses[0x03] = {0, state->powerLimitValue};
    }

    bool ready() override
    {
        return state->readyValue;
    }

    bool supportsControl() override
    {
        return state->controlValue;
    }

    bool ppEnabled() override
    {
        return state->ppEnabledValue;
    }

    unsigned int currentLevel() override
    {
        if (state->throwCurrentLevelRuntime)
        {
            throw std::runtime_error("current level runtime");
        }
        if (state->throwCurrentLevel)
        {
            throw PECIError("current level");
        }
        return state->currentLevelValue;
    }

    unsigned int maxLevel() override
    {
        return state->maxLevelValue;
    }

    bool levelSupported(unsigned int level) override
    {
        return state->supportedLevels.contains(level);
    }

    bool bfSupported(unsigned int) override
    {
        return state->bfSupportedValue;
    }

    bool tfSupported(unsigned int) override
    {
        return state->tfSupportedValue;
    }

    bool bfEnabled(unsigned int) override
    {
        if (state->throwBfEnabledRuntime)
        {
            throw std::runtime_error("bf enabled runtime");
        }
        if (state->throwBfEnabled)
        {
            throw PECIError("bf enabled");
        }
        return state->bfEnabledValue;
    }

    bool tfEnabled(unsigned int) override
    {
        return state->tfEnabledValue;
    }

    unsigned int coreCount(unsigned int) override
    {
        return state->enabledCores.size();
    }

    std::vector<unsigned int> enabledCoreList(unsigned int) override
    {
        return state->enabledCores;
    }

    std::vector<TurboEntry> sseTurboProfile(unsigned int) override
    {
        return state->turboProfile;
    }

    unsigned int p1Freq(unsigned int) override
    {
        return state->p1Value;
    }

    unsigned int p0Freq(unsigned int) override
    {
        return state->p0Value;
    }

    unsigned int prochotTemp(unsigned int) override
    {
        return state->prochotValue;
    }

    std::vector<unsigned int> bfHighPriorityCoreList(unsigned int) override
    {
        return state->highPriorityCores;
    }

    unsigned int bfHighPriorityFreq(unsigned int) override
    {
        return state->highPriorityFreq;
    }

    unsigned int bfLowPriorityFreq(unsigned int) override
    {
        return state->lowPriorityFreq;
    }

    void setBfEnabled(bool enable) override
    {
        state->setBfValues.push_back(enable);
    }

    void setTfEnabled(bool enable) override
    {
        state->setTfValues.push_back(enable);
    }

    void setCurrentLevel(unsigned int level) override
    {
        if (state->throwSetCurrentLevel)
        {
            throw PECIError("set current level");
        }
        state->setLevels.push_back(level);
        state->currentLevelValue = level;
    }

  private:
    std::shared_ptr<FakeSSTState> state;
};

struct ProviderControl
{
    bool enabled{false};
    CPUModel model{modelA06D0};
    WakePolicy lastWakePolicy{cpu_info::sst::dontWake};
    uint8_t lastAddress{0};
    unsigned int calls{0};
    std::shared_ptr<FakeSSTState> state{std::make_shared<FakeSSTState>()};
};

ProviderControl provider;
std::once_flag providerRegistration;

void ensureTestProviders()
{
    std::call_once(providerRegistration, []() {
        cpu_info::sst::registerBackend(
            [](uint8_t, CPUModel, WakePolicy) -> std::unique_ptr<SSTInterface> {
                throw std::runtime_error("provider probe failed");
            });
        cpu_info::sst::registerBackend(
            [](uint8_t address, CPUModel model,
               WakePolicy wakePolicy) -> std::unique_ptr<SSTInterface> {
                ++provider.calls;
                provider.lastAddress = address;
                provider.lastWakePolicy = wakePolicy;
                if (!provider.enabled || model != provider.model)
                {
                    return nullptr;
                }
                return std::make_unique<FakeSST>(provider.state);
            });
    });
}

void resetProvider()
{
    ensureTestProviders();
    provider = {};
}

class SpeedSelectTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        resetPeci();
        resetProvider();
        cpu_info::hostState = HostState::off;
    }
};

} // namespace

extern "C"
{
EPECIStatus peci_WrPkgConfig(uint8_t, uint8_t, uint16_t parameter, uint32_t,
                             uint8_t, uint8_t* completionCode)
{
    IoResult result = popResult(peci.wakeWrites);
    *completionCode = result.completionCode;
    peci.wakeValues.push_back(parameter != 0);
    return result.status;
}

EPECIStatus peci_WrEndPointPCIConfigLocal(
    uint8_t, uint8_t, uint8_t bus, uint8_t, uint8_t, uint16_t reg, uint8_t,
    uint32_t value, uint8_t* completionCode)
{
    IoResult result = popResult(peci.writes);
    *completionCode = result.completionCode;
    ++peci.writeCalls;
    peci.lastBus = bus;
    if (result.status != PECI_CC_SUCCESS ||
        result.completionCode != PECI_DEV_CC_SUCCESS)
    {
        return result.status;
    }

    if (reg == 0xA0)
    {
        peci.inputData = value;
    }
    else if (reg == 0xA4)
    {
        uint8_t command = value & 0xFF;
        uint8_t subcommand = (value >> 8) & 0xFF;
        peci.commands.emplace_back(subcommand, peci.inputData);
        EXPECT_EQ(command, 0x7F);
        MailboxResponse response = responseFor(subcommand, peci.inputData);
        peci.interfaceValue = response.status;
        peci.outputData = response.data;
    }
    return result.status;
}

EPECIStatus peci_RdEndPointConfigPciLocal(
    uint8_t, uint8_t, uint8_t bus, uint8_t, uint8_t, uint16_t reg,
    uint8_t readLength, uint8_t* output, uint8_t* completionCode)
{
    IoResult result = popResult(peci.reads);
    *completionCode = result.completionCode;
    ++peci.readCalls;
    peci.lastBus = bus;
    if (result.status != PECI_CC_SUCCESS ||
        result.completionCode != PECI_DEV_CC_SUCCESS)
    {
        return result.status;
    }

    uint32_t value = 0;
    if (reg == 0xA4)
    {
        if (peci.interfaceReads.empty())
        {
            value = peci.interfaceValue;
        }
        else
        {
            value = peci.interfaceReads.front();
            peci.interfaceReads.pop_front();
        }
    }
    else if (reg == 0xA0)
    {
        if (peci.dataReads.empty())
        {
            value = peci.outputData;
        }
        else
        {
            value = peci.dataReads.front();
            peci.dataReads.pop_front();
        }
    }
    std::memcpy(output, &value, std::min<size_t>(readLength, sizeof(value)));
    return result.status;
}

EPECIStatus peci_RdIAMSR(uint8_t, uint8_t, uint16_t, uint64_t* value,
                         uint8_t* completionCode)
{
    IoResult result = popResult(peci.msrReads);
    *completionCode = result.completionCode;
    if (result.status == PECI_CC_SUCCESS &&
        result.completionCode == PECI_DEV_CC_SUCCESS)
    {
        *value = peci.msrValue;
    }
    return result.status;
}

EPECIStatus peci_GetCPUID(const uint8_t address, CPUModel* model,
                          uint8_t* stepping, uint8_t* completionCode)
{
    auto found = peci.cpus.find(address);
    CpuResponse response =
        found == peci.cpus.end() ? CpuResponse{} : found->second;
    *model = response.model;
    *stepping = response.stepping;
    *completionCode = response.completionCode;
    return response.status;
}

} // extern "C"

namespace
{

using namespace cpu_info;
using namespace cpu_info::sst;

using GetPowerControlCommand = OsMailboxCommand<0x01>;
using GetPowerInfoCommand = OsMailboxCommand<0x03>;
using SetPowerControlCommand = OsMailboxCommand<0x02>;

TEST_F(SpeedSelectTest, HelpersAndProviderSelection)
{
    EXPECT_TRUE(checkPECIStatus(PECI_CC_SUCCESS, PECI_DEV_CC_SUCCESS));
    EXPECT_FALSE(checkPECIStatus(PECI_CC_HW_ERR, PECI_DEV_CC_SUCCESS));
    EXPECT_FALSE(checkPECIStatus(PECI_CC_SUCCESS, 0x81));

    std::ostringstream stream;
    stream << static_cast<uint8_t>(65);
    EXPECT_EQ(stream.str(), "65");

    EXPECT_TRUE(convertMaskToList(std::bitset<64>{}).empty());
    EXPECT_EQ(convertMaskToList(std::bitset<64>{0x800000000000000BULL}),
              (std::vector<uint32_t>{0, 1, 3, 63}));
    EXPECT_EQ(extendedModel(model806F0), 8);

    EXPECT_EQ(getInstance(0x30, modelA06D0, dontWake), nullptr);

    provider.enabled = true;
    auto fake = getInstance(0x31, modelA06D0, wakeAllowed);
    ASSERT_NE(fake, nullptr);
    EXPECT_EQ(provider.lastAddress, 0x31);
    EXPECT_EQ(provider.lastWakePolicy, wakeAllowed);

    EXPECT_NE(getInstance(0x30, model606A0, dontWake), nullptr);
    EXPECT_NE(getInstance(0x30, model606C0, dontWake), nullptr);
    EXPECT_NE(getInstance(0x30, model806F0, dontWake), nullptr);
    EXPECT_NE(getInstance(0x30, modelC06F0, dontWake), nullptr);
}

TEST_F(SpeedSelectTest, PeciManagerStatusBusSelectionAndWakeCleanup)
{
    EXPECT_FALSE(PECIManager::isSleeping(PECI_CC_SUCCESS, 0x82));
    EXPECT_FALSE(
        PECIManager::isSleeping(PECI_CC_DRIVER_ERR, PECI_DEV_CC_SUCCESS));
    EXPECT_TRUE(PECIManager::isSleeping(PECI_CC_DRIVER_ERR, 0x82));

    {
        PECIManager mailbox6(0x30, model606A0, wakeAllowed);
        PECIManager other(0x31, model806F0, dontWake);
        EXPECT_EQ(mailbox6.mbBus, model606A0MailboxBus);
        EXPECT_EQ(other.mbBus, PECIManager::mbBusOther);
        mailbox6.setWakeOnPECI(true);
        EXPECT_TRUE(mailbox6.peciWoken);
    }
    ASSERT_EQ(peci.wakeValues.size(), 2u);
    EXPECT_TRUE(peci.wakeValues[0]);
    EXPECT_FALSE(peci.wakeValues[1]);

    resetPeci();
    peci.wakeWrites.push_back({PECI_CC_HW_ERR, 0x81});
    PECIManager failedEnable(0x30, model606A0, wakeAllowed);
    EXPECT_THROW(failedEnable.setWakeOnPECI(true), PECIError);

    resetPeci();
    {
        PECIManager cleanupFailure(0x30, model606A0, wakeAllowed);
        cleanupFailure.peciWoken = true;
        peci.wakeWrites.push_back({PECI_CC_HW_ERR, 0x81});
    }
    EXPECT_EQ(peci.wakeValues, (std::vector<bool>{false}));
}

TEST_F(SpeedSelectTest, PeciManagerReadWriteAndWakeBranches)
{
    {
        PECIManager manager(0x30, model606A0, dontWake);
        manager.wrMailboxReg(PECIManager::mbDataReg, 0x12345678);
        peci.outputData = 0xAABBCCDD;
        EXPECT_EQ(manager.rdMailboxReg(PECIManager::mbDataReg), 0xAABBCCDD);
        EXPECT_EQ(peci.lastBus, model606A0MailboxBus);
    }

    resetPeci();
    {
        PECIManager manager(0x30, model806F0, wakeAllowed);
        peci.writes.push_back({PECI_CC_DRIVER_ERR, 0x82});
        peci.writes.push_back({});
        manager.wrMailboxReg(PECIManager::mbDataReg, 1);
        EXPECT_TRUE(manager.peciWoken);
    }
    EXPECT_EQ(peci.wakeValues, (std::vector<bool>{true, false}));

    resetPeci();
    {
        PECIManager manager(0x30, model806F0, wakeAllowed);
        peci.reads.push_back({PECI_CC_DRIVER_ERR, 0x82});
        peci.reads.push_back({});
        peci.outputData = 9;
        EXPECT_EQ(manager.rdMailboxReg(PECIManager::mbDataReg), 9u);
    }
    EXPECT_EQ(peci.wakeValues, (std::vector<bool>{true, false}));

    resetPeci();
    PECIManager noWake(0x30, model806F0, dontWake);
    peci.writes.push_back({PECI_CC_HW_ERR, 0x81});
    EXPECT_THROW(noWake.wrMailboxReg(PECIManager::mbDataReg, 0), PECIError);

    peci.reads.push_back({PECI_CC_HW_ERR, 0x81});
    EXPECT_THROW(noWake.rdMailboxReg(PECIManager::mbDataReg), PECIError);

    resetPeci();
    {
        PECIManager retryFails(0x30, model806F0, wakeAllowed);
        peci.writes.push_back({PECI_CC_DRIVER_ERR, 0x82});
        peci.writes.push_back({PECI_CC_HW_ERR, 0x81});
        EXPECT_THROW(retryFails.wrMailboxReg(PECIManager::mbDataReg, 0),
                     PECIError);
    }
}

TEST_F(SpeedSelectTest, MailboxCommandSuccessStatusAndTimeouts)
{
    PECIManager manager(0x30, model806F0, dontWake);
    peci.responses[0x44] = {0, 0x11223344};
    EXPECT_EQ(manager.sendPECIOSMailboxCmd(0x7F, 0x44, 0x55667788),
              0x11223344u);
    ASSERT_EQ(peci.commands.size(), 1u);
    EXPECT_EQ(peci.commands[0],
              (std::pair<uint8_t, uint32_t>{0x44, 0x55667788}));

    peci.responses[0x45] = {1, 0xA5A5A5A5};
    EXPECT_THROW(manager.sendPECIOSMailboxCmd(0x7F, 0x45), PECIError);

    PECIManager::MailboxStatus status = PECIManager::MailboxStatus::NoError;
    EXPECT_EQ(manager.sendPECIOSMailboxCmd(0x7F, 0x45, 0, &status),
              0xA5A5A5A5u);
    EXPECT_EQ(status, PECIManager::MailboxStatus::InvalidCommand);

    resetPeci();
    peci.interfaceReads.assign(10, bit(31));
    EXPECT_THROW(manager.sendPECIOSMailboxCmd(0x7F, 1), PECIError);

    resetPeci();
    peci.interfaceReads.push_back(0);
    for (int i = 0; i < 10; ++i)
    {
        peci.interfaceReads.push_back(bit(31));
    }
    EXPECT_THROW(manager.sendPECIOSMailboxCmd(0x7F, 1), PECIError);
}

TEST_F(SpeedSelectTest, MailboxInterfaceExposesAllDataAndControlMethods)
{
    constexpr unsigned int level = 2;
    peci.responses[0x00] = {0, bit(31) | (level << 16) | (3u << 8) | 7u};
    peci.exactResponses[{0x01, level}] = {0,
                                          bit(17) | bit(16) | bit(1) | bit(0)};
    peci.exactResponses[{0x01, 9}] = {1, 0};
    peci.exactResponses[{0x03, level}] = {0, (42u << 16) | 275u};
    peci.exactResponses[{0x06, level}] = {0, 0x0000000B};
    peci.exactResponses[{0x06, level | (1u << 8)}] = {0, 0x80000000};
    peci.exactResponses[{0x0C, level}] = {0, (10u << 24) | (20u << 16) |
                                                 (22u << 8) | 36u};
    peci.exactResponses[{0x05, level}] = {0, 97};
    peci.exactResponses[{0x20, level}] = {0, 0x0000000A};
    peci.exactResponses[{0x20, level | (1u << 8)}] = {0, 0x40000000};
    peci.exactResponses[{0x21, level}] = {0, (30u << 8) | 18u};
    peci.exactResponses[{0x07, level}] = {0, 40u | (30u << 8) | (25u << 16)};
    peci.exactResponses[{0x07, level | (1u << 8)}] = {0, 0};
    peci.msrValue = 1u | (2u << 8) | (3ULL << 24);

    SSTMailbox mailbox(0x30, model806F0, dontWake);
    EXPECT_TRUE(mailbox.ready());
    EXPECT_TRUE(mailbox.ppEnabled());
    EXPECT_EQ(mailbox.currentLevel(), level);
    EXPECT_EQ(mailbox.maxLevel(), 3u);
    EXPECT_TRUE(mailbox.levelSupported(level));
    EXPECT_FALSE(mailbox.levelSupported(9));
    EXPECT_TRUE(mailbox.bfSupported(level));
    EXPECT_TRUE(mailbox.tfSupported(level));
    EXPECT_TRUE(mailbox.bfEnabled(level));
    EXPECT_TRUE(mailbox.tfEnabled(level));
    EXPECT_EQ(mailbox.enabledCoreList(level),
              (std::vector<unsigned int>{0, 1, 3, 63}));
    EXPECT_EQ(mailbox.coreCount(level), 4u);
    EXPECT_EQ(mailbox.p1Freq(level), 2200u);
    EXPECT_EQ(mailbox.p0Freq(level), 3600u);
    EXPECT_EQ(mailbox.prochotTemp(level), 97u);
    EXPECT_EQ(mailbox.bfHighPriorityCoreList(level),
              (std::vector<unsigned int>{1, 3, 62}));
    EXPECT_EQ(mailbox.bfHighPriorityFreq(level), 3000u);
    EXPECT_EQ(mailbox.bfLowPriorityFreq(level), 1800u);
    EXPECT_EQ(mailbox.sseTurboProfile(level),
              (std::vector<TurboEntry>{{4000, 1}, {3000, 2}}));

    mailbox.setBfEnabled(true);
    mailbox.setBfEnabled(false);
    mailbox.setTfEnabled(true);
    mailbox.setTfEnabled(false);
    mailbox.setCurrentLevel(3);

    EXPECT_NE(std::find(peci.commands.begin(), peci.commands.end(),
                        std::pair<uint8_t, uint32_t>{0x02, bit(17)}),
              peci.commands.end());
    EXPECT_NE(std::find(peci.commands.begin(), peci.commands.end(),
                        std::pair<uint8_t, uint32_t>{0x02, bit(16)}),
              peci.commands.end());
    EXPECT_NE(std::find(peci.commands.begin(), peci.commands.end(),
                        std::pair<uint8_t, uint32_t>{0x08, 3}),
              peci.commands.end());

    SSTMailbox mailbox6(0x30, model606A0, dontWake);
    SSTMailbox mailbox7(0x30, model606C0, dontWake);

    peci.msrReads.push_back({PECI_CC_HW_ERR, 0x81});
    EXPECT_THROW(mailbox.sseTurboProfile(level), PECIError);
}
TEST_F(SpeedSelectTest, MailboxAlternatePoliciesAndAllModelCases)
{
    PECIManager manager(0x30, model806F0, dontWake);

    expectMailboxCommandStatuses<GetLevelsInfo>(manager, 0x00);
    expectMailboxCommandStatuses<GetPowerControlCommand>(manager, 0x01);
    expectMailboxCommandStatuses<SetPowerControlCommand>(manager, 0x02);
    expectMailboxCommandStatuses<GetPowerInfoCommand>(manager, 0x03);
    expectMailboxCommandStatuses<GetTjmaxInfo>(manager, 0x05);
    expectMailboxCommandStatuses<GetCoreMask>(manager, 0x06);
    expectMailboxCommandStatuses<GetTurboLimitRatios>(manager, 0x07);
    expectMailboxCommandStatuses<SetLevel>(manager, 0x08);
    expectMailboxCommandStatuses<GetRatioInfo>(manager, 0x0C);
    expectMailboxCommandStatuses<PbfGetCoreMaskInfo>(manager, 0x20);
    expectMailboxCommandStatuses<PbfGetP1HiP1LoInfo>(manager, 0x21);

    peci.responses.clear();
    EXPECT_NO_THROW(GetPowerControlCommand(
        manager, GetPowerControlCommand::ErrorPolicy::Throw));

    SSTMailbox mailbox0(0x30, modelC06F0, dontWake);
    SSTMailbox mailbox1(0x30, modelA06D0, dontWake);
    SSTMailbox mailbox2(0x30, modelA06E0, dontWake);
    SSTMailbox mailbox3(0x30, modelA06F0, dontWake);
    SSTMailbox mailbox4(0x30, model50650, dontWake);
    SSTMailbox mailbox5(0x30, model806F0, dontWake);
    SSTMailbox mailbox6(0x30, model606A0, dontWake);
    SSTMailbox mailbox7(0x30, model606C0, dontWake);
    std::vector<std::pair<SSTInterface*, bool>> capabilities = {
        {&mailbox0, true},  {&mailbox1, false}, {&mailbox2, false},
        {&mailbox3, false}, {&mailbox4, false}, {&mailbox5, true},
        {&mailbox6, false}, {&mailbox7, false},
    };
    for (const auto& [mailbox, expectedControl] : capabilities)
    {
        EXPECT_TRUE(mailbox->ready());
        EXPECT_EQ(mailbox->supportsControl(), expectedControl);
    }

    resetPeci();
    PECIManager wakeWriteFailure(0x30, model806F0, wakeAllowed);
    peci.writes.push_back({PECI_CC_HW_ERR, 0x81});
    EXPECT_THROW(wakeWriteFailure.wrMailboxReg(PECIManager::mbDataReg, 0),
                 PECIError);

    resetPeci();
    PECIManager wakeReadFailure(0x30, model806F0, wakeAllowed);
    peci.reads.push_back({PECI_CC_HW_ERR, 0x81});
    EXPECT_THROW(wakeReadFailure.rdMailboxReg(PECIManager::mbDataReg),
                 PECIError);
}

TEST_F(SpeedSelectTest, SingleConfigPublishesBaseAndPriorityProperties)
{
    auto connection = dbus::getConnection();
    FakeSST fake(provider.state);

    OperatingConfig withoutBf(*connection, 0,
                              CPUConfig::generatePath(20) + "/direct0");
    provider.state->bfSupportedValue = false;
    getSingleConfig(fake, 0, withoutBf);
    EXPECT_EQ(withoutBf.powerLimit(), provider.state->powerLimitValue);
    EXPECT_EQ(withoutBf.availableCoreCount(),
              provider.state->enabledCores.size());
    EXPECT_EQ(withoutBf.baseSpeed(), provider.state->p1Value);
    EXPECT_EQ(withoutBf.maxSpeed(), provider.state->p0Value);
    EXPECT_EQ(withoutBf.maxJunctionTemperature(), provider.state->prochotValue);
    EXPECT_TRUE(withoutBf.baseSpeedPrioritySettings().empty());
    EXPECT_EQ(withoutBf.turboProfile(), provider.state->turboProfile);

    OperatingConfig withBf(*connection, 1,
                           CPUConfig::generatePath(20) + "/direct1");
    provider.state->bfSupportedValue = true;
    getSingleConfig(fake, 1, withBf);
    auto settings = withBf.baseSpeedPrioritySettings();
    ASSERT_EQ(settings.size(), 2u);
    EXPECT_EQ(settings[0], (std::tuple<uint32_t, std::vector<uint32_t>>{
                               provider.state->highPriorityFreq, {1, 3}}));
    EXPECT_EQ(settings[1], (std::tuple<uint32_t, std::vector<uint32_t>>{
                               provider.state->lowPriorityFreq, {0, 2}}));
}

TEST_F(SpeedSelectTest, CpuConfigGettersCacheValuesAndHandleProviderErrors)
{
    auto connection = dbus::getConnection();
    CPUConfig config(*connection, 21, modelA06D0, 1, false);

    EXPECT_EQ(config.generateConfigPath(1),
              CPUConfig::generatePath(21) + "/config1");
    EXPECT_EQ(config.appliedConfig().str, config.generateConfigPath(1));
    EXPECT_FALSE(config.baseSpeedPriorityEnabled());

    hostState = HostState::postInProgress;
    EXPECT_EQ(config.appliedConfig().str, config.generateConfigPath(1));
    EXPECT_FALSE(config.baseSpeedPriorityEnabled());

    provider.enabled = true;
    provider.state->readyValue = false;
    EXPECT_EQ(config.appliedConfig().str, config.generateConfigPath(1));
    EXPECT_FALSE(config.baseSpeedPriorityEnabled());

    provider.state->readyValue = true;
    provider.state->currentLevelValue = 2;
    provider.state->bfEnabledValue = true;
    EXPECT_EQ(config.appliedConfig().str, config.generateConfigPath(2));
    EXPECT_TRUE(config.baseSpeedPriorityEnabled());

    provider.state->throwCurrentLevel = true;
    provider.state->throwBfEnabled = true;
    EXPECT_EQ(config.appliedConfig().str, config.generateConfigPath(2));
    EXPECT_TRUE(config.baseSpeedPriorityEnabled());

    provider.state->throwCurrentLevel = false;
    provider.state->throwBfEnabled = false;
    provider.state->throwCurrentLevelRuntime = true;
    EXPECT_THROW(config.appliedConfig(), std::runtime_error);
    provider.state->throwCurrentLevelRuntime = false;
    provider.state->throwBfEnabledRuntime = true;
    EXPECT_THROW(config.baseSpeedPriorityEnabled(), std::runtime_error);
    provider.state->throwBfEnabledRuntime = false;
}

TEST_F(SpeedSelectTest, DiscoveryPropagatesNonPeciBackendError)
{
    hostState = HostState::postInProgress;
    provider.enabled = true;
    peci.cpus[MIN_CLIENT_ADDR] = {PECI_CC_SUCCESS, modelA06D0, 0,
                                  PECI_DEV_CC_SUCCESS};
    provider.state->throwCurrentLevelRuntime = true;

    EXPECT_THROW(discoverOrWait(), std::runtime_error);
}

TEST_F(SpeedSelectTest, CpuConfigSettersEnforceValidationAndUpdateLevel)
{
    auto connection = dbus::getConnection();
    CPUConfig config(*connection, 22, modelA06D0, 0, false);
    OperatingConfig& levelOne = config.newConfig(1);

    EXPECT_THROW(
        config.appliedConfig(sdbusplus::object_path{"/invalid/config"}),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);

    auto unavailableProvider =
        config.appliedConfig(sdbusplus::object_path{levelOne.path});
    EXPECT_TRUE(unavailableProvider.str.empty());

    provider.enabled = true;
    provider.state->controlValue = false;
    EXPECT_THROW(config.appliedConfig(sdbusplus::object_path{levelOne.path}),
                 sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed);

    provider.state->controlValue = true;
    hostState = HostState::postInProgress;
    EXPECT_THROW(config.appliedConfig(sdbusplus::object_path{levelOne.path}),
                 sdbusplus::xyz::openbmc_project::Common::Error::Unavailable);

    hostState = HostState::postComplete;
    provider.state->readyValue = false;
    EXPECT_THROW(config.appliedConfig(sdbusplus::object_path{levelOne.path}),
                 sdbusplus::xyz::openbmc_project::Common::Error::Unavailable);

    provider.state->readyValue = true;
    provider.state->throwSetCurrentLevel = true;
    EXPECT_THROW(
        config.appliedConfig(sdbusplus::object_path{levelOne.path}),
        sdbusplus::xyz::openbmc_project::Common::Device::Error::WriteFailure);

    provider.state->throwSetCurrentLevel = false;
    EXPECT_NO_THROW(
        config.appliedConfig(sdbusplus::object_path{levelOne.path}));
    EXPECT_EQ(provider.state->setLevels, (std::vector<unsigned int>{1}));

    hostState = HostState::off;
    EXPECT_EQ(config.appliedConfig().str, levelOne.path);
    EXPECT_THROW(config.baseSpeedPriorityEnabled(true),
                 sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed);
    EXPECT_NO_THROW(config.finalize());
}

TEST_F(SpeedSelectTest, DiscoveryHandlesPeciAndBackendAvailability)
{
    auto connection = dbus::getConnection();
    auto& io = dbus::getIOContext();

    hostState = HostState::off;
    EXPECT_FALSE(discoverCPUsAndConfigs(io, *connection));

    hostState = HostState::postInProgress;
    peci.cpus[MIN_CLIENT_ADDR] = {PECI_CC_TIMEOUT, modelA06D0, 0,
                                  PECI_DEV_CC_SUCCESS};
    EXPECT_THROW(discoverCPUsAndConfigs(io, *connection), PECIError);

    resetPeci();
    peci.cpus[MIN_CLIENT_ADDR] = {PECI_CC_CPU_NOT_PRESENT, modelA06D0, 0,
                                  PECI_DEV_CC_SUCCESS};
    peci.cpus[MIN_CLIENT_ADDR + 1] = {PECI_CC_HW_ERR, modelA06D0, 0,
                                      PECI_DEV_CC_SUCCESS};
    peci.cpus[MIN_CLIENT_ADDR + 2] = {PECI_CC_SUCCESS, modelA06D0, 0, 0x81};
    peci.cpus[MIN_CLIENT_ADDR + 3] = {PECI_CC_SUCCESS, modelA06D0, 0,
                                      PECI_DEV_CC_SUCCESS};
    EXPECT_TRUE(discoverCPUsAndConfigs(io, *connection));

    provider.enabled = true;
    resetPeci();
    peci.cpus[MIN_CLIENT_ADDR] = {PECI_CC_SUCCESS, modelA06D0, 0,
                                  PECI_DEV_CC_SUCCESS};
    provider.state->readyValue = false;
    EXPECT_FALSE(discoverCPUsAndConfigs(io, *connection));

    provider.state->readyValue = true;
    provider.state->ppEnabledValue = false;
    EXPECT_TRUE(discoverCPUsAndConfigs(io, *connection));
}

TEST_F(SpeedSelectTest, DiscoveryPublishesValidConfigsAndDropsInvalidCurrent)
{
    auto connection = dbus::getConnection();
    auto& io = dbus::getIOContext();
    hostState = HostState::postInProgress;
    provider.enabled = true;
    peci.cpus[MIN_CLIENT_ADDR] = {PECI_CC_SUCCESS, modelA06D0, 0,
                                  PECI_DEV_CC_SUCCESS};

    provider.state->currentLevelValue = 1;
    provider.state->maxLevelValue = 2;
    provider.state->supportedLevels = {0, 1};
    EXPECT_TRUE(discoverCPUsAndConfigs(io, *connection));

    provider.state->currentLevelValue = 2;
    EXPECT_TRUE(discoverCPUsAndConfigs(io, *connection));

    provider.enabled = false;
    resetPeci();
    EXPECT_TRUE(discoverCPUsAndConfigs(io, *connection));
}

TEST_F(SpeedSelectTest, HostStateSetupHandlesMissingInitialServices)
{
    auto connection = dbus::getConnection();
    auto& io = dbus::getIOContext();

    hostStateSetup(connection);
    boost::asio::steady_timer stopTimer(io, std::chrono::milliseconds(100));
    stopTimer.async_wait([&io](const boost::system::error_code&) {
        io.stop();
    });
    io.restart();
    io.run();
}

TEST_F(SpeedSelectTest, RetryAndHostStateHandlersCoverSchedulingPaths)
{
    auto& io = dbus::getIOContext();
    hostState = HostState::off;

    EXPECT_NO_THROW(discoverOrWait());
    EXPECT_NO_THROW(discoverOrWait());
    io.restart();
    io.poll();

    EXPECT_NO_THROW(
        hostStateHandler(HostState::postInProgress, HostState::off));
    EXPECT_NO_THROW(hostStateHandler(HostState::off, HostState::off));
    EXPECT_NO_THROW(discoverOrWait());
    io.restart();
    io.poll();
}

TEST_F(SpeedSelectTest, RetryTimerExpiresAndPropagatesRetryError)
{
    auto& io = dbus::getIOContext();

    hostState = HostState::off;
    EXPECT_NO_THROW(discoverOrWait());
    io.restart();
    EXPECT_NO_THROW(io.run_for(std::chrono::milliseconds(10500)));

    hostState = HostState::postInProgress;
    provider.enabled = true;
    peci.cpus[MIN_CLIENT_ADDR] = {PECI_CC_SUCCESS, modelA06D0, 0,
                                  PECI_DEV_CC_SUCCESS};
    provider.state->throwCurrentLevelRuntime = true;

    io.restart();
    EXPECT_THROW(io.run_for(std::chrono::milliseconds(10500)),
                 std::runtime_error);
}

TEST_F(SpeedSelectTest, RepeatedPeciErrorsAbortDiscovery)
{
    auto& io = dbus::getIOContext();
    hostState = HostState::postInProgress;
    peci.cpus[MIN_CLIENT_ADDR] = {PECI_CC_TIMEOUT, modelA06D0, 0,
                                  PECI_DEV_CC_SUCCESS};

    for (int attempt = 0; attempt < 50; ++attempt)
    {
        EXPECT_NO_THROW(discoverOrWait());
    }

    io.restart();
    io.poll();
}

TEST_F(SpeedSelectTest, ZInitStartsDiscoveryWhenHostPowersOn)
{
    resetPeci();
    updatePowerState("xyz.openbmc_project.State.Host.HostState.Off");
    init();
    EXPECT_NO_THROW(
        updatePowerState("xyz.openbmc_project.State.Host.HostState.Running"));
    EXPECT_EQ(hostState, HostState::postInProgress);
}

} // namespace
#endif // PECI_ENABLED
