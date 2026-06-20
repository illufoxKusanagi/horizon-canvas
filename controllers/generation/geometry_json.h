#ifndef GEOMETRY_JSON_H
#define GEOMETRY_JSON_H

#include <QString>
#include <QVector>
#include "../../models/shape_data.h"
#include "../../models/generation_settings.h"

class GeometryJson {
public:
    static bool saveToFile(const QString& path, const QVector<ShapeData>& shapes,
                           int canvasWidth, int canvasHeight,
                           const QString& sourceImage, bool stickerMode = false,
                           const GenerationSettings& settings = GenerationSettings(),
                           const QString& profileName = "custom");
    static bool loadFromFile(const QString& path, QVector<ShapeData>& shapes,
                             int& canvasWidth, int& canvasHeight, QString& sourceImage,
                             GenerationSettings* settings = nullptr, QString* profileName = nullptr);
    static bool loadMetadataOnly(const QString &path, int &shapeCount,
                                 int &canvasWidth, int &canvasHeight,
                                 QString &sourceImage);
    static bool isHorizonCanvasJson(const QString &path);
};

#endif // GEOMETRY_JSON_H
