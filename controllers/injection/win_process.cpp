#include "win_process.h"
#include <tlhelp32.h>
#include <psapi.h>

WinProcess::WinProcess() : m_handle(nullptr), m_pid(0) {}

WinProcess::~WinProcess() {
    detach();
}

static void enableDebugPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return;
    LUID luid;
    if (LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid)) {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }
    CloseHandle(hToken);
}

bool WinProcess::attach(DWORD pid, bool requireWrite) {
    detach();
    enableDebugPrivilege(); // Request SeDebugPrivilege so we don't need to run as admin
    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    if (requireWrite) {
        access |= PROCESS_VM_OPERATION | PROCESS_VM_WRITE;
    }
    m_handle = OpenProcess(access, FALSE, pid);
    if (m_handle) {
        m_pid = pid;
        return true;
    }
    return false;
}

void WinProcess::detach() {
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = nullptr;
        m_pid = 0;
    }
}

bool WinProcess::isAttached() const {
    return m_handle != nullptr;
}

QByteArray WinProcess::readMemory(uint64_t address, size_t size) const {
    if (!m_handle || size == 0) return QByteArray();
    
    QByteArray buffer;
    buffer.resize(static_cast<int>(size));
    SIZE_T bytesRead = 0;
    
    if (ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address), buffer.data(), size, &bytesRead)) {
        buffer.resize(static_cast<int>(bytesRead));
        return buffer;
    }
    return QByteArray();
}

bool WinProcess::writeMemory(uint64_t address, const QByteArray& data) {
    if (!m_handle || data.isEmpty()) return false;
    
    SIZE_T bytesWritten = 0;
    return WriteProcessMemory(m_handle, reinterpret_cast<LPVOID>(address), data.constData(), data.size(), &bytesWritten) && bytesWritten == data.size();
}

uint64_t WinProcess::readPointer(uint64_t address) const {
    QByteArray data = readMemory(address, 8);
    if (data.size() == 8) {
        return *reinterpret_cast<const uint64_t*>(data.constData());
    }
    return 0;
}

uint32_t WinProcess::readU32(uint64_t address) const {
    QByteArray data = readMemory(address, 4);
    if (data.size() == 4) {
        return *reinterpret_cast<const uint32_t*>(data.constData());
    }
    return 0;
}

uint16_t WinProcess::readU16(uint64_t address) const {
    QByteArray data = readMemory(address, 2);
    if (data.size() == 2) {
        return *reinterpret_cast<const uint16_t*>(data.constData());
    }
    return 0;
}

QVector<MemoryRegion> WinProcess::getRegions(uint32_t typeFilter, bool writableOnly) const {
    QVector<MemoryRegion> regions;
    if (!m_handle) return regions;

    uint64_t address = 0x10000;
    uint64_t maxAddress = 0x7FFFFFFFFFFF;
    MEMORY_BASIC_INFORMATION info;

    while (address < maxAddress) {
        if (VirtualQueryEx(m_handle, reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0) {
            address += 0x10000;
            continue;
        }

        uint64_t base = reinterpret_cast<uint64_t>(info.BaseAddress);
        uint64_t size = info.RegionSize;

        bool typeOk = (typeFilter == 0) || (info.Type == typeFilter);
        
        bool protectOk = false;
        if (!(info.Protect & PAGE_GUARD) && !(info.Protect & PAGE_NOACCESS)) {
            if (writableOnly) {
                protectOk = (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
            } else {
                protectOk = (info.Protect & 0xFE); // Any accessible page
            }
        }

        if (info.State == MEM_COMMIT && typeOk && protectOk) {
            MemoryRegion r;
            r.baseAddress = base;
            r.size = size;
            r.protect = info.Protect;
            r.type = info.Type;
            regions.append(r);
        }

        uint64_t nextAddress = base + size;
        if (nextAddress <= address) break; // prevent infinite loop on overflow
        address = nextAddress;
    }

    return regions;
}

uint64_t WinProcess::getModuleBaseAddress() const {
    if (!m_handle) return 0;
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(m_handle, hMods, sizeof(hMods), &cbNeeded)) {
        MODULEINFO modInfo;
        if (GetModuleInformation(m_handle, hMods[0], &modInfo, sizeof(modInfo))) {
            return reinterpret_cast<uint64_t>(modInfo.lpBaseOfDll);
        }
    }
    return 0;
}
