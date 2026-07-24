#include "chunk_parser.h"
#include "utils.h"

#include <windows.h>
#include <compressapi.h>
#include <algorithm>

#pragma comment(lib, "Cabinet.lib")

namespace dedup {

bool ChunkContainerLocator::Open(const std::wstring& containerPath) {
    containerPath_ = containerPath;
    lastError_.clear();

    HANDLE h = util::OpenForRead(containerPath_);
    if (h == INVALID_HANDLE_VALUE) {
        lastError_ = L"Failed to open chunk container: " + containerPath_;
        return false;
    }
    CloseHandle(h);
    return true;
}

std::vector<uint8_t> ChunkContainerLocator::DecompressChunk(const std::vector<uint8_t>& compressed,
                                                              uint32_t maxOutLen) const {
    std::vector<uint8_t> output(maxOutLen);

    DECOMPRESSOR_HANDLE decompressor = nullptr;
    if (CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor)) {
        SIZE_T decompressedSize = 0;
        BOOL ok = Decompress(
            decompressor,
            const_cast<PVOID>(static_cast<const void*>(compressed.data())),
            compressed.size(),
            output.data(),
            output.size(),
            &decompressedSize);

        CloseDecompressor(decompressor);

        if (ok && decompressedSize > 0) {
            output.resize(decompressedSize);
            return output;
        }
    }

    // Fallback to COMPRESS_ALGORITHM_XPRESS
    decompressor = nullptr;
    if (CreateDecompressor(COMPRESS_ALGORITHM_XPRESS, nullptr, &decompressor)) {
        SIZE_T decompressedSize = 0;
        BOOL ok = Decompress(
            decompressor,
            const_cast<PVOID>(static_cast<const void*>(compressed.data())),
            compressed.size(),
            output.data(),
            output.size(),
            &decompressedSize);

        CloseDecompressor(decompressor);

        if (ok && decompressedSize > 0) {
            output.resize(decompressedSize);
            return output;
        }
    }

    // If decompression fails or data is uncompressed, return raw payload
    return compressed;
}

std::optional<std::vector<uint8_t>> ChunkContainerLocator::ReadChunk(const ChunkHashRecord& rec) const {
    HANDLE h = util::OpenForRead(containerPath_);
    if (h == INVALID_HANDLE_VALUE) {
        lastError_ = L"Failed to open chunk container: " + containerPath_;
        return std::nullopt;
    }

    // Inside container file (.ccc), chunk header is 0x58 (88) bytes; payload starts after it
    uint64_t payloadOffset = rec.chunkOffsetInContainer + 0x58;
    uint32_t payloadLength = rec.chunkLengthCompressed;

    if (payloadLength == 0) {
        CloseHandle(h);
        return std::nullopt;
    }

    std::vector<uint8_t> raw(payloadLength);
    bool ok = util::ReadAt(h, payloadOffset, raw.data(), payloadLength);
    CloseHandle(h);

    if (!ok) {
        lastError_ = L"Failed to read chunk bytes at offset " + std::to_wstring(payloadOffset);
        return std::nullopt;
    }

    return DecompressChunk(raw, std::max<uint32_t>(262144, payloadLength * 4));
}

} // namespace dedup