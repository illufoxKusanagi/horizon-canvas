#ifndef QUALITY_SELECTOR_H
#define QUALITY_SELECTOR_H

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMetaType>
#include <QString>

struct QualityPreset {
    int maxShapes;
    int maxResolution;
    int randomSamples;
    int mutateSamples;
    QString checkpoints;
    QString preprocessMode;
};

Q_DECLARE_METATYPE(QualityPreset)

class QualitySelector : public QWidget {
    Q_OBJECT
public:
    explicit QualitySelector(QWidget* parent = nullptr);
    int getRandomSamples() const;
    int getMutateSamples() const;
    int getMaxShapes() const;
    int getMaxResolution() const;
    QString getCheckpoints() const;
    void setMaxShapes(int count);
    void setCurrentPresetName(const QString& name);
    QString currentPresetName() const;
    void setCustomSettings(const QualityPreset& preset);
    QString getPreprocessMode() const;

private slots:
    void onPresetChanged(int index);
    void onUseCustomToggled(bool checked);
    void onSavePresetClicked();
    void onLoadPresetClicked();

private:
    void loadPresets();
    void updateFieldsFromPreset(int index);

    QComboBox* presetCombo;
    QGroupBox* customGroup;
    QCheckBox* checkUseCustom;
    
    QSpinBox* spinOutputLayers;
    QSpinBox* spinMaxResolution;
    QSpinBox* spinRandomSamples;
    QSpinBox* spinMutateSamples;
    QLineEdit* editCheckpoints;
    QComboBox* comboPreprocess;
    
    QPushButton* btnSavePreset;
    QPushButton* btnLoadPreset;
};

#endif // QUALITY_SELECTOR_H
