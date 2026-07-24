#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "dedup_types.h"

namespace dedup {

class ChunkContainerLocator {
public:
    // Opens a single chunk container file (.ccc) directly.
    bool Open(const std::wstring& containerPath);

    // Reads the chunk payload at chunkOffsetInContainer + 0x58 (skipping container header)
    // and decompresses it via XPRESS_HUFF / XPRESS.
    std::optional<std::vector<uint8_t>> ReadChunk(const ChunkHashRecord& rec) const;

    const std::wstring& LastError() const { return lastError_; }

private:
    std::vector<uint8_t> DecompressChunk(const std::vector<uint8_t>& compressed,
                                          uint32_t maxOutLen = 262144) const;

    std::wstring containerPath_;
    mutable std::wstring lastError_;
};

} // namespace dedup