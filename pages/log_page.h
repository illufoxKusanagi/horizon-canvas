#ifndef LOG_PAGE_H
#define LOG_PAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

class LogPage : public QWidget {
    Q_OBJECT
public:
    explicit LogPage(QWidget* parent = nullptr);
};

#endif // LOG_PAGE_H
