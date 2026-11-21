#include "arbiter/snapshot.hpp"
#include "arbiter/codec.hpp"
#include "snapshot_io.hpp"
#include "arbiter/errors.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <unistd.h>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace arbiter {

namespace {
constexpr char kMagic[4] = {'A', 'R', 'X', 'S'};

bool checkpoint_seq(std::string_view name, u64& seq) {
    if (!name.starts_with("ck_") || !name.ends_with(".snap") || name.size() < 9)
        return false;
    const std::string_view digits = name.substr(3, name.size() - 8);
    if (digits.size() > 1 && digits.front() == '0') return false;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), seq);
    return parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size();
}

}  // namespace

Status write_snapshot(const std::string& path, const SnapshotData& snap) {
    if (const Status st = check_book_invariants(snap.ladder); !st)
        return fail(Errc::SnapshotInvalid, st.context);
    ByteWriter body;
    body.put_u64(snap.config_hash);
    body.put_u64(snap.last_event_seq);
    body.put_i64(snap.logical_t);
    body.put_u64(snap.next_order_id);
    body.put_u64(snap.next_book_seq);
    encode_ladder(body, snap.ladder);
    return snapshot_io::write(path, kMagic, 1, body.data());
}

Result<SnapshotData> read_snapshot(const std::string& path) {
    auto body = snapshot_io::read(path, kMagic, 1, Errc::SnapshotInvalid);
    if (!body) return ErrT<SnapshotData>(body.status.code, body.status.context);
    ByteReader r(body.value);
    SnapshotData snap;
    snap.config_hash = r.get_u64();
    snap.last_event_seq = r.get_u64();
    snap.logical_t = r.get_i64();
    snap.next_order_id = r.get_u64();
    snap.next_book_seq = r.get_u64();
    if (!r.ok()) return ErrT<SnapshotData>(Errc::SnapshotInvalid, 3);
    if (!decode_ladder(r, snap.ladder) || !check_book_invariants(snap.ladder))
        return ErrT<SnapshotData>(Errc::SnapshotInvalid, 4);
    if (!r.ok() || r.remaining() != 0) return ErrT<SnapshotData>(Errc::SnapshotInvalid, 6);
    return OkT(std::move(snap));
}

CheckpointStore::CheckpointStore(std::string dir, size_t keep) : dir_(std::move(dir)), keep_(keep) {}

Status CheckpointStore::save(const SnapshotData& snap) {
    if (::mkdir(dir_.c_str(), 0755) != 0 && errno != EEXIST) return fail(Errc::IoError, errno);
    const std::string path = dir_ + "/ck_" + std::to_string(snap.last_event_seq) + ".snap";
    if (Status s = write_snapshot(path, snap); !s) return s;

    std::error_code ec;
    const auto seqs = available_seqs(ec);
    if (ec) return fail(Errc::IoError, ec.value());
    for (size_t i = keep_; i < seqs.size(); ++i)
        if (::unlink((dir_ + "/ck_" + std::to_string(seqs[i - keep_]) + ".snap").c_str()) != 0 && errno != ENOENT)
            return fail(Errc::IoError, errno);
    return good();
}

std::vector<u64> CheckpointStore::available_seqs() const {
    std::error_code ec;
    auto seqs = available_seqs(ec);
    if (ec) throw std::filesystem::filesystem_error("list checkpoints", dir_, ec);
    return seqs;
}

std::vector<u64> CheckpointStore::available_seqs(std::error_code& ec) const {
    std::vector<u64> seqs;
    std::filesystem::directory_iterator it(dir_, ec), end;
    if (ec == std::errc::no_such_file_or_directory) { ec.clear(); return {}; }
    for (; !ec && it != end; it.increment(ec)) {
        u64 seq;
        if (checkpoint_seq(it->path().filename().string(), seq) && it->is_regular_file(ec))
            seqs.push_back(seq);
        if (ec) break;
    }
    std::sort(seqs.begin(), seqs.end());
    return seqs;
}

Result<SnapshotData> CheckpointStore::load_at_or_before(u64 seq) const {
    std::error_code ec;
    const auto seqs = available_seqs(ec);
    if (ec) return ErrT<SnapshotData>(Errc::IoError, ec.value());
    const auto end = std::upper_bound(seqs.begin(), seqs.end(), seq);
    if (end == seqs.begin()) return ErrT<SnapshotData>(Errc::NotFound, seq);
    const u64 best = *std::prev(end);
    auto snap = read_snapshot(dir_ + "/ck_" + std::to_string(best) + ".snap");
    if (snap && snap.value.last_event_seq != best) return ErrT<SnapshotData>(Errc::SnapshotInvalid, best);
    return snap;
}

}  // namespace arbiter
