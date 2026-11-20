#include "arbiter/journal.hpp"
#ifdef ARBITER_ENABLE_JOURNAL_TEST_HOOKS
#include "journal_test_hooks.hpp"
#endif
#include "arbiter/codec.hpp"
#include "arbiter/crc32.hpp"
#include "arbiter/errors.hpp"

#include <cerrno>
#include <cstring>
#include <limits>
#include <unistd.h>

namespace arbiter {

namespace {
constexpr char kMagic[4] = {'A', 'R', 'X', 'J'};
constexpr size_t kFileHeaderSize = 24;  // magic 4 + version 4 + seed 8 + cfg 8
}  // namespace

// ---------------- Writer ----------------

#ifdef ARBITER_ENABLE_JOURNAL_TEST_HOOKS
namespace journal_test {
namespace { FailureHook failure_hook = nullptr; }
void set_failure_hook_for_tests(FailureHook hook) noexcept { failure_hook = hook; }
}
#endif

Result<JournalWriter> JournalWriter::create(const std::string& path, u64 master_seed, u64 config_hash, bool exclusive) {
    JournalWriter w;
    w.f_ = std::fopen(path.c_str(), exclusive ? "wbx" : "wb");
    if (!w.f_) return ErrT<JournalWriter>(Errc::IoError, static_cast<u64>(errno));
    ByteWriter hw;
    hw.put_bytes(kMagic, 4);
    hw.put_u32(1);
    hw.put_u64(master_seed);
    hw.put_u64(config_hash);
    if (std::fwrite(hw.data().data(), 1, hw.size(), w.f_) != hw.size()) {
        std::fclose(w.f_);
        w.f_ = nullptr;
        return ErrT<JournalWriter>(Errc::IoError, 1);
    }
    w.buf_.reserve(1u << 16);
    return OkT(std::move(w));
}

JournalWriter::~JournalWriter() { close(); }

JournalWriter::JournalWriter(JournalWriter&& o) noexcept { *this = std::move(o); }

JournalWriter& JournalWriter::operator=(JournalWriter&& o) noexcept {
    if (this != &o) {
        close();
        f_ = o.f_;
        o.f_ = nullptr;
        buf_ = std::move(o.buf_);
        records_ = o.records_;
        next_seq_ = o.next_seq_;
    }
    return *this;
}

Status JournalWriter::append(const Event& e) {
    if (!f_) return fail(Errc::IoError, 0);
#ifdef ARBITER_ENABLE_JOURNAL_TEST_HOOKS
    if (journal_test::failure_hook) {
        if (const Status injected = journal_test::failure_hook(journal_test::JournalFailurePoint::Append); !injected)
            return injected;
    }
#endif
    if (next_seq_ == 0) return fail(Errc::Capacity, next_seq_);
    Event with_seq = e;
    with_seq.seq = next_seq_;  // journal owns sequence numbering
    ByteWriter payload;
    encode_event(payload, with_seq);
    const std::string& pl = payload.data();
    ByteWriter frame;
    frame.put_u32(static_cast<u32>(pl.size()));
    frame.put_u32(crc32(pl.data(), pl.size()));
    frame.put_bytes(pl.data(), pl.size());
    const std::string& fr = frame.data();
    buf_.insert(buf_.end(), fr.begin(), fr.end());
    ++next_seq_;
    ++records_;
    if (buf_.size() >= (1u << 16)) return flush();
    return good();
}

Status JournalWriter::flush() {
    if (!f_) return fail(Errc::IoError, 0);
    const bool written = buf_.empty() || std::fwrite(buf_.data(), 1, buf_.size(), f_) == buf_.size();
    buf_.clear();
    if (written && std::fflush(f_) == 0) return good();
    const int error = errno;
    std::fclose(f_);
    f_ = nullptr;
    return fail(Errc::IoError, error);
}

Status JournalWriter::sync() {
#ifdef ARBITER_ENABLE_JOURNAL_TEST_HOOKS
    if (journal_test::failure_hook) {
        if (const Status injected = journal_test::failure_hook(journal_test::JournalFailurePoint::Sync); !injected)
            return injected;
    }
#endif
    if (Status s = flush(); !s) return s;
    if (::fsync(::fileno(f_)) != 0) return fail(Errc::IoError, 4);
    return good();
}

void JournalWriter::close() noexcept {
    if (!f_) return;
    (void)flush();
    if (f_) std::fclose(f_);
    f_ = nullptr;
}

// ---------------- Reader ----------------

JournalReader::~JournalReader() {
    if (f_) std::fclose(f_);
}

JournalReader::JournalReader(JournalReader&& o) noexcept { *this = std::move(o); }

JournalReader& JournalReader::operator=(JournalReader&& o) noexcept {
    if (this != &o) {
        if (f_) std::fclose(f_);
        f_ = o.f_;
        o.f_ = nullptr;
        hdr_ = o.hdr_;
        pos_ = o.pos_;
        records_ = o.records_;
        last_seq_ = o.last_seq_;
        eof_ = o.eof_;
        failed_ = o.failed_;
        peeked_ = o.peeked_;
        peeked_len_ = o.peeked_len_;
        error_ = o.error_;
    }
    return *this;
}

Result<JournalReader> JournalReader::open(const std::string& path) {
    JournalReader r;
    r.f_ = std::fopen(path.c_str(), "rb");
    if (!r.f_) return ErrT<JournalReader>(errno == ENOENT ? Errc::NotFound : Errc::IoError, errno);
    const auto bad_header = [&](u64 offset) {
        return ErrT<JournalReader>(std::ferror(r.f_) ? Errc::IoError : Errc::JournalCorrupt, offset);
    };
    char magic[4];
    if (std::fread(magic, 1, 4, r.f_) != 4 || std::memcmp(magic, kMagic, 4) != 0) return bad_header(0);
    u32 version = 0;
    if (std::fread(&version, 1, 4, r.f_) != 4 || version != 1) return bad_header(4);
    if (std::fread(&r.hdr_.master_seed, 1, 8, r.f_) != 8) return bad_header(8);
    if (std::fread(&r.hdr_.config_hash, 1, 8, r.f_) != 8) return bad_header(16);
    r.hdr_.version = version;
    r.pos_ = kFileHeaderSize;
    return OkT(std::move(r));
}

bool JournalReader::has_next() {
    if (eof_ || failed_) return false;
    if (!f_) {
        failed_ = true;
        error_ = fail(Errc::IoError, pos_);
        return false;
    }
    if (peeked_) return true;
    u32 len = 0;
    const size_t got = std::fread(&len, 1, 4, f_);
    if (got == 0) {
        if (std::ferror(f_)) {
            failed_ = true;
            error_ = fail(Errc::IoError, pos_);
            return false;
        }
        eof_ = true;  // clean EOF: the boundary is a valid stopping point
        return false;
    }
    if (got != 4) {
        failed_ = true;
        error_ = fail(std::ferror(f_) ? Errc::IoError : Errc::JournalCorrupt, pos_);
        return false;
    }
    peeked_len_ = len;
    peeked_ = true;
    return true;
}

Result<Event> JournalReader::next() {
    if (!has_next()) return failed_ ? ErrT<Event>(error_.code, error_.context)
                                    : ErrT<Event>(Errc::NotFound, pos_);
    peeked_ = false;
    const auto reject = [&](Errc code) {
        failed_ = true;
        error_ = fail(code, pos_);
        return ErrT<Event>(code, pos_);
    };
    u32 crc;
    u8 payload[kEncodedEventSize];
    if (peeked_len_ != sizeof payload) return reject(Errc::JournalCorrupt);
    if (std::fread(&crc, 1, sizeof crc, f_) != sizeof crc ||
        std::fread(payload, 1, sizeof payload, f_) != sizeof payload)
        return reject(std::ferror(f_) ? Errc::IoError : Errc::JournalCorrupt);
    if (crc32(payload, sizeof payload) != crc) return reject(Errc::ChecksumMismatch);
    auto ev = decode_event(payload, sizeof payload);
    if (!ev) return reject(Errc::ProtocolInvalid);
    if (last_seq_ == std::numeric_limits<u64>::max() || ev.value.seq != last_seq_ + 1)
        return reject(Errc::SequenceGap);
    pos_ += 8 + sizeof payload;
    last_seq_ = ev.value.seq;
    ++records_;
    return OkT(ev.value);
}

// ---------------- verify / recover ----------------

JournalVerifyReport journal_verify(const std::string& path) {
    auto rd = JournalReader::open(path);
    if (!rd) return {0, false, rd.status, rd.status.context, 0};
    JournalReader& r = rd.value;
    while (r.has_next() && r.next()) {}
    return {r.records_read(), !r.failed(), r.error(), r.error().context, r.records_read()};
}

Result<u64> journal_recover(const std::string& path) {
    auto rd = JournalReader::open(path);
    if (!rd) return ErrT<u64>(rd.status.code, rd.status.context);
    JournalReader& r = rd.value;
    while (r.has_next() && r.next()) {}
    if (r.failed()) {
        if (r.error().code == Errc::IoError) return ErrT<u64>(Errc::IoError, r.error().context);
        if (::truncate(path.c_str(), static_cast<off_t>(r.offset())) != 0)
            return ErrT<u64>(Errc::IoError, static_cast<u64>(errno));
    }
    return OkT(r.records_read());
}

}  // namespace arbiter
