#ifndef CUSTOM_SETTINGS_PANEL_H
#define CUSTOM_SETTINGS_PANEL_H

#include <QWidget>
#include <QSpinBox>
#include <QCheckBox>
#include "../models/generation_settings.h"

class CustomSettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit CustomSettingsPanel(QWidget* parent = nullptr);
    GenerationSettings getSettings() const;
    void setSettings(const GenerationSettings& settings);
    
private:
    QSpinBox* spinThreads;
    QSpinBox* spinSaveInterval;
    QCheckBox* chkUseOpenCL;
};

#endif // CUSTOM_SETTINGS_PANEL_H
