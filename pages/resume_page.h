#ifndef RESUME_PAGE_H
#define RESUME_PAGE_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include "../controllers/generation/shape_generator.h"
#include "../widgets/custom_settings_panel.h"
#include "../widgets/quality_selector.h"
#include "../widgets/zoomable_graphics_view.h"
#include "../widgets/json_drop_widget.h"

class ResumePage : public QWidget {
    Q_OBJECT
public:
    explicit ResumePage(QWidget* parent = nullptr);
    ~ResumePage();

private slots:
    void onBrowseClicked();
    void loadJsonFile(const QString& path);
    void onStartClicked();
    void onStopClicked();
    void onShapeAdded(const QImage& canvas, int count);
    void onLogMessage(const QString& msg);
    void onFileSaved(const QString& path);
    void onGenerationFinished();

private:
    void showOriginalImage(const QImage& img);
    bool loadResumeState(SessionState& state);

    JsonDropWidget* dropWidget;
    QPushButton* btnBrowse;
    QualitySelector* qualitySelector;
    CustomSettingsPanel* settingsPanel;

    // Side-by-side comparison panel
    ZoomableGraphicsView* originalLabel;    // left  — source image
    QLabel* originalCaption;
    ZoomableGraphicsView* previewLabel;     // right — generated shapes canvas
    QLabel* previewCaption;
    QLabel* shapeCountLabel;

    QTextEdit* logOutput;
    QLabel* savedPathLabel;   // shows the output file path after first save
    QPushButton* btnStart;
    QPushButton* btnStop;

    QString jsonFilePath;     // stores the selected checkpoint JSON file path
    QString lastSavedPath;    // tracks the most recently saved file

    QThread* generatorThread;
    ShapeGenerator* generator;
};

#endif // RESUME_PAGE_H
