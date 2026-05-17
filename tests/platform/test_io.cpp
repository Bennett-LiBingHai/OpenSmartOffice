#include "oso/base/ErrorCode.h"
#include "oso/io/IFileSystem.h"
#include "oso/io/IStream.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace oso;

// ============================================================
// MemoryStream — construction / basic state
// ============================================================

TEST(MemoryStream, DefaultConstructed) {
    MemoryStream stream;
    EXPECT_TRUE(stream.isOpen());
    EXPECT_EQ(stream.size().value(), 0);
    EXPECT_EQ(stream.tell().value(), 0);
}

TEST(MemoryStream, ConstructedWithData) {
    std::vector<uint8_t> input = {1, 2, 3, 4, 5};
    MemoryStream stream(input);
    EXPECT_TRUE(stream.isOpen());
    EXPECT_EQ(stream.size().value(), 5);
    EXPECT_EQ(stream.tell().value(), 0);
}

// ============================================================
// MemoryStream — read
// ============================================================

TEST(MemoryStream, ReadPartial) {
    std::vector<uint8_t> input = {10, 20, 30, 40, 50};
    MemoryStream stream(input);

    uint8_t buf[3] = {};
    auto r = stream.read(buf, 3);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 3);
    EXPECT_EQ(buf[0], 10);
    EXPECT_EQ(buf[1], 20);
    EXPECT_EQ(buf[2], 30);
    EXPECT_EQ(stream.tell().value(), 3);
}

TEST(MemoryStream, ReadExactSize) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);

    uint8_t buf[3] = {};
    auto r = stream.read(buf, 3);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 3);
}

TEST(MemoryStream, ReadMoreThanAvailable) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);

    uint8_t buf[10] = {};
    auto r = stream.read(buf, 10);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 3);  // only 3 available
    EXPECT_EQ(stream.tell().value(), 3);
}

TEST(MemoryStream, ReadFromEmptyStream) {
    MemoryStream stream;

    uint8_t buf[4] = {};
    auto r = stream.read(buf, 4);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0);  // nothing to read
}

TEST(MemoryStream, ReadAtEof) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);

    uint8_t buf[3] = {};
    stream.read(buf, 3);  // consume all

    auto r = stream.read(buf, 1);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 0);
}

// ============================================================
// MemoryStream — readAll
// ============================================================

TEST(MemoryStream, ReadAll) {
    std::vector<uint8_t> input = {10, 20, 30, 40, 50};
    MemoryStream stream(input);

    // seek to middle, then readAll should return remaining
    stream.seek(2, StreamSeek::START);  // SEEK_SET

    auto r = stream.readAll();
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value().size(), 3);
    EXPECT_EQ(r.value()[0], 30);
    EXPECT_EQ(r.value()[1], 40);
    EXPECT_EQ(r.value()[2], 50);
    EXPECT_EQ(stream.tell().value(), 5);
}

TEST(MemoryStream, ReadAllFromStart) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);

    auto r = stream.readAll();
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value().size(), 3);
}

TEST(MemoryStream, ReadAllEmptyStream) {
    MemoryStream stream;

    auto r = stream.readAll();
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value().size(), 0);
}

// ============================================================
// MemoryStream — write
// ============================================================

TEST(MemoryStream, WriteIntoEmpty) {
    MemoryStream stream;
    uint8_t data[] = {10, 20, 30};
    auto r = stream.write(data, 3);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(stream.size().value(), 3);
    EXPECT_EQ(stream.tell().value(), 3);
    EXPECT_EQ(stream.data()[0], 10);
    EXPECT_EQ(stream.data()[1], 20);
    EXPECT_EQ(stream.data()[2], 30);
}

TEST(MemoryStream, WriteThenReadBack) {
    MemoryStream stream;
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    stream.write(data, 3);

    // seek back to start
    stream.seek(0, StreamSeek::START);
    uint8_t buf[3] = {};
    auto r = stream.read(buf, 3);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 3);
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[1], 0xBB);
    EXPECT_EQ(buf[2], 0xCC);
}

TEST(MemoryStream, WriteExtendsData) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);

    // seek to end and write more
    stream.seek(0, StreamSeek::END);  // SEEK_END
    uint8_t data[] = {4, 5};
    stream.write(data, 2);

    EXPECT_EQ(stream.size().value(), 5);
    stream.seek(0, StreamSeek::START);
    auto all = stream.readAll();
    EXPECT_EQ(all.value().size(), 5);
    EXPECT_EQ(all.value()[3], 4);
    EXPECT_EQ(all.value()[4], 5);
}

TEST(MemoryStream, WriteAtMiddleOverwrites) {
    std::vector<uint8_t> input = {1, 2, 3, 4, 5};
    MemoryStream stream(input);

    stream.seek(2, StreamSeek::START);  // SEEK_SET to position 2
    uint8_t data[] = {0xAA, 0xBB};
    stream.write(data, 2);

    EXPECT_EQ(stream.size().value(), 5);  // still 5, no expansion
    stream.seek(0, StreamSeek::START);
    auto all = stream.readAll();
    EXPECT_EQ(all.value()[0], 1);
    EXPECT_EQ(all.value()[1], 2);
    EXPECT_EQ(all.value()[2], 0xAA);
    EXPECT_EQ(all.value()[3], 0xBB);
    EXPECT_EQ(all.value()[4], 5);
}

TEST(MemoryStream, WriteBeyondCurrentSize) {
    std::vector<uint8_t> input = {1, 2};
    MemoryStream stream(input);

    // position at end (2), write 3 more — triggers resize
    stream.seek(0, StreamSeek::END);
    uint8_t data[] = {3, 4, 5};
    stream.write(data, 3);

    EXPECT_EQ(stream.size().value(), 5);
    stream.seek(0, StreamSeek::START);
    auto all = stream.readAll();
    EXPECT_EQ(all.value().size(), 5);
}

TEST(MemoryStream, WriteAllConvenience) {
    MemoryStream stream;
    std::vector<uint8_t> data = {7, 8, 9};
    auto r = stream.writeAll(data);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(stream.size().value(), 3);
}

// ============================================================
// MemoryStream — seek
// ============================================================

TEST(MemoryStream, SeekSet) {
    std::vector<uint8_t> input = {10, 20, 30, 40, 50};
    MemoryStream stream(input);

    auto r = stream.seek(3, StreamSeek::START);  // SEEK_SET
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 3);
    EXPECT_EQ(stream.tell().value(), 3);
}

TEST(MemoryStream, SeekCurForward) {
    std::vector<uint8_t> input = {10, 20, 30, 40, 50};
    MemoryStream stream(input);

    stream.seek(2, StreamSeek::START);  // to pos 2
    auto r = stream.seek(1, StreamSeek::CUR);  // SEEK_CUR forward 1
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 3);
}

TEST(MemoryStream, SeekCurBackward) {
    std::vector<uint8_t> input = {10, 20, 30, 40, 50};
    MemoryStream stream(input);

    stream.seek(4, StreamSeek::START);  // to pos 4
    auto r = stream.seek(-2, StreamSeek::CUR);  // SEEK_CUR back 2
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 2);
}

TEST(MemoryStream, SeekEnd) {
    std::vector<uint8_t> input = {10, 20, 30, 40, 50};
    MemoryStream stream(input);

    auto r = stream.seek(0, StreamSeek::END);  // SEEK_END
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 5);
}

TEST(MemoryStream, SeekEndNegativeOffset) {
    std::vector<uint8_t> input = {10, 20, 30, 40, 50};
    MemoryStream stream(input);

    auto r = stream.seek(-2, StreamSeek::END);  // SEEK_END minus 2
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 3);
}

TEST(MemoryStream, SeekSetNegative) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);

    auto r = stream.seek(-1, StreamSeek::START);  // SEEK_SET negative
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), ErrorCode::IOReadError);
}

TEST(MemoryStream, SeekPastEnd) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);

    auto r = stream.seek(100, StreamSeek::START);  // beyond data size
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), ErrorCode::IOReadError);
}

TEST(MemoryStream, SeekCurTooFarBack) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);

    stream.seek(1, StreamSeek::START);  // pos 1
    auto r = stream.seek(-10, StreamSeek::CUR);  // too far back
    EXPECT_TRUE(r.isErr());
}

// ============================================================
// MemoryStream — tell
// ============================================================

TEST(MemoryStream, TellInitial) {
    std::vector<uint8_t> input = {1, 2, 3};
    MemoryStream stream(input);
    EXPECT_EQ(stream.tell().value(), 0);
}

TEST(MemoryStream, TellAfterRead) {
    std::vector<uint8_t> input = {1, 2, 3, 4, 5};
    MemoryStream stream(input);

    uint8_t buf[2] = {};
    stream.read(buf, 2);
    EXPECT_EQ(stream.tell().value(), 2);
}

TEST(MemoryStream, TellAfterSeek) {
    std::vector<uint8_t> input = {1, 2, 3, 4, 5};
    MemoryStream stream(input);

    stream.seek(4, StreamSeek::START);
    EXPECT_EQ(stream.tell().value(), 4);
}

// ============================================================
// MemoryStream — close
// ============================================================

TEST(MemoryStream, Close) {
    MemoryStream stream;
    EXPECT_TRUE(stream.isOpen());

    auto r = stream.close();
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(stream.isOpen());
}

TEST(MemoryStream, ReadAfterClose) {
    MemoryStream stream(std::vector<uint8_t>{1, 2, 3});
    stream.close();

    uint8_t buf[4] = {};
    auto r = stream.read(buf, 4);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), ErrorCode::IOReadError);
}

TEST(MemoryStream, ReadAllAfterClose) {
    MemoryStream stream(std::vector<uint8_t>{1, 2, 3});
    stream.close();

    auto r = stream.readAll();
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), ErrorCode::IOReadError);
}

TEST(MemoryStream, WriteAfterClose) {
    MemoryStream stream;
    stream.close();

    uint8_t data[] = {1, 2};
    auto r = stream.write(data, 2);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), ErrorCode::IOWriteError);
}

// ============================================================
// MemoryStream — size
// ============================================================

TEST(MemoryStream, SizeEmpty) {
    MemoryStream stream;
    EXPECT_EQ(stream.size().value(), 0);
}

TEST(MemoryStream, SizeAfterWrite) {
    MemoryStream stream;
    uint8_t data[] = {1, 2, 3, 4};
    stream.write(data, 4);
    EXPECT_EQ(stream.size().value(), 4);
}

TEST(MemoryStream, SizeUnchangedAfterRead) {
    std::vector<uint8_t> input = {1, 2, 3, 4, 5};
    MemoryStream stream(input);

    uint8_t buf[3] = {};
    stream.read(buf, 3);
    EXPECT_EQ(stream.size().value(), 5);
}

// ============================================================
// MemoryStream — flush
// ============================================================

TEST(MemoryStream, FlushReturnsOk) {
    MemoryStream stream;
    EXPECT_TRUE(stream.flush().isOk());
}

// ============================================================
// MemoryStream — data() accessor
// ============================================================

TEST(MemoryStream, DataReturnsUnderlyingBuffer) {
    std::vector<uint8_t> input = {9, 8, 7};
    MemoryStream stream(input);
    EXPECT_EQ(stream.data().size(), 3);
    EXPECT_EQ(stream.data()[0], 9);
}

// ============================================================
// MemoryStream — round-trip (milestone acceptance)
// ============================================================

TEST(MemoryStream, RoundTrip_ReadWriteRead) {
    // Write
    MemoryStream stream;
    uint8_t original[] = {0xDE, 0xAD, 0xBE, 0xEF};
    stream.write(original, 4);

    // Seek back
    stream.seek(0, StreamSeek::START);

    // Read
    uint8_t buf[4] = {};
    auto r = stream.read(buf, 4);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 4);
    EXPECT_EQ(buf[0], 0xDE);
    EXPECT_EQ(buf[1], 0xAD);
    EXPECT_EQ(buf[2], 0xBE);
    EXPECT_EQ(buf[3], 0xEF);
}

TEST(MemoryStream, RoundTrip_LargeData) {
    MemoryStream stream;
    std::vector<uint8_t> original(65536);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>(i & 0xFF);
    }

    auto wr = stream.write(original.data(), original.size());
    EXPECT_TRUE(wr.isOk());
    EXPECT_EQ(stream.size().value(), 65536);

    stream.seek(0, StreamSeek::START);
    auto rd = stream.readAll();
    EXPECT_TRUE(rd.isOk());
    EXPECT_EQ(rd.value(), original);
}

namespace {

std::string tmpPath(const std::string& name) { return "/tmp/oso_fs_test_" + name; }

struct TmpFileGuard {
    std::string path;
    explicit TmpFileGuard(std::string p) : path(std::move(p)) {}
    ~TmpFileGuard() { std::remove(path.c_str()); }
};

void writeTestFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
    out.close();
}

}  // anonymous namespace

// ============================================================
// FileStream — 读取
// ============================================================

TEST(FileStream, OpenRead) {
    auto p = tmpPath("openread.txt");
    TmpFileGuard guard(p);
    writeTestFile(p, "hello world");

    FileStream stream(p, "rb");
    ASSERT_TRUE(stream.isOpen());

    uint8_t buf[128];
    auto result = stream.read(buf, 11);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), 11u);
    buf[11] = '\0';
    EXPECT_STREQ(reinterpret_cast<const char*>(buf), "hello world");
}

TEST(FileStream, ReadAll) {
    auto p = tmpPath("readall.txt");
    TmpFileGuard guard(p);
    writeTestFile(p, "abcdefghij");

    FileStream stream(p, "rb");
    auto result = stream.readAll();
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().size(), 10u);
    std::string s(reinterpret_cast<const char*>(result.value().data()), 10);
    EXPECT_EQ(s, "abcdefghij");
}

TEST(FileStream, OpenNonExistent) {
    FileStream stream("/tmp/oso_fs_nonexistent_xxxxxx.txt", "rb");
    EXPECT_FALSE(stream.isOpen());
}

// ============================================================
// FileStream — 写入
// ============================================================

TEST(FileStream, WriteAndReadBack) {
    auto p = tmpPath("writeback.bin");
    TmpFileGuard guard(p);

    {
        FileStream w(p, "wb");
        ASSERT_TRUE(w.isOpen());
        const uint8_t data[] = {0x00, 0x01, 0x02, 0xFF};
        ASSERT_TRUE(w.write(data, 4).isOk());
    }

    FileStream r(p, "rb");
    auto result = r.readAll();
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(result.value().size(), 4u);
    EXPECT_EQ(result.value()[0], 0x00);
    EXPECT_EQ(result.value()[1], 0x01);
    EXPECT_EQ(result.value()[2], 0x02);
    EXPECT_EQ(result.value()[3], 0xFF);
}

// ============================================================
// FileStream — seek / tell
// ============================================================

TEST(FileStream, SeekAndTell) {
    auto p = tmpPath("seek.bin");
    TmpFileGuard guard(p);
    writeTestFile(p, "0123456789");

    FileStream stream(p, "rb");
    auto t0 = stream.tell();
    ASSERT_TRUE(t0.isOk());
    EXPECT_EQ(t0.value(), 0u);

    auto s1 = stream.seek(5, StreamSeek::START);
    ASSERT_TRUE(s1.isOk());
    EXPECT_EQ(s1.value(), 5u);

    uint8_t buf[1];
    ASSERT_TRUE(stream.read(buf, 1).isOk());
    EXPECT_EQ(buf[0], '5');

    auto s2 = stream.seek(2, StreamSeek::CUR);
    ASSERT_TRUE(s2.isOk());
    EXPECT_EQ(s2.value(), 8u);

    auto s3 = stream.seek(-2, StreamSeek::END);
    ASSERT_TRUE(s3.isOk());
    EXPECT_EQ(s3.value(), 8u);
}

// ============================================================
// FileStream — size
// ============================================================

TEST(FileStream, Size) {
    auto p = tmpPath("size.bin");
    TmpFileGuard guard(p);
    {
        FileStream w(p, "wb");
        const uint8_t d[7] = {};
        w.write(d, 7);
    }

    FileStream r(p, "rb");
    auto sz = r.size();
    ASSERT_TRUE(sz.isOk());
    EXPECT_EQ(sz.value(), 7u);
}

// ============================================================
// FileStream — close 后操作
// ============================================================

TEST(FileStream, ReadAfterClose) {
    auto p = tmpPath("closed.txt");
    TmpFileGuard guard(p);
    writeTestFile(p, "data");

    FileStream stream(p, "rb");
    ASSERT_TRUE(stream.isOpen());
    ASSERT_TRUE(stream.close().isOk());
    EXPECT_FALSE(stream.isOpen());

    uint8_t buf[4];
    auto result = stream.read(buf, 4);
    EXPECT_TRUE(result.isErr());
}

// ============================================================
// FileStream — flush
// ============================================================

TEST(FileStream, Flush) {
    auto p = tmpPath("flush.bin");
    TmpFileGuard guard(p);

    FileStream w(p, "wb");
    ASSERT_TRUE(w.isOpen());
    const uint8_t d[] = {0xAB};
    ASSERT_TRUE(w.write(d, 1).isOk());
    ASSERT_TRUE(w.flush().isOk());
    w.close();

    FileStream r(p, "rb");
    auto result = r.readAll();
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0], 0xAB);
}

// ============================================================
// FileStream — move semantics
// ============================================================

TEST(FileStream, MoveSemantics) {
    auto p = tmpPath("move.bin");
    TmpFileGuard guard(p);
    writeTestFile(p, "test");

    FileStream a(p, "rb");
    ASSERT_TRUE(a.isOpen());

    FileStream b(std::move(a));
    EXPECT_FALSE(a.isOpen());
    EXPECT_TRUE(b.isOpen());

    auto result = b.readAll();
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().size(), 4u);
}

// ============================================================
// NativeFileSystem — exists / fileSize
// ============================================================

TEST(NativeFileSystem, Exists) {
    NativeFileSystem fs;
    auto p = tmpPath("exists.txt");
    TmpFileGuard guard(p);

    EXPECT_FALSE(fs.exists(p));
    writeTestFile(p, "x");
    EXPECT_TRUE(fs.exists(p));
    std::remove(p.c_str());
    EXPECT_FALSE(fs.exists(p));
}

TEST(NativeFileSystem, FileSize) {
    NativeFileSystem fs;
    auto p = tmpPath("fsize.bin");
    TmpFileGuard guard(p);
    writeTestFile(p, "1234567890");  // 10 chars

    auto sz = fs.fileSize(p);
    ASSERT_TRUE(sz.isOk());
    EXPECT_EQ(sz.value(), 10u);
}

TEST(NativeFileSystem, FileSizeNonexistent) {
    NativeFileSystem fs;
    auto r = fs.fileSize("/tmp/nonexistent_xxxxxxxxxx.bin");
    EXPECT_TRUE(r.isErr());
}

// ============================================================
// NativeFileSystem — openRead / openWrite
// ============================================================

TEST(NativeFileSystem, OpenRead) {
    NativeFileSystem fs;
    auto p = tmpPath("openread.txt");
    TmpFileGuard guard(p);
    writeTestFile(p, "native read test");

    auto streamResult = fs.openRead(p);
    ASSERT_TRUE(streamResult.isOk());
    auto stream = std::move(streamResult.value());
    ASSERT_TRUE(stream->isOpen());

    auto data = stream->readAll();
    ASSERT_TRUE(data.isOk());
    std::string s(reinterpret_cast<const char*>(data.value().data()), data.value().size());
    EXPECT_EQ(s, "native read test");
}

TEST(NativeFileSystem, OpenReadNonexistent) {
    NativeFileSystem fs;
    auto r = fs.openRead("/tmp/nonexistent_deadbeef.txt");
    EXPECT_TRUE(r.isErr());
}

TEST(NativeFileSystem, OpenWrite) {
    NativeFileSystem fs;
    auto p = tmpPath("openwrite.txt");
    TmpFileGuard guard(p);

    {
        auto s = fs.openWrite(p);
        ASSERT_TRUE(s.isOk());
        const uint8_t d[] = {'A', 'B', 'C'};
        ASSERT_TRUE(s.value()->write(d, 3).isOk());
    }

    FileStream r(p, "rb");
    auto data = r.readAll();
    ASSERT_TRUE(data.isOk());
    ASSERT_EQ(data.value().size(), 3u);
    EXPECT_EQ(data.value()[0], 'A');
    EXPECT_EQ(data.value()[1], 'B');
    EXPECT_EQ(data.value()[2], 'C');
}

// ============================================================
// NativeFileSystem — remove
// ============================================================

TEST(NativeFileSystem, Remove) {
    NativeFileSystem fs;
    auto p = tmpPath("remove.txt");
    writeTestFile(p, "x");
    ASSERT_TRUE(fs.exists(p));

    auto result = fs.remove(p);
    EXPECT_TRUE(result.isOk());
    EXPECT_FALSE(fs.exists(p));
}

// ============================================================
// NativeFileSystem — createDirectory / listDirectory
// ============================================================

TEST(NativeFileSystem, CreateAndListDirectory) {
    NativeFileSystem fs;
    auto dir = tmpPath("testdir");
    // clean up any leftover
    std::filesystem::remove_all(dir);

    auto createResult = fs.createDirectory(dir);
    ASSERT_TRUE(createResult.isOk());
    EXPECT_TRUE(fs.exists(dir));

    // write a couple files
    writeTestFile(dir + "/a.txt", "a");
    writeTestFile(dir + "/b.txt", "b");

    auto listResult = fs.listDirectory(dir);
    ASSERT_TRUE(listResult.isOk());
    auto& entries = listResult.value();
    EXPECT_GE(entries.size(), 2u);
    EXPECT_NE(std::find(entries.begin(), entries.end(), "a.txt"), entries.end());
    EXPECT_NE(std::find(entries.begin(), entries.end(), "b.txt"), entries.end());

    std::filesystem::remove_all(dir);
}

TEST(NativeFileSystem, ListDirectoryNonexistent) {
    NativeFileSystem fs;
    auto r = fs.listDirectory("/tmp/nonexistent_dir_xxxxxx");
    EXPECT_TRUE(r.isErr());
}

// ============================================================
// NativeFileSystem — tempPath
// ============================================================

TEST(NativeFileSystem, TempPath) {
    NativeFileSystem fs;
    auto p = fs.tempPath();
    EXPECT_FALSE(p.empty());
    EXPECT_TRUE(fs.exists(p));
}
