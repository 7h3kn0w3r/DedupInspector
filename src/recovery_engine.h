// recovery_engine.h
//
// Orchestrates the full pipeline for one or many DedupFileEntry records:
// resolve each chunk hash to a location, read + decompress it, and write
// the chunks out sequentially to reconstruct the original file.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "dedup_types.h"
#include "chunk_parser.h"

namespace dedup {

class RecoveryEngine {
public:
    explicit RecoveryEngine(const ChunkContainerLocator& locator) : locator_(locator) {}

    // Recovers a single file to `outputFolder`, using entry.fullPath (with
    // path separators sanitized) to preserve directory structure, or just
    // entry.fileName if fullPath is unavailable. Updates entry.status /
    // entry.statusMessage in place.
    bool RecoverFile(DedupFileEntry& entry, const std::wstring& outputFolder);

    // Recovers every entry, reporting aggregate progress via onProgress.
    // Continues past individual failures so one bad file doesn't abort a
    // batch recovery; check each entry's status afterward for results.
    void RecoverAll(std::vector<DedupFileEntry>& entries,
                     const std::wstring& outputFolder,
                     ProgressCallback onProgress);

private:
    std::wstring BuildOutputPath(const DedupFileEntry& entry, const std::wstring& outputFolder) const;

    const ChunkContainerLocator& locator_;
};

} // namespace dedup
