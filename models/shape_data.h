#ifndef SHAPE_DATA_H
#define SHAPE_DATA_H

#include <QColor>

struct ShapeData {
    double x = 0.0;
    double y = 0.0;
    double rx = 10.0;
    double ry = 10.0;
    double angle = 0.0;
    int type = 102; // 102 for ellipse, 101 for rect
    QColor color;
};

#endif // SHAPE_DATA_H
