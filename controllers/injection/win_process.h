#ifndef WIN_PROCESS_H
#define WIN_PROCESS_H

#include <QString>
#include <QVector>
#include <QByteArray>
#include <windows.h>
#include <cstdint>

struct MemoryRegion {
    uint64_t baseAddress;
    uint64_t size;
    uint32_t protect;
    uint32_t type;
};

class WinProcess {
public:
    WinProcess();
    ~WinProcess();

    bool attach(DWORD pid, bool requireWrite = false);
    void detach();
    bool isAttached() const;

    QByteArray readMemory(uint64_t address, size_t size) const;
    bool writeMemory(uint64_t address, const QByteArray& data);

    uint64_t readPointer(uint64_t address) const;
    uint32_t readU32(uint64_t address) const;
    uint16_t readU16(uint64_t address) const;

    QVector<MemoryRegion> getRegions(uint32_t typeFilter = 0, bool writableOnly = false) const;
    uint64_t getModuleBaseAddress() const;
    
    DWORD getPid() const { return m_pid; }

private:
    HANDLE m_handle;
    DWORD m_pid;
};

#endif // WIN_PROCESS_H
