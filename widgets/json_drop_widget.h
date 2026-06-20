#ifndef JSON_DROP_WIDGET_H
#define JSON_DROP_WIDGET_H

#include <QWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QString>

class JsonDropWidget : public QWidget {
    Q_OBJECT
public:
    explicit JsonDropWidget(QWidget* parent = nullptr);
    QString getJsonPath() const { return jsonPath; }
    void setJsonPath(const QString& path);

signals:
    void jsonLoaded(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QLabel* label;
    QString jsonPath;
};

#endif // JSON_DROP_WIDGET_H
