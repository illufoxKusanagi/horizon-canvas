#ifndef SHAPE_GENERATOR_H
#define SHAPE_GENERATOR_H

#include <QObject>
#include <atomic>
#include "../../models/generation_settings.h"
#include "../../models/session_state.h"

class ShapeGenerator : public QObject {
    Q_OBJECT
public:
    explicit ShapeGenerator(QObject* parent = nullptr);
    void startGeneration(const SessionState& initialState, const GenerationSettings& settings);

public slots:
    void stopGeneration();

signals:
    void shapeAdded(const QImage& currentCanvas, int shapeCount);
    void logMessage(const QString& msg);
    void fileSaved(const QString& path);   // emitted on every auto-save and final save
    void finished();
    
private:
    void syncRawCanvas(const QImage& canvas, const ShapeData& s, QVector<uint8_t>& rawCanvas);
    void saveCheckpoint(const QString& reason, const QString& overridePath, 
                        const QString& outPath, const SessionState& initialState,
                        const QVector<ShapeData>& shapes, int width, int height,
                        bool stickerMode, const GenerationSettings& settings);

    std::atomic<bool> m_stop{false};
};

#endif // SHAPE_GENERATOR_H
