#include <windows.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
// Nota: Usamos wWinMain como punto de entrada de la app de ventana.
#include <iostream>
#include <windows.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Comdlg32.lib")

// UI IDs
#define IDC_BTN_BROWSE     1001
#define IDC_BTN_BUILD      1002
#define IDC_BTN_RUN        1003
#define IDC_LABEL_FILE     1004
#define IDC_EDIT_LOG       1005
#define IDC_EDIT_CODE      1006

// Menu/accelerator command IDs
#define IDM_COPY           40001
#define IDM_PASTE          40002
#define IDM_SELECTALL      40003

static std::wstring g_repoRoot;  // …/MultiProcessor_MESI_DotProduct
static std::wstring g_srcDir;    // …/src
static std::wstring g_incDir;    // …/include
static std::wstring g_buildDir;  // …/build
static std::wstring g_outputExe; // …/build/multiprocessor_sim.exe
static std::wstring g_currentFile; // currently loaded/edited file
static bool g_compiledOk = false;
static bool g_editorDirty = false; // track if editor content changed since last successful build/save

static void AppendLogEx(HWND hLog, const std::wstring& text, bool addCRLF, bool scroll) {
    int len = GetWindowTextLengthW(hLog);
    SendMessageW(hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    if (addCRLF)
        SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    if (scroll)
        SendMessageW(hLog, EM_SCROLLCARET, 0, 0);
}

static void AppendLog(HWND hLog, const std::wstring& line) {
    AppendLogEx(hLog, line, true, true);
}

static void ClearLog(HWND hLog) {
    SetWindowTextW(hLog, L"");
}

static std::wstring Quote(const std::wstring& s) {
    if (s.find(L' ') != std::wstring::npos)
        return L"\"" + s + L"\"";
    return s;
}

// Enumerate *.cpp from src dir
static std::vector<std::wstring> ListCppFiles(const std::wstring& dir) {
    std::vector<std::wstring> out;
    std::wstring pattern = dir + L"\\*.cpp";
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            out.push_back(dir + L"\\" + fd.cFileName);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

// Run a process and capture stdout/stderr to log
static DWORD RunProcessCapture(HWND hLog, const std::wstring& cmd, const std::wstring& workDir=L"") {
    SECURITY_ATTRIBUTES sa{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hRead=0, hWrite=0;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return (DWORD)-1;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;

    PROCESS_INFORMATION pi{};
    std::wstring cmdline = cmd; // mutable for CreateProcess

    std::wstring cwd = workDir.empty() ? g_repoRoot : workDir;

    BOOL ok = CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi);
    CloseHandle(hWrite);
    if (!ok) {
        DWORD err = GetLastError();
        std::wstringstream ss; ss << L"[Error] CreateProcess failed: " << err;
        AppendLog(hLog, ss.str());
        CloseHandle(hRead);
        return err;
    }

    // Reduce flicker/overhead while appending many lines
    SendMessageW(hLog, WM_SETREDRAW, FALSE, 0);
    char buffer[4096]; DWORD read=0;
    while (true) {
        BOOL r = ReadFile(hRead, buffer, sizeof(buffer)-1, &read, nullptr);
        if (!r || read==0) {
            DWORD code=0; GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(hRead);
            // Re-enable redraw and scroll to bottom once
            SendMessageW(hLog, WM_SETREDRAW, TRUE, 0);
            int len = GetWindowTextLengthW(hLog);
            SendMessageW(hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            SendMessageW(hLog, EM_SCROLLCARET, 0, 0);
            return code;
        }
        buffer[read]=0;
        // Convert to wide
        int need = MultiByteToWideChar(CP_UTF8, 0, buffer, (int)read, nullptr, 0);
        std::wstring w; w.resize(need);
        MultiByteToWideChar(CP_UTF8, 0, buffer, (int)read, &w[0], need);
        // Split lines
        size_t start=0; while (start<w.size()) {
            size_t pos = w.find(L'\n', start);
            std::wstring line = w.substr(start, pos==std::wstring::npos ? std::wstring::npos : pos-start);
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (!line.empty()) AppendLogEx(hLog, line, true, false);
            if (pos==std::wstring::npos) break; else start = pos+1;
        }
    }
}

static void EnsureDir(const std::wstring& dir) {
    if (!PathFileExistsW(dir.c_str())) {
        CreateDirectoryW(dir.c_str(), nullptr);
    }
}

static void ComputeRepoPaths() {
    wchar_t exePathW[MAX_PATH];
    GetModuleFileNameW(nullptr, exePathW, MAX_PATH);
    std::filesystem::path exePath(exePathW);
    auto dir = exePath.parent_path();
    // Support either gui\gui.exe or gui\output\MESI_GUI.exe
    if (dir.filename() == L"output") {
        dir = dir.parent_path(); // go to gui
    }
    auto root = dir.parent_path(); // repo root
    g_repoRoot = root.wstring();
    g_srcDir   = g_repoRoot + L"\\src";
    g_incDir   = g_repoRoot + L"\\include";
    g_buildDir = g_repoRoot + L"\\build";
    g_outputExe= g_buildDir + L"\\multiprocessor_sim.exe";
}

static void SetChildText(HWND hWnd, UINT id, const std::wstring& text) {
    HWND h = GetDlgItem(hWnd, id);
    SetWindowTextW(h, text.c_str());
}

static void DoBrowse(HWND hWnd) {
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn);
    wchar_t fileBuf[MAX_PATH]{};
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"Text/C++\0*.txt;*.asm;*.s;*.cpp;*.hpp;*.h;*.c\0All\0*.*\0";
    ofn.lpstrFile = fileBuf; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = g_repoRoot.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn)) {
        g_currentFile = fileBuf;
        // load (supporting Unicode paths via std::filesystem::path)
        std::ifstream f(std::filesystem::path(g_currentFile), std::ios::binary);
        if (!f) {
            AppendLog(GetDlgItem(hWnd, IDC_EDIT_LOG), L"[Archivo] Error al abrir: " + g_currentFile);
            return;
        }
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        int need = MultiByteToWideChar(CP_UTF8, 0, content.data(), (int)content.size(), nullptr, 0);
        std::wstring w; w.resize(need);
        MultiByteToWideChar(CP_UTF8, 0, content.data(), (int)content.size(), &w[0], need);
        HWND hCode = GetDlgItem(hWnd, IDC_EDIT_CODE);
        SetWindowTextW(hCode, w.c_str());
        std::wstring rel = g_currentFile;
        wchar_t relPath[MAX_PATH];
        if (PathRelativePathToW(relPath, g_repoRoot.c_str(), FILE_ATTRIBUTE_DIRECTORY, g_currentFile.c_str(), 0)) {
            rel = relPath;
        }
        SetChildText(hWnd, IDC_LABEL_FILE, g_currentFile);
        AppendLog(GetDlgItem(hWnd, IDC_EDIT_LOG), L"[Archivo] Cargado: " + g_currentFile);
        g_compiledOk = false;
        g_editorDirty = false;
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_RUN), FALSE);
    }
}

static bool SaveEditorToFile(HWND hWnd) {
    HWND hCode = GetDlgItem(hWnd, IDC_EDIT_CODE);
    int len = GetWindowTextLengthW(hCode);
    // Allocate buffer including null-terminator
    std::wstring w; w.resize(len + 1);
    int copied = GetWindowTextW(hCode, &w[0], len + 1);
    if (copied < 0) copied = 0;
    // Trim to actual length (exclude trailing null)
    if ((size_t)copied < w.size()) w.resize((size_t)copied);
    // Choose path if none
    if (g_currentFile.empty()) {
        OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn);
        wchar_t fileBuf[MAX_PATH]{};
        ofn.hwndOwner = hWnd;
        ofn.lpstrFilter = L"Text\0*.txt\0All\0*.*\0";
        ofn.lpstrFile = fileBuf; ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = g_repoRoot.c_str();
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
        if (!GetSaveFileNameW(&ofn)) return false;
        g_currentFile = fileBuf;
        SetChildText(hWnd, IDC_LABEL_FILE, g_currentFile);
    }
    // UTF-8 encode
    int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string u8; u8.resize(need);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &u8[0], need, nullptr, nullptr);
    // Write with Unicode path support
    std::ofstream f(std::filesystem::path(g_currentFile), std::ios::binary);
    f.write(u8.data(), (std::streamsize)u8.size());
    AppendLog(GetDlgItem(hWnd, IDC_EDIT_LOG), L"[Guardar] " + g_currentFile);
    g_editorDirty = false;
    return true;
}

static void DoBuild(HWND hWnd) {
    HWND hLog = GetDlgItem(hWnd, IDC_EDIT_LOG);
    bool shouldClear = g_editorDirty; // if user modified input, clear old logs
    if (!SaveEditorToFile(hWnd)) return;
    if (shouldClear) ClearLog(hLog);
    EnsureDir(g_buildDir);

    // Build command: g++ -std=c++17 -Wall -Wextra -pthread -I <inc> <all src cpp> -o <exe>
    std::vector<std::wstring> files = ListCppFiles(g_srcDir);
    if (files.empty()) { AppendLog(hLog, L"[Compilar] No se encontraron fuentes en src/."); return; }

    std::wstringstream cmd;
    cmd << L"g++ -std=c++17 -Wall -Wextra -pthread -I " << Quote(g_incDir) << L" ";
    for (auto& f : files) cmd << Quote(f) << L" ";
    cmd << L"-o " << Quote(g_outputExe);

    AppendLog(hLog, L"[Compilar] Ejecutando: " + cmd.str());
    DWORD code = RunProcessCapture(hLog, cmd.str(), g_repoRoot);
    if (code == 0) {
        AppendLog(hLog, L"[Compilar] OK: " + g_outputExe);
        g_compiledOk = true;
        g_editorDirty = false;
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_RUN), TRUE);
    } else {
        std::wstringstream ss; ss << L"[Compilar] ERROR (código " << code << L")";
        AppendLog(hLog, ss.str());
        g_compiledOk = false;
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_RUN), FALSE);
    }
}

static void DoRun(HWND hWnd) {
    HWND hLog = GetDlgItem(hWnd, IDC_EDIT_LOG);
    if (!g_compiledOk) { AppendLog(hLog, L"[Ejecutar] No compilado."); return; }
    if (!PathFileExistsW(g_outputExe.c_str())) {
        AppendLog(hLog, L"[Ejecutar] Ejecutable no encontrado.");
        return;
    }
    // Build argument: program file (relative to repo root to avoid Unicode path issues)
    std::wstring arg;
    if (!g_currentFile.empty()) {
        wchar_t relPath[MAX_PATH];
        if (PathRelativePathToW(relPath, g_repoRoot.c_str(), FILE_ATTRIBUTE_DIRECTORY, g_currentFile.c_str(), FILE_ATTRIBUTE_NORMAL)) {
            // PathRelativePathTo returns a path starting with .\\ when relative; strip leading .\\ if present
            std::wstring rel = relPath;
            if (rel.rfind(L".\\", 0) == 0) rel = rel.substr(2);
            // Normalize to forward slashes for portability
            for (auto &ch : rel) if (ch == L'\\') ch = L'/';
            arg = rel;
        } else {
            // Fallback: default program under repo
            arg = L"programs/program_pe0.txt";
        }
    } else {
        arg = L"programs/program_pe0.txt";
    }

    std::wstring cmd = Quote(g_outputExe) + L" " + Quote(arg);
    AppendLog(hLog, L"[Ejecutar] " + cmd);
    DWORD code = RunProcessCapture(hLog, cmd, g_repoRoot);
    std::wstringstream ss; ss << L"[Ejecutar] Finalizado (" << code << L")";
    AppendLog(hLog, ss.str());
}

static void DoLayout(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    const int pad = 6;
    int top = pad;
    int w = rc.right - rc.left - pad*2;

    // Toolbar: buttons + label
    int btnH = 28, btnW = 120;
    MoveWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE), pad, top, 90, btnH, TRUE);
    MoveWindow(GetDlgItem(hWnd, IDC_BTN_BUILD),  pad+95, top, 130, btnH, TRUE);
    MoveWindow(GetDlgItem(hWnd, IDC_BTN_RUN),    pad+230, top, 100, btnH, TRUE);
    MoveWindow(GetDlgItem(hWnd, IDC_LABEL_FILE), pad+335, top+6, w-335, 20, TRUE);

    top += btnH + pad;
    int remainingH = (rc.bottom - rc.top) - top - pad;
    int half = remainingH / 2;

    // Code editor (arriba)
    MoveWindow(GetDlgItem(hWnd, IDC_EDIT_CODE), pad, top, w, half, TRUE);
    top += half + pad;

    // Log (abajo)
    MoveWindow(GetDlgItem(hWnd, IDC_EDIT_LOG), pad, top, w, remainingH - half, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        ComputeRepoPaths();
        CreateWindowW(L"BUTTON", L"Examinar", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)IDC_BTN_BROWSE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Guardar y compilar", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)IDC_BTN_BUILD, nullptr, nullptr);
        HWND hRun = CreateWindowW(L"BUTTON", L"Ejecutar", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)IDC_BTN_RUN, nullptr, nullptr);
        EnableWindow(hRun, FALSE);
        CreateWindowW(L"STATIC", L"[Sin archivo]", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)IDC_LABEL_FILE, nullptr, nullptr);
        // Log
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,
                        0,0,0,0, hWnd, (HMENU)IDC_EDIT_LOG, nullptr, nullptr);
        // Code editor
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|ES_WANTRETURN|WS_VSCROLL|WS_HSCROLL,
                        0,0,0,0, hWnd, (HMENU)IDC_EDIT_CODE, nullptr, nullptr);
        DoLayout(hWnd);
        // Mensaje inicial para confirmar GUI y rutas
        AppendLog(GetDlgItem(hWnd, IDC_EDIT_LOG), L"[GUI] Ventana inicializada.");
        AppendLog(GetDlgItem(hWnd, IDC_EDIT_LOG), L"[GUI] Repo root: " + g_repoRoot);
        AppendLog(GetDlgItem(hWnd, IDC_EDIT_LOG), L"[GUI] src: " + g_srcDir + L"  include: " + g_incDir);
        return 0; }
    case WM_SIZE:
        DoLayout(hWnd);
        return 0;
    case WM_COMMAND: {
        // Global accelerators for Ctrl+C / Ctrl+V -> forward to focused edit
        if (LOWORD(wParam) == IDM_COPY) {
            HWND hFocus = GetFocus();
            if (hFocus == GetDlgItem(hWnd, IDC_EDIT_CODE) || hFocus == GetDlgItem(hWnd, IDC_EDIT_LOG)) {
                SendMessageW(hFocus, WM_COPY, 0, 0);
            }
            return 0;
        }
        if (LOWORD(wParam) == IDM_PASTE) {
            HWND hFocus = GetFocus();
            if (hFocus == GetDlgItem(hWnd, IDC_EDIT_CODE)) {
                SendMessageW(hFocus, WM_PASTE, 0, 0);
            }
            return 0;
        }
        if (LOWORD(wParam) == IDM_SELECTALL) {
            HWND hFocus = GetFocus();
            if (hFocus == GetDlgItem(hWnd, IDC_EDIT_CODE) || hFocus == GetDlgItem(hWnd, IDC_EDIT_LOG)) {
                SendMessageW(hFocus, EM_SETSEL, 0, -1);
            }
            return 0;
        }
        // Detect edits in code editor and enforce recompile before run
        if (LOWORD(wParam) == IDC_EDIT_CODE && HIWORD(wParam) == EN_CHANGE) {
            if (!g_editorDirty) {
                g_editorDirty = true;
                g_compiledOk = false;
                EnableWindow(GetDlgItem(hWnd, IDC_BTN_RUN), FALSE);
                AppendLog(GetDlgItem(hWnd, IDC_EDIT_LOG), L"[Editar] Contenido modificado. Debe Guardar y compilar antes de Ejecutar.");
            }
            return 0;
        }
        switch (LOWORD(wParam)) {
        case IDC_BTN_BROWSE: DoBrowse(hWnd); break;
        case IDC_BTN_BUILD:  DoBuild(hWnd); break;
        case IDC_BTN_RUN:    DoRun(hWnd); break;
        }
        return 0; }
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    const wchar_t* cls = L"MESI_GUI_CLASS";
    WNDCLASSW wc{}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = cls;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    if (!RegisterClassW(&wc)) {
        DWORD err = GetLastError();
        std::wstringstream ss; ss << L"No se pudo registrar la clase de ventana. Error=" << err;
        MessageBoxW(nullptr, ss.str().c_str(), L"Error GUI", MB_ICONERROR|MB_OK);
        return (int)err;
    }

    HWND hWnd = CreateWindowW(cls, L"MESI Processor GUI", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 450, 450,
                              nullptr, nullptr, hInst, nullptr);
    if (!hWnd) {
        DWORD err = GetLastError();
        std::wstringstream ss; ss << L"No se pudo crear la ventana. Error=" << err;
        MessageBoxW(nullptr, ss.str().c_str(), L"Error GUI", MB_ICONERROR|MB_OK);
        return (int)err;
    }
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // Create accelerators for Ctrl+C and Ctrl+V
    ACCEL accels[3]{};
    accels[0].fVirt = FCONTROL | FVIRTKEY; accels[0].key = 'C'; accels[0].cmd = IDM_COPY;
    accels[1].fVirt = FCONTROL | FVIRTKEY; accels[1].key = 'V'; accels[1].cmd = IDM_PASTE;
    accels[2].fVirt = FCONTROL | FVIRTKEY; accels[2].key = 'A'; accels[2].cmd = IDM_SELECTALL;
    HACCEL hAccel = CreateAcceleratorTableW(accels, 3);

    MSG msg; while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (hAccel && TranslateAcceleratorW(hWnd, hAccel, &msg)) continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if (hAccel) DestroyAcceleratorTable(hAccel);
    return (int)msg.wParam;
}
