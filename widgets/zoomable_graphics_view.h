#ifndef ZOOMABLE_GRAPHICS_VIEW_H
#define ZOOMABLE_GRAPHICS_VIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QMouseEvent>

class ZoomableGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ZoomableGraphicsView(const QString& placeholder, QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void clearImage();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updatePlaceholder();

    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pixmapItem;
    QGraphicsTextItem* m_placeholderItem;
    QString m_placeholderText;
    bool m_hasImage;
    bool m_isPanning;
    QPoint m_lastMousePos;
    double m_zoomLevel;
};

#endif // ZOOMABLE_GRAPHICS_VIEW_H
