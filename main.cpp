#include "mainwindow/mainwindow.h"

#include <QApplication>

#include <QIcon>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(":/app_icon.png"));
  MainWindow w;
  w.show();
  return QApplication::exec();
}
