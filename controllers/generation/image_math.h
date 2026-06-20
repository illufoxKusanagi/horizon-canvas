#ifndef IMAGE_MATH_H
#define IMAGE_MATH_H

#include <QImage>
#include <QVector>
#include <QPair>
#include "../../models/shape_data.h"

class ImageMath {
public:
    /**
     * @brief Computes edge weights using a Sobel filter.
     * @param target The target image to compute weights for.
     * @param boost Edge enhancement multiplier.
     * @param alphaMask Optional alpha mask for sticker mode.
     * @return A flattened vector of edge weights.
     */
    static QVector<float> computeEdgeWeight(const QImage& target, float boost = 6.0f,
                                            const QVector<float>& alphaMask = {});
    
    /**
     * @brief Precomputes the initial canvas error against the target image.
     * @param current The current canvas.
     * @param target The target image.
     * @param edgeWeight The edge weights to apply.
     * @param alphaMask Optional alpha mask.
     * @return A pair containing the full squared error and the total weight (n).
     */
    static QPair<float, float> precomputeCanvasError(const QImage& current, const QImage& target,
                                                     const QVector<float>& edgeWeight,
                                                     const QVector<float>& alphaMask = {});
    
    /**
     * @brief Composites a shape onto the canvas and returns the new error (RMS).
     * @param current The current canvas image.
     * @param shape The shape to draw. Its color will be calculated if not set.
     * @param target The target image.
     * @param edgeWeight The edge weights.
     * @param alphaMask Optional alpha mask.
     * @param presetRms If >= 0, avoids recomputing error and returns presetRms.
     * @return A pair of the new canvas image and the new RMS error.
     */
    static QPair<QImage, float> composite(const QImage& current, ShapeData& shape,
                                          const QImage& target, const QVector<float>& edgeWeight,
                                          const QVector<float>& alphaMask = {},
                                          float presetRms = -1.0f);

    /**
     * @brief Draws a shape onto the canvas using its predefined color.
     * @param current The canvas image.
     * @param shape The shape to draw.
     * @return The updated canvas image.
     */
    static QImage drawShape(const QImage& current, const ShapeData& shape);
};


#endif // IMAGE_MATH_H
