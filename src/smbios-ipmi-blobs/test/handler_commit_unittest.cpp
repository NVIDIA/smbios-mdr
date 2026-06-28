#include "handler_unittest.hpp"

#include <blobs-ipmid/blobs.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>

#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace blobs
{

namespace internal
{

bool syncSmbiosData(sdbusplus::bus_t& bus);

} // namespace internal

ACTION_P(SetReadDbusBool, value)
{
    *static_cast<int*>(arg2) = value ? 1 : 0;
    return 0;
}

class SmbiosBlobHandlerCommitTest : public SmbiosBlobHandlerTest
{
  protected:
    blobs::BlobMeta meta;
};

class SuccessfulSyncSmbiosBlobHandler : public SmbiosBlobHandler
{
  protected:
    bool syncSmbiosData() override
    {
        return true;
    }
};
class ExposedSyncSmbiosBlobHandler : public SmbiosBlobHandler
{
  public:
    using SmbiosBlobHandler::syncSmbiosData;
};

TEST(SmbiosBlobHandlerSyncTest, BusAcquisitionSdbusErrorReturnsFalse)
{
    ExposedSyncSmbiosBlobHandler handler;
    test::setIpmiBusBehavior(test::IpmiBusBehavior::throwSdbus);

    EXPECT_FALSE(handler.syncSmbiosData());

    test::setIpmiBusBehavior(test::IpmiBusBehavior::returnBus);
}

TEST(SmbiosBlobHandlerSyncTest, BusAcquisitionStandardErrorReturnsFalse)
{
    ExposedSyncSmbiosBlobHandler handler;
    test::setIpmiBusBehavior(test::IpmiBusBehavior::throwStandard);

    EXPECT_FALSE(handler.syncSmbiosData());

    test::setIpmiBusBehavior(test::IpmiBusBehavior::returnBus);
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitWithUnexpectedDataIsRejected)
{
    // commit() rejects any non-empty data payload.
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    std::vector<uint8_t> data = {0x1};
    EXPECT_FALSE(handler.commit(session, data));
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitWithNoOpenBlobIsRejected)
{
    // commit() with no open blob (blobPtr is null) returns false.
    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitWithWrongSessionIsRejected)
{
    // commit() checks the session id matches the open blob.
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    EXPECT_FALSE(handler.commit(session + 1, std::vector<uint8_t>()));
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitWritesDataAndAttemptsSync)
{
    // Open, stage some data, and commit to a destination that cannot be
    // opened for writing (an existing directory path). commit() reports
    // failure and sets the commit_error state. Using an explicit failing path
    // keeps the result deterministic regardless of the privileges of the
    // environment running the test.
    handler.setSmbiosFilePath(std::filesystem::temp_directory_path().string());

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    std::vector<uint8_t> data(64, 0xAB);
    EXPECT_TRUE(handler.write(session, 0, data));

    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));

    // The commit_error bit should be set after a failed commit.
    EXPECT_TRUE(handler.stat(session, &meta));
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::commit_error);
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitEmptyBufferAttemptsSync)
{
    // Committing without staging any data still drives the commit path. Point
    // at an uncreatable directory so commit() fails deterministically before
    // the D-Bus sync step in any environment.
    handler.setSmbiosFilePath("/proc/smbios_blob_ut_nope/smbios2");

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));
}

TEST_F(SmbiosBlobHandlerCommitTest, DeleteBlobAlwaysReturnsFalse)
{
    EXPECT_FALSE(handler.deleteBlob(expectedBlobId));
}

TEST_F(SmbiosBlobHandlerCommitTest, WriteMetaAlwaysReturnsFalse)
{
    std::vector<uint8_t> data = {0x1, 0x2};
    EXPECT_FALSE(handler.writeMeta(session, 0, data));
}

TEST_F(SmbiosBlobHandlerCommitTest, ExpireWithInvalidSessionFails)
{
    EXPECT_FALSE(handler.expire(session));
}

TEST_F(SmbiosBlobHandlerCommitTest, ExpireClosesAnOpenSession)
{
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    EXPECT_TRUE(handler.expire(session));

    // After expiring, the session is gone and a stat must fail.
    EXPECT_FALSE(handler.stat(session, &meta));
}

TEST_F(SmbiosBlobHandlerCommitTest, PathStatWithNoOpenBlobFails)
{
    // stat(path) returns false when no blob is open (blobPtr is null).
    EXPECT_FALSE(handler.stat(expectedBlobId, &meta));
}

TEST_F(SmbiosBlobHandlerCommitTest, PathStatWithMismatchedPathFails)
{
    // stat(path) returns false when the open blob's id differs from path.
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    EXPECT_FALSE(handler.stat("/smbios_wrong", &meta));
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitCreatesDirectoryAndWritesFile)
{
    // Point the handler at a destination whose parent directory does not yet
    // exist. commit() should create the directory, write the staged data, and
    // then fail only at the D-Bus sync step (no bus in the unit-test env).
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "smbios_blob_ut_new";
    std::filesystem::remove_all(dir);
    handler.setSmbiosFilePath((dir / "smbios2").string());

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));
    std::vector<uint8_t> data(32, 0xCD);
    EXPECT_TRUE(handler.write(session, 0, data));

    // The directory is created and the data file is written; the commit then
    // attempts to sync over D-Bus, which is unavailable in the unit-test
    // environment. The file must already exist by then.
    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));
    EXPECT_TRUE(std::filesystem::exists(dir / "smbios2"));

    EXPECT_TRUE(handler.stat(session, &meta));
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::commit_error);

    std::filesystem::remove_all(dir);
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitWritesWhenDirectoryAlreadyExists)
{
    // Parent directory already exists, so the mkdir branch is skipped.
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "smbios_blob_ut_exist";
    std::filesystem::create_directories(dir);
    handler.setSmbiosFilePath((dir / "smbios2").string());

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));
    std::vector<uint8_t> data(8, 0x55);
    EXPECT_TRUE(handler.write(session, 0, data));

    // mkdir is skipped (directory exists); the file is written and the commit
    // then reports failure at the unavailable D-Bus sync step.
    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));
    EXPECT_TRUE(std::filesystem::exists(dir / "smbios2"));

    EXPECT_TRUE(handler.stat(session, &meta));
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::commit_error);

    std::filesystem::remove_all(dir);
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitSuccessWritesFileAndSetsCommitted)
{
    SuccessfulSyncSmbiosBlobHandler successHandler;
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "smbios_blob_ut_success";
    std::filesystem::remove_all(dir);
    successHandler.setSmbiosFilePath((dir / "smbios2").string());

    EXPECT_TRUE(
        successHandler.open(session, blobs::OpenFlags::write, expectedBlobId));
    std::vector<uint8_t> data = {0x10, 0x20, 0x30, 0x40};
    EXPECT_TRUE(successHandler.write(session, 0, data));

    EXPECT_TRUE(successHandler.commit(session, std::vector<uint8_t>()));
    EXPECT_TRUE(std::filesystem::exists(dir / "smbios2"));

    EXPECT_TRUE(successHandler.stat(session, &meta));
    EXPECT_EQ(meta.size, data.size());
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::committed);
    EXPECT_FALSE(meta.blobState & blobs::StateFlags::committing);
    EXPECT_FALSE(meta.blobState & blobs::StateFlags::commit_error);

    std::ifstream file(dir / "smbios2", std::ios::binary);
    ASSERT_TRUE(file.good());
    MDRSMBIOSHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    EXPECT_EQ(header.dataSize, data.size());
    std::vector<uint8_t> written(data.size());
    file.read(reinterpret_cast<char*>(written.data()), written.size());
    EXPECT_EQ(written, data);

    std::filesystem::remove_all(dir);
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitAlreadyCommittedReturnsTrue)
{
    SuccessfulSyncSmbiosBlobHandler successHandler;
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "smbios_blob_ut_committed";
    std::filesystem::remove_all(dir);
    successHandler.setSmbiosFilePath((dir / "smbios2").string());

    EXPECT_TRUE(
        successHandler.open(session, blobs::OpenFlags::write, expectedBlobId));
    EXPECT_TRUE(successHandler.write(session, 0, std::vector<uint8_t>{0x5a}));
    EXPECT_TRUE(successHandler.commit(session, std::vector<uint8_t>()));

    EXPECT_TRUE(successHandler.commit(session, std::vector<uint8_t>()));
    EXPECT_TRUE(successHandler.stat(session, &meta));
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::committed);

    std::filesystem::remove_all(dir);
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitFailsWhenFileCannotBeOpened)
{
    // Point the destination at an existing directory path. Opening it as a
    // file for writing fails, exercising the open-failure branch.
    handler.setSmbiosFilePath(std::filesystem::temp_directory_path().string());

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));
    std::vector<uint8_t> data(8, 0x66);
    EXPECT_TRUE(handler.write(session, 0, data));

    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));

    EXPECT_TRUE(handler.stat(session, &meta));
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::commit_error);
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitFailsWhenDirectoryCannotBeCreated)
{
    // Parent directory does not exist and cannot be created (no permission to
    // create directories under /proc), exercising the mkdir-failure branch.
    handler.setSmbiosFilePath("/proc/smbios_blob_ut_nope/smbios2");

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));
    std::vector<uint8_t> data(8, 0x77);
    EXPECT_TRUE(handler.write(session, 0, data));

    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));

    EXPECT_TRUE(handler.stat(session, &meta));
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::commit_error);
}

TEST_F(SmbiosBlobHandlerCommitTest, CommitHandlesWriteFailure)
{
    // /dev/full opens successfully but every write fails with ENOSPC. With the
    // failbit/badbit exception mask set during commit(), the write throws
    // std::ofstream::failure, which commit() catches and converts into a
    // commit_error. /dev/full is present and world-writable on Linux, so this
    // is deterministic regardless of privilege.
    if (!std::filesystem::exists("/dev/full"))
    {
        GTEST_SKIP() << "/dev/full not available";
    }
    handler.setSmbiosFilePath("/dev/full");

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));
    std::vector<uint8_t> data(64, 0x99);
    EXPECT_TRUE(handler.write(session, 0, data));

    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));

    EXPECT_TRUE(handler.stat(session, &meta));
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::commit_error);
}

TEST_F(SmbiosBlobHandlerCommitTest, WriteWithWrongSessionWhileBlobOpenFails)
{
    // A blob is open, but a write arrives for a different session id. This
    // covers the second half of the (!blobPtr || sessionId != session) check.
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    std::vector<uint8_t> data = {0x1, 0x2};
    EXPECT_FALSE(handler.write(session + 1, 0, data));
}

TEST_F(SmbiosBlobHandlerCommitTest, CloseWithWrongSessionWhileBlobOpenFails)
{
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    EXPECT_FALSE(handler.close(session + 1));
}

TEST_F(SmbiosBlobHandlerCommitTest,
       SessionStatWithWrongSessionWhileBlobOpenFails)
{
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    EXPECT_FALSE(handler.stat(session + 1, &meta));
}

TEST_F(SmbiosBlobHandlerCommitTest, WriteWithinExistingBufferDoesNotResize)
{
    // First write grows the staging buffer to the full size; a subsequent
    // smaller write at offset zero stays within the buffer and skips the
    // resize branch.
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    std::vector<uint8_t> full(handlerMaxBufferSize, 0x11);
    EXPECT_TRUE(handler.write(session, 0, full));

    std::vector<uint8_t> small = {0x22, 0x33};
    EXPECT_TRUE(handler.write(session, 0, small));
}

TEST_F(SmbiosBlobHandlerCommitTest,
       CommitFileWriteWithUnconnectedBusReportsSyncFailure)
{
    test::setIpmiBus(nullptr);

    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "smbios_blob_ut_unconnected";
    std::filesystem::remove_all(dir);
    handler.setSmbiosFilePath((dir / "smbios2").string());

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));
    std::vector<uint8_t> data(16, 0x5a);
    EXPECT_TRUE(handler.write(session, 0, data));

    EXPECT_FALSE(handler.commit(session, std::vector<uint8_t>()));
    EXPECT_TRUE(std::filesystem::exists(dir / "smbios2"));

    EXPECT_TRUE(handler.stat(session, &meta));
    EXPECT_TRUE(meta.blobState & blobs::StateFlags::commit_error);

    std::filesystem::remove_all(dir);
}

TEST(SmbiosBlobHandlerSyncTest, SyncSmbiosDataReturnsTrueWhenServiceReportsTrue)
{
    testing::NiceMock<sdbusplus::SdBusMock> mock;
    auto bus = sdbusplus::get_mocked_new(&mock);

    EXPECT_CALL(mock, sd_bus_message_new_method_call(
                          nullptr, testing::_,
                          testing::StrEq("xyz.openbmc_project.Smbios.MDR_V2"),
                          testing::StrEq("/xyz/openbmc_project/Smbios/MDR_V2"),
                          testing::StrEq("xyz.openbmc_project.Smbios.MDR_V2"),
                          testing::StrEq("AgentSynchronizeData")))
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock, sd_bus_call(nullptr, nullptr, testing::_, testing::_,
                                  testing::_))
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock, sd_bus_message_read_basic(nullptr, SD_BUS_TYPE_BOOLEAN,
                                                testing::_))
        .WillOnce(SetReadDbusBool(true));

    EXPECT_TRUE(internal::syncSmbiosData(bus));
}

TEST(SmbiosBlobHandlerSyncTest,
     SyncSmbiosDataReturnsFalseWhenServiceReportsFalse)
{
    testing::NiceMock<sdbusplus::SdBusMock> mock;
    auto bus = sdbusplus::get_mocked_new(&mock);

    EXPECT_CALL(mock, sd_bus_message_new_method_call(testing::_, testing::_,
                                                     testing::_, testing::_,
                                                     testing::_, testing::_))
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock, sd_bus_call(testing::_, testing::_, testing::_,
                                  testing::_, testing::_))
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock, sd_bus_message_read_basic(testing::_, SD_BUS_TYPE_BOOLEAN,
                                                testing::_))
        .WillOnce(SetReadDbusBool(false));

    EXPECT_FALSE(internal::syncSmbiosData(bus));
}

TEST(SmbiosBlobHandlerSyncTest, SyncSmbiosDataReturnsFalseWhenReplyReadFails)
{
    testing::NiceMock<sdbusplus::SdBusMock> mock;
    auto bus = sdbusplus::get_mocked_new(&mock);

    EXPECT_CALL(mock, sd_bus_message_new_method_call(testing::_, testing::_,
                                                     testing::_, testing::_,
                                                     testing::_, testing::_))
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock, sd_bus_call(testing::_, testing::_, testing::_,
                                  testing::_, testing::_))
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock, sd_bus_message_read_basic(testing::_, SD_BUS_TYPE_BOOLEAN,
                                                testing::_))
        .WillOnce(testing::Return(-EINVAL));

    EXPECT_FALSE(internal::syncSmbiosData(bus));
}

TEST(SmbiosBlobHandlerSyncTest,
     SyncSmbiosDataReturnsFalseWhenMethodCreationFails)
{
    testing::NiceMock<sdbusplus::SdBusMock> mock;
    auto bus = sdbusplus::get_mocked_new(&mock);

    EXPECT_CALL(mock, sd_bus_message_new_method_call(testing::_, testing::_,
                                                     testing::_, testing::_,
                                                     testing::_, testing::_))
        .WillOnce(testing::Return(-EINVAL));

    EXPECT_FALSE(internal::syncSmbiosData(bus));
}

TEST(SmbiosBlobHandlerSyncTest, SyncSmbiosDataReturnsFalseWhenCallThrowsStd)
{
    testing::NiceMock<sdbusplus::SdBusMock> mock;
    auto bus = sdbusplus::get_mocked_new(&mock);

    EXPECT_CALL(mock, sd_bus_message_new_method_call(testing::_, testing::_,
                                                     testing::_, testing::_,
                                                     testing::_, testing::_))
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock, sd_bus_call(testing::_, testing::_, testing::_,
                                  testing::_, testing::_))
        .WillOnce(testing::Throw(std::runtime_error("forced sd_bus_call")));

    EXPECT_FALSE(internal::syncSmbiosData(bus));
}

} // namespace blobs
