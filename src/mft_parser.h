// mft_parser.h
//
// Minimal NTFS $MFT reader. Parses exported $MFT files (raw, contiguous
// dump of the MFT, as produced by tools like FTK Imager / RawCopy / KAPE)
// and extracts the subset of attributes we care about:
//   0x10 $STANDARD_INFORMATION (size hint)
//   0x30 $FILE_NAME            (name, parent record for path building)
//   0xC0 $REPARSE_POINT        (raw payload, handed to reparse_parser)
//
// This is intentionally NOT a full NTFS driver: no $DATA runlist parsing,
// no non-resident attribute followers beyond REPARSE_POINT (which is
// always resident in practice), no $ATTRIBUTE_LIST support for records
// that overflow into extension records. Good enough for locating
// deduplicated files and their reparse metadata.

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "dedup_types.h"

namespace dedup {

#pragma pack(push, 1)

struct MftRecordHeader {
    char     signature[4];      // "FILE"
    uint16_t updateSeqOffset;
    uint16_t updateSeqSize;
    uint64_t logFileSeqNumber;
    uint16_t sequenceNumber;
    uint16_t hardLinkCount;
    uint16_t firstAttrOffset;
    uint16_t flags;             // bit0 = in use, bit1 = directory
    uint32_t usedSize;
    uint32_t allocatedSize;
    uint64_t baseRecordRef;     // for extension records
    uint16_t nextAttrId;
};

struct AttributeHeader {
    uint32_t type;              // attribute type code, 0xFFFFFFFF = end marker
    uint32_t length;
    uint8_t  nonResident;
    uint8_t  nameLength;
    uint16_t nameOffset;
    uint16_t flags;
    uint16_t attrId;
};

// Resident-attribute specific fields (follow AttributeHeader when nonResident==0)
struct ResidentAttrHeader {
    uint32_t valueLength;
    uint16_t valueOffset;
    uint8_t  indexedFlag;
    uint8_t  padding;
};

#pragma pack(pop)

constexpr uint32_t ATTR_STANDARD_INFORMATION = 0x10;
constexpr uint32_t ATTR_FILE_NAME            = 0x30;
constexpr uint32_t ATTR_DATA                 = 0x80;
constexpr uint32_t ATTR_REPARSE_POINT        = 0xC0;
constexpr uint32_t ATTR_END_MARKER           = 0xFFFFFFFF;

class MftParser {
public:
    // Parses the given exported $MFT file. `recordSize` is normally 1024
    // bytes on modern NTFS volumes; pass 0 to auto-detect from the first
    // record header via allocatedSize.
    bool Parse(const std::wstring& mftPath, uint32_t recordSize, ProgressCallback onProgress);

    // All records that carried a REPARSE_POINT attribute.
    const std::vector<MftFileRecord>& ReparseRecords() const { return reparseRecords_; }

    // Attempts to build a best-effort full path for a record by walking
    // parent record numbers. Falls back to just the filename if the chain
    // can't be resolved (e.g. parent record missing from the export).
    std::wstring ResolvePath(const MftFileRecord& record) const;

    const std::wstring& LastError() const { return lastError_; }

private:
    bool ParseRecord(const uint8_t* recordData, uint32_t recordSize, uint64_t recordNumber);
    void ApplyFixup(uint8_t* recordData, uint32_t recordSize);

    std::vector<MftFileRecord> reparseRecords_;
    // recordNumber -> (fileName, parentRecordNumber) for path reconstruction
    std::unordered_map<uint64_t, std::pair<std::wstring, uint64_t>> nameIndex_;
    std::wstring lastError_;
};

} // namespace dedup
