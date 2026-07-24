#include "mft_parser.h"
#include "utils.h"

#include <windows.h>
#include <cstring>

namespace dedup {

namespace {

// $FILE_NAME attribute resident value layout (partial, fields we need).
#pragma pack(push, 1)
struct FileNameAttrValue {
    uint64_t parentRef;         // low 48 bits = record number, high 16 = sequence
    uint64_t creationTime;
    uint64_t modifiedTime;
    uint64_t mftModifiedTime;
    uint64_t accessTime;
    uint64_t allocatedSize;
    uint64_t realSize;
    uint32_t flags;
    uint32_t reparseTag;
    uint8_t  nameLength;         // in UTF-16 code units
    uint8_t  nameNamespace;
    // wchar_t name[nameLength] follows
};

struct StandardInfoAttrValue {
    uint64_t creationTime;
    uint64_t modifiedTime;
    uint64_t mftModifiedTime;
    uint64_t accessTime;
    uint32_t dosFileAttributes;
    // remainder not needed
};
#pragma pack(pop)

uint64_t RecordNumberFromRef(uint64_t fileRef) {
    return fileRef & 0x0000FFFFFFFFFFFFull;
}

} // namespace

void MftParser::ApplyFixup(uint8_t* recordData, uint32_t recordSize) {
    // NTFS "Update Sequence Array" fixup: the last two bytes of every 512
    // byte sector in the record are replaced with a check value, and the
    // real bytes are stashed in the fixup array at the front of the
    // record. We must restore them before trusting any attribute data
    // near a sector boundary.
    auto* header = reinterpret_cast<MftRecordHeader*>(recordData);
    if (header->updateSeqOffset == 0 || header->updateSeqSize == 0) return;

    const uint16_t* usa = reinterpret_cast<const uint16_t*>(recordData + header->updateSeqOffset);
    uint16_t usaValue = usa[0];
    uint32_t sectors = header->updateSeqSize - 1;

    for (uint32_t i = 0; i < sectors; ++i) {
        uint32_t sectorEndOffset = (i + 1) * 512 - 2;
        if (sectorEndOffset + 2 > recordSize) break;

        uint16_t* sectorEnd = reinterpret_cast<uint16_t*>(recordData + sectorEndOffset);
        // The last 2 bytes of the sector should equal the USA "check" value;
        // if not, the record is corrupt/torn - we still proceed best-effort.
        *sectorEnd = usa[i + 1];
        (void)usaValue;
    }
}

bool MftParser::ParseRecord(const uint8_t* recordData, uint32_t recordSize, uint64_t recordNumber) {
    if (recordSize < sizeof(MftRecordHeader)) return false;
    if (std::memcmp(recordData, "FILE", 4) != 0) return false; // not a valid/allocated record

    // Apply fixup on a mutable copy.
    std::vector<uint8_t> buf(recordData, recordData + recordSize);
    ApplyFixup(buf.data(), recordSize);

    const auto* header = reinterpret_cast<const MftRecordHeader*>(buf.data());
    bool isDirectory = (header->flags & 0x0002) != 0;

    MftFileRecord record;
    record.recordNumber = recordNumber;
    record.isDirectory = isDirectory;

    uint32_t offset = header->firstAttrOffset;
    while (offset + sizeof(AttributeHeader) <= recordSize) {
        const auto* attr = reinterpret_cast<const AttributeHeader*>(buf.data() + offset);
        if (attr->type == ATTR_END_MARKER) break;
        if (attr->length == 0 || offset + attr->length > recordSize) break; // guard against corruption

        if (attr->nonResident == 0) {
            const auto* res = reinterpret_cast<const ResidentAttrHeader*>(
                buf.data() + offset + sizeof(AttributeHeader));
            const uint8_t* valuePtr = buf.data() + offset + res->valueOffset;
            uint32_t valueLen = res->valueLength;

            if (attr->type == ATTR_FILE_NAME && valueLen >= sizeof(FileNameAttrValue)) {
                const auto* fn = reinterpret_cast<const FileNameAttrValue*>(valuePtr);
                uint32_t nameBytes = static_cast<uint32_t>(fn->nameLength) * 2;
                if (sizeof(FileNameAttrValue) + nameBytes <= valueLen) {
                    const wchar_t* namePtr = reinterpret_cast<const wchar_t*>(valuePtr + sizeof(FileNameAttrValue));
                    // Prefer the Win32 (or Win32+DOS / POSIX) namespace name over the
                    // short 8.3 DOS alias, matching how Explorer would display it.
                    if (record.fileName.empty() || fn->nameNamespace != 2 /* DOS-only */) {
                        record.fileName.assign(namePtr, fn->nameLength);
                        record.parentRecordNumber = RecordNumberFromRef(fn->parentRef);
                    }
                    record.logicalFileSize = fn->realSize;
                }
            } else if (attr->type == ATTR_STANDARD_INFORMATION && valueLen >= sizeof(StandardInfoAttrValue)) {
                // Reserved for future use (timestamps); size comes from FILE_NAME.
            } else if (attr->type == ATTR_REPARSE_POINT) {
                record.hasReparsePoint = true;
                record.reparseData.assign(valuePtr, valuePtr + valueLen);
            }
        }
        // Non-resident REPARSE_POINT / FILE_NAME essentially never happens in
        // practice (they are small, always resident), so non-resident
        // attributes of interest are intentionally skipped here.

        offset += attr->length;
    }

    if (!record.fileName.empty()) {
        nameIndex_[recordNumber] = {record.fileName, record.parentRecordNumber};
    }

    if (record.hasReparsePoint && !record.fileName.empty()) {
        reparseRecords_.push_back(std::move(record));
    }

    return true;
}

bool MftParser::Parse(const std::wstring& mftPath, uint32_t recordSize, ProgressCallback onProgress) {
    reparseRecords_.clear();
    nameIndex_.clear();
    lastError_.clear();

    HANDLE h = util::OpenForRead(mftPath);
    if (h == INVALID_HANDLE_VALUE) {
        lastError_ = L"Unable to open MFT file.";
        return false;
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(h, &fileSize)) {
        CloseHandle(h);
        lastError_ = L"Unable to determine MFT file size.";
        return false;
    }

    if (recordSize == 0) {
        // Peek the first record's allocatedSize to auto-detect (usually 1024).
        uint8_t probe[64]{};
        DWORD read = 0;
        if (ReadFile(h, probe, sizeof(probe), &read, nullptr) && read >= sizeof(MftRecordHeader)) {
            auto* hdr = reinterpret_cast<MftRecordHeader*>(probe);
            if (std::memcmp(hdr->signature, "FILE", 4) == 0 && hdr->allocatedSize > 0) {
                recordSize = hdr->allocatedSize;
            }
        }
        if (recordSize == 0) recordSize = 1024; // sane default
        SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    }

    uint64_t totalRecords = static_cast<uint64_t>(fileSize.QuadPart) / recordSize;
    std::vector<uint8_t> buffer(recordSize);
    uint64_t recordNumber = 0;

    while (true) {
        DWORD read = 0;
        BOOL ok = ReadFile(h, buffer.data(), recordSize, &read, nullptr);
        if (!ok || read == 0) break;
        if (read < recordSize) break; // trailing partial record, stop

        ParseRecord(buffer.data(), recordSize, recordNumber);

        if (onProgress && (recordNumber % 4096 == 0)) {
            onProgress(recordNumber, totalRecords, L"Scanning $MFT records...");
        }
        ++recordNumber;
    }

    CloseHandle(h);

    if (onProgress) {
        onProgress(totalRecords, totalRecords, L"MFT scan complete.");
    }

    return true;
}

std::wstring MftParser::ResolvePath(const MftFileRecord& record) const {
    std::vector<std::wstring> parts;
    parts.push_back(record.fileName);

    uint64_t parent = record.parentRecordNumber;
    int guard = 0; // avoid infinite loops on cyclic/corrupt data
    while (guard++ < 64) {
        auto it = nameIndex_.find(parent);
        if (it == nameIndex_.end()) break;
        const auto& [name, grandparent] = it->second;
        if (name.empty() || name == L".") break;
        parts.push_back(name);
        if (grandparent == parent) break; // root points to itself
        parent = grandparent;
    }

    std::wstring path;
    for (auto rit = parts.rbegin(); rit != parts.rend(); ++rit) {
        if (!path.empty()) path += L"\\";
        path += *rit;
    }
    return path;
}

} // namespace dedup
