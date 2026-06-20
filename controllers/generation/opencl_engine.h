#ifndef OPENCL_ENGINE_H
#define OPENCL_ENGINE_H

#include <QImage>
#include <QVector>
#include <QString>
#include <random>
#include "../../models/shape_data.h"
#include "search_engine.h"

class OpenCLEnginePrivate;

class OpenCLEngine : public ISearchEngine {
public:
    OpenCLEngine();
    virtual ~OpenCLEngine();
    
    bool init(QString& gpuName);
    void prepare(const QImage& target, const QVector<float>& edgeWeight) override;
    void updateCanvas(const QVector<uint8_t>& rawRgba);
    void updateCanvasRegion(const QVector<uint8_t>& rawRgba, int x0, int y0, int x1, int y1);
    QPair<float, ShapeData> search(const QImage& canvas, int nRandom, int nMutate, float maxSizeFrac, int maxThreads, std::mt19937& rng, float baselineErr, float nTotal) override;

private:
    OpenCLEnginePrivate* d;
};

#endif // OPENCL_ENGINE_H
