#include "mainwindow.h"
#include "../pages/generate_page.h"
#include "../pages/resume_page.h"
#include "../pages/inject_page.h"
#include "../pages/log_page.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  auto centralWidget = new QWidget(this);
  auto mainLayout = new QHBoxLayout(centralWidget);

  // Sidebar
  auto sidebarLayout = new QVBoxLayout();
  auto btnGenerate = new QPushButton("Generate", this);
  auto btnResume = new QPushButton("Resume", this);
  auto btnInject = new QPushButton("Inject", this);
  auto btnLog = new QPushButton("Log", this);

  sidebarLayout->addWidget(btnGenerate);
  sidebarLayout->addWidget(btnResume);
  sidebarLayout->addWidget(btnInject);
  sidebarLayout->addWidget(btnLog);
  sidebarLayout->addStretch();

  // Pages
  pageSwitcher = new QStackedWidget(this);
  pageSwitcher->addWidget(new GeneratePage(this));
  pageSwitcher->addWidget(new ResumePage(this));
  pageSwitcher->addWidget(new InjectPage(this));
  pageSwitcher->addWidget(new LogPage(this));

  mainLayout->addLayout(sidebarLayout);
  mainLayout->addWidget(pageSwitcher, 1);

  setCentralWidget(centralWidget);
  resize(1024, 768);
  setWindowTitle("Horizon Canvas");

  connect(btnGenerate, &QPushButton::clicked,
          [this]() { pageSwitcher->setCurrentIndex(0); });
  connect(btnResume, &QPushButton::clicked,
          [this]() { pageSwitcher->setCurrentIndex(1); });
  connect(btnInject, &QPushButton::clicked,
          [this]() { pageSwitcher->setCurrentIndex(2); });
  connect(btnLog, &QPushButton::clicked,
          [this]() { pageSwitcher->setCurrentIndex(3); });
}

MainWindow::~MainWindow() {}
