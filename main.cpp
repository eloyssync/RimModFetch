#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <process.h>
#include <regex>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")

#define IDC_EDIT_MODS       101
#define IDC_EDIT_STEAMCMD   102
#define IDC_EDIT_MODS_DIR   103
#define IDC_BTN_START       104
#define IDC_EDIT_LOG        105
#define IDC_BTN_FORMAT      106

HWND hModsEdit, hSteamCmdEdit, hModsDirEdit, hBtnStart, hBtnFormat, hLogEdit;

void Log(const std::wstring& text) {
    int len = GetWindowTextLengthW(hLogEdit);
    SendMessageW(hLogEdit, EM_SETSEL, len, len);
    std::wstring line = text + L"\r\n";
    SendMessageW(hLogEdit, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
}

std::vector<std::wstring> ExtractIds(const std::wstring& input) {
    std::vector<std::wstring> ids;
    std::wregex e(L"\\b\\d{8,12}\\b");
    std::wsregex_iterator iter(input.begin(), input.end(), e);
    std::wsregex_iterator end;

    for (; iter != end; ++iter) {
        ids.push_back(iter->str());
    }

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    return ids;
}

std::wstring GetModName(const std::wstring& modFolder) {
    std::wstring aboutPath = modFolder + L"\\About\\About.xml";
    std::ifstream file(aboutPath.c_str());
    if (!file.is_open()) return L"Unknown Mod";

    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find("<name>");
        if (start != std::string::npos) {
            size_t end = line.find("</name>", start);
            if (end != std::string::npos) {
                std::string name = line.substr(start + 6, end - start - 6);
                int len = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, NULL, 0);
                if (len > 0) {
                    std::wstring wname(len - 1, 0);
                    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, &wname[0], len);
                    return wname;
                }
            }
        }
    }
    return L"Unknown Mod";
}

bool CopyDirectory(const std::wstring& src, const std::wstring& dst) {
    CreateDirectoryW(dst.c_str(), NULL);
    std::wstring searchPattern = src + L"\\*.*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);

    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        std::wstring srcPath = src + L"\\" + fd.cFileName;
        std::wstring dstPath = dst + L"\\" + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CopyDirectory(srcPath, dstPath);
        } else {
            CopyFileW(srcPath.c_str(), dstPath.c_str(), FALSE);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return true;
}

unsigned __stdcall WorkerThread(void* pParam) {
    EnableWindow(hBtnStart, FALSE);
    EnableWindow(hBtnFormat, FALSE);

    wchar_t bufSteamCmd[MAX_PATH], bufModsDir[MAX_PATH];
    GetWindowTextW(hSteamCmdEdit, bufSteamCmd, MAX_PATH);
    GetWindowTextW(hModsDirEdit, bufModsDir, MAX_PATH);

    int textLen = GetWindowTextLengthW(hModsEdit);
    std::vector<wchar_t> bufMods(textLen + 1);
    GetWindowTextW(hModsEdit, &bufMods[0], textLen + 1);

    std::vector<std::wstring> ids = ExtractIds(std::wstring(&bufMods[0]));

    if (ids.empty()) {
        Log(L"[ERROR] No valid mod IDs found!");
        EnableWindow(hBtnStart, TRUE);
        EnableWindow(hBtnFormat, TRUE);
        return 0;
    }

    Log(L"=== Launching SteamCMD ===");
    Log(L"Mods detected: " + std::to_wstring(ids.size()));

    std::wstring cmdArgs = L"\"" + std::wstring(bufSteamCmd) + L"\" +login anonymous";
    for (const auto& id : ids) {
        cmdArgs += L" +workshop_download_item 294100 " + id;
    }
    cmdArgs += L" +quit";

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    if (!CreateProcessW(NULL, &cmdArgs[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        Log(L"[ERROR] Failed to run steamcmd.exe. Check the file path!");
        EnableWindow(hBtnStart, TRUE);
        EnableWindow(hBtnFormat, TRUE);
        return 0;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    Log(L"=== Download complete, copying files ===");

    std::wstring steamCmdPath(bufSteamCmd);
    std::wstring steamDir = steamCmdPath.substr(0, steamCmdPath.find_last_of(L"\\/"));
    std::wstring workshopDir = steamDir + L"\\steamapps\\workshop\\content\\294100";

    for (const auto& id : ids) {
        std::wstring src = workshopDir + L"\\" + id;
        std::wstring dst = std::wstring(bufModsDir) + L"\\" + id;

        DWORD attr = GetFileAttributesW(src.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring modName = GetModName(src);
            Log(L"Copying: [" + modName + L"] (ID: " + id + L")");
            CopyDirectory(src, dst);
        } else {
            Log(L"[SKIP] Folder not found: " + id);
        }
    }

    Log(L"=== Finished! Mods are installed ===");
    EnableWindow(hBtnStart, TRUE);
    EnableWindow(hBtnFormat, TRUE);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        CreateWindowW(L"STATIC", L"SteamCMD.exe:", WS_CHILD | WS_VISIBLE, 15, 15, 120, 20, hWnd, NULL, NULL, NULL);
        hSteamCmdEdit = CreateWindowW(L"EDIT", L"C:\\steamcmd\\steamcmd.exe", WS_CHILD | WS_VISIBLE | WS_BORDER, 140, 12, 425, 22, hWnd, (HMENU)IDC_EDIT_STEAMCMD, NULL, NULL);

        CreateWindowW(L"STATIC", L"RimWorld Mods folder:", WS_CHILD | WS_VISIBLE, 15, 45, 150, 20, hWnd, NULL, NULL, NULL);
        hModsDirEdit = CreateWindowW(L"EDIT", L"C:\\Games\\RimWorld\\Mods", WS_CHILD | WS_VISIBLE | WS_BORDER, 165, 42, 400, 22, hWnd, (HMENU)IDC_EDIT_MODS_DIR, NULL, NULL);

        CreateWindowW(L"STATIC", L"Mod links or IDs (paste anywhere):", WS_CHILD | WS_VISIBLE, 15, 75, 400, 20, hWnd, NULL, NULL, NULL);

        hModsEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 15, 95, 550, 120, hWnd, (HMENU)IDC_EDIT_MODS, NULL, NULL);

        hBtnFormat = CreateWindowW(L"BUTTON", L"Extract IDs", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, 225, 270, 32, hWnd, (HMENU)IDC_BTN_FORMAT, NULL, NULL);
        hBtnStart = CreateWindowW(L"BUTTON", L"Download & Install", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 295, 225, 270, 32, hWnd, (HMENU)IDC_BTN_START, NULL, NULL);

        hLogEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 15, 270, 550, 170, hWnd, (HMENU)IDC_EDIT_LOG, NULL, NULL);

        SendMessageW(hSteamCmdEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hModsDirEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hModsEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hBtnFormat, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hBtnStart, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hLogEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_BTN_START) {
            _beginthreadex(NULL, 0, WorkerThread, NULL, 0, NULL);
        }
        else if (LOWORD(wParam) == IDC_BTN_FORMAT) {
            int textLen = GetWindowTextLengthW(hModsEdit);
            std::vector<wchar_t> bufMods(textLen + 1);
            GetWindowTextW(hModsEdit, &bufMods[0], textLen + 1);

            std::vector<std::wstring> ids = ExtractIds(std::wstring(&bufMods[0]));
            std::wstring cleanText = L"";
            for (const auto& id : ids) {
                cleanText += id + L" ";
            }
            SetWindowTextW(hModsEdit, cleanText.c_str());
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"RimModDownloader";

    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, wc.lpszClassName, L"RimWorld Mod Downloader Pro",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 490, NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}