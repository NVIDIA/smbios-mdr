#include "handler_unittest.hpp"
#include "smbios_mdrv2.hpp"

#include <openssl/sha.h>
#include <unistd.h>

#include <blobs-ipmid/blobs.hpp>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

namespace blobs
{

class SmbiosBlobHandlerStatCloseTest : public SmbiosBlobHandlerTest
{
  protected:
    SmbiosBlobHandlerStatCloseTest()
    {
        smbiosFile = std::filesystem::temp_directory_path() /
                     ("smbios2-stat-unittest-" + std::to_string(getpid()));
        std::filesystem::remove(smbiosFile);
        handler.setSmbiosFilePath(smbiosFile);
    }

    ~SmbiosBlobHandlerStatCloseTest() override
    {
        std::filesystem::remove(smbiosFile);
    }

    void writeSmbiosFile(uint8_t mdrType, uint32_t dataSize,
                         uint32_t payloadSize, uint8_t dirVer = mdrDirVersion)
    {
        MDRSMBIOSHeader hdr{};
        hdr.dirVer = dirVer;
        hdr.mdrType = mdrType;
        hdr.timestamp = 0;
        hdr.dataSize = dataSize;

        std::ofstream out(smbiosFile,
                          std::ios_base::binary | std::ios_base::trunc);
        out.write(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        std::vector<char> payload(payloadSize, 0x5a);
        out.write(payload.data(), payload.size());
        out.flush();
        ASSERT_TRUE(out.good());
    }

    std::filesystem::path smbiosFile;
    blobs::BlobMeta meta;

    // Initialize expected_meta_ with empty members
    blobs::BlobMeta expected_meta_session = {};
    blobs::BlobMeta expected_meta_path = {};
};

TEST_F(SmbiosBlobHandlerStatCloseTest, InvalidSessionStatIsRejected)
{
    EXPECT_FALSE(handler.stat(session, &meta));
}

TEST_F(SmbiosBlobHandlerStatCloseTest, SessionStatAlwaysInitialReadAndWrite)
{
    // Verify the session stat returns the information for a session.

    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));

    EXPECT_TRUE(handler.stat(session, &meta));
    expected_meta_session.blobState = blobs::StateFlags::open_write;
    EXPECT_EQ(meta, expected_meta_session);

    EXPECT_TRUE(handler.stat(expectedBlobId, &meta));
    expected_meta_path.blobState = blobs::StateFlags::open_write;
    EXPECT_EQ(meta, expected_meta_path);
}

TEST_F(SmbiosBlobHandlerStatCloseTest, AfterWriteMetadataLengthMatches)
{
    // Verify that after writes, the length returned matches.

    std::vector<uint8_t> data = {0x01};
    EXPECT_TRUE(handler.open(session, blobs::OpenFlags::write, expectedBlobId));
    EXPECT_TRUE(handler.write(session, handlerMaxBufferSize - 1, data));

    // We wrote one byte to the last index, making the length the buffer size.
    EXPECT_TRUE(handler.stat(session, &meta));
    expected_meta_session.size = handlerMaxBufferSize;
    expected_meta_session.blobState = blobs::StateFlags::open_write;
    EXPECT_EQ(meta, expected_meta_session);

    EXPECT_TRUE(handler.stat(expectedBlobId, &meta));
    expected_meta_path.size = handlerMaxBufferSize;
    expected_meta_path.blobState = blobs::StateFlags::open_write;
    EXPECT_EQ(meta, expected_meta_path);
}

TEST_F(SmbiosBlobHandlerStatCloseTest, PathStatWithoutSessionNoFileSizeZero)
{
    // No persisted table: Size=0 and no hash -> host resends. A stat
    // FAILURE is reserved for old FW without this support.
    blobs::BlobMeta statMeta;
    EXPECT_TRUE(handler.stat(expectedBlobId, &statMeta));
    EXPECT_EQ(statMeta.size, 0u);
    EXPECT_TRUE(statMeta.metadata.empty());
}

TEST_F(SmbiosBlobHandlerStatCloseTest, PathStatWithoutSessionBadHeaderSizeZero)
{
    // Corrupt persisted files are treated like a missing table: stat
    // succeeds with Size=0 and no hash, telling the host to resend.
    blobs::BlobMeta statMeta;

    auto expectSizeZero = [&]() {
        statMeta = {};
        EXPECT_TRUE(handler.stat(expectedBlobId, &statMeta));
        EXPECT_EQ(statMeta.size, 0u);
        EXPECT_TRUE(statMeta.metadata.empty());
    };

    // Persisted file with a wrong MDR type is not a valid table.
    writeSmbiosFile(mdrTypeII + 1, 16, 16);
    expectSizeZero();

    // Persisted file whose header dataSize disagrees with the file length.
    writeSmbiosFile(mdrTypeII, 16, 8);
    expectSizeZero();

    // Persisted file with an unexpected MDR directory version.
    writeSmbiosFile(mdrTypeII, 16, 16, mdrDirVersion + 1);
    expectSizeZero();

    // Header-only persisted file (no table payload).
    writeSmbiosFile(mdrTypeII, 0, 0);
    expectSizeZero();

    // Persisted file larger than the handler's maximum table size.
    writeSmbiosFile(mdrTypeII, handlerMaxBufferSize + 1,
                    handlerMaxBufferSize + 1);
    expectSizeZero();

    // Truncated file smaller than the MDR header.
    std::ofstream out(smbiosFile, std::ios_base::binary | std::ios_base::trunc);
    out << 'x';
    out.close();
    expectSizeZero();
}

TEST_F(SmbiosBlobHandlerStatCloseTest, PathStatWithoutSessionReportsTable)
{
    // A valid persisted table is reported as a committed blob with the
    // table size and SHA-256 (payload only, MDR header excluded).
    constexpr uint32_t tableSize = 32;
    writeSmbiosFile(mdrTypeII, tableSize, tableSize);

    blobs::BlobMeta statMeta;
    EXPECT_TRUE(handler.stat(expectedBlobId, &statMeta));

    const std::vector<uint8_t> payload(tableSize, 0x5a);
    blobs::BlobMeta wantMeta = {};
    wantMeta.metadata.resize(SHA256_DIGEST_LENGTH);
    SHA256(payload.data(), payload.size(), wantMeta.metadata.data());
    wantMeta.size = tableSize;
    wantMeta.blobState = blobs::StateFlags::committed;
    EXPECT_EQ(statMeta, wantMeta);
}

TEST_F(SmbiosBlobHandlerStatCloseTest, CloseWithInvalidSessionFails)
{
    // Verify you cannot close an invalid session.

    EXPECT_FALSE(handler.close(session));
}

TEST_F(SmbiosBlobHandlerStatCloseTest, CloseWithValidSessionSuccess)
{
    // Verify you can close a valid session.

    EXPECT_TRUE(handler.open(session, 0, expectedBlobId));

    EXPECT_TRUE(handler.close(session));
}
} // namespace blobs
