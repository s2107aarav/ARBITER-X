#pragma once

#include "arbiter/codec.hpp"
#include "arbiter/crc32.hpp"

#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

namespace arbiter::snapshot_io {

inline Status write(const std::string& path, const char (&magic)[4], u32 version, const std::string& body) {
    if (body.size() > std::numeric_limits<u32>::max()) return fail(Errc::Capacity, body.size());
    ByteWriter header;
    header.put_bytes(magic, 4);
    header.put_u32(version);
    header.put_u32(static_cast<u32>(body.size()));
    header.put_u32(crc32(body.data(), body.size()));

    std::error_code ec;
    const auto target = std::filesystem::weakly_canonical(path, ec);
    if (ec) return fail(Errc::IoError, ec.value());
    const auto status = std::filesystem::status(target, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) return fail(Errc::IoError, ec.value());
    if (std::filesystem::exists(status) && !std::filesystem::is_regular_file(status)) return fail(Errc::IoError);
    std::string temp = target.string() + ".XXXXXX";
    const int fd = ::mkstemp(temp.data());
    if (fd == -1) return fail(Errc::IoError, errno);
    FILE* file = ::fdopen(fd, "wb");
    bool complete = false;
    if (file) {
        const bool written = std::fwrite(header.data().data(), 1, header.size(), file) == header.size() &&
                             std::fwrite(body.data(), 1, body.size(), file) == body.size();
        complete = std::fclose(file) == 0 && written;
    } else {
        ::close(fd);
    }
    if (complete && std::filesystem::exists(status))
        complete = ::chmod(temp.c_str(), static_cast<mode_t>(status.permissions()) & 0777) == 0;
    if (complete && ::rename(temp.c_str(), target.c_str()) == 0) return good();
    const int error = errno;
    ::unlink(temp.c_str());
    return fail(Errc::IoError, error);
}

inline Result<std::string> read(const std::string& path, const char (&magic)[4], u32 version, Errc version_error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return ErrT<std::string>(errno == ENOENT ? Errc::NotFound : Errc::IoError, errno);
    const auto size = file.tellg();
    u8 header[16];
    if (!file.seekg(0) || !file.read(reinterpret_cast<char*>(header), sizeof header) ||
        std::memcmp(header, magic, 4) != 0) return ErrT<std::string>(Errc::SnapshotInvalid);
    ByteReader r(header + 4, 12);
    const u32 stored_version = r.get_u32(), length = r.get_u32(), checksum = r.get_u32();
    if (stored_version != version) return ErrT<std::string>(version_error, stored_version);
    if (size != static_cast<std::streamoff>(sizeof header) + length) return ErrT<std::string>(Errc::SnapshotInvalid, 2);
    std::string body(length, '\0');
    if (!file.read(body.data(), length)) return ErrT<std::string>(Errc::IoError);
    if (crc32(body.data(), length) != checksum) return ErrT<std::string>(Errc::ChecksumMismatch);
    return OkT(std::move(body));
}

}  // namespace arbiter::snapshot_io
