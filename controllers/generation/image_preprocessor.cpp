#include "image_preprocessor.h"
#include <QColor>
#include <cmath>

PreparedImage ImagePreprocessor::prepare(const QImage &source, const QString& mode) {
  PreparedImage result;
  result.stickerMode = false;

  // --- Step 1: Detect transparency and resolve it ---
  QImage rgb;
  QVector<float> contentMask; // 1.0 = opaque, 0.0 = transparent

  // Always convert to ARGB32 and pixel-scan for real transparency.
  // hasAlphaChannel() alone is unreliable: Qt may load a transparent PNG
  // as Format_RGB32 if the format detector strips alpha early.
  QImage rgba = source.convertToFormat(QImage::Format_ARGB32);
  int w = rgba.width();
  int h = rgba.height();
  bool hasTransparentPixel = false;
  for (int y = 0; y < h && !hasTransparentPixel; ++y) {
    const QRgb *line = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
    for (int x = 0; x < w; ++x) {
      if (qAlpha(line[x]) < 255) {
        hasTransparentPixel = true;
        break;
      }
    }
  }
  bool hasAlpha = hasTransparentPixel;

  if (hasAlpha) {
    // Reuse the rgba already converted above
    contentMask.resize(w * h, 1.0f);
    result.stickerMode = true;

    rgb = QImage(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
      const QRgb *srcLine = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
      QRgb *dstLine = reinterpret_cast<QRgb *>(rgb.scanLine(y));
      int rowOffset = y * w;
      for (int x = 0; x < w; ++x) {
        QRgb p = srcLine[x];
        float a = qAlpha(p) / 255.0f;
        contentMask[rowOffset + x] = a;
        dstLine[x] = p; // Keep full ARGB so kernel can read .w
      }
    }
  } else {
    // No transparency: treat entire image as valid drawing space.
    rgb = rgba;
    contentMask.fill(1.0f, w * h);
  }

  // --- Step 1.5: Apply Preprocess Mode ---
  if (mode == "blur") {
    QImage blurred(w, h, QImage::Format_ARGB32);
    blurred.fill(qRgba(0, 0, 0, 0));
    const int kernel[5][5] = {
      {1,  4,  7,  4, 1},
      {4, 16, 26, 16, 4},
      {7, 26, 41, 26, 7},
      {4, 16, 26, 16, 4},
      {1,  4,  7,  4, 1}
    };
    const int kernelSum = 273;

    for (int y = 0; y < h; ++y) {
      QRgb *dstLine = reinterpret_cast<QRgb *>(blurred.scanLine(y));
      for (int x = 0; x < w; ++x) {
        long r = 0, g = 0, b = 0, a = 0;
        for (int ky = -2; ky <= 2; ++ky) {
          int py = qBound(0, y + ky, h - 1);
          const QRgb *srcLine = reinterpret_cast<const QRgb *>(rgb.constScanLine(py));
          for (int kx = -2; kx <= 2; ++kx) {
            int px = qBound(0, x + kx, w - 1);
            int weight = kernel[ky + 2][kx + 2];
            QRgb p = srcLine[px];
            r += qRed(p) * weight;
            g += qGreen(p) * weight;
            b += qBlue(p) * weight;
            a += qAlpha(p) * weight;
          }
        }
        dstLine[x] = qRgba(r / kernelSum, g / kernelSum, b / kernelSum, a / kernelSum);
      }
    }
    rgb = blurred;
  } else if (mode == "outline" && result.stickerMode) {
    QVector<float> erodedMask = contentMask;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        if (contentMask[y * w + x] > 0.0f) {
          bool nearEdge = false;
          for (int ky = -2; ky <= 2 && !nearEdge; ++ky) {
            int py = y + ky;
            if (py < 0 || py >= h) { nearEdge = true; continue; }
            for (int kx = -2; kx <= 2 && !nearEdge; ++kx) {
              int px = x + kx;
              if (px < 0 || px >= w) { nearEdge = true; continue; }
              if (contentMask[py * w + px] == 0.0f) {
                nearEdge = true;
              }
            }
          }
          if (nearEdge) {
            erodedMask[y * w + x] = 0.0f;
          }
        }
      }
    }
    contentMask = erodedMask;
  }

  // --- Step 2: Apply 8% edge buffer padding ---
  const float BUFFER_FRAC = 0.08f;
  int srcW = rgb.width();
  int srcH = rgb.height();
  int padPx = std::max(
      8, static_cast<int>(std::round(std::max(srcW, srcH) * BUFFER_FRAC)));

  int newW = srcW + 2 * padPx;
  int newH = srcH + 2 * padPx;

  // Build padded image (transparent for buffer zone)
  QImage padded(newW, newH, QImage::Format_ARGB32);
  padded.fill(qRgba(255, 255, 255, 0));

  for (int y = 0; y < srcH; ++y) {
    const QRgb *srcLine = reinterpret_cast<const QRgb *>(rgb.constScanLine(y));
    QRgb *dstLine = reinterpret_cast<QRgb *>(padded.scanLine(y + padPx));
    for (int x = 0; x < srcW; ++x) {
      dstLine[x + padPx] = srcLine[x];
    }
  }

  // Build padded alpha mask (buffer ring = 0.0, content area = contentMask
  // value)
  QVector<float> paddedMask(newW * newH, 0.0f);
  for (int y = 0; y < srcH; ++y) {
    for (int x = 0; x < srcW; ++x) {
      paddedMask[(y + padPx) * newW + (x + padPx)] = contentMask[y * srcW + x];
    }
  }

  result.target = padded;
  result.alphaMask = paddedMask;
  result.padPixels = padPx;
  return result;
}
