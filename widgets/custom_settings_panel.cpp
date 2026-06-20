#include "custom_settings_panel.h"
#include <QFormLayout>

CustomSettingsPanel::CustomSettingsPanel(QWidget* parent) : QWidget(parent) {
    auto layout = new QFormLayout(this);
    
    spinThreads = new QSpinBox(this);
    spinThreads->setRange(0, 64);
    spinThreads->setSpecialValueText("Auto");
    spinThreads->setValue(0);
    
    spinSaveInterval = new QSpinBox(this);
    spinSaveInterval->setRange(1, 1000);
    spinSaveInterval->setValue(10);
    
    chkUseOpenCL = new QCheckBox("Enable GPU Acceleration (OpenCL)", this);
    chkUseOpenCL->setChecked(true);
    
    layout->addRow("Threads (0=Auto):", spinThreads);
    layout->addRow("Auto-save Interval:", spinSaveInterval);
    layout->addRow("", chkUseOpenCL);
}

GenerationSettings CustomSettingsPanel::getSettings() const {
    GenerationSettings s;
    s.maxThreads = spinThreads->value();
    s.saveInterval = spinSaveInterval->value();
    s.useOpenCL = chkUseOpenCL->isChecked();
    return s;
}

void CustomSettingsPanel::setSettings(const GenerationSettings& settings) {
    spinThreads->setValue(settings.maxThreads);
    spinSaveInterval->setValue(settings.saveInterval);
    chkUseOpenCL->setChecked(settings.useOpenCL);
}
