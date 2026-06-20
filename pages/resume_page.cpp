#include "resume_page.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>
#include <QUrl>

#include "../controllers/generation/geometry_json.h"
#include "../controllers/generation/image_math.h"
#include "../styles/text_style.h"

// ─── helpers ────────────────────────────────────────────────────────────────

static QLabel* makeCaption(QWidget* parent, const QString& text) {
    auto* lbl = new QLabel(text, parent);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(
        "QLabel {"
        "  color: #8888aa;"
        "  font-size: 11px;"
        "  letter-spacing: 1px;"
        "  text-transform: uppercase;"
        "  padding: 3px 0;"
        "}");
    return lbl;
}

// ─── constructor ─────────────────────────────────────────────────────────────

ResumePage::ResumePage(QWidget* parent)
    : QWidget(parent), generatorThread(nullptr), generator(nullptr)
{
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(16);

    // ── LEFT PANEL (controls) ────────────────────────────────────────────────
    auto* leftPanel = new QVBoxLayout();
    leftPanel->setSpacing(8);

    auto* title = new QLabel("Resume Generation", this);
    title->setStyleSheet(TextStyle::Header1);
    leftPanel->addWidget(title);

    // Drop + buttons row
    auto* dropLayout = new QHBoxLayout();
    dropWidget = new JsonDropWidget(this);
    dropLayout->addWidget(dropWidget, 1);

    auto* buttonsLayout = new QVBoxLayout();
    btnBrowse = new QPushButton("Choose Save File...", this);
    buttonsLayout->addWidget(btnBrowse);
    buttonsLayout->addStretch();
    dropLayout->addLayout(buttonsLayout);
    leftPanel->addLayout(dropLayout);

    leftPanel->addWidget(new QLabel("Quality Preset:", this));
    qualitySelector = new QualitySelector(this);
    // Set "Extreme" as default preset for resumed sessions
    qualitySelector->setCurrentPresetName("Extreme");
    leftPanel->addWidget(qualitySelector);

    settingsPanel = new CustomSettingsPanel(this);
    leftPanel->addWidget(settingsPanel);

    auto* btnLayout = new QHBoxLayout();
    btnStart = new QPushButton("Start Generation", this);
    btnStop  = new QPushButton("Stop", this);
    btnStop->setEnabled(false);
    btnLayout->addWidget(btnStart);
    btnLayout->addWidget(btnStop);
    leftPanel->addLayout(btnLayout);
    leftPanel->addStretch();

    // ── RIGHT PANEL (side-by-side comparison + log) ──────────────────────────
    auto* rightPanel = new QVBoxLayout();
    rightPanel->setSpacing(6);

    // Header row: "Original" | divider | "Generated  (N shapes)"
    auto* headerRow = new QHBoxLayout();

    originalCaption = makeCaption(this, "Original");
    shapeCountLabel = new QLabel("", this);
    shapeCountLabel->setStyleSheet(
        "QLabel {"
        "  color: #55aaff;"
        "  font-size: 11px;"
        "  letter-spacing: 1px;"
        "}");

    auto* genCaptionRow = new QHBoxLayout();
    previewCaption = makeCaption(this, "Generated");
    genCaptionRow->addWidget(previewCaption);
    genCaptionRow->addWidget(shapeCountLabel);
    genCaptionRow->addStretch();

    headerRow->addWidget(originalCaption, 1, Qt::AlignCenter);
    headerRow->addLayout(genCaptionRow, 1);
    rightPanel->addLayout(headerRow);

    // Image panels side by side
    auto* imagesRow = new QHBoxLayout();
    imagesRow->setSpacing(8);

    originalLabel = new ZoomableGraphicsView("Browse a checkpoint JSON<br>to see original here", this);
    previewLabel  = new ZoomableGraphicsView("Current saved state shapes<br>will appear here", this);

    imagesRow->addWidget(originalLabel, 1);

    // Thin vertical divider
    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setStyleSheet("color: #2a2a40;");
    imagesRow->addWidget(divider);

    imagesRow->addWidget(previewLabel, 1);
    rightPanel->addLayout(imagesRow, 1);

    // Log output
    logOutput = new QTextEdit(this);
    logOutput->setReadOnly(true);
    logOutput->setMaximumHeight(140);
    logOutput->setStyleSheet(
        "QTextEdit {"
        "  background: #0f0f18;"
        "  color: #88cc88;"
        "  font-family: 'Consolas', monospace;"
        "  font-size: 11px;"
        "  border: 1px solid #2a2a40;"
        "  border-radius: 4px;"
        "}");
    rightPanel->addWidget(logOutput);

    // Saved-file path indicator
    savedPathLabel = new QLabel("", this);
    savedPathLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    savedPathLabel->setWordWrap(false);
    savedPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    savedPathLabel->setStyleSheet(
        "QLabel {"
        "  color: #5588ff;"
        "  font-size: 10px;"
        "  font-family: 'Consolas', monospace;"
        "  padding: 2px 4px;"
        "  background: #0a0a14;"
        "  border: 1px solid #1e2a40;"
        "  border-radius: 3px;"
        "}");
    savedPathLabel->hide();
    rightPanel->addWidget(savedPathLabel);

    // ── assemble ─────────────────────────────────────────────────────────────
    rootLayout->addLayout(leftPanel, 1);
    rootLayout->addLayout(rightPanel, 3);   // give comparison panel more room

    connect(btnBrowse,  &QPushButton::clicked, this, &ResumePage::onBrowseClicked);
    connect(dropWidget, &JsonDropWidget::jsonLoaded, this, &ResumePage::loadJsonFile);
    connect(btnStart,   &QPushButton::clicked, this, &ResumePage::onStartClicked);
    connect(btnStop,    &QPushButton::clicked, this, &ResumePage::onStopClicked);

    // Open the output folder when clicking the path label
    connect(savedPathLabel, &QLabel::linkActivated, this, [](const QString& link) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(link));
    });
}

ResumePage::~ResumePage() {
    onStopClicked();
}

// ─── helpers ─────────────────────────────────────────────────────────────────

void ResumePage::showOriginalImage(const QImage& img) {
    originalLabel->setImage(img);
}

// ─── slots ───────────────────────────────────────────────────────────────────

void ResumePage::onBrowseClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this, "Select Save File", "",
        "JSON (*.json)");
    if (!fileName.isEmpty()) {
        dropWidget->setJsonPath(fileName);
    }
}

void ResumePage::loadJsonFile(const QString& path) {
    if (!path.isEmpty()) {
        jsonFilePath = path;
        logOutput->append("Loaded project save: " + QFileInfo(path).fileName());
        logOutput->append("Click 'Start Generation' to resume drawing from this file!");

        // Try to show the source image and render the saved shapes
        int w = 0, h = 0;
        QString srcImgName;
        QVector<ShapeData> shapes;
        GenerationSettings loadedSettings;
        QString profileName;
        if (GeometryJson::loadFromFile(path, shapes, w, h, srcImgName, &loadedSettings, &profileName)) {
            // Restore settings to controls
            settingsPanel->setSettings(loadedSettings);
            if (!profileName.isEmpty() && profileName != "custom") {
                qualitySelector->setCurrentPresetName(profileName);
                logOutput->append(QString("Auto-applied quality preset: %1").arg(profileName));
            } else if (profileName == "custom") {
                QualityPreset customPreset{
                    loadedSettings.maxShapes,
                    loadedSettings.maxResolution,
                    loadedSettings.randomSamples,
                    loadedSettings.mutateSamples,
                    loadedSettings.customCheckpoints,
                    loadedSettings.preprocessMode
                };
                qualitySelector->setCustomSettings(customPreset);
                logOutput->append("Auto-applied custom quality settings.");
            }

            if (shapes.size() > 0) {
                qualitySelector->setMaxShapes(shapes.size());
                logOutput->append(QString("Auto-set expected shapes to %1.").arg(shapes.size()));
            }
            QString srcPath = QFileInfo(path).absoluteDir().filePath(srcImgName);
            QImage src(srcPath);
            if (!src.isNull()) showOriginalImage(src);

            if (w > 0 && h > 0) {
                QImage previewImg(w, h, QImage::Format_ARGB32);
                previewImg.fill(Qt::transparent);
                for (const auto& s : shapes) {
                    previewImg = ImageMath::drawShape(previewImg, s);
                }
                previewLabel->setImage(previewImg);
                shapeCountLabel->setText(QString::number(shapes.size()));
            }
        }
    }
}

void ResumePage::onStartClicked() {
    if (jsonFilePath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please browse and select a checkpoint JSON file first.");
        return;
    }

    btnStart->setEnabled(false);
    btnStop->setEnabled(true);
    btnBrowse->setEnabled(false);
    dropWidget->setEnabled(false);

    GenerationSettings settings = settingsPanel->getSettings();
    settings.randomSamples = qualitySelector->getRandomSamples();
    settings.mutateSamples = qualitySelector->getMutateSamples();
    settings.maxShapes = qualitySelector->getMaxShapes();
    settings.maxResolution = qualitySelector->getMaxResolution();
    settings.customCheckpoints = qualitySelector->getCheckpoints();
    settings.preprocessMode = qualitySelector->getPreprocessMode();

    SessionState state;
    state.imagePath = jsonFilePath;

    if (!loadResumeState(state)) {
        return;
    }

    logOutput->append(QString("Effective generation resolution: %1x%2").arg(state.originalImage.width()).arg(state.originalImage.height()));

    generatorThread = new QThread(this);
    generator       = new ShapeGenerator();
    generator->moveToThread(generatorThread);

    connect(generatorThread, &QThread::started, [=]() {
        generator->startGeneration(state, settings);
    });
    connect(generator, &ShapeGenerator::shapeAdded,   this, &ResumePage::onShapeAdded);
    connect(generator, &ShapeGenerator::logMessage,   this, &ResumePage::onLogMessage);
    connect(generator, &ShapeGenerator::fileSaved,    this, &ResumePage::onFileSaved);
    connect(generator, &ShapeGenerator::finished,     this, &ResumePage::onGenerationFinished);
    connect(btnStop,   &QPushButton::clicked, generator, &ShapeGenerator::stopGeneration,
            Qt::DirectConnection);
    connect(generator, &ShapeGenerator::finished, generatorThread, &QThread::quit);
    connect(generator, &ShapeGenerator::finished, generator, &QObject::deleteLater);
    connect(generatorThread, &QThread::finished, generatorThread, &QObject::deleteLater);

    logOutput->clear();
    logOutput->append("--- Resuming Generation ---");
    savedPathLabel->hide();   // reset path label for a new session
    lastSavedPath.clear();
    generatorThread->start();
}

void ResumePage::onStopClicked() {
    if (generator) generator->stopGeneration();
}

void ResumePage::onShapeAdded(const QImage& canvas, int count) {
    previewLabel->setImage(canvas);
    shapeCountLabel->setText(QString("— %1 shapes").arg(count));
}

void ResumePage::onLogMessage(const QString& msg) {
    logOutput->append(QDateTime::currentDateTime().toString("[HH:mm:ss] ") + msg);
}

void ResumePage::onGenerationFinished() {
    btnStart->setEnabled(true);
    btnStop->setEnabled(false);
    btnBrowse->setEnabled(true);
    dropWidget->setEnabled(true);
    generator       = nullptr;
    generatorThread = nullptr;
}

void ResumePage::onFileSaved(const QString& path) {
    lastSavedPath = path;
    QFileInfo fi(path);
    // Show a compact, selectable path below the log
    savedPathLabel->setText(QString("💾  %1").arg(QDir::toNativeSeparators(path)));
    savedPathLabel->setToolTip(
        QString("Output saved to: %1\nFolder: %2")
            .arg(QDir::toNativeSeparators(path))
            .arg(QDir::toNativeSeparators(fi.absolutePath())));
    savedPathLabel->show();
}

bool ResumePage::loadResumeState(SessionState& state) {
    int w, h;
    QString srcImgName;
    QVector<ShapeData> loadedShapes;
    if (!GeometryJson::loadFromFile(state.imagePath, loadedShapes, w, h, srcImgName)) {
        QMessageBox::warning(this, "Error", "Failed to parse JSON.");
        btnStart->setEnabled(true);
        btnStop->setEnabled(false);
        btnBrowse->setEnabled(true);
        dropWidget->setEnabled(true);
        return false;
    }

    QFileInfo jsonInfo(state.imagePath);
    QString srcPath = jsonInfo.absoluteDir().filePath(srcImgName);
    QImage targetImg(srcPath);
    if (targetImg.isNull()) {
        QMessageBox::warning(this, "Error", "Cannot find original image: " + srcPath);
        btnStart->setEnabled(true);
        btnStop->setEnabled(false);
        btnBrowse->setEnabled(true);
        dropWidget->setEnabled(true);
        return false;
    }

    showOriginalImage(targetImg);

    state.originalImage    = targetImg;
    state.shapes           = loadedShapes;
    state.generatedCount   = loadedShapes.size();

    // Re-render canvas from loaded shapes
    state.currentCanvas = QImage(w, h, QImage::Format_RGB32);
    state.currentCanvas.fill(Qt::black);
    for (const auto& s : loadedShapes) {
        state.currentCanvas = ImageMath::drawShape(state.currentCanvas, s);
    }
    state.imagePath = srcPath;
    shapeCountLabel->setText(QString("— %1 shapes").arg(state.generatedCount));
    logOutput->append(QString("Resuming from %1 shapes.").arg(state.generatedCount));
    return true;
}
