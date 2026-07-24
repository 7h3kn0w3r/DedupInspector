#include "reparse_parser.h"
#include "utils.h"

#include <windows.h>
#include <cstring>

namespace dedup {

#pragma pack(push, 1)
struct ReparseDataBufferHeader {
    uint32_t reparseTag;
    uint16_t reparseDataLength;
    uint16_t reserved;
};
#pragma pack(pop)

DedupReparseInfo ReparsePointParser::Parse(const std::vector<uint8_t>& payload) {
    DedupReparseInfo info;

    if (payload.size() < sizeof(ReparseDataBufferHeader)) {
        return info;
    }

    const auto* header = reinterpret_cast<const ReparseDataBufferHeader*>(payload.data());
    info.reparseTag = header->reparseTag;

    if (header->reparseTag != IO_REPARSE_TAG_DEDUP) {
        return info;
    }

    const uint8_t* dataBuffer = payload.data() + sizeof(ReparseDataBufferHeader);
    size_t dataBufferLen = payload.size() - sizeof(ReparseDataBufferHeader);

    if (dataBufferLen < kMinDataBufferLen) {
        return info;
    }

    uint64_t originalSize = 0;
    std::memcpy(&originalSize, dataBuffer + kOffsetOriginalSize, sizeof(originalSize));

    uint32_t streamOffset = 0;
    std::memcpy(&streamOffset, dataBuffer + kOffsetStreamOffset, sizeof(streamOffset));

    GUID guid{};
    std::memcpy(&guid, dataBuffer + kOffsetChunkStoreGuid, sizeof(GUID));

    std::memcpy(info.streamHeaderIdHash.data(), dataBuffer + kOffsetStreamHeaderIdHash, 32);

    info.originalFileSize = originalSize;
    info.streamOffset = streamOffset;
    info.chunkStoreGuid = util::FormatGuid(guid);
    info.valid = true;

    return info;
}

} // namespace dedup
