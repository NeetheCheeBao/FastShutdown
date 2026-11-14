#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <cwchar>
#include <string>

#ifdef _MSC_VER
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#endif

namespace {

//  常量

constexpr const wchar_t* kAppTitle      = L"FastShutdown";
constexpr const wchar_t* kShutdownLabel = L"一键关机";
constexpr const wchar_t* kRestartLabel  = L"一键重启";

// shell32.dll 中的图标索引
constexpr int kShutdownIconIndex = 27;
constexpr int kRestartIconIndex  = 238;

// 弹出失败信息：概要 + HRESULT 错误码 + 系统错误描述
void ShowFailure(const std::wstring& summary, HRESULT hr)
{
    wchar_t code[32] = {};
    std::swprintf(code, ARRAYSIZE(code), L"0x%08X", static_cast<unsigned int>(hr));

    std::wstring message = summary;
    message += L"\n\n错误码：";
    message += code;

    wchar_t* systemText = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(hr), 0,
        reinterpret_cast<LPWSTR>(&systemText), 0, nullptr);

    if (length != 0 && systemText != nullptr) {
        std::wstring detail(systemText);
        while (!detail.empty() &&
               (detail.back() == L'\r' || detail.back() == L'\n' || detail.back() == L' ')) {
            detail.pop_back();
        }
        if (!detail.empty()) {
            message += L"\n";
            message += detail;
        }
        LocalFree(systemText);
    }

    MessageBoxW(nullptr, message.c_str(), kAppTitle, MB_OK | MB_ICONERROR);
}

// 解析命令行，识别静默开关（/silent、-silent、/s、-s）
bool HasSilentFlag()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return false;
    }

    bool silent = false;
    for (int i = 1; i < argc; ++i) {
        if (lstrcmpiW(argv[i], L"/silent") == 0 ||
            lstrcmpiW(argv[i], L"-silent") == 0 ||
            lstrcmpiW(argv[i], L"/s") == 0 ||
            lstrcmpiW(argv[i], L"-s") == 0) {
            silent = true;
            break;
        }
    }

    LocalFree(argv);
    return silent;
}

// 优先读取注册表 User Shell Folders，兼容 OneDrive 桌面重定向；
// 读取失败时回退到 SHGetKnownFolderPath(FOLDERID_Desktop)。
std::wstring GetDesktopFolder()
{
    std::wstring desktop;

    const wchar_t* kSubKey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders";

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSubKey, 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        wchar_t raw[1024] = {};
        DWORD   type  = 0;
        DWORD   bytes = static_cast<DWORD>(sizeof(raw) - sizeof(wchar_t));

        if (RegQueryValueExW(key, L"Desktop", nullptr, &type,
                             reinterpret_cast<LPBYTE>(raw), &bytes) == ERROR_SUCCESS) {
            raw[bytes / sizeof(wchar_t)] = L'\0';

            if (type == REG_SZ) {
                desktop.assign(raw);
            } else if (type == REG_EXPAND_SZ) {
                wchar_t expanded[2048] = {};
                if (ExpandEnvironmentStringsW(raw, expanded, ARRAYSIZE(expanded)) != 0) {
                    desktop.assign(expanded);
                } else {
                    desktop.assign(raw);
                }
            }
        }
        RegCloseKey(key);
    }

    if (desktop.empty()) {
        PWSTR known = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr, &known))) {
            desktop.assign(known);
            CoTaskMemFree(known);
        }
    }

    return desktop;
}

//  快捷方式创建

HRESULT CreateShortcut(const std::wstring& linkPath,
                       const std::wstring& target,
                       const std::wstring& arguments,
                       const std::wstring& workingDir,
                       const std::wstring& iconPath,
                       int                 iconIndex,
                       const std::wstring& description)
{
    IShellLinkW* link = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void**>(&link));
    if (FAILED(hr)) {
        return hr;
    }

    hr = link->SetPath(target.c_str());
    if (SUCCEEDED(hr)) hr = link->SetArguments(arguments.c_str());
    if (SUCCEEDED(hr)) hr = link->SetWorkingDirectory(workingDir.c_str());
    if (SUCCEEDED(hr)) hr = link->SetDescription(description.c_str());
    if (SUCCEEDED(hr)) hr = link->SetIconLocation(iconPath.c_str(), iconIndex);

    if (SUCCEEDED(hr)) {
        IPersistFile* file = nullptr;
        hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file));
        if (SUCCEEDED(hr)) {
            hr = file->Save(linkPath.c_str(), TRUE);
            file->Release();
        }
    }

    link->Release();
    return hr;
}

//  主流程

HRESULT CreateAllShortcuts(std::wstring& desktop, std::wstring& error)
{
    desktop = GetDesktopFolder();
    if (desktop.empty()) {
        error = L"无法获取桌面文件夹路径。";
        return E_FAIL;
    }

    wchar_t systemDir[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(systemDir, ARRAYSIZE(systemDir));
    if (length == 0 || length >= ARRAYSIZE(systemDir)) {
        error = L"无法获取系统目录路径。";
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const std::wstring system32(systemDir);
    const std::wstring shutdownExe = system32 + L"\\shutdown.exe";
    const std::wstring shell32Dll  = system32 + L"\\shell32.dll";

    if (GetFileAttributesW(shutdownExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error = L"未找到系统关机组件的路径：\n" + shutdownExe;
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    struct Entry {
        const wchar_t* label;
        const wchar_t* arguments;
        const wchar_t* description;
        int            iconIndex;
    };

    const Entry entries[] = {
        { kShutdownLabel, L"-s -t 0", L"一键关机：shutdown -s -t 0", kShutdownIconIndex },
        { kRestartLabel,  L"-r -t 0", L"一键重启：shutdown -r -t 0", kRestartIconIndex  },
    };

    for (const Entry& entry : entries) {
        const std::wstring linkPath = desktop + L"\\" + entry.label + L".lnk";
        const HRESULT hr = CreateShortcut(linkPath, shutdownExe, entry.arguments,
                                          system32, shell32Dll, entry.iconIndex,
                                          entry.description);
        if (FAILED(hr)) {
            error = std::wstring(L"创建“") + entry.label + L"”快捷方式失败。";
            return hr;
        }
    }

    return S_OK;
}

}

//  入口
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const bool silent = HasSilentFlag();

    const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hrInit)) {
        ShowFailure(L"COM 组件初始化失败。", hrInit);
        return 1;
    }

    std::wstring desktop;
    std::wstring error;
    const HRESULT hr = CreateAllShortcuts(desktop, error);

    CoUninitialize();

    if (FAILED(hr)) {
        ShowFailure(error, hr);
        return 2;
    }

    if (!silent) {
        const std::wstring message = L"快捷方式已成功创建到桌面！\n\n路径：" + desktop;
        MessageBoxW(nullptr, message.c_str(), L"创建成功", MB_OK | MB_ICONINFORMATION);
    }

    return 0;
}
