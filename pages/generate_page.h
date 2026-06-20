#ifndef GENERATE_PAGE_H
#define GENERATE_PAGE_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include "../controllers/generation/shape_generator.h"
#include "../widgets/custom_settings_panel.h"
#include "../widgets/image_drop_widget.h"
#include "../widgets/quality_selector.h"
#include "../widgets/zoomable_graphics_view.h"

class GeneratePage : public QWidget {
    Q_OBJECT
public:
    explicit GeneratePage(QWidget* parent = nullptr);
    ~GeneratePage();

private slots:
    void onChooseImage();
    void onStartClicked();
    void onStopClicked();
    void onShapeAdded(const QImage& canvas, int count);
    void onLogMessage(const QString& msg);
    void onFileSaved(const QString& path);
    void onGenerationFinished();

private:
    void showOriginalImage(const QImage& img);
    bool loadNewImage(SessionState& state, const GenerationSettings& settings);

    ImageDropWidget* dropWidget;
    QPushButton* btnChooseImage;
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

    QString lastSavedPath;    // tracks the most recently saved file

    QThread* generatorThread;
    ShapeGenerator* generator;
};

#endif // GENERATE_PAGE_H
