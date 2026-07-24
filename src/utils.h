// utils.h - small shared helpers used across the parser modules.
#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace dedup::util {

std::vector<uint8_t> ReadWholeFile(const std::wstring& path);

bool ReadAt(HANDLE file, uint64_t offset, void* buffer, uint32_t length);

HANDLE OpenForRead(const std::wstring& path);

std::wstring FormatGuid(const GUID& guid);

std::wstring ToHex(const uint8_t* data, size_t len);

std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);

std::wstring PathJoin(const std::wstring& a, const std::wstring& b);

std::wstring FormatSize(uint64_t bytes);

} // namespace dedup::util