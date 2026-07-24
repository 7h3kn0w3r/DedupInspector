#include "ckhr_parser.h"

#include <cstring>
#include <algorithm>

namespace dedup {

std::vector<ChunkHashRecord> CkhrParser::ParseAtOffset(const uint8_t* streamData,
                                                        size_t streamDataSize,
                                                        uint32_t streamOffset) {
    std::vector<ChunkHashRecord> records;

    if (streamOffset + 0x10 > streamDataSize) {
        return records;
    }

    uint32_t magic = 0;
    std::memcpy(&magic, streamData + streamOffset, 4);
    if (magic != kCkhrMagic && magic != kCkhrMagicUpper) {
        return records;
    }

    uint32_t chunkDataLen = 0;
    std::memcpy(&chunkDataLen, streamData + streamOffset + 0x0C, 4);

    uint32_t numChunks = 0;
    if (chunkDataLen >= 8) {
        numChunks = (chunkDataLen - 8) / 0x40;
    } else {
        numChunks = chunkDataLen / 0x40;
    }

    for (uint32_t i = 0; i < numChunks; ++i) {
        size_t entryOffset = streamOffset + 0x70 + (static_cast<size_t>(i) * 0x40);
        if (entryOffset + 0x40 > streamDataSize) {
            break;
        }

        const uint8_t* p = streamData + entryOffset;

        uint64_t chunkLoc = 0;
        std::memcpy(&chunkLoc, p + 8, 8);
        uint64_t chunkOffsetInContainer = chunkLoc & 0xFFFFFFFFull;

        uint32_t compLen = 0;
        std::memcpy(&compLen, p + 16, 4);

        if (chunkOffsetInContainer == 0 || compLen == 0) {
            break;
        }

        ChunkHashRecord rec;
        std::memcpy(rec.hash.data(), p + 24, 32);
        rec.chunkOffsetInContainer = chunkOffsetInContainer;
        rec.chunkLengthCompressed = compLen;
        rec.chunkLengthUncompressed = compLen;
        rec.isCompressed = true;

        records.push_back(rec);
    }

    return records;
}

} // namespace dedup
