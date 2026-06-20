#include "zoomable_graphics_view.h"
#include <QScrollBar>
#include <QGraphicsTextItem>
#include <QFont>

ZoomableGraphicsView::ZoomableGraphicsView(const QString& placeholder, QWidget* parent)
    : QGraphicsView(parent)
    , m_placeholderText(placeholder)
    , m_hasImage(false)
    , m_isPanning(false)
    , m_zoomLevel(1.0)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_pixmapItem = new QGraphicsPixmapItem();
    m_scene->addItem(m_pixmapItem);
    m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
    m_pixmapItem->hide();

    m_placeholderItem = new QGraphicsTextItem();
    m_scene->addItem(m_placeholderItem);
    
    QFont f = m_placeholderItem->font();
    f.setPointSize(11);
    m_placeholderItem->setFont(f);
    m_placeholderItem->setDefaultTextColor(QColor("#555577"));
    m_placeholderItem->setHtml(QString("<div align='center'>%1</div>").arg(m_placeholderText));
    
    // Setup view properties
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    setMinimumSize(320, 320);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet(
        "QGraphicsView {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "      stop:0 #1c1c2e, stop:1 #12121a);"
        "  border: 1px solid #2a2a40;"
        "  border-radius: 6px;"
        "}");

    updatePlaceholder();
}

void ZoomableGraphicsView::setImage(const QImage& img) {
    if (img.isNull()) {
        clearImage();
        return;
    }

    m_hasImage = true;
    m_placeholderItem->hide();
    
    m_pixmapItem->setPixmap(QPixmap::fromImage(img));
    m_pixmapItem->show();
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    
    // Only fit in view if it's the first time setting an image, to avoid resetting zoom constantly
    if (m_zoomLevel == 1.0) {
        fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
        m_zoomLevel = transform().m11(); // Capture initial zoom scale
    }
}

void ZoomableGraphicsView::clearImage() {
    m_hasImage = false;
    m_pixmapItem->hide();
    m_placeholderItem->show();
    m_zoomLevel = 1.0;
    resetTransform();
    updatePlaceholder();
}

void ZoomableGraphicsView::wheelEvent(QWheelEvent* event) {
    if (!m_hasImage) return;
    
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    
    // Zoom In
    if (event->angleDelta().y() > 0) {
        scale(1.15, 1.15);
        m_zoomLevel *= 1.15;
    } 
    // Zoom Out
    else {
        scale(1.0 / 1.15, 1.0 / 1.15);
        m_zoomLevel /= 1.15;
    }
}

void ZoomableGraphicsView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_hasImage) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QGraphicsView::mousePressEvent(event);
}

void ZoomableGraphicsView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void ZoomableGraphicsView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning && m_hasImage) {
        int dx = event->pos().x() - m_lastMousePos.x();
        int dy = event->pos().y() - m_lastMousePos.y();
        
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - dx);
        verticalScrollBar()->setValue(verticalScrollBar()->value() - dy);
        
        m_lastMousePos = event->pos();
    }
    QGraphicsView::mouseMoveEvent(event);
}

void ZoomableGraphicsView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (!m_hasImage) {
        updatePlaceholder();
    } else {
        // If they haven't manually zoomed much, try to keep it fit to screen
        if (m_zoomLevel < 2.0) {
            fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
            m_zoomLevel = transform().m11();
        }
    }
}

void ZoomableGraphicsView::updatePlaceholder() {
    if (!m_hasImage) {
        m_scene->setSceneRect(0, 0, width(), height());
        m_placeholderItem->setPos(
            (width() - m_placeholderItem->boundingRect().width()) / 2.0,
            (height() - m_placeholderItem->boundingRect().height()) / 2.0
        );
    }
}
