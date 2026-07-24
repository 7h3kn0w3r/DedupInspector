#include "mainwindow.h"
#include "utils.h"
#include "mft_parser.h"
#include "reparse_parser.h"
#include "stream_parser.h"
#include "ckhr_parser.h"
#include "recovery_engine.h"
#include "chunk_parser.h"

#include <shobjidl.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")

namespace dedup {

const wchar_t MainWindow::kClassName[] = L"DedupExplorerMainWindow";

namespace {

MainWindow* GetThis(HWND hwnd) {
    return reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

HWND CreateLabel(HWND parent, HINSTANCE hInst, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, nullptr, hInst, nullptr);
}

HWND CreateEdit(HWND parent, HINSTANCE hInst, int id, int x, int y, int w, int h, bool readOnly = false) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    if (readOnly) style |= ES_READONLY;
    return CreateWindowExW(0, L"EDIT", L"", style,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
}

HWND CreateButton(HWND parent, HINSTANCE hInst, int id, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
}

const wchar_t* StatusToText(DedupFileEntry::RecoveryStatus s) {
    switch (s) {
        case DedupFileEntry::RecoveryStatus::NotAttempted:  return L"Not attempted";
        case DedupFileEntry::RecoveryStatus::Ready:          return L"Ready";
        case DedupFileEntry::RecoveryStatus::PartiallyReady: return L"Partial";
        case DedupFileEntry::RecoveryStatus::Recovered:      return L"Recovered";
        case DedupFileEntry::RecoveryStatus::Failed:         return L"Failed";
    }
    return L"Unknown";
}

bool PickFolder(HWND owner, std::wstring& outPath) {
    bool result = false;
    IFileOpenDialog* dialog = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) return false;

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    hr = dialog->Show(owner);
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                outPath = path;
                CoTaskMemFree(path);
                result = true;
            }
            item->Release();
        }
    }

    dialog->Release();
    return result;
}

} // namespace

bool MainWindow::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    return RegisterClassExW(&wc) != 0;
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    hwnd_ = CreateWindowExW(
        0, kClassName, L"Dedup Explorer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 550,
        nullptr, nullptr, hInstance, this);

    if (!hwnd_) return false;

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = GetThis(hwnd);

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self) return self->HandleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate(hwnd);
            return 0;
        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_COMMAND:
            OnCommand(wParam, lParam);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void MainWindow::OnCreate(HWND hwnd) {
    HINSTANCE hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));

    int y = 10;

    // --- MFT file picker ---
    CreateLabel(hwnd, hInst, L"MFT File:", 10, y + 2, 80, 20);
    editMftPath_ = CreateEdit(hwnd, hInst, IDC_EDIT_MFT, 95, y, 600, 22, true);
    btnBrowseMft_ = CreateButton(hwnd, hInst, IDC_BTN_BROWSE_MFT, L"Browse...", 705, y, 90, 24);
    y += 34;

    // --- Stream file picker ---
    CreateLabel(hwnd, hInst, L"Stream File:", 10, y + 2, 80, 20);
    editStreamPath_ = CreateEdit(hwnd, hInst, IDC_EDIT_STREAM, 95, y, 600, 22, true);
    btnBrowseStream_ = CreateButton(hwnd, hInst, IDC_BTN_BROWSE_STREAM, L"Browse...", 705, y, 90, 24);
    y += 34;

    // --- Chunk file picker ---
    CreateLabel(hwnd, hInst, L"Chunk File:", 10, y + 2, 80, 20);
    editChunkPath_ = CreateEdit(hwnd, hInst, IDC_EDIT_CHUNK, 95, y, 600, 22, true);
    btnBrowseChunk_ = CreateButton(hwnd, hInst, IDC_BTN_BROWSE_CHUNK, L"Browse...", 705, y, 90, 24);
    y += 34;

    // --- Search box ---
    CreateLabel(hwnd, hInst, L"Search:", 10, y + 2, 80, 20);
    editSearch_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        95, y, 600, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDIT_SEARCH)), hInst, nullptr);
    y += 34;

    // --- Scan button ---
    btnScan_ = CreateButton(hwnd, hInst, IDC_BTN_SCAN, L"Scan", 10, y, 785, 28);
    y += 38;

    // --- Main table (ListView) ---
    listView_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        10, y, 865, 280, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LISTVIEW)), hInst, nullptr);
    ListView_SetExtendedListViewStyle(listView_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;

    col.pszText = const_cast<LPWSTR>(L"Filename");
    col.cx = 380;
    ListView_InsertColumn(listView_, 0, &col);

    col.pszText = const_cast<LPWSTR>(L"Original Size");
    col.cx = 130;
    ListView_InsertColumn(listView_, 1, &col);

    col.pszText = const_cast<LPWSTR>(L"Chunks");
    col.cx = 90;
    ListView_InsertColumn(listView_, 2, &col);

    col.pszText = const_cast<LPWSTR>(L"Status");
    col.cx = 120;
    ListView_InsertColumn(listView_, 3, &col);

    y += 290;

    // --- Bottom buttons ---
    btnRecoverSelected_ = CreateButton(hwnd, hInst, IDC_BTN_RECOVER_SELECTED, L"Recover Selected", 10, y, 180, 28);
    btnRecoverAll_ = CreateButton(hwnd, hInst, IDC_BTN_RECOVER_ALL, L"Recover All", 200, y, 180, 28);

    // --- Status bar ---
    statusBar_ = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready.", WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUSBAR)), hInst, nullptr);
}

void MainWindow::OnSize(int width, int height) {
    if (statusBar_) SendMessageW(statusBar_, WM_SIZE, 0, 0);

    RECT sbRect{};
    int sbHeight = 0;
    if (statusBar_) {
        GetWindowRect(statusBar_, &sbRect);
        sbHeight = sbRect.bottom - sbRect.top;
    }

    int listBottom = height - sbHeight - 50;
    if (listView_) {
        MoveWindow(listView_, 10, 184, width - 20, std::max(100, listBottom - 184), TRUE);
    }

    int btnY = listBottom + 10;
    if (btnRecoverSelected_) MoveWindow(btnRecoverSelected_, 10, btnY, 180, 28, TRUE);
    if (btnRecoverAll_) MoveWindow(btnRecoverAll_, 200, btnY, 180, 28, TRUE);

    if (editMftPath_) MoveWindow(editMftPath_, 95, 10, width - 200, 22, TRUE);
    if (btnBrowseMft_) MoveWindow(btnBrowseMft_, width - 95, 9, 90, 24, TRUE);

    if (editStreamPath_) MoveWindow(editStreamPath_, 95, 44, width - 200, 22, TRUE);
    if (btnBrowseStream_) MoveWindow(btnBrowseStream_, width - 95, 43, 90, 24, TRUE);

    if (editChunkPath_) MoveWindow(editChunkPath_, 95, 78, width - 200, 22, TRUE);
    if (btnBrowseChunk_) MoveWindow(btnBrowseChunk_, width - 95, 77, 90, 24, TRUE);

    if (editSearch_) MoveWindow(editSearch_, 95, 112, width - 200, 22, TRUE);
    if (btnScan_) MoveWindow(btnScan_, 10, 146, width - 20, 28, TRUE);
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (LOWORD(wParam)) {
        case IDC_BTN_BROWSE_MFT: BrowseForMftFile(); break;
        case IDC_BTN_BROWSE_STREAM: BrowseForStreamFile(); break;
        case IDC_BTN_BROWSE_CHUNK: BrowseForChunkFile(); break;
        case IDC_BTN_SCAN: DoScan(); break;
        case IDC_BTN_RECOVER_SELECTED: RecoverSelected(); break;
        case IDC_BTN_RECOVER_ALL: RecoverAll(); break;
        case IDC_EDIT_SEARCH:
            if (HIWORD(wParam) == EN_CHANGE) {
                FilterListView();
            }
            break;
    }
}

void MainWindow::BrowseForMftFile() {
    wchar_t buffer[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Select exported $MFT file";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        mftPath_ = buffer;
        SetWindowTextW(editMftPath_, mftPath_.c_str());
    }
}

void MainWindow::BrowseForStreamFile() {
    wchar_t buffer[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Stream Files (*.ccc;*.cccm)\0*.ccc;*.cccm\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Select stream container file";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        streamFilePath_ = buffer;
        SetWindowTextW(editStreamPath_, streamFilePath_.c_str());
    }
}

void MainWindow::BrowseForChunkFile() {
    wchar_t buffer[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Chunk Files (*.ccc)\0*.ccc\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Select chunk container file";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        chunkFilePath_ = buffer;
        SetWindowTextW(editChunkPath_, chunkFilePath_.c_str());
    }
}

void MainWindow::BrowseForOutputFolder(std::wstring& outPath) {
    PickFolder(hwnd_, outPath);
}

void MainWindow::SetStatus(const std::wstring& text) {
    SendMessageW(statusBar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
}

void MainWindow::DoScan() {
    wchar_t buf[MAX_PATH] = {};
    GetWindowTextW(editMftPath_, buf, MAX_PATH);
    mftPath_ = buf;
    GetWindowTextW(editStreamPath_, buf, MAX_PATH);
    streamFilePath_ = buf;
    GetWindowTextW(editChunkPath_, buf, MAX_PATH);
    chunkFilePath_ = buf;

    if (mftPath_.empty() || streamFilePath_.empty() || chunkFilePath_.empty()) {
        MessageBoxW(hwnd_, L"Please select MFT, Stream, and Chunk files before scanning.",
            L"Dedup Explorer", MB_OK | MB_ICONWARNING);
        return;
    }

    entries_.clear();
    filteredEntries_.clear();
    ListView_DeleteAllItems(listView_);
    SetStatus(L"Parsing $MFT...");

    MftParser mftParser;
    bool ok = mftParser.Parse(mftPath_, 0, nullptr);

    if (!ok) {
        MessageBoxW(hwnd_, mftParser.LastError().c_str(), L"MFT Parse Error", MB_OK | MB_ICONERROR);
        return;
    }

    SetStatus(L"Reading stream file...");

    std::vector<uint8_t> streamData;
    try {
        streamData = util::ReadWholeFile(streamFilePath_);
    } catch (const std::exception& e) {
        MessageBoxW(hwnd_, util::Utf8ToWide(e.what()).c_str(),
            L"Stream File Error", MB_OK | MB_ICONERROR);
        return;
    }

    SetStatus(L"Resolving dedup reparse points and chunk metadata...");

    for (const auto& rec : mftParser.ReparseRecords()) {
        DedupReparseInfo reparseInfo = ReparsePointParser::Parse(rec.reparseData);
        if (!reparseInfo.valid) continue;

        DedupFileEntry entry;
        entry.fileName = rec.fileName;
        entry.fullPath = mftParser.ResolvePath(rec);
        entry.originalFileSize = reparseInfo.originalFileSize;
        entry.streamOffset = reparseInfo.streamOffset;
        entry.chunkStoreGuid = reparseInfo.chunkStoreGuid;

        StreamRecord stream = StreamParser::FindStreamInData(streamData, reparseInfo.streamOffset, reparseInfo.originalFileSize);
        entry.chunks = stream.chunks;

        entry.status = entry.chunks.empty()
            ? DedupFileEntry::RecoveryStatus::Failed
            : DedupFileEntry::RecoveryStatus::Ready;
        entry.statusMessage = entry.chunks.empty()
            ? L"Could not locate CKHR/chunk metadata for this file."
            : L"Chunk metadata resolved, ready to recover.";

        entries_.push_back(std::move(entry));
    }

    filteredEntries_ = entries_;
    PopulateListView();
    SetStatus(L"Scan complete: " + std::to_wstring(entries_.size()) + L" deduplicated file(s) found.");
}

void MainWindow::PopulateListView() {
    ListView_DeleteAllItems(listView_);

    for (size_t i = 0; i < filteredEntries_.size(); ++i) {
        const auto& e = filteredEntries_[i];

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(i);
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(e.fileName.c_str());
        item.lParam = static_cast<LPARAM>(i);
        int row = ListView_InsertItem(listView_, &item);

        std::wstring size = util::FormatSize(e.originalFileSize);
        ListView_SetItemText(listView_, row, 1, const_cast<LPWSTR>(size.c_str()));

        std::wstring chunkCount = std::to_wstring(e.chunks.size());
        ListView_SetItemText(listView_, row, 2, const_cast<LPWSTR>(chunkCount.c_str()));

        std::wstring statusText = StatusToText(e.status);
        ListView_SetItemText(listView_, row, 3, const_cast<LPWSTR>(statusText.c_str()));
    }
}

void MainWindow::FilterListView() {
    wchar_t buf[256] = {};
    GetWindowTextW(editSearch_, buf, 256);
    std::wstring query = buf;

    std::transform(query.begin(), query.end(), query.begin(), ::towlower);

    filteredEntries_.clear();

    if (query.empty()) {
        filteredEntries_ = entries_;
    } else {
        for (const auto& e : entries_) {
            std::wstring lowerName = e.fileName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
            if (lowerName.find(query) != std::wstring::npos) {
                filteredEntries_.push_back(e);
            }
        }
    }

    PopulateListView();
}

void MainWindow::RecoverSelected() {
    int sel = ListView_GetNextItem(listView_, -1, LVNI_SELECTED);
    if (sel < 0) {
        MessageBoxW(hwnd_, L"Select a file in the table first.", L"Dedup Explorer", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring outputFolder;
    BrowseForOutputFolder(outputFolder);
    if (outputFolder.empty()) return;

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = sel;
    ListView_GetItem(listView_, &item);
    size_t originalIndex = static_cast<size_t>(item.lParam);

    if (originalIndex >= entries_.size()) return;

    auto& entry = entries_[originalIndex];

    ChunkContainerLocator locator;
    if (!locator.Open(chunkFilePath_)) {
        MessageBoxW(hwnd_, locator.LastError().c_str(), L"Recovery Error", MB_OK | MB_ICONERROR);
        return;
    }

    RecoveryEngine engine(locator);
    SetStatus(L"Recovering " + entry.fileName + L"...");
    bool ok = engine.RecoverFile(entry, outputFolder);
    SetStatus(ok ? L"Recovery succeeded." : L"Recovery completed with issues - see status column.");

    FilterListView();
}

void MainWindow::RecoverAll() {
    if (entries_.empty()) {
        MessageBoxW(hwnd_, L"Nothing to recover - run a scan first.", L"Dedup Explorer", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring outputFolder;
    BrowseForOutputFolder(outputFolder);
    if (outputFolder.empty()) return;

    ChunkContainerLocator locator;
    if (!locator.Open(chunkFilePath_)) {
        MessageBoxW(hwnd_, locator.LastError().c_str(), L"Recovery Error", MB_OK | MB_ICONERROR);
        return;
    }

    RecoveryEngine engine(locator);

    engine.RecoverAll(entries_, outputFolder, [this](uint64_t cur, uint64_t total, const std::wstring& msg) {
        (void)cur;
        (void)total;
        SetStatus(msg);
    });

    FilterListView();
    SetStatus(L"Batch recovery complete.");
}

} // namespace dedup