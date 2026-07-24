// main.cpp - WinMain entry point. Initializes COM (for the folder picker)
// and Common Controls (for ListView/ProgressBar/StatusBar), then runs the
// single-window message loop.

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>

#include "mainwindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"Failed to initialize COM.", L"Dedup Explorer", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    if (!dedup::MainWindow::RegisterClass(hInstance)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Dedup Explorer", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    dedup::MainWindow window;
    if (!window.Create(hInstance, nCmdShow)) {
        MessageBoxW(nullptr, L"Failed to create main window.", L"Dedup Explorer", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
