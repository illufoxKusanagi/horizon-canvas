#ifndef IMAGE_DROP_WIDGET_H
#define IMAGE_DROP_WIDGET_H

#include <QWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QString>

class ImageDropWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImageDropWidget(QWidget* parent = nullptr);
    QString getCurrentImagePath() const { return imagePath; }
    void setImagePath(const QString& path);

signals:
    void imageLoaded(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QLabel* label;
    QString imagePath;
};

#endif // IMAGE_DROP_WIDGET_H
