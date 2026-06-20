#include "rtti_locator.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSet>
#include <cmath>
#define FH6_COUNT_OFFSET 0x60
#define FH6_TABLE_OFFSET 0x80

RTTILocator::RTTILocator(const WinProcess &process, Logger logger)
    : m_process(process), m_logger(logger) {}

void RTTILocator::log(const QString &msg) {
  if (m_logger)
    m_logger(msg);
  qDebug().noquote() << msg;
}

QByteArray RTTILocator::readRegion(uint64_t base, size_t size) {
  if (size == 0 || size > 512 * 1024 * 1024)
    return QByteArray();
  return m_process.readMemory(base, size);
}

// Returns the number of valid-looking layer pointers out of all
// expectedLayerCount entries. To avoid false positives on random pointer
// arrays, we deeply inspect the memory of up to 64 pointers to see if they
// contain valid layer struct data (plausible floats).
int RTTILocator::scoreTable(uint64_t tableAddress, uint16_t layerCount) {
  // STRICT 16-layer sample: if the first 16 layers aren't perfect, reject instantly.
  int sampleN = qMin(static_cast<int>(layerCount), 16);
  for (int i = 0; i < sampleN; ++i) {
    uint64_t layerPtr = m_process.readPointer(tableAddress + i * 8);
    if (layerPtr < 0x10000 || layerPtr >= 0x7FF000000000ULL) return 0;
    QByteArray layerData = m_process.readMemory(layerPtr, 0x80);
    if (layerData.size() < 0x7C) return 0;
    
    float x, y, sx, sy;
    memcpy(&x, layerData.constData() + 0x18, 4);
    memcpy(&y, layerData.constData() + 0x1C, 4);
    memcpy(&sx, layerData.constData() + 0x28, 4);
    memcpy(&sy, layerData.constData() + 0x2C, 4);
    uint8_t shapeByte = layerData.at(0x7A);
    uint8_t maskByte = layerData.at(0x78);
    
    bool ok = std::isfinite(x) && std::isfinite(y) && x >= -8192.0f && x <= 8192.0f && y >= -8192.0f && y <= 8192.0f &&
              std::isfinite(sx) && std::isfinite(sy) && sx > 0.0f && sx <= 64.0f && sy > 0.0f && sy <= 64.0f &&
              (shapeByte == 101 || shapeByte == 102) &&
              (maskByte == 0 || maskByte == 1);
    if (!ok) return 0;
  }

  int validPointers = 0;
  // Passed the strict gate! Now evaluate the whole table.
  for (int i = 0; i < static_cast<int>(layerCount); ++i) {
    uint64_t layerPtr = m_process.readPointer(tableAddress + i * 8);
    if (layerPtr < 0x10000 || layerPtr >= 0x7FF000000000ULL) continue;
    QByteArray layerData = m_process.readMemory(layerPtr, 0x80);
    if (layerData.size() < 0x7C) continue;
    
    float x, y, sx, sy;
    memcpy(&x, layerData.constData() + 0x18, 4);
    memcpy(&y, layerData.constData() + 0x1C, 4);
    memcpy(&sx, layerData.constData() + 0x28, 4);
    memcpy(&sy, layerData.constData() + 0x2C, 4);
    uint8_t shapeByte = layerData.at(0x7A);
    uint8_t maskByte = layerData.at(0x78);
    
    if (std::isfinite(x) && std::isfinite(y) && x >= -8192.0f && x <= 8192.0f && y >= -8192.0f && y <= 8192.0f &&
        std::isfinite(sx) && std::isfinite(sy) && sx > 0.0f && sx <= 64.0f && sy > 0.0f && sy <= 64.0f &&
        (shapeByte == 101 || shapeByte == 102) &&
        (maskByte == 0 || maskByte == 1)) {
        validPointers++;
    }
  }
  return validPointers;
}

// Direct heap scan for the active CLiveryGroup in FH6.
// FH6 has RTTI stripped, so we scan the heap directly for the expected
// layer count as a uint16, then validate surrounding fields to find the real
// struct.
QVector<LiveryGroupInfo>
RTTILocator::locateGroups(uint16_t expectedLayerCount) {
  QVector<LiveryGroupInfo> groups;

  // Candidate (structBase, countOffset, tableOffset) layouts to try.
  // We'll compute structBase = hitAddress - countOffset, then read the table at
  // structBase + tableOffset.
  struct Layout {
    int countOff;
    int tableOff;
  };
  // Confirmed FH6 offsets and fallback candidates for different UI tabs
  const QVector<Layout> layouts = {
      {0x60, 0x80}, // Vinyl Group Editor
      {0x5A, 0x78}, // Car Livery Editor
      {0x58, 0x70}, {0x5C, 0x78}, {0x54, 0x70},
      {0x50, 0x68}, {0x64, 0x88}, {0x68, 0x90}, {0x70, 0x98},
  };

  // The u16 pattern to search for
  uint16_t target = expectedLayerCount;
  QByteArray pattern(reinterpret_cast<const char *>(&target), 2);

  auto regions = m_process.getRegions(MEM_PRIVATE, true);

  // Sort regions by size descending.
  // FH6 usually places the ACTIVE Live layer group in massive heap regions.
  // Smaller regions often contain Undo/Redo or Clipboard buffers.
  std::sort(regions.begin(), regions.end(),
            [](const MemoryRegion &a, const MemoryRegion &b) {
              return a.size > b.size;
            });

  int totalRegions = regions.size();
  log(QString("Direct count scan: searching %1 heap regions for layer count %2 "
              "(largest first)...")
          .arg(totalRegions)
          .arg(expectedLayerCount));

  int regionsScanned = 0;
  int totalHits = 0;
  LiveryGroupInfo bestCandidate;
  int bestScore = 0;

  for (const auto &region : regions) {
    regionsScanned++;
    if (regionsScanned % 1000 == 0) {
      log(QString("  Progress: %1/%2 regions, %3 count hits, best score: %4")
              .arg(regionsScanned)
              .arg(totalRegions)
              .arg(totalHits)
              .arg(bestScore));
      QCoreApplication::processEvents();
    }

    QByteArray memory = readRegion(region.baseAddress, region.size);
    if (memory.isEmpty())
      continue;

    int start = 0;
    while (true) {
      int pos = memory.indexOf(pattern, start);
      if (pos == -1)
        break;

      uint64_t hitAddress = region.baseAddress + pos;
      start = pos + 1;
      totalHits++;

      // Try each candidate layout
      for (const Layout &layout : layouts) {
        if (pos < layout.countOff)
          continue; // would underflow

        // The count hit is at hitAddress, which should equal structBase +
        // layout.countOff
        uint64_t structBase = hitAddress - layout.countOff;

        // Make sure structBase is in this region
        if (structBase < region.baseAddress)
          continue;

        // Read the table pointer
        int tableFieldPos = pos - layout.countOff + layout.tableOff;
        if (tableFieldPos + 8 > memory.size())
          continue;

        uint64_t tableAddress;
        memcpy(&tableAddress, memory.constData() + tableFieldPos, 8);

        // Must be a plausible heap pointer:
        //   - Within user-space range
        //   - NOT in module image range (0x7FF0_0000_0000+), which is where
        //     exe/dll code lives on Windows x64. Writing there crashes the
        //     game.
        if (tableAddress < 0x10000 || tableAddress > 0x7FFFFFFFFFFF)
          continue;
        if (tableAddress >= 0x7FF000000000ULL)
          continue; // module image — skip

        // Score: how many of ALL table entries look like valid heap pointers
        int score = scoreTable(tableAddress, expectedLayerCount);
        int total = static_cast<int>(expectedLayerCount);

        // Require at least 95% valid — a real vinyl table has all slots filled.
        // Our previous false positive only had 45% (899/2000).
        int minScore = total * 95 / 100;
        if (score >= minScore && score > bestScore) {
          bestScore = score;
          bestCandidate.groupAddress = structBase;
          bestCandidate.countAddress = hitAddress;
          bestCandidate.tableField = structBase + layout.tableOff;
          bestCandidate.tableAddress = tableAddress;
          bestCandidate.currentCount = expectedLayerCount;
          bestCandidate.score = score;

          log(QString("  Candidate: structBase=0x%1 countOff=0x%2 "
                      "tableOff=0x%3 score=%4/%5")
                  .arg(structBase, 0, 16)
                  .arg(layout.countOff, 0, 16)
                  .arg(layout.tableOff, 0, 16)
                  .arg(score)
                  .arg(total));

          // Perfect score — stop immediately
          if (score >= total) {
            log(QString("Perfect match found! Stopping scan early."));
            groups.append(bestCandidate);
            return groups;
          }
        }
      }
    }
  }

  log(QString("Scan complete. Regions: %1, count hits: %2, best score: %3")
          .arg(totalRegions)
          .arg(totalHits)
          .arg(bestScore));

  if (bestScore > 0) {
    log(QString("Using best candidate (score %1).").arg(bestScore));
    groups.append(bestCandidate);
  } else if (totalHits == 0) {
    log("No occurrences of the count value were found in heap memory.");
    log("Make sure you are inside the vinyl group editor in Forza Horizon 6.");
  } else {
    log(QString(
            "Found %1 count hits but none had a valid pointer table nearby.")
            .arg(totalHits));
    log("The layer table offset is likely different for this version of FH6.");
    log("Try using a different number of layers (e.g. 100, 200) to reduce "
        "false positives.");
  }

  return groups;
}
