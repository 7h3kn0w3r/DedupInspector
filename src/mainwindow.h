#pragma once

#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

#include "dedup_types.h"

namespace dedup {

class MainWindow {
public:
    static bool RegisterClass(HINSTANCE hInstance);
    bool Create(HINSTANCE hInstance, int nCmdShow);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate(HWND hwnd);
    void OnSize(int width, int height);
    void OnCommand(WPARAM wParam, LPARAM lParam);

    void BrowseForMftFile();
    void BrowseForStreamFile();
    void BrowseForChunkFile();
    void BrowseForOutputFolder(std::wstring& outPath);

    void DoScan();
    void PopulateListView();
    void FilterListView();

    void RecoverSelected();
    void RecoverAll();

    void SetStatus(const std::wstring& text);

    static const wchar_t kClassName[];

    HWND hwnd_ = nullptr;

    //--------------------------------------------------------
    // Input Controls
    //--------------------------------------------------------

    HWND editMftPath_ = nullptr;
    HWND btnBrowseMft_ = nullptr;

    HWND editStreamPath_ = nullptr;
    HWND btnBrowseStream_ = nullptr;

    HWND editChunkPath_ = nullptr;
    HWND btnBrowseChunk_ = nullptr;

    HWND editSearch_ = nullptr;

    HWND btnScan_ = nullptr;

    //--------------------------------------------------------
    // File List
    //--------------------------------------------------------

    HWND listView_ = nullptr;

    //--------------------------------------------------------
    // Recovery
    //--------------------------------------------------------

    HWND btnRecoverSelected_ = nullptr;
    HWND btnRecoverAll_ = nullptr;

    //--------------------------------------------------------
    // Status
    //--------------------------------------------------------

    HWND statusBar_ = nullptr;

    //--------------------------------------------------------
    // Data
    //--------------------------------------------------------

    std::wstring mftPath_;
    std::wstring streamFilePath_;
    std::wstring chunkFilePath_;

    std::vector<DedupFileEntry> entries_;
    std::vector<DedupFileEntry> filteredEntries_;

    //--------------------------------------------------------
    // Control IDs
    //--------------------------------------------------------

    static constexpr int IDC_EDIT_MFT = 1001;
    static constexpr int IDC_BTN_BROWSE_MFT = 1002;

    static constexpr int IDC_EDIT_STREAM = 1003;
    static constexpr int IDC_BTN_BROWSE_STREAM = 1004;

    static constexpr int IDC_EDIT_CHUNK = 1005;
    static constexpr int IDC_BTN_BROWSE_CHUNK = 1006;

    static constexpr int IDC_EDIT_SEARCH = 1007;

    static constexpr int IDC_BTN_SCAN = 1008;

    static constexpr int IDC_LISTVIEW = 1009;

    static constexpr int IDC_BTN_RECOVER_SELECTED = 1010;
    static constexpr int IDC_BTN_RECOVER_ALL = 1011;

    static constexpr int IDC_STATUSBAR = 1012;
};

} // namespace dedup