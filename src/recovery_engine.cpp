#include "recovery_engine.h"
#include "utils.h"

#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <algorithm>

namespace dedup {

std::wstring RecoveryEngine::BuildOutputPath(const DedupFileEntry& entry, const std::wstring& outputFolder) const {
    std::wstring relative = !entry.fullPath.empty() ? entry.fullPath : entry.fileName;

    // Strip drive letter / leading separators
    while (!relative.empty() && (relative.front() == L'\\' || relative.front() == L'/')) {
        relative.erase(0, 1);
    }
    size_t colon = relative.find(L':');
    if (colon != std::wstring::npos) {
        relative = relative.substr(colon + 1);
        while (!relative.empty() && (relative.front() == L'\\' || relative.front() == L'/')) {
            relative.erase(0, 1);
        }
    }

    std::wstring fullOut = util::PathJoin(outputFolder, relative);

    // Ensure directory tree exists
    std::wstring dir = fullOut;
    size_t lastSep = dir.find_last_of(L'\\');
    if (lastSep != std::wstring::npos) {
        dir = dir.substr(0, lastSep);
        SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    }

    return fullOut;
}

bool RecoveryEngine::RecoverFile(DedupFileEntry& entry, const std::wstring& outputFolder) {
    if (entry.chunks.empty()) {
        entry.status = DedupFileEntry::RecoveryStatus::Failed;
        entry.statusMessage = L"No chunk records available for this file.";
        return false;
    }

    std::wstring outPath = BuildOutputPath(entry, outputFolder);

    HANDLE outFile = CreateFileW(
        outPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (outFile == INVALID_HANDLE_VALUE) {
        entry.status = DedupFileEntry::RecoveryStatus::Failed;
        entry.statusMessage = L"Failed to create output file: " + outPath;
        return false;
    }

    uint64_t writtenBytes = 0;
    uint32_t missingChunks = 0;
    uint64_t targetSize = entry.originalFileSize;

    for (const auto& chunk : entry.chunks) {
        if (writtenBytes >= targetSize) {
            break;
        }

        auto data = locator_.ReadChunk(chunk);
        if (!data) {
            ++missingChunks;
            uint64_t padSize = chunk.chunkLengthCompressed > 0 ? chunk.chunkLengthCompressed : 65536;
            if (writtenBytes + padSize > targetSize) {
                padSize = targetSize - writtenBytes;
            }
            std::vector<uint8_t> zeros(static_cast<size_t>(padSize), 0);
            DWORD written = 0;
            WriteFile(outFile, zeros.data(), static_cast<DWORD>(zeros.size()), &written, nullptr);
            writtenBytes += written;
            continue;
        }

        uint64_t bytesToWrite = data->size();
        if (writtenBytes + bytesToWrite > targetSize) {
            bytesToWrite = targetSize - writtenBytes;
        }

        DWORD written = 0;
        WriteFile(outFile, data->data(), static_cast<DWORD>(bytesToWrite), &written, nullptr);
        writtenBytes += written;
    }

    CloseHandle(outFile);

    if (missingChunks == 0 && writtenBytes == targetSize) {
        entry.status = DedupFileEntry::RecoveryStatus::Recovered;
        entry.statusMessage = L"Recovered " + util::FormatSize(writtenBytes) + L" -> " + outPath;
        return true;
    } else if (writtenBytes > 0) {
        entry.status = DedupFileEntry::RecoveryStatus::PartiallyReady;
        entry.statusMessage = L"Recovered " + std::to_wstring(writtenBytes) + L"/" + std::to_wstring(targetSize) +
                               L" bytes (" + std::to_wstring(missingChunks) + L" missing chunks) -> " + outPath;
        return false;
    } else {
        entry.status = DedupFileEntry::RecoveryStatus::Failed;
        entry.statusMessage = L"Failed to recover chunk data.";
        return false;
    }
}

void RecoveryEngine::RecoverAll(std::vector<DedupFileEntry>& entries,
                                 const std::wstring& outputFolder,
                                 ProgressCallback onProgress) {
    uint64_t total = entries.size();
    uint64_t current = 0;

    for (auto& entry : entries) {
        if (onProgress) {
            onProgress(current, total, L"Recovering: " + entry.fileName);
        }
        RecoverFile(entry, outputFolder);
        ++current;
    }

    if (onProgress) {
        onProgress(total, total, L"Recovery complete.");
    }
}

} // namespace dedup
