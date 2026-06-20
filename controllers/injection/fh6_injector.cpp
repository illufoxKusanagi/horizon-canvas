#include "fh6_injector.h"
#include "rtti_locator.h"
#include "win_process.h"
#include <QDebug>
#include <QThread>
#include <cmath>
#include <cstring>

namespace {
constexpr int OFFSET_POS_X = 0x18;
constexpr int OFFSET_POS_Y = 0x1C;
constexpr int OFFSET_SCALE_X = 0x28;
constexpr int OFFSET_SCALE_Y = 0x2C;
constexpr int OFFSET_ROTATION = 0x50;
constexpr int OFFSET_SKEW = 0x70;
constexpr int OFFSET_COLOR = 0x74;
constexpr int OFFSET_MASK = 0x78;
constexpr int OFFSET_SHAPE_TYPE = 0x7A;
}

FH6Injector::FH6Injector(QObject *parent) : QObject(parent) {}

void FH6Injector::stopInjection() { m_stop.store(true); }

void FH6Injector::injectShapes(DWORD pid, uint16_t expectedLayerCount,
                               QVector<ShapeData> shapes) {
  m_stop.store(false);
  emit logMessage("Injection started...");

  WinProcess process;
  if (!process.attach(pid, true)) {
    emit logMessage("Failed to attach to the Forza Horizon 6 process.");
    emit finished(false);
    return;
  }

  emit logMessage("Scanning for CLiveryGroup...");
  RTTILocator locator(process,
                      [this](const QString &msg) { emit logMessage(msg); });
  auto groups = locator.locateGroups(expectedLayerCount);

  if (groups.isEmpty()) {
    emit logMessage(
        QString("Failed to find a layer table with exactly %1 layers.")
            .arg(expectedLayerCount));
    emit finished(false);
    return;
  }

  auto targetGroup = groups.first();
  emit logMessage(QString("Found layer table at 0x%1 (Score: %2)")
                      .arg(targetGroup.tableAddress, 0, 16)
                      .arg(targetGroup.score));

  int limit = qMin(shapes.size(), static_cast<int>(expectedLayerCount));
  emit logMessage(QString("Injecting %1 shapes...").arg(limit));

  int skipped = 0;
  int written = 0;

  // --- DIAGNOSTIC: dump the BEFORE state of first 3 layers ---
  dumpLayerState(process, targetGroup.tableAddress, limit, "BEFORE");

  for (int i = 0; i < limit; ++i) {
    if (m_stop.load()) {
      emit logMessage("Injection stopped by user.");
      break;
    }

    uint64_t layerPtr = process.readPointer(targetGroup.tableAddress + i * 8);

    // Skip invalid pointers
    if (layerPtr < 0x10000 || layerPtr > 0x7FFFFFFFFFFF ||
        layerPtr >= 0x7FF000000000ULL) {
      skipped++;
      continue;
    }

    // Read the existing layer to discover its current shape word.
    // We preserve the template's shape type and only change position,
    // scale, rotation, color, and mask.
    QByteArray existingLayer = process.readMemory(layerPtr, 0x7C);
    uint16_t templateShapeWord = 0x0066; // default to Circle
    if (existingLayer.size() >= 0x7C) {
      memcpy(&templateShapeWord, existingLayer.constData() + OFFSET_SHAPE_TYPE, 2);
    }

    const ShapeData &shape = shapes[i];

    // Position: raw pixel coords, negate Y (matches Python
    // legacy_geometry_shape)
    float x = static_cast<float>(shape.x);
    float y = static_cast<float>(-shape.y);

    // Scale: convert radius (rx, ry) to game scale using the TEMPLATE's shape
    // divisor. Python reference divisors:
    //   0x0065 (Square/Rectangle): 127.0
    //   0x0066 (Circle):           63.0
    //   0x0088 (Ellipse):          63.0  (same as Circle per Python code)
    //   Other:                     63.0
    float divisor;
    uint8_t templateShapeByte = templateShapeWord & 0xFF;
    if (templateShapeByte == 0x65) { // Square
      divisor = 127.0f;
    } else {
      divisor = 63.0f; // Circle, Ellipse, others
    }

    float sx;
    float sy;
    if (templateShapeByte == 0x65) {
      // Square template uses full width/height
      sx = (static_cast<float>(shape.rx) * 2.0f) / divisor;
      sy = (static_cast<float>(shape.ry) * 2.0f) / divisor;
    } else {
      // Ellipse template uses radius (half-extent)
      sx = static_cast<float>(shape.rx) / divisor;
      sy = static_cast<float>(shape.ry) / divisor;
    }

    // Rotation: reverse direction to match game convention
    float rot = static_cast<float>(std::fmod(360.0 - shape.angle, 360.0));
    float skew = 0.0f;

    uint8_t color[4];
    color[0] = shape.color.red();
    color[1] = shape.color.green();
    color[2] = shape.color.blue();
    color[3] = shape.color.alpha();

    uint8_t mask = 0;

    // Log what we're about to write for the first 3 layers
    if (i < 3) {
      emit logMessage(QString("WRITING layer[%1] ptr=0x%2: "
                              "JSON(x=%3,y=%4,rx=%5,ry=%6,angle=%7,type=%8) -> "
                              "pos=(%9,%10) scale=(%11,%12) rot=%13 "
                              "templateShape=0x%14 divisor=%15")
                          .arg(i)
                          .arg(layerPtr, 0, 16)
                          .arg(shape.x)
                          .arg(shape.y)
                          .arg(shape.rx)
                          .arg(shape.ry)
                          .arg(shape.angle)
                          .arg(shape.type)
                          .arg(x, 0, 'f', 3)
                          .arg(y, 0, 'f', 3)
                          .arg(sx, 0, 'f', 6)
                          .arg(sy, 0, 'f', 6)
                          .arg(rot, 0, 'f', 3)
                          .arg(templateShapeWord, 0, 16)
                          .arg(divisor, 0, 'f', 1));
    }

    // Write to layer memory — position, scale, rotation, skew, color, mask.
    // We do NOT overwrite the shape word at OFFSET_SHAPE_TYPE — we keep the template's shape.
    bool ok = true;
    ok &= process.writeMemory(layerPtr + OFFSET_POS_X, QByteArray(reinterpret_cast<const char *>(&x), 4));
    ok &= process.writeMemory(layerPtr + OFFSET_POS_Y, QByteArray(reinterpret_cast<const char *>(&y), 4));
    ok &= process.writeMemory(layerPtr + OFFSET_SCALE_X, QByteArray(reinterpret_cast<const char *>(&sx), 4));
    ok &= process.writeMemory(layerPtr + OFFSET_SCALE_Y, QByteArray(reinterpret_cast<const char *>(&sy), 4));
    ok &= process.writeMemory(layerPtr + OFFSET_ROTATION, QByteArray(reinterpret_cast<const char *>(&rot), 4));
    ok &= process.writeMemory(layerPtr + OFFSET_SKEW, QByteArray(reinterpret_cast<const char *>(&skew), 4));
    ok &= process.writeMemory(layerPtr + OFFSET_COLOR, QByteArray(reinterpret_cast<const char *>(color), 4));
    ok &= process.writeMemory(layerPtr + OFFSET_MASK, QByteArray(reinterpret_cast<const char *>(&mask), 1));

    if (!ok) {
      emit logMessage(
          QString("Failed to write layer %1 memory at 0x%2. Is the game protected or did the pointer shift?")
              .arg(i).arg(layerPtr, 0, 16));
      break;
    }

    // Verify the write actually stuck
    QByteArray verify = process.readMemory(layerPtr + OFFSET_POS_X, 4);
    if (verify.size() == 4) {
        if (memcmp(verify.constData(), &x, 4) != 0) {
            emit logMessage(QString("Verification failed for layer %1! The game immediately overwrote our injection.").arg(i));
            break;
        }
    } else {
        emit logMessage(QString("Failed to read back layer %1 to verify.").arg(i));
        break;
    }

    written++;
    if (written % 100 == 0 || i == limit - 1) {
      emit progressUpdated(written, limit);
    }
  }

  // --- DIAGNOSTIC: dump the AFTER state of first 3 layers ---
  dumpLayerState(process, targetGroup.tableAddress, limit, "AFTER");

  if (skipped > 0) {
    emit logMessage(
        QString("WARNING: Injection finished with %1 written, but %2 shapes were skipped due to invalid pointers! "
                "Did you set the correct Expected Layer Count?")
            .arg(written)
            .arg(skipped));
  } else {
    emit logMessage(
        QString("Injection complete: %1 successfully written.")
            .arg(written));
  }

  emit finished(written > 0);
}

void FH6Injector::dumpLayerState(WinProcess& process, uint64_t tableAddress, int limit, const QString& prefix) {
  for (int d = 0; d < qMin(3, limit); ++d) {
    uint64_t dPtr = process.readPointer(tableAddress + d * 8);
    if (dPtr < 0x10000 || dPtr > 0x7FFFFFFFFFFF)
      continue;
    QByteArray raw = process.readMemory(dPtr, 0x80);
    if (raw.size() >= 0x7C) {
      float bx, by, bsx, bsy, brot, bskew;
      memcpy(&bx, raw.constData() + OFFSET_POS_X, 4);
      memcpy(&by, raw.constData() + OFFSET_POS_Y, 4);
      memcpy(&bsx, raw.constData() + OFFSET_SCALE_X, 4);
      memcpy(&bsy, raw.constData() + OFFSET_SCALE_Y, 4);
      memcpy(&brot, raw.constData() + OFFSET_ROTATION, 4);
      memcpy(&bskew, raw.constData() + OFFSET_SKEW, 4);
      uint8_t bc0 = raw.at(OFFSET_COLOR), bc1 = raw.at(OFFSET_COLOR + 1), bc2 = raw.at(OFFSET_COLOR + 2),
              bc3 = raw.at(OFFSET_COLOR + 3);
      uint8_t bmask = raw.at(OFFSET_MASK);
      uint16_t bshape;
      memcpy(&bshape, raw.constData() + OFFSET_SHAPE_TYPE, 2);
      emit logMessage(
          QString("%1 layer[%2] ptr=0x%3: pos=(%4,%5) scale=(%6,%7) "
                  "rot=%8 skew=%9 color=(%10,%11,%12,%13) mask=%14 shape=0x%15")
              .arg(prefix)
              .arg(d)
              .arg(dPtr, 0, 16)
              .arg(bx, 0, 'f', 3)
              .arg(by, 0, 'f', 3)
              .arg(bsx, 0, 'f', 6)
              .arg(bsy, 0, 'f', 6)
              .arg(brot, 0, 'f', 3)
              .arg(bskew, 0, 'f', 3)
              .arg(bc0).arg(bc1).arg(bc2).arg(bc3)
              .arg(bmask)
              .arg(bshape, 0, 16));
    }
  }
}
