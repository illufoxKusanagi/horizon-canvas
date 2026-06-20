#include "shape_generator.h"
#include "cpu_engine.h"
#include "geometry_json.h"
#include "image_math.h"
#include "image_preprocessor.h"
#include "opencl_engine.h"
#include "search_engine.h"
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <cmath>
#include <algorithm>

ShapeGenerator::ShapeGenerator(QObject *parent) : QObject(parent) {}

// ─── internal helper ─────────────────────────────────────────────────────────

static QString buildOutputPath(const QString &imagePath) {
  QFileInfo fi(imagePath);
  return fi.absolutePath() + "/" + fi.baseName() + "_shapes.json";
}

// ─── main loop ───────────────────────────────────────────────────────────────

void ShapeGenerator::startGeneration(const SessionState &initialState,
                                     const GenerationSettings &settings) {
  emit logMessage("Starting generation...");

  // --- Pre-process image: handle transparency + edge buffer ---
  QImage rawImage = initialState.originalImage;
  emit logMessage("Preprocessing image...");
  PreparedImage prep = ImagePreprocessor::prepare(rawImage, settings.preprocessMode);

  if (prep.stickerMode) {
    emit logMessage("Sticker Mode: ON (transparent background detected — "
                    "shapes will only be placed inside opaque areas).");
  } else {
    emit logMessage("Sticker Mode: OFF (opaque image — full canvas is valid "
                    "drawing space).");
  }
  emit logMessage(QString("Edge Buffer: %1px padding added on each side.")
                      .arg(prep.padPixels));

  QImage target = prep.target;
  QImage canvas = QImage(target.size(), QImage::Format_RGB32);
  canvas.fill(qRgb(40, 40, 40));

  // If resuming, redraw shapes onto the padded canvas
  if (initialState.generatedCount > 0) {
    emit logMessage(
        QString("Re-rendering %1 saved shapes onto padded canvas...")
            .arg(initialState.generatedCount));
    for (const auto &s : initialState.shapes)
      canvas = ImageMath::drawShape(canvas, s);
    emit logMessage("Resume canvas ready.");
  }

  emit logMessage("Computing edge weights...");
  QVector<float> edgeWeights =
      ImageMath::computeEdgeWeight(target, 6.0f, prep.alphaMask);

  auto initialErr = ImageMath::precomputeCanvasError(canvas, target, edgeWeights, prep.alphaMask);
  float baselineErr = initialErr.first;
  float nTotal = initialErr.second;

  int w = target.width();
  int h = target.height();
  int pixels = w * h;
  QVector<uint8_t> rawCanvas(pixels * 4);
  for (int y = 0; y < h; ++y) {
    const QRgb* line = reinterpret_cast<const QRgb*>(canvas.constScanLine(y));
    int rowOffset = y * w;
    for (int x = 0; x < w; ++x) {
      QRgb p = line[x];
      int idx = (rowOffset + x) * 4;
      rawCanvas[idx] = qRed(p);
      rawCanvas[idx + 1] = qGreen(p);
      rawCanvas[idx + 2] = qBlue(p);
      rawCanvas[idx + 3] = 255;
    }
  }

  // --- Engine setup ---
  ISearchEngine* engine = nullptr;
  CPUEngine cpu;
  OpenCLEngine gpu;
  QString gpuName;
  bool useGpu = settings.useOpenCL && gpu.init(gpuName);

  if (settings.useOpenCL)
    emit logMessage("GPU Status: " + gpuName);

  if (useGpu) {
    emit logMessage("Using OpenCL Engine.");
    gpu.prepare(target, edgeWeights);
    gpu.updateCanvas(rawCanvas);
    engine = &gpu;
  } else {
    emit logMessage("Using CPU Engine.");
    cpu.prepare(target, edgeWeights);
    engine = &cpu;
  }

  // --- Output path (determined once, reused for every save) ---
  const QString outPath = buildOutputPath(initialState.imagePath);
  emit logMessage("Output file: " + outPath);

  QList<int> checkpointTargets;
  for (const QString& s : settings.customCheckpoints.split(',', Qt::SkipEmptyParts)) {
      checkpointTargets.append(s.trimmed().toInt());
  }

  // --- Generation loop ---
  std::mt19937 rng(1337);
  int currentCount = initialState.generatedCount;
  m_stop.store(false);
  QVector<ShapeData> shapes = initialState.shapes;

  while (currentCount < settings.maxShapes) {
    if (m_stop.load())
      break;

    // Decay schedule for sizeDecayFactor: starts at 1.0f and decays to 0.05f
    float progress = static_cast<float>(currentCount) / settings.maxShapes;
    float sizeDecayFactor = 1.0f * std::pow(0.05f / 1.0f, progress);
    sizeDecayFactor = std::clamp(sizeDecayFactor, 0.05f, 1.0f);

    // --- Score a candidate ---
    QPair<float, ShapeData> result =
        engine->search(canvas, settings.randomSamples, settings.mutateSamples,
                       sizeDecayFactor, settings.maxThreads, rng, baselineErr, nTotal);

    // --- Commit ---
    auto blend = ImageMath::composite(canvas, result.second, target,
                                      edgeWeights, prep.alphaMask, result.first);
    if (std::isinf(blend.second)) {
      continue;
    }

    canvas = blend.first;
    baselineErr = blend.second * blend.second * nTotal;

    // Sync rawCanvas buffer with QImage canvas and upload to GPU
    const ShapeData& s = result.second;
    syncRawCanvas(canvas, s, rawCanvas);
    if (useGpu) {
      float angRad = s.angle * 3.14159265f / 180.0f;
      float ca = std::cos(angRad), sa = std::sin(angRad);
      int bx = static_cast<int>(std::ceil(std::sqrt(s.rx*s.rx*ca*ca + s.ry*s.ry*sa*sa)));
      int by = static_cast<int>(std::ceil(std::sqrt(s.rx*s.rx*sa*sa + s.ry*s.ry*ca*ca)));
      int ux0 = std::max(0, static_cast<int>(s.x) - bx);
      int uy0 = std::max(0, static_cast<int>(s.y) - by);
      int ux1 = std::min(w, static_cast<int>(s.x) + bx + 1);
      int uy1 = std::min(h, static_cast<int>(s.y) + by + 1);
      gpu.updateCanvasRegion(rawCanvas, ux0, uy0, ux1, uy1);
    }

    currentCount++;
    shapes.append(result.second);

    emit shapeAdded(canvas, currentCount);
    emit logMessage(QString("Shape %1 added. Score: %2")
                        .arg(currentCount)
                        .arg(blend.second));

    // --- Auto-save at every saveInterval shapes ---
    if (settings.saveInterval > 0 &&
        (currentCount % settings.saveInterval) == 0) {
      saveCheckpoint(QString("Auto @%1").arg(currentCount), "", outPath, initialState, shapes, target.width(), target.height(), prep.stickerMode, settings);
    }

    // --- Create specific milestone checkpoints ---
    if (checkpointTargets.contains(currentCount)) {
      QFileInfo fi(initialState.imagePath);
      QString cpPath = fi.absolutePath() + "/" + fi.baseName() +
                       QString("_%1.json").arg(currentCount);
      saveCheckpoint(QString("Milestone @%1").arg(currentCount), cpPath, outPath, initialState, shapes, target.width(), target.height(), prep.stickerMode, settings);
    }
  }

  // --- Final save (always, even if stopped early) ---
  if (!shapes.isEmpty())
    saveCheckpoint("Final", "", outPath, initialState, shapes, target.width(), target.height(), prep.stickerMode, settings);

  emit logMessage("Generation finished.");
  emit finished();
}

void ShapeGenerator::stopGeneration() {
  m_stop.store(true);
  emit logMessage("Stop signal received. Saving final checkpoint...");
}

void ShapeGenerator::syncRawCanvas(const QImage& canvas, const ShapeData& s, QVector<uint8_t>& rawCanvas) {
    int w = canvas.width();
    int h = canvas.height();
    float angRad = s.angle * 3.14159265f / 180.0f;
    float ca = std::cos(angRad), sa = std::sin(angRad);
    int bx = static_cast<int>(std::ceil(std::sqrt(s.rx*s.rx*ca*ca + s.ry*s.ry*sa*sa)));
    int by = static_cast<int>(std::ceil(std::sqrt(s.rx*s.rx*sa*sa + s.ry*s.ry*ca*ca)));
    int ux0 = std::max(0, static_cast<int>(s.x) - bx);
    int uy0 = std::max(0, static_cast<int>(s.y) - by);
    int ux1 = std::min(w, static_cast<int>(s.x) + bx + 1);
    int uy1 = std::min(h, static_cast<int>(s.y) + by + 1);

    for (int y = uy0; y < uy1; ++y) {
      const QRgb* line = reinterpret_cast<const QRgb*>(canvas.constScanLine(y));
      int rowOffset = y * w;
      for (int x = ux0; x < ux1; ++x) {
        QRgb p = line[x];
        int idx = (rowOffset + x) * 4;
        rawCanvas[idx]     = qRed(p);
        rawCanvas[idx + 1] = qGreen(p);
        rawCanvas[idx + 2] = qBlue(p);
        rawCanvas[idx + 3] = 255;
      }
    }
}

void ShapeGenerator::saveCheckpoint(const QString& reason, const QString& overridePath, 
                    const QString& outPath, const SessionState& initialState,
                    const QVector<ShapeData>& shapes, int width, int height,
                    bool stickerMode, const GenerationSettings& settings) {
    QString finalPath = overridePath.isEmpty() ? outPath : overridePath;
    QFileInfo fi(initialState.imagePath);
    if (GeometryJson::saveToFile(finalPath, shapes, width, height, fi.fileName(), stickerMode, settings, settings.profileName)) {
      emit logMessage(QString("[%1] Checkpoint saved — %2 shapes → %3")
                          .arg(reason).arg(shapes.size()).arg(finalPath));
      if (overridePath.isEmpty()) {
        emit fileSaved(finalPath);
      }
    } else {
      emit logMessage(QString("[%1] Warning: failed to save checkpoint to %2")
                          .arg(reason).arg(finalPath));
    }
}
