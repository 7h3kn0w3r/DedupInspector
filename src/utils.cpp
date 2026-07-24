#include "utils.h"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace dedup::util {

HANDLE OpenForRead(const std::wstring& path) {
    return CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
}

std::vector<uint8_t> ReadWholeFile(const std::wstring& path) {
    HANDLE h = OpenForRead(path);
    if (h == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open file for reading: " + WideToUtf8(path));
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        throw std::runtime_error("GetFileSizeEx failed: " + WideToUtf8(path));
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size.QuadPart));
    uint64_t totalRead = 0;
    const uint32_t chunk = 1 << 20;
    while (totalRead < buffer.size()) {
        DWORD toRead = static_cast<DWORD>(std::min<uint64_t>(chunk, buffer.size() - totalRead));
        DWORD read = 0;
        if (!ReadFile(h, buffer.data() + totalRead, toRead, &read, nullptr) || read == 0) {
            CloseHandle(h);
            throw std::runtime_error("ReadFile failed: " + WideToUtf8(path));
        }
        totalRead += read;
    }

    CloseHandle(h);
    return buffer;
}

bool ReadAt(HANDLE file, uint64_t offset, void* buffer, uint32_t length) {
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFFull);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);

    DWORD read = 0;
    BOOL ok = ReadFile(file, buffer, length, &read, &ov);
    if (!ok) {
        return false;
    }
    return read == length;
}

std::wstring FormatGuid(const GUID& guid) {
    wchar_t buf[64];
    swprintf_s(buf, L"{%08lX-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return std::wstring(buf);
}

std::wstring ToHex(const uint8_t* data, size_t len) {
    static const wchar_t* digits = L"0123456789abcdef";
    std::wstring out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(digits[(data[i] >> 4) & 0xF]);
        out.push_back(digits[data[i] & 0xF]);
    }
    return out;
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), size);
    return out;
}

std::wstring PathJoin(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (!a.empty() && a.back() == L'\\') return a + b;
    return a + L"\\" + b;
}

std::wstring FormatSize(uint64_t bytes) {
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(unit == 0 ? 0 : 2) << value << L" " << units[unit];
    return ss.str();
}

} // namespace dedup::util