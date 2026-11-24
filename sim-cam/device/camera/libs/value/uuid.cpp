#include "value/uuid.h"
#include "generated/common_generated.h"
#include <cstring>
#include <random>

namespace camera {

UuidV7 generate_uuid_v7() {
    // Use getrandom on Linux, BCryptGenRandom / RtlGenRandom on Windows
    UuidV7 id{};

#ifdef _WIN32
    // Windows: use BCryptGenRandom from bcrypt.dll (modern)
    HMODULE bcrypt = LoadLibraryA("bcrypt.dll");
    if (bcrypt) {
        using Fn = NTSTATUS(WINAPI *)(BCRYPT_ALG_HANDLE, UCHAR *, ULONG, ULONG);
        auto fn = reinterpret_cast<Fn>(GetProcAddress(bcrypt, "BCryptGenRandom"));
        if (fn) {
            BCRYPT_ALG_HANDLE alg{};
            NTSTATUS status = fn(&alg, reinterpret_cast<UCHAR *>(&id), sizeof(id), 0);
            if (status == 0) {
                FreeLibrary(bcrypt);
                // Embed millisecond timestamp in top 48 bits of hi
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                id.hi = (ms & 0xFFFFFFFFFFFF) | (id.hi & 0xFFFFFFFF);
                return id;
            }
        }
        FreeLibrary(bcrypt);
    }
#endif

    // Fallback: std::random_device + system_clock
    static thread_local std::mt19937_64 rng(std::random_device{}());
    id.hi = rng();
    id.lo = rng();

    // Embed millisecond timestamp in top 48 bits of hi (UUIDv7 format)
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    id.hi = (ms & 0xFFFFFFFFFFFF) | (id.hi & 0xFFFFFFFF);

    return id;
}

camera::v1::Uuid to_fb_uuid(UuidV7 id) {
    // FlatBuffers struct: constructed directly, no setters
    return camera::v1::Uuid(id.hi, id.lo);
}

std::optional<UuidV7> parse_uuid_v7(const uint8_t bytes[16]) {
    if (!bytes)
        return std::nullopt;
    UuidV7 id{};
    // Big-endian to host (hi = bytes 0-7, lo = bytes 8-15)
    std::memcpy(&id.hi, bytes, 8);
    std::memcpy(&id.lo, bytes + 8, 8);
    return id;
}

} // namespace camera
