#include "image_drop_widget.h"
#include <QMimeData>
#include <QVBoxLayout>

ImageDropWidget::ImageDropWidget(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setMinimumHeight(200);
    setStyleSheet("border: 2px dashed #aaaaaa; border-radius: 8px; background-color: #1e1e1e;");

    auto layout = new QVBoxLayout(this);
    label = new QLabel("Drag & Drop .png or .jpg image here", this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("border: none; color: #aaaaaa; font-size: 16px; font-weight: bold;");
    layout->addWidget(label);
}

void ImageDropWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        auto url = event->mimeData()->urls().first();
        if (url.isLocalFile() && (url.fileName().endsWith(".png") || url.fileName().endsWith(".jpg"))) {
            event->acceptProposedAction();
        }
    }
}

void ImageDropWidget::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (!urlList.isEmpty()) {
            setImagePath(urlList.first().toLocalFile());
        }
    }
}

void ImageDropWidget::setImagePath(const QString& path) {
    imagePath = path;
    QImage img(imagePath);
    if (!img.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(img);
        label->setPixmap(pixmap.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        emit imageLoaded(imagePath);
    }
}
