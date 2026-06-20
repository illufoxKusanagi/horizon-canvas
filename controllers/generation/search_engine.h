#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include <QImage>
#include <QVector>
#include <QPair>
#include <random>
#include "../../models/shape_data.h"

class ISearchEngine {
public:
    virtual ~ISearchEngine() = default;
    virtual void prepare(const QImage& target, const QVector<float>& edgeWeight) = 0;
    virtual QPair<float, ShapeData> search(const QImage& canvas, int nRandom, int nMutate, float maxSizeFrac, int maxThreads, std::mt19937& rng, float baselineErr, float nTotal) = 0;
};

#endif // SEARCH_ENGINE_H
