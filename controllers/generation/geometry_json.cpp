#include "geometry_json.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

bool GeometryJson::saveToFile(const QString& path, const QVector<ShapeData>& shapes,
                              int canvasWidth, int canvasHeight,
                              const QString& sourceImage, bool stickerMode,
                              const GenerationSettings& settings,
                              const QString& profileName) {
    QJsonObject root;
    root["format"] = "fd6.shapes";
    root["version"] = 1;
    root["source_image"] = sourceImage;
    QJsonArray sizeArr;
    sizeArr.append(canvasWidth);
    sizeArr.append(canvasHeight);
    root["image_size"] = sizeArr;
    root["shape_count"] = shapes.size();
    root["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["profile"] = profileName;
    root["sticker_mode"] = stickerMode;

    QJsonObject settingsObj;
    settingsObj["max_threads"] = settings.maxThreads;
    settingsObj["random_samples"] = settings.randomSamples;
    settingsObj["mutate_samples"] = settings.mutateSamples;
    settingsObj["max_shapes"] = settings.maxShapes;
    settingsObj["max_resolution"] = settings.maxResolution;
    settingsObj["save_interval"] = settings.saveInterval;
    settingsObj["custom_checkpoints"] = settings.customCheckpoints;
    settingsObj["use_opencl"] = settings.useOpenCL;
    settingsObj["preprocess_mode"] = settings.preprocessMode;
    root["settings"] = settingsObj;
    
    QJsonArray shapesArray;
    for (const auto& shape : shapes) {
        QJsonObject obj;
        obj["type"] = (shape.type == 102) ? "rotated_ellipse" : "rotated_rect";
        obj["x"] = shape.x;
        obj["y"] = shape.y;
        obj["rx"] = shape.rx;
        obj["ry"] = shape.ry;
        obj["angle"] = shape.angle;
        QJsonArray c;
        c.append(shape.color.red());
        c.append(shape.color.green());
        c.append(shape.color.blue());
        c.append(shape.color.alpha());
        obj["color"] = c;
        shapesArray.append(obj);
    }
    root["shapes"] = shapesArray;

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(doc.toJson());
    return true;
}

bool GeometryJson::loadFromFile(const QString& path, QVector<ShapeData>& shapes,
                                 int& canvasWidth, int& canvasHeight, QString& sourceImage,
                                 GenerationSettings* settings, QString* profileName) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) return false;
    
    QJsonObject root = doc.object();
    if (root.contains("source_image")) sourceImage = root["source_image"].toString();
    if (root.contains("image_size")) {
        QJsonArray sizeArr = root["image_size"].toArray();
        if (sizeArr.size() == 2) {
            canvasWidth = sizeArr[0].toInt();
            canvasHeight = sizeArr[1].toInt();
        }
    }

    if (profileName && root.contains("profile")) {
        *profileName = root["profile"].toString();
    }

    if (settings && root.contains("settings")) {
        QJsonObject settingsObj = root["settings"].toObject();
        if (settingsObj.contains("max_threads")) settings->maxThreads = settingsObj["max_threads"].toInt();
        if (settingsObj.contains("random_samples")) settings->randomSamples = settingsObj["random_samples"].toInt();
        if (settingsObj.contains("mutate_samples")) settings->mutateSamples = settingsObj["mutate_samples"].toInt();
        if (settingsObj.contains("max_shapes")) settings->maxShapes = settingsObj["max_shapes"].toInt();
        if (settingsObj.contains("max_resolution")) settings->maxResolution = settingsObj["max_resolution"].toInt();
        if (settingsObj.contains("save_interval")) settings->saveInterval = settingsObj["save_interval"].toInt();
        if (settingsObj.contains("custom_checkpoints")) settings->customCheckpoints = settingsObj["custom_checkpoints"].toString();
        if (settingsObj.contains("use_opencl")) settings->useOpenCL = settingsObj["use_opencl"].toBool();
        if (settingsObj.contains("preprocess_mode")) settings->preprocessMode = settingsObj["preprocess_mode"].toString();
    }
    
    QJsonArray arr = root["shapes"].toArray();
    shapes.clear();
    for (int i=0; i<arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        ShapeData s;
        QString t = obj["type"].toString();
        s.type = (t == "rotated_ellipse") ? 102 : 101;
        s.x = obj["x"].toDouble();
        s.y = obj["y"].toDouble();
        s.rx = obj["rx"].toDouble();
        s.ry = obj["ry"].toDouble();
        s.angle = obj["angle"].toDouble();
        
        QJsonArray c = obj["color"].toArray();
        if (c.size() >= 4) {
            s.color = QColor(c[0].toInt(), c[1].toInt(), c[2].toInt(), c[3].toInt());
        }
        shapes.append(s);
    }
    return true;
}

bool GeometryJson::isHorizonCanvasJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.read(1024)); // Only read first 1KB
    if (doc.isNull() || !doc.isObject()) return false;
    QJsonObject root = doc.object();
    return root.contains("format") && root["format"].toString() == "fd6.shapes";
}

bool GeometryJson::loadMetadataOnly(const QString &path, int &shapeCount,
                                    int &canvasWidth, int &canvasHeight,
                                    QString &sourceImage) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    if (root.contains("source_image")) sourceImage = root["source_image"].toString();
    if (root.contains("image_size")) {
        QJsonArray sizeArr = root["image_size"].toArray();
        if (sizeArr.size() == 2) {
            canvasWidth = sizeArr[0].toInt();
            canvasHeight = sizeArr[1].toInt();
        }
    }
    if (root.contains("shape_count")) {
        shapeCount = root["shape_count"].toInt();
    } else if (root.contains("shapes")) {
        shapeCount = root["shapes"].toArray().size();
    } else {
        shapeCount = 0;
    }
    return true;
}
