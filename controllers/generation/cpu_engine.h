#ifndef CPU_ENGINE_H
#define CPU_ENGINE_H

#include <QImage>
#include <QVector>
#include <random>
#include "../../models/shape_data.h"
#include "search_engine.h"

class CPUEngine : public ISearchEngine {
public:
    CPUEngine();
    void prepare(const QImage& target, const QVector<float>& edgeWeight) override;
    QPair<float, ShapeData> search(const QImage& canvas, int nRandom, int nMutate, float maxSizeFrac, int maxThreads, std::mt19937& rng, float baselineErr, float nTotal) override;

private:
    QImage m_target;
    QVector<float> m_edgeWeight;
};

#endif // CPU_ENGINE_H
