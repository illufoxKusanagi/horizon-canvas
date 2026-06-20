#include "json_drop_widget.h"
#include <QMimeData>
#include <QVBoxLayout>
#include <QFileInfo>

JsonDropWidget::JsonDropWidget(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setMinimumHeight(200);
    setStyleSheet("border: 2px dashed #aaaaaa; border-radius: 8px; background-color: #1e1e1e;");

    auto layout = new QVBoxLayout(this);
    label = new QLabel("Drag & Drop .json save file here", this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("border: none; color: #aaaaaa; font-size: 16px; font-weight: bold;");
    layout->addWidget(label);
}

void JsonDropWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        auto url = event->mimeData()->urls().first();
        if (url.isLocalFile() && url.fileName().endsWith(".json", Qt::CaseInsensitive)) {
            event->acceptProposedAction();
        }
    }
}

void JsonDropWidget::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (!urlList.isEmpty()) {
            setJsonPath(urlList.first().toLocalFile());
        }
    }
}

void JsonDropWidget::setJsonPath(const QString& path) {
    jsonPath = path;
    QFileInfo fi(path);
    label->setText("📂 " + fi.fileName());
    emit jsonLoaded(jsonPath);
}
