#include "platform/windows/Process.h"

#include "core/PathSafety.h"

#include <tlhelp32.h>

#include <cwctype>

namespace citron::platform {

namespace {

bool sameName(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) {
            return false;
        }
    }
    return true;
}

}

std::vector<ProcessInfo> findProcesses(std::wstring_view exeName) {
    std::vector<ProcessInfo> out;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return out;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (!sameName(entry.szExeFile, exeName)) {
                continue;
            }
            ProcessInfo info;
            info.pid = entry.th32ProcessID;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (process != nullptr) {
                wchar_t buffer[1024];
                DWORD size = 1024;
                if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
                    info.path = std::wstring(buffer, size);
                }
                CloseHandle(process);
            }
            out.push_back(std::move(info));
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return out;
}

bool isProcessAlive(DWORD pid) {
    if (pid == 0) {
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return false;
    }
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
}

bool anyProcessUnder(std::wstring_view exeName, const std::filesystem::path& directory) {
    for (const auto& process : findProcesses(exeName)) {
        if (!process.path.empty() && pathsafety::isInside(directory, process.path)) {
            return true;
        }
    }
    return false;
}

}
