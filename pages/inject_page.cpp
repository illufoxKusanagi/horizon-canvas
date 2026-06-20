#include "inject_page.h"
#include "../controllers/generation/geometry_json.h"
#include "../controllers/injection/fh6_injector.h"
#include "../styles/text_style.h"
#include "../widgets/zoomable_graphics_view.h"
#include "../controllers/generation/image_math.h"

#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>

#include <tlhelp32.h>
#include <windows.h>

InjectPage::InjectPage(QWidget *parent)
    : QWidget(parent), injectorThread(nullptr), injector(nullptr) {
  auto mainLayout = new QVBoxLayout(this);

  auto title = new QLabel("Memory Injection (FH6)", this);
  title->setStyleSheet(TextStyle::Header1);
  mainLayout->addWidget(title);

  // --- Configuration Group ---
  auto configGroup = new QGroupBox("Configuration", this);
  auto configLayout = new QFormLayout(configGroup);

  // Process selection
  auto procLayout = new QHBoxLayout();
  processCombo = new QComboBox(this);
  refreshProcessBtn = new QPushButton("Refresh", this);
  procLayout->addWidget(processCombo, 1);
  procLayout->addWidget(refreshProcessBtn);
  configLayout->addRow("Target Process:", procLayout);

  // JSON file selection
  auto jsonLayout = new QHBoxLayout();
  jsonPathEdit = new QLineEdit(this);
  jsonPathEdit->setPlaceholderText("Select the _shapes.json file...");
  browseJsonBtn = new QPushButton("Browse...", this);
  jsonLayout->addWidget(jsonPathEdit, 1);
  jsonLayout->addWidget(browseJsonBtn);
  configLayout->addRow("Generated Shapes:", jsonLayout);

  // Layer Count
  layerCountSpin = new QSpinBox(this);
  layerCountSpin->setRange(1, 3000);
  layerCountSpin->setValue(16);
  layerCountSpin->setToolTip(
      "The EXACT number of layers in the current vinyl group in-game.");
  configLayout->addRow("Expected Layer Count:", layerCountSpin);

  mainLayout->addWidget(configGroup);

  // --- Preview Group ---
  auto previewGroup = new QGroupBox("Preview", this);
  auto previewLayout = new QVBoxLayout(previewGroup);
  
  shapeInfoLabel = new QLabel("No shape file loaded.", this);
  shapeInfoLabel->setStyleSheet("color: #aaaaaa;");
  previewLayout->addWidget(shapeInfoLabel);

  previewLabel = new ZoomableGraphicsView("Source image will appear here", this);
  previewLabel->setMinimumHeight(200);
  previewLayout->addWidget(previewLabel, 1);

  mainLayout->addWidget(previewGroup, 1);

  // --- Actions ---
  auto actionLayout = new QHBoxLayout();
  injectBtn = new QPushButton("Inject to Game", this);
  stopBtn = new QPushButton("Stop", this);
  stopBtn->setEnabled(false);
  actionLayout->addWidget(injectBtn);
  actionLayout->addWidget(stopBtn);
  actionLayout->addStretch();
  mainLayout->addLayout(actionLayout);

  // --- Progress & Logs ---
  progressBar = new QProgressBar(this);
  progressBar->setRange(0, 100);
  progressBar->setValue(0);
  mainLayout->addWidget(progressBar);

  logArea = new QTextEdit(this);
  logArea->setReadOnly(true);
  mainLayout->addWidget(logArea);

  // Connections
  connect(refreshProcessBtn, &QPushButton::clicked, this,
          &InjectPage::refreshProcesses);
  connect(browseJsonBtn, &QPushButton::clicked, this,
          &InjectPage::browseJsonFile);
  connect(injectBtn, &QPushButton::clicked, this, &InjectPage::startInjection);
  connect(stopBtn, &QPushButton::clicked, this, &InjectPage::stopInjection);

  refreshProcesses();
}

InjectPage::~InjectPage() {
  stopInjection();
  if (injectorThread) {
    injectorThread->wait();
  }
}

void InjectPage::refreshProcesses() {
  processCombo->clear();
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &pe32)) {
      do {
        QString exeName = QString::fromWCharArray(pe32.szExeFile);
        if (exeName.contains("ForzaHorizon6", Qt::CaseInsensitive)) {
          processCombo->addItem(
              QString("%1 (PID: %2)").arg(exeName).arg(pe32.th32ProcessID),
              QVariant(static_cast<qulonglong>(pe32.th32ProcessID)));
        }
      } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
  }

  if (processCombo->count() == 0) {
    processCombo->addItem("No Forza Horizon 6 process found");
  }
}

void InjectPage::browseJsonFile() {
  QString path = QFileDialog::getOpenFileName(this, "Select Generated JSON", "",
                                              "JSON Files (*.json)");
  if (!path.isEmpty()) {
    jsonPathEdit->setText(path);

    int w = 0, h = 0;
    QString srcImgName;
    QVector<ShapeData> shapes;
    
    // Load the full file to get the shapes to draw
    if (GeometryJson::loadFromFile(path, shapes, w, h, srcImgName)) {
      layerCountSpin->setValue(shapes.size());
      shapeInfoLabel->setText(QString("Loaded %1 generated shapes from JSON.").arg(shapes.size()));

      if (w > 0 && h > 0) {
          QImage previewImg(w, h, QImage::Format_ARGB32);
          previewImg.fill(Qt::transparent);
          for (const auto& s : shapes) {
              previewImg = ImageMath::drawShape(previewImg, s);
          }
          previewLabel->setImage(previewImg);
      } else {
          previewLabel->clearImage();
      }
    } else {
      shapeInfoLabel->setText("Failed to parse JSON file.");
      previewLabel->clearImage();
    }
  }
}

void InjectPage::startInjection() {
  if (processCombo->currentData().isNull()) {
    QMessageBox::warning(this, "Error",
                         "Please select a valid Forza Horizon 6 process.");
    return;
  }

  QString jsonPath = jsonPathEdit->text();
  if (jsonPath.isEmpty() || !QFileInfo::exists(jsonPath)) {
    QMessageBox::warning(this, "Error",
                         "Please select a valid shapes JSON file.");
    return;
  }

  QVector<ShapeData> shapes;
  int w = 0, h = 0;
  QString srcPath;
  if (!GeometryJson::loadFromFile(jsonPath, shapes, w, h, srcPath)) {
    QMessageBox::warning(this, "Error", "Failed to parse the JSON file.");
    return;
  }

  // Coordinates are passed through as-is (raw pixel space).
  // The injector negates Y to match the game's coordinate system.

  DWORD pid = static_cast<DWORD>(processCombo->currentData().toULongLong());
  uint16_t layerCount = static_cast<uint16_t>(layerCountSpin->value());

  injectBtn->setEnabled(false);
  stopBtn->setEnabled(true);
  progressBar->setValue(0);
  progressBar->setRange(0, qMin(shapes.size(), static_cast<int>(layerCount)));
  logArea->clear();
  appendLog("Preparing injection...");

  if (injectorThread) {
    injectorThread->wait();
    delete injectorThread;
    injectorThread = nullptr;
  }

  injectorThread = new QThread(this);
  injector = new FH6Injector();
  injector->moveToThread(injectorThread);

  connect(injectorThread, &QThread::finished, injector, &QObject::deleteLater);
  connect(injector, &FH6Injector::logMessage, this, &InjectPage::appendLog);
  connect(injector, &FH6Injector::progressUpdated, this,
          &InjectPage::updateProgress);
  connect(injector, &FH6Injector::finished, this,
          &InjectPage::injectionFinished);

  // Call injectShapes on the new thread
  connect(injectorThread, &QThread::started, [this, pid, layerCount, shapes]() {
    injector->injectShapes(pid, layerCount, shapes);
  });

  injectorThread->start();
}

void InjectPage::stopInjection() {
  if (injector && injectorThread && injectorThread->isRunning()) {
    appendLog("Requesting stop...");
    injector->stopInjection(); // Safe direct call because m_stop is atomic
    stopBtn->setEnabled(false);
  }
}

void InjectPage::appendLog(const QString &msg) {
  QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
  logArea->append(QString("[%1] %2").arg(timeStr).arg(msg));
}

void InjectPage::updateProgress(int current, int total) {
  progressBar->setValue(current);
}

void InjectPage::injectionFinished(bool success) {
  injectBtn->setEnabled(true);
  stopBtn->setEnabled(false);
  if (success) {
    progressBar->setValue(progressBar->maximum());
  }
  if (injectorThread) {
    injectorThread->quit();
  }
}
