#include "platform/windows/Shell.h"

#include <shellapi.h>
#include <shobjidl.h>

#include <cstring>
#include <string>

namespace citron::platform {

namespace {

bool allowedScheme(std::wstring_view url) {
    for (const auto* scheme : {L"https://", L"http://", L"ms-settings:", L"ms-windows-store://"}) {
        const std::wstring_view s(scheme);
        if (url.size() >= s.size() && url.substr(0, s.size()) == s) {
            return true;
        }
    }
    return false;
}

}

bool openUrl(std::wstring_view url) {
    if (!allowedScheme(url)) {
        return false;
    }
    const std::wstring copy(url);
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", copy.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
}

std::optional<std::filesystem::path> pickFolder(HWND owner, const std::filesystem::path& initial) {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))) || dialog == nullptr) {
        return std::nullopt;
    }
    std::optional<std::filesystem::path> result;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Citron Launcher");
    if (!initial.empty()) {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&folder))) && folder != nullptr) {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr) {
                result = std::filesystem::path(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

bool copyToClipboard(HWND owner, std::wstring_view text) {
    if (!OpenClipboard(owner)) {
        return false;
    }
    bool ok = false;
    if (EmptyClipboard()) {
        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (global != nullptr) {
            if (void* memory = GlobalLock(global)) {
                std::memcpy(memory, text.data(), text.size() * sizeof(wchar_t));
                static_cast<wchar_t*>(memory)[text.size()] = L'\0';
                GlobalUnlock(global);
                ok = SetClipboardData(CF_UNICODETEXT, global) != nullptr;
            }
            if (!ok) {
                GlobalFree(global);
            }
        }
    }
    CloseClipboard();
    return ok;
}

std::optional<std::wstring> readClipboard(HWND owner) {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(owner)) {
        return std::nullopt;
    }
    std::optional<std::wstring> result;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* memory = static_cast<const wchar_t*>(GlobalLock(data))) {
            result = std::wstring(memory);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return result;
}

}
