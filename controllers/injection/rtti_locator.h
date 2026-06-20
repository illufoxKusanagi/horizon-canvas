#ifndef RTTI_LOCATOR_H
#define RTTI_LOCATOR_H

#include <QString>
#include <QVector>
#include <cstdint>
#include <functional>
#include "win_process.h"

struct LiveryGroupInfo {
    uint64_t groupAddress;
    uint64_t countAddress;
    uint64_t tableField;
    uint64_t tableAddress;
    uint16_t currentCount;
    int score;
};

class RTTILocator {
public:
    using Logger = std::function<void(const QString&)>;

    explicit RTTILocator(const WinProcess& process, Logger logger = nullptr);

    // Locates the active CLiveryGroup in the game's heap by scanning for the
    // expected layer count directly (no RTTI required).
    QVector<LiveryGroupInfo> locateGroups(uint16_t expectedLayerCount);

private:
    QByteArray readRegion(uint64_t base, size_t size);
    int scoreTable(uint64_t tableAddress, uint16_t layerCount);
    void log(const QString& msg);

    const WinProcess& m_process;
    Logger m_logger;
};

#endif // RTTI_LOCATOR_H
