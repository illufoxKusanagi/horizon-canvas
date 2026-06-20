#include "log_page.h"
#include "../styles/text_style.h"

LogPage::LogPage(QWidget* parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    auto title = new QLabel("Generation Log", this);
    title->setStyleSheet(TextStyle::Header1);
    layout->addWidget(title);
    layout->addStretch();
}
