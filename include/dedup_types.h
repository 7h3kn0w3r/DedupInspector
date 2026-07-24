#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace dedup {

// SHA-256 digest identifying a chunk or stream header
using ChunkHash = std::array<uint8_t, 32>;

// A single MFT file record of interest
struct MftFileRecord {
    uint64_t recordNumber = 0;
    uint64_t parentRecordNumber = 0;
    std::wstring fileName;
    bool isDirectory = false;
    bool hasReparsePoint = false;
    std::vector<uint8_t> reparseData;
    uint64_t logicalFileSize = 0;
};

// Parsed contents of an IO_REPARSE_TAG_DEDUP reparse point
struct DedupReparseInfo {
    bool valid = false;
    uint32_t reparseTag = 0;
    uint64_t originalFileSize = 0;
    uint32_t streamOffset = 0;                  // Offset into Stream container (.ccc) where Ckhr header starts
    ChunkHash streamHeaderIdHash{};             // 32-byte Stream Header Identifier hash
    std::wstring chunkStoreGuid;               // GUID string of owning chunk store, e.g. "{6F89FF76-...}"
};

// One chunk hash entry parsed from the Stream file's Ckhr/Smap block
struct ChunkHashRecord {
    ChunkHash hash{};
    uint64_t chunkOffsetInContainer = 0;       // Absolute byte offset in Data container file
    uint32_t chunkLengthCompressed = 0;         // Payload byte length in container
    uint32_t chunkLengthUncompressed = 0;       // Uncompressed byte length
    uint32_t containerIndex = 0;
    bool isCompressed = false;
};

// Result of parsing stream file metadata for a file
struct StreamRecord {
    uint32_t streamOffset = 0;
    uint64_t originalFileSize = 0;
    std::vector<ChunkHashRecord> chunks;
};

// Aggregate, UI-facing record describing one deduplicated file
struct DedupFileEntry {
    enum class RecoveryStatus {
        NotAttempted,
        Ready,
        PartiallyReady,
        Recovered,
        Failed
    };

    std::wstring fileName;
    std::wstring fullPath;
    uint64_t originalFileSize = 0;
    uint32_t streamOffset = 0;
    std::wstring chunkStoreGuid;
    std::vector<ChunkHashRecord> chunks;
    RecoveryStatus status = RecoveryStatus::NotAttempted;
    std::wstring statusMessage;
};

// Progress callback used by long running scans / recoveries
using ProgressCallback = std::function<void(uint64_t current, uint64_t total, const std::wstring& message)>;

} // namespace dedup
