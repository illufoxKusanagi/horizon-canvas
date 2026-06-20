#ifndef IMAGE_PREPROCESSOR_H
#define IMAGE_PREPROCESSOR_H

#include <QImage>
#include <QVector>
#include <QString>

struct PreparedImage {
    QImage target;            // RGB image ready for generation
    QVector<float> alphaMask; // 1.0 = allowed, 0.0 = forbidden (transparent + edge buffer)
    bool stickerMode;         // true if transparency was auto-detected
    int padPixels;            // number of pixels added as edge buffer on each side
};

class ImagePreprocessor {
public:
    // Prepares an image for generation.
    // - Auto-detects sticker mode (transparent PNG).
    // - Always applies an 8% edge buffer ring.
    // - Mode can be "none", "blur", or "outline".
    // - Returns a PreparedImage struct with the processed target and alpha mask.
    static PreparedImage prepare(const QImage& source, const QString& mode = "none");
};

#endif // IMAGE_PREPROCESSOR_H
