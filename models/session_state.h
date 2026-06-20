#ifndef SESSION_STATE_H
#define SESSION_STATE_H

#include <QImage>
#include <QVector>
#include <QString>
#include "shape_data.h"

struct SessionState {
    QString imagePath;
    QImage originalImage;
    QImage currentCanvas;
    QVector<ShapeData> shapes;
    int generatedCount = 0;
    bool isRunning = false;
};

#endif // SESSION_STATE_H
