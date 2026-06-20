#ifndef FH6_INJECTOR_H
#define FH6_INJECTOR_H

#include <QObject>
#include <QVector>
#include <windows.h>
#include <atomic>
#include "../../models/shape_data.h"

class WinProcess;

class FH6Injector : public QObject {
    Q_OBJECT
public:
    explicit FH6Injector(QObject* parent = nullptr);

public slots:
    void injectShapes(DWORD pid, uint16_t expectedLayerCount, QVector<ShapeData> shapes);
    void stopInjection();
    
signals:
    void logMessage(const QString& msg);
    void progressUpdated(int current, int total);
    void finished(bool success);

private:
    void dumpLayerState(WinProcess& process, uint64_t tableAddress, int limit, const QString& prefix);

    std::atomic<bool> m_stop{false};
};

#endif // FH6_INJECTOR_H
