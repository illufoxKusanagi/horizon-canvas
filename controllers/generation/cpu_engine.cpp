#include "cpu_engine.h"
#include "image_math.h"
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

CPUEngine::CPUEngine() {}

void CPUEngine::prepare(const QImage &target,
                        const QVector<float> &edgeWeight) {
  m_target = target;
  m_edgeWeight = edgeWeight;
}

QPair<float, ShapeData> CPUEngine::search(const QImage &canvas, int nRandom,
                                          int nMutate, float maxSizeFrac, int maxThreads,
                                          std::mt19937 &rng, float baselineErr, float nTotal) {
  Q_UNUSED(baselineErr);
  Q_UNUSED(nTotal);
  int w = m_target.width();
  int h = m_target.height();

  QVector<ShapeData> candidates;
  for (int i = 0; i < std::max(1, nRandom); ++i) {
    ShapeData s;
    s.x = std::uniform_int_distribution<int>(0, w - 1)(rng);
    s.y = std::uniform_int_distribution<int>(0, h - 1)(rng);
    int maxRx = std::max(1, static_cast<int>(w * maxSizeFrac));
    int maxRy = std::max(1, static_cast<int>(h * maxSizeFrac));
    s.rx = std::uniform_int_distribution<int>(1, maxRx)(rng);
    s.ry = std::uniform_int_distribution<int>(1, maxRy)(rng);
    s.angle = std::uniform_int_distribution<int>(0, 180)(rng);
    s.type = 102;
    candidates.append(s);
  }

  QList<QPair<float, ShapeData>> results = QtConcurrent::blockingMapped(
      candidates, [&](const ShapeData &s) -> QPair<float, ShapeData> {
        ShapeData copy = s;
        auto res = ImageMath::composite(canvas, copy, m_target, m_edgeWeight);
        return {res.second, copy};
      });

  float bestScore = results[0].first;
  int bestIdx = 0;
  for (int i = 1; i < results.size(); ++i) {
    if (results[i].first < bestScore) {
      bestScore = results[i].first;
      bestIdx = i;
    }
  }

  ShapeData bestShape = results[bestIdx].second;

  int batch = maxThreads > 0 ? maxThreads : 64;
  batch = std::max(1, std::min(nMutate, batch));
  int steps = std::max(1, nMutate / batch);
  int no_improve = 0;

  for (int step = 0; step < steps; ++step) {
    QVector<ShapeData> muts;
    for (int b = 0; b < batch; ++b) {
      ShapeData m = bestShape;
      int kind = std::uniform_int_distribution<int>(0, 3)(rng);
      int maxRx = std::max(1, static_cast<int>(w * maxSizeFrac));
      int maxRy = std::max(1, static_cast<int>(h * maxSizeFrac));
      if (kind == 0) {
        m.x += std::normal_distribution<double>(0.0, std::max(2.0, m.rx * 0.2))(rng);
        m.y += std::normal_distribution<double>(0.0, std::max(2.0, m.ry * 0.2))(rng);
      } else if (kind == 1) {
        m.rx += std::normal_distribution<double>(0.0, std::max(1.0, m.rx * 0.15))(rng);
        m.ry += std::normal_distribution<double>(0.0, std::max(1.0, m.ry * 0.15))(rng);
      } else if (kind == 2) {
        m.angle += std::normal_distribution<double>(0.0, 25.0)(rng);
      } else {
        m.x += std::normal_distribution<double>(0.0, std::max(1.0, m.rx * 0.1))(rng);
        m.angle += std::normal_distribution<double>(0.0, 15.0)(rng);
      }
      m.x = std::clamp(m.x, 0.0, static_cast<double>(w - 1));
      m.y = std::clamp(m.y, 0.0, static_cast<double>(h - 1));
      m.rx = std::clamp(m.rx, 1.0, static_cast<double>(maxRx));
      m.ry = std::clamp(m.ry, 1.0, static_cast<double>(maxRy));
      muts.append(m);
    }

    QList<QPair<float, ShapeData>> mresults = QtConcurrent::blockingMapped(
        muts, [&](const ShapeData &s) -> QPair<float, ShapeData> {
          ShapeData copy = s;
          auto res = ImageMath::composite(canvas, copy, m_target, m_edgeWeight);
          return {res.second, copy};
        });

    float ms = std::numeric_limits<float>::infinity();
    int mIdx = -1;
    for (int i = 0; i < mresults.size(); ++i) {
      if (mresults[i].first < ms) {
        ms = mresults[i].first;
        mIdx = i;
      }
    }

    if (ms < bestScore) {
      bestScore = ms;
      bestShape = mresults[mIdx].second;
      no_improve = 0;
    } else {
      no_improve++;
      if (no_improve >= std::max(2, steps / 4))
        break;
    }
  }

  return {bestScore, bestShape};
}
