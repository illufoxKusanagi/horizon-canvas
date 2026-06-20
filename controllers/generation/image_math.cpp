#include "image_math.h"
#include <QColor>
#include <algorithm>
#include <cmath>

QVector<float> ImageMath::computeEdgeWeight(const QImage &target, float boost,
                                            const QVector<float> &alphaMask) {
  int w = target.width();
  int h = target.height();
  bool hasMask = !alphaMask.isEmpty();
  QVector<float> lum(w * h);
  for (int y = 0; y < h; ++y) {
    const QRgb* line = reinterpret_cast<const QRgb*>(target.constScanLine(y));
    int rowOffset = y * w;
    for (int x = 0; x < w; ++x) {
      QRgb p = line[x];
      lum[rowOffset + x] =
          qRed(p) * 0.299f + qGreen(p) * 0.587f + qBlue(p) * 0.114f;
    }
  }

  QVector<float> mag(w * h, 0.0f);
  float maxMag = 0.0f;

  // Sobel
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      float gX = -1.0f * lum[(y - 1) * w + (x - 1)] +
                 1.0f * lum[(y - 1) * w + (x + 1)] -
                 2.0f * lum[y * w + (x - 1)] + 2.0f * lum[y * w + (x + 1)] -
                 1.0f * lum[(y + 1) * w + (x - 1)] +
                 1.0f * lum[(y + 1) * w + (x + 1)];

      float gY =
          -1.0f * lum[(y - 1) * w + (x - 1)] - 2.0f * lum[(y - 1) * w + x] -
          1.0f * lum[(y - 1) * w + (x + 1)] +
          1.0f * lum[(y + 1) * w + (x - 1)] + 2.0f * lum[(y + 1) * w + x] +
          1.0f * lum[(y + 1) * w + (x + 1)];

      float m = std::sqrt(gX * gX + gY * gY);
      mag[y * w + x] = m;
      if (m > maxMag)
        maxMag = m;
    }
  }

  QVector<float> norm(w * h, 1.0f);
  if (maxMag >= 1e-6f) {
    for (int i = 0; i < w * h; ++i) {
      norm[i] = 1.0f + (boost - 1.0f) * (mag[i] / maxMag);
    }
  }

  // Zero out forbidden zones (edge buffer + transparent areas in sticker mode)
  if (hasMask) {
    for (int i = 0; i < w * h; ++i) {
      norm[i] *= (alphaMask[i] > 0.0f ? 1.0f : 0.0f);
    }
  }
  return norm;
}

QPair<float, float>
ImageMath::precomputeCanvasError(const QImage &current, const QImage &target,
                                 const QVector<float> &edgeWeight,
                                 const QVector<float> &alphaMask) {
  int w = current.width();
  int h = current.height();
  float fullSq = 0.0f;
  float n = 0.0f;

  bool hasWeight = !edgeWeight.isEmpty();
  bool hasMask = !alphaMask.isEmpty();
  const float* edgeWeightPtr = hasWeight ? edgeWeight.constData() : nullptr;
  const float* alphaMaskPtr = hasMask ? alphaMask.constData() : nullptr;

  for (int y = 0; y < h; ++y) {
    const QRgb* currLine = reinterpret_cast<const QRgb*>(current.constScanLine(y));
    const QRgb* targetLine = reinterpret_cast<const QRgb*>(target.constScanLine(y));
    int rowOffset = y * w;
    for (int x = 0; x < w; ++x) {
      int idx = rowOffset + x;
      // Skip forbidden pixels
      if (hasMask && alphaMaskPtr[idx] <= 0.0f)
        continue;

      QRgb cp = currLine[x];
      QRgb tp = targetLine[x];

      float diffR = qRed(cp) - qRed(tp);
      float diffG = qGreen(cp) - qGreen(tp);
      float diffB = qBlue(cp) - qBlue(tp);

      float sq = diffR * diffR + diffG * diffG + diffB * diffB;
      float wgt = hasWeight ? edgeWeightPtr[idx] : 1.0f;

      fullSq += sq * wgt;
      n += wgt * 3.0f;
    }
  }

  return {fullSq, n};
}

struct Bounds {
  int x0, y0, x1, y1;
  float ca, sa;
};

static Bounds computeShapeBounds(int w, int h, const ShapeData& shape) {
  int rx = shape.rx;
  int ry = shape.ry;
  int cx = shape.x;
  int cy = shape.y;

  float angRad = shape.angle * 3.14159265f / 180.0f;
  float ca = std::cos(angRad);
  float sa = std::sin(angRad);

  float boundX = std::sqrt(rx * rx * ca * ca + ry * ry * sa * sa);
  float boundY = std::sqrt(rx * rx * sa * sa + ry * ry * ca * ca);
  int bx = static_cast<int>(std::ceil(boundX));
  int by = static_cast<int>(std::ceil(boundY));

  int x0 = std::max(0, cx - bx);
  int y0 = std::max(0, cy - by);
  int x1 = std::min(w, cx + bx + 1);
  int y1 = std::min(h, cy + by + 1);
  
  return {x0, y0, x1, y1, ca, sa};
}

QPair<QImage, float> ImageMath::composite(const QImage &current,
                                          ShapeData &shape,
                                          const QImage &target,
                                          const QVector<float> &edgeWeight,
                                          const QVector<float> &alphaMask,
                                          float presetRms) {
  int w = current.width();
  int h = current.height();

  Bounds b = computeShapeBounds(w, h, shape);
  int x0 = b.x0, y0 = b.y0, x1 = b.x1, y1 = b.y1;
  float ca = b.ca, sa = b.sa;
  int rx = shape.rx, ry = shape.ry;
  int cx = shape.x, cy = shape.y;

  float effSum = 0.0f;
  float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

  float a = 128.0f / 255.0f; // Fixed search alpha

  bool hasMask = !alphaMask.isEmpty();
  const float* alphaMaskPtr = hasMask ? alphaMask.constData() : nullptr;
  float body_total = 0.0f;
  float inside = 0.0f;
  for (int y = y0; y < y1; ++y) {
    const QRgb* targetLine = reinterpret_cast<const QRgb*>(target.constScanLine(y));
    const QRgb* currLine = reinterpret_cast<const QRgb*>(current.constScanLine(y));
    int rowOffset = y * w;
    for (int x = x0; x < x1; ++x) {
      float fx = x - cx;
      float fy = y - cy;
      float xr = ca * fx + sa * fy;
      float yr = -sa * fx + ca * fy;
      if ((xr / rx) * (xr / rx) + (yr / ry) * (yr / ry) <= 1.0f) {
        int idx = rowOffset + x;
        body_total += 1.0f;
        QRgb tp = targetLine[x];
        if (qAlpha(tp) >= 128) {
          inside += 1.0f;
          QRgb cp = currLine[x];
          float m = hasMask ? alphaMaskPtr[idx] : 1.0f;
          effSum += m;
          n0 += m * (qRed(tp) - (1.0f - a) * qRed(cp));
          n1 += m * (qGreen(tp) - (1.0f - a) * qGreen(cp));
          n2 += m * (qBlue(tp) - (1.0f - a) * qBlue(cp));
        }
      }
    }
  }

  if (body_total > 0.0f && (inside / body_total) < 0.995f) {
      return {current, std::numeric_limits<float>::infinity()};
  }

  if (effSum > 0.5f) {
    shape.color =
        QColor(std::clamp(static_cast<int>(n0 / (effSum * a)), 0, 255),
               std::clamp(static_cast<int>(n1 / (effSum * a)), 0, 255),
               std::clamp(static_cast<int>(n2 / (effSum * a)), 0, 255), 128);
  } else {
    shape.color = QColor(0, 0, 0, 128);
  }

  QImage newCanvas = current;
  for (int y = y0; y < y1; ++y) {
    const QRgb* currLine = reinterpret_cast<const QRgb*>(current.constScanLine(y));
    QRgb* newCanvasLine = reinterpret_cast<QRgb*>(newCanvas.scanLine(y));
    for (int x = x0; x < x1; ++x) {
      float fx = x - cx;
      float fy = y - cy;
      float xr = ca * fx + sa * fy;
      float yr = -sa * fx + ca * fy;
      if ((xr / rx) * (xr / rx) + (yr / ry) * (yr / ry) <= 1.0f) {
        // FH6 doesn't clip shapes. We draw the whole shape to the canvas.
        // The penalty check above ensures it doesn't bleed significantly into
        // forbidden zones.
        QRgb cp = currLine[x];
        float bR = a * shape.color.red() + (1.0f - a) * qRed(cp);
        float bG = a * shape.color.green() + (1.0f - a) * qGreen(cp);
        float bB = a * shape.color.blue() + (1.0f - a) * qBlue(cp);
        newCanvasLine[x] = qRgb(bR, bG, bB);
      }
    }
  }

  if (presetRms >= 0.0f) {
    return {newCanvas, presetRms};
  }

  auto err = precomputeCanvasError(newCanvas, target, edgeWeight, alphaMask);
  float rms =
      err.second > 0 ? std::sqrt(std::max(0.0f, err.first) / err.second) : 0.0f;

  return {newCanvas, rms};
}

QImage ImageMath::drawShape(const QImage &current, const ShapeData &shape) {
  int w = current.width();
  int h = current.height();

  Bounds b = computeShapeBounds(w, h, shape);
  int x0 = b.x0, y0 = b.y0, x1 = b.x1, y1 = b.y1;
  float ca = b.ca, sa = b.sa;
  int rx = shape.rx, ry = shape.ry;
  int cx = shape.x, cy = shape.y;

  float a = shape.color.alphaF();

  QImage newCanvas = current;
  for (int y = y0; y < y1; ++y) {
    const QRgb* currLine = reinterpret_cast<const QRgb*>(current.constScanLine(y));
    QRgb* newCanvasLine = reinterpret_cast<QRgb*>(newCanvas.scanLine(y));
    for (int x = x0; x < x1; ++x) {
      float fx = x - cx;
      float fy = y - cy;
      float xr = ca * fx + sa * fy;
      float yr = -sa * fx + ca * fy;
      if ((xr / rx) * (xr / rx) + (yr / ry) * (yr / ry) <= 1.0f) {
        QRgb cp = currLine[x];
        float bR = a * shape.color.red() + (1.0f - a) * qRed(cp);
        float bG = a * shape.color.green() + (1.0f - a) * qGreen(cp);
        float bB = a * shape.color.blue() + (1.0f - a) * qBlue(cp);
        newCanvasLine[x] = qRgb(bR, bG, bB);
      }
    }
  }
  return newCanvas;
}
