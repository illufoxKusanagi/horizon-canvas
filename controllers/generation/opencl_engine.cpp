#include "opencl_engine.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#include "CL/opencl.hpp"
#include "image_math.h"

const char *KERNEL_SOURCE = R"CLC(
__kernel void search_shapes(
    __global const uchar4* target,
    __global const float* edgeWeight,
    __global const uchar4* canvas,
    __global const float* shapes,
    __global float* scoresOut,
    __global uchar4* colorsOut,
    int w, int h, float a) 
{
    int id = get_global_id(0);
    
    int base_idx = id * 5;
    float cx = shapes[base_idx];
    float cy = shapes[base_idx+1];
    float rx = shapes[base_idx+2];
    float ry = shapes[base_idx+3];
    float angle = shapes[base_idx+4];
    
    float angRad = angle * 3.14159265f / 180.0f;
    float ca = cos(angRad);
    float sa = sin(angRad);
    
    float boundX = sqrt(rx*rx*ca*ca + ry*ry*sa*sa);
    float boundY = sqrt(rx*rx*sa*sa + ry*ry*ca*ca);
    int bx = (int)(ceil(boundX));
    int by = (int)(ceil(boundY));
    
    int x0 = max(0, (int)(cx - bx));
    int y0 = max(0, (int)(cy - by));
    int x1 = min(w, (int)(cx + bx + 1));
    int y1 = min(h, (int)(cy + by + 1));
    
    float effSum = 0.0f;
    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;
    
    float body_total = 0.0f;
    float inside = 0.0f;
    
    float inv_rx = 1.0f / rx;
    float inv_ry = 1.0f / ry;
    float inv_a = 1.0f - a;
    
    float dxr = ca * inv_rx;
    float dyr = -sa * inv_ry;
    
    // Pass 1: Find optimal color and check sticker overlap
    for (int y = y0; y < y1; ++y) {
        float fy = y - cy;
        float sa_fy = sa * fy;
        float ca_fy = ca * fy;
        int rowOffset = y * w;
        
        float fx0 = x0 - cx;
        float xr = (ca * fx0 + sa_fy) * inv_rx;
        float yr = (-sa * fx0 + ca_fy) * inv_ry;
        
        for (int x = x0; x < x1; ++x) {
            if (xr*xr + yr*yr <= 1.0f) {
                int idx = rowOffset + x;
                uchar4 tp = target[idx];
                
                body_total += 1.0f;
                if (tp.w >= 128) {
                    inside += 1.0f;
                    uchar4 cp = canvas[idx];
                    float m = (float)tp.w / 255.0f;
                    effSum += m;
                    n0 += m * ((float)tp.x - inv_a*(float)cp.x);
                    n1 += m * ((float)tp.y - inv_a*(float)cp.y);
                    n2 += m * ((float)tp.z - inv_a*(float)cp.z);
                }
            }
            xr += dxr;
            yr += dyr;
        }
    }
    

    uchar4 optColor = (uchar4)(0, 0, 0, (uchar)(a*255.0f));
    if (effSum > 0.5f) {
        optColor.x = clamp((int)(n0 / (effSum * a)), 0, 255);
        optColor.y = clamp((int)(n1 / (effSum * a)), 0, 255);
        optColor.z = clamp((int)(n2 / (effSum * a)), 0, 255);
    }
    
    if (body_total > 0.0f && (inside / body_total) < 0.995f) {
        scoresOut[id] = 9999999.0f;
        colorsOut[id] = (uchar4)(0, 0, 0, 0);
        return;
    }

    // Pass 2: Calculate error difference
    float errDiff = 0.0f;
    float base_R = a * (float)optColor.x;
    float base_G = a * (float)optColor.y;
    float base_B = a * (float)optColor.z;
    
    for (int y = y0; y < y1; ++y) {
        float fy = y - cy;
        float sa_fy = sa * fy;
        float ca_fy = ca * fy;
        int rowOffset = y * w;
        
        float fx0 = x0 - cx;
        float xr = (ca * fx0 + sa_fy) * inv_rx;
        float yr = (-sa * fx0 + ca_fy) * inv_ry;
        
        for (int x = x0; x < x1; ++x) {
            if (xr*xr + yr*yr <= 1.0f) {
                int idx = rowOffset + x;
                uchar4 cp = canvas[idx];
                uchar4 tp = target[idx];
                float wgt = edgeWeight[idx];
                
                float oldR = (float)cp.x - (float)tp.x;
                float oldG = (float)cp.y - (float)tp.y;
                float oldB = (float)cp.z - (float)tp.z;
                float oldSq = oldR*oldR + oldG*oldG + oldB*oldB;
                
                float bR = base_R + inv_a*(float)cp.x;
                float bG = base_G + inv_a*(float)cp.y;
                float bB = base_B + inv_a*(float)cp.z;
                
                float newR = bR - (float)tp.x;
                float newG = bG - (float)tp.y;
                float newB = bB - (float)tp.z;
                float newSq = newR*newR + newG*newG + newB*newB;
                
                errDiff += (newSq - oldSq) * wgt;
            }
            xr += dxr;
            yr += dyr;
        }
    }
    
    scoresOut[id] = errDiff;
    colorsOut[id] = optColor;
}
)CLC";

class OpenCLEnginePrivate {
public:
  cl::Context context;
  cl::CommandQueue queue;
  cl::Program program;
  cl::Kernel kernel;

  cl::Buffer d_target;
  cl::Buffer d_edgeWeight;
  cl::Buffer d_canvas;
  cl::Buffer d_shapesIn;
  cl::Buffer d_scoresOut;
  cl::Buffer d_colorsOut;

  int width = 0;
  int height = 0;
  bool isReady = false;
  cl_uint computeUnits = 1;

  QImage m_target;
  QVector<float> m_edgeWeight;
};

OpenCLEngine::OpenCLEngine() : d(new OpenCLEnginePrivate()) {}
OpenCLEngine::~OpenCLEngine() { delete d; }

bool OpenCLEngine::init(QString &gpuName) {
  try {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty()) {
      gpuName = "No OpenCL platforms found.";
      return false;
    }

    cl::Device bestDevice;
    bool found = false;
    for (auto &p : platforms) {
      std::vector<cl::Device> pldevices;
      p.getDevices(CL_DEVICE_TYPE_GPU, &pldevices);
      if (!pldevices.empty()) {
        bestDevice = pldevices[0];
        found = true;
        break;
      }
    }

    if (!found) {
      gpuName = "No OpenCL GPU found. Falling back to CPU.";
      return false;
    }

    gpuName = QString::fromStdString(bestDevice.getInfo<CL_DEVICE_NAME>());

    cl_uint computeUnits;
    if (bestDevice.getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &computeUnits) ==
        CL_SUCCESS) {
      d->computeUnits = computeUnits;
    } else {
      d->computeUnits = 16; // Safe default for RTX 3050 fallback
    }

    d->context = cl::Context(bestDevice);
    d->queue = cl::CommandQueue(d->context, bestDevice);

    cl::Program::Sources sources;
    sources.push_back({KERNEL_SOURCE, strlen(KERNEL_SOURCE)});
    d->program = cl::Program(d->context, sources);

    if (d->program.build({bestDevice}) != CL_SUCCESS) {
      qWarning() << "OpenCL Build Error:"
                 << QString::fromStdString(
                        d->program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(
                            bestDevice));
      return false;
    }

    d->kernel = cl::Kernel(d->program, "search_shapes");
    d->isReady = true;

    return true;
  } catch (std::exception &err) {
    gpuName = QString("OpenCL Error: ") + err.what();
    return false;
  }
}

void OpenCLEngine::prepare(const QImage &target,
                           const QVector<float> &edgeWeight) {
  if (!d->isReady)
    return;

  d->width = target.width();
  d->height = target.height();
  int pixels = d->width * d->height;

  // Format target data to RGBA uchar4 array
  QVector<uint8_t> targetData(pixels * 4);
  for (int y = 0; y < d->height; ++y) {
    const QRgb* line = reinterpret_cast<const QRgb*>(target.constScanLine(y));
    int rowOffset = y * d->width;
    for (int x = 0; x < d->width; ++x) {
      QRgb p = line[x];
      int idx = (rowOffset + x) * 4;
      targetData[idx] = qRed(p);
      targetData[idx + 1] = qGreen(p);
      targetData[idx + 2] = qBlue(p);
      targetData[idx + 3] = qAlpha(p);
    }
  }
  d->m_target = target;
  d->m_edgeWeight = edgeWeight;

  // Allocate static buffers (cast sizes to size_t to avoid ambiguity)
  d->d_target = cl::Buffer(d->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           (size_t)(pixels * 4), (void *)targetData.data());
  d->d_edgeWeight =
      cl::Buffer(d->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                 (size_t)(pixels * sizeof(float)), (void *)edgeWeight.data());
  d->d_canvas = cl::Buffer(d->context, CL_MEM_READ_ONLY, (size_t)(pixels * 4));

  // Max batch size is 65536 shapes per launch to support Extreme settings
  d->d_shapesIn = cl::Buffer(d->context, CL_MEM_READ_ONLY,
                             (size_t)(65536 * 5 * sizeof(float)));
  d->d_scoresOut = cl::Buffer(d->context, CL_MEM_WRITE_ONLY,
                              (size_t)(65536 * sizeof(float)));
  d->d_colorsOut =
      cl::Buffer(d->context, CL_MEM_WRITE_ONLY, (size_t)(65536 * 4));
}

void OpenCLEngine::updateCanvas(const QVector<uint8_t> &rawRgba) {
  if (!d->isReady)
    return;
  int pixels = d->width * d->height;
  d->queue.enqueueWriteBuffer(d->d_canvas, CL_TRUE, 0, pixels * 4,
                              rawRgba.constData());
}

void OpenCLEngine::updateCanvasRegion(const QVector<uint8_t> &rawRgba, int x0, int y0, int x1, int y1) {
  if (!d->isReady)
    return;
  int w = d->width;
  x0 = std::max(0, x0);
  y0 = std::max(0, y0);
  x1 = std::min(w, x1);
  y1 = std::min(d->height, y1);

  if (x0 >= x1 || y0 >= y1) return;

  int rowSize = (x1 - x0) * 4;
  for (int y = y0; y < y1; ++y) {
    size_t offset = (static_cast<size_t>(y) * w + x0) * 4;
    d->queue.enqueueWriteBuffer(d->d_canvas, CL_FALSE, offset, rowSize,
                                rawRgba.constData() + offset);
  }
}

QPair<float, ShapeData> OpenCLEngine::search(const QImage &canvas, int nRandom,
                                             int nMutate, float maxSizeFrac,
                                             int maxThreads, std::mt19937 &rng,
                                             float baselineErr, float nTotal) {
  Q_UNUSED(canvas);
  if (!d->isReady) {
    ShapeData dummy;
    return {std::numeric_limits<float>::infinity(), dummy};
  }

  int w = d->width;
  int h = d->height;

  auto evaluateBatch =
      [&](const QVector<ShapeData> &candidates) -> QPair<float, ShapeData> {
    int totalCandidates = candidates.size();
    if (totalCandidates == 0)
      return {std::numeric_limits<float>::infinity(), ShapeData()};

    float globalBestDiff = std::numeric_limits<float>::max();
    ShapeData globalBestShape = candidates[0];

    int chunkSize = 65536;
    for (int offset = 0; offset < totalCandidates; offset += chunkSize) {
      int count = std::min(chunkSize, totalCandidates - offset);

      QVector<float> shapesFlat(count * 5);
      for (int i = 0; i < count; ++i) {
        const ShapeData &s = candidates[offset + i];
        shapesFlat[i * 5 + 0] = s.x;
        shapesFlat[i * 5 + 1] = s.y;
        shapesFlat[i * 5 + 2] = s.rx;
        shapesFlat[i * 5 + 3] = s.ry;
        shapesFlat[i * 5 + 4] = s.angle;
      }

      d->queue.enqueueWriteBuffer(d->d_shapesIn, CL_TRUE, 0,
                                  count * 5 * sizeof(float), shapesFlat.data());

      d->kernel.setArg(0, d->d_target);
      d->kernel.setArg(1, d->d_edgeWeight);
      d->kernel.setArg(2, d->d_canvas);
      d->kernel.setArg(3, d->d_shapesIn);
      d->kernel.setArg(4, d->d_scoresOut);
      d->kernel.setArg(5, d->d_colorsOut);
      d->kernel.setArg(6, w);
      d->kernel.setArg(7, h);
      d->kernel.setArg(8, 128.0f / 255.0f); // alpha

      d->queue.enqueueNDRangeKernel(d->kernel, cl::NullRange,
                                    cl::NDRange(count), cl::NullRange);

      QVector<float> outScores(count);
      QVector<uint8_t> outColors(count * 4);
      d->queue.enqueueReadBuffer(d->d_scoresOut, CL_FALSE, 0,
                                 count * sizeof(float), outScores.data());
      d->queue.enqueueReadBuffer(d->d_colorsOut, CL_FALSE, 0, count * 4,
                                 outColors.data());
      d->queue.finish();

      for (int i = 0; i < count; ++i) {
        float sc = outScores[i];
        if (sc < globalBestDiff) {
          globalBestDiff = sc;
          globalBestShape = candidates[offset + i];
          globalBestShape.color = QColor(outColors[i * 4], outColors[i * 4 + 1],
                                         outColors[i * 4 + 2], 128);
        }
      }
    }

    return {globalBestDiff, globalBestShape};
  };

  // 1. Evaluate randoms
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

  auto bestRes = evaluateBatch(candidates);
  float bestDiff = bestRes.first;
  ShapeData bestShape = bestRes.second;

  // 2. Hill climb
  int batch;
  if (maxThreads > 0) {
    batch = maxThreads * 256;
  } else {
    batch = d->computeUnits * 256;
  }
  batch = std::max(256, std::min(nMutate, std::min(batch, 65536)));

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
        m.x += std::normal_distribution<double>(0.0,
                                                std::max(2.0, m.rx * 0.2))(rng);
        m.y += std::normal_distribution<double>(0.0,
                                                std::max(2.0, m.ry * 0.2))(rng);
      } else if (kind == 1) {
        m.rx += std::normal_distribution<double>(
            0.0, std::max(1.0, m.rx * 0.15))(rng);
        m.ry += std::normal_distribution<double>(
            0.0, std::max(1.0, m.ry * 0.15))(rng);
      } else if (kind == 2) {
        m.angle += std::normal_distribution<double>(0.0, 25.0)(rng);
      } else {
        m.x += std::normal_distribution<double>(0.0,
                                                std::max(1.0, m.rx * 0.1))(rng);
        m.angle += std::normal_distribution<double>(0.0, 15.0)(rng);
      }
      m.x = std::clamp(m.x, 0.0, static_cast<double>(w - 1));
      m.y = std::clamp(m.y, 0.0, static_cast<double>(h - 1));
      m.rx = std::clamp(m.rx, 1.0, static_cast<double>(maxRx));
      m.ry = std::clamp(m.ry, 1.0, static_cast<double>(maxRy));
      muts.append(m);
    }

    auto mRes = evaluateBatch(muts);
    if (mRes.first < bestDiff) {
      bestDiff = mRes.first;
      bestShape = mRes.second;
      no_improve = 0;
    } else {
      no_improve++;
      if (no_improve >= std::max(2, steps / 4))
        break;
    }
  }
  float finalRMS =
      nTotal > 0 ? std::sqrt(std::max(0.0f, baselineErr + bestDiff) / nTotal)
                 : 0.0f;
  return {finalRMS, bestShape};
}
