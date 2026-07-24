#pragma once

#include <cstdint>
#include <vector>
#include "dedup_types.h"

namespace dedup {

class ReparsePointParser {
public:
    // `payload` is the raw REPARSE_POINT attribute value as stored in the MFT
    // (starts at ReparseTag, matching REPARSE_DATA_BUFFER layout).
    static DedupReparseInfo Parse(const std::vector<uint8_t>& payload);

private:
    // Offsets relative to the start of DataBuffer (i.e. after the 8-byte
    // generic REPARSE_DATA_BUFFER header: Tag + Length + Reserved).
    static constexpr size_t kOffsetOriginalSize        = 0x08; // 8 bytes (uint64_t)
    static constexpr size_t kOffsetChunkStoreGuid      = 0x18; // 16 bytes (GUID)
    static constexpr size_t kOffsetStreamOffset        = 0x40; // 4 bytes (uint32_t offset in Stream container)
    static constexpr size_t kOffsetStreamHeaderIdHash  = 0x58; // 32 bytes (SHA-256 stream header identifier)
    static constexpr size_t kMinDataBufferLen          = kOffsetStreamHeaderIdHash + 32;
};

} // namespace dedup
