#include "generate_page.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>
#include <QUrl>

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

GeneratePage::GeneratePage(QWidget* parent)
    : QWidget(parent), generatorThread(nullptr), generator(nullptr)
{
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(16);

    // ── LEFT PANEL (controls) ────────────────────────────────────────────────
    auto* leftPanel = new QVBoxLayout();
    leftPanel->setSpacing(8);

    auto* title = new QLabel("Generate Shapes", this);
    title->setStyleSheet(TextStyle::Header1);
    leftPanel->addWidget(title);

    // Drop + buttons row
    auto* dropLayout = new QHBoxLayout();
    dropWidget = new ImageDropWidget(this);
    dropLayout->addWidget(dropWidget, 1);

    auto* buttonsLayout = new QVBoxLayout();
    btnChooseImage = new QPushButton("Choose Image...", this);
    buttonsLayout->addWidget(btnChooseImage);
    buttonsLayout->addStretch();
    dropLayout->addLayout(buttonsLayout);
    leftPanel->addLayout(dropLayout);

    leftPanel->addWidget(new QLabel("Quality Preset:", this));
    qualitySelector = new QualitySelector(this);
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

    originalLabel = new ZoomableGraphicsView("Drop or choose an image<br>to see it here", this);
    previewLabel  = new ZoomableGraphicsView("Generated shapes will<br>appear here", this);

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

    connect(btnChooseImage, &QPushButton::clicked, this, &GeneratePage::onChooseImage);
    connect(btnStart,       &QPushButton::clicked, this, &GeneratePage::onStartClicked);
    connect(btnStop,        &QPushButton::clicked, this, &GeneratePage::onStopClicked);

    // Open the output folder when clicking the path label
    connect(savedPathLabel, &QLabel::linkActivated, this, [](const QString& link) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(link));
    });
}

GeneratePage::~GeneratePage() {
    onStopClicked();
}

// ─── helpers ─────────────────────────────────────────────────────────────────

void GeneratePage::showOriginalImage(const QImage& img) {
    originalLabel->setImage(img);
}

// ─── slots ───────────────────────────────────────────────────────────────────

void GeneratePage::onChooseImage() {
    QString fileName = QFileDialog::getOpenFileName(
        this, "Select Target Image", "",
        "Images (*.png *.jpg *.jpeg)");
    if (!fileName.isEmpty()) {
        dropWidget->setImagePath(fileName);
        QImage img(fileName);
        if (!img.isNull()) {
            showOriginalImage(img);
            previewLabel->clearImage();
            shapeCountLabel->setText("");
        }
    }
}

void GeneratePage::onStartClicked() {
    if (dropWidget->getCurrentImagePath().isEmpty()) {
        QMessageBox::warning(this, "Error", "Please drop an image first.");
        return;
    }

    btnStart->setEnabled(false);
    btnStop->setEnabled(true);
    btnChooseImage->setEnabled(false);

    GenerationSettings settings = settingsPanel->getSettings();
    settings.randomSamples = qualitySelector->getRandomSamples();
    settings.mutateSamples = qualitySelector->getMutateSamples();
    settings.maxShapes = qualitySelector->getMaxShapes();
    settings.maxResolution = qualitySelector->getMaxResolution();
    settings.customCheckpoints = qualitySelector->getCheckpoints();
    settings.preprocessMode = qualitySelector->getPreprocessMode();
    settings.profileName = qualitySelector->currentPresetName();

    SessionState state;
    state.imagePath = dropWidget->getCurrentImagePath();

    if (state.imagePath.endsWith(".json", Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Error", "To resume an existing generation, please use the dedicated Resume page.");
        btnStart->setEnabled(true);
        btnStop->setEnabled(false);
        btnChooseImage->setEnabled(true);
        return;
    } else {
        if (!loadNewImage(state, settings)) return;
    }

    logOutput->append(QString("Effective generation resolution: %1x%2").arg(state.originalImage.width()).arg(state.originalImage.height()));

    generatorThread = new QThread(this);
    generator       = new ShapeGenerator();
    generator->moveToThread(generatorThread);

    connect(generatorThread, &QThread::started, [=]() {
        generator->startGeneration(state, settings);
    });
    connect(generator, &ShapeGenerator::shapeAdded,   this, &GeneratePage::onShapeAdded);
    connect(generator, &ShapeGenerator::logMessage,   this, &GeneratePage::onLogMessage);
    connect(generator, &ShapeGenerator::fileSaved,    this, &GeneratePage::onFileSaved);
    connect(generator, &ShapeGenerator::finished,     this, &GeneratePage::onGenerationFinished);
    connect(btnStop,   &QPushButton::clicked, generator, &ShapeGenerator::stopGeneration,
            Qt::DirectConnection);
    connect(generator, &ShapeGenerator::finished, generatorThread, &QThread::quit);
    connect(generator, &ShapeGenerator::finished, generator, &QObject::deleteLater);
    connect(generatorThread, &QThread::finished, generatorThread, &QObject::deleteLater);

    logOutput->clear();
    logOutput->append("--- Starting Generation ---");
    savedPathLabel->hide();   // reset path label for a new session
    lastSavedPath.clear();
    generatorThread->start();
}

void GeneratePage::onStopClicked() {
    if (generator) generator->stopGeneration();
}

void GeneratePage::onShapeAdded(const QImage& canvas, int count) {
    previewLabel->setImage(canvas);
    shapeCountLabel->setText(QString("— %1 shapes").arg(count));
}

void GeneratePage::onLogMessage(const QString& msg) {
    logOutput->append(QDateTime::currentDateTime().toString("[HH:mm:ss] ") + msg);
}

void GeneratePage::onGenerationFinished() {
    btnStart->setEnabled(true);
    btnStop->setEnabled(false);
    btnChooseImage->setEnabled(true);
    generator       = nullptr;
    generatorThread = nullptr;
}

void GeneratePage::onFileSaved(const QString& path) {
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


bool GeneratePage::loadNewImage(SessionState& state, const GenerationSettings& settings) {
    QImage target(state.imagePath);
    if (target.isNull()) {
        QMessageBox::warning(this, "Error", "Failed to load image.");
        btnStart->setEnabled(true);
        btnStop->setEnabled(false);
        btnChooseImage->setEnabled(true);
        return false;
    }
    int maxRes = settings.maxResolution;
    if (target.width() > maxRes || target.height() > maxRes) {
        target = target.scaled(maxRes, maxRes, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    showOriginalImage(target);
    state.originalImage    = target;
    state.currentCanvas    = QImage(target.size(), QImage::Format_RGB32);
    state.currentCanvas.fill(Qt::black);
    state.generatedCount   = 0;
    return true;
}
