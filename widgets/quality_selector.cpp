#include "quality_selector.h"
#include <QVariant>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QInputDialog>
#include <QMessageBox>

QualitySelector::QualitySelector(QWidget* parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    presetCombo = new QComboBox(this);
    layout->addWidget(presetCombo);

    customGroup = new QGroupBox("Custom settings", this);
    auto groupLayout = new QVBoxLayout(customGroup);
    
    auto infoLabel = new QLabel("The selected preset fills these values. Enable custom settings if you want to edit them before generating.", this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #4a90e2;");
    groupLayout->addWidget(infoLabel);

    checkUseCustom = new QCheckBox("Use custom settings", this);
    groupLayout->addWidget(checkUseCustom);

    auto formLayout = new QFormLayout();
    
    spinOutputLayers = new QSpinBox(this);
    spinOutputLayers->setRange(1, 3000); // Capped at 3000
    spinOutputLayers->setSingleStep(100);
    formLayout->addRow("Output layers", spinOutputLayers);
    
    spinMaxResolution = new QSpinBox(this);
    spinMaxResolution->setRange(100, 4000);
    spinMaxResolution->setSingleStep(100);
    formLayout->addRow("Max resolution", spinMaxResolution);
    
    spinRandomSamples = new QSpinBox(this);
    spinRandomSamples->setRange(100, 5000000);
    spinRandomSamples->setSingleStep(1000);
    formLayout->addRow("Random samples", spinRandomSamples);
    
    spinMutateSamples = new QSpinBox(this);
    spinMutateSamples->setRange(10, 500000);
    spinMutateSamples->setSingleStep(100);
    formLayout->addRow("Mutated samples", spinMutateSamples);

    editCheckpoints = new QLineEdit(this);
    formLayout->addRow("Save checkpoints", editCheckpoints);

    comboPreprocess = new QComboBox(this);
    comboPreprocess->addItem("none");
    comboPreprocess->addItem("outline");
    comboPreprocess->addItem("blur");
    formLayout->addRow("Preprocess mode", comboPreprocess);

    groupLayout->addLayout(formLayout);

    connect(editCheckpoints, &QLineEdit::editingFinished, this, [this]() {
        QStringList parts = editCheckpoints->text().split(',', Qt::SkipEmptyParts);
        QStringList validParts;
        for (const QString& part : parts) {
            int val = part.trimmed().toInt();
            if (val > 3000) val = 3000;
            if (val > 0) validParts.append(QString::number(val));
        }
        editCheckpoints->setText(validParts.join(','));
    });

    btnSavePreset = new QPushButton("Save as preset", this);
    btnLoadPreset = new QPushButton("Load Preset", this);
    auto btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnSavePreset);
    btnLayout->addWidget(btnLoadPreset);
    btnLayout->addStretch();
    groupLayout->addLayout(btnLayout);

    layout->addWidget(customGroup);

    connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QualitySelector::onPresetChanged);
    connect(checkUseCustom, &QCheckBox::toggled, this, &QualitySelector::onUseCustomToggled);
    connect(btnSavePreset, &QPushButton::clicked, this, &QualitySelector::onSavePresetClicked);
    connect(btnLoadPreset, &QPushButton::clicked, this, &QualitySelector::onLoadPresetClicked);

    loadPresets();
    onUseCustomToggled(false); // Initialize state
}

void QualitySelector::loadPresets() {
    presetCombo->clear();
    
    // Default presets
    QString ckpts = "500,1000,1500,2000,2500,3000";
    presetCombo->addItem("Fastest", QVariant::fromValue(QualityPreset{3000, 1000, 5000, 1000, ckpts, "none"}));
    presetCombo->addItem("Fast", QVariant::fromValue(QualityPreset{3000, 1200, 20000, 2500, ckpts, "none"}));
    presetCombo->addItem("Medium", QVariant::fromValue(QualityPreset{3000, 1400, 50000, 5000, ckpts, "none"}));
    presetCombo->addItem("Best", QVariant::fromValue(QualityPreset{3000, 1800, 100000, 8000, ckpts, "none"}));
    presetCombo->addItem("Extreme", QVariant::fromValue(QualityPreset{3000, 2400, 200000, 12000, ckpts, "none"}));
    presetCombo->addItem("Most Extreme / Rip my GPU", QVariant::fromValue(QualityPreset{3000, 3000, 500000, 12000, ckpts, "none"}));
    
    // Load custom presets
    QSettings settings("ForzaGenerator", "PainterGenerator");
    int size = settings.beginReadArray("CustomPresets");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString name = settings.value("name").toString();
        QList<QVariant> data = settings.value("data").toList();
        if (data.size() >= 6) {
            QualityPreset preset{
                data[0].toInt(), data[1].toInt(), data[2].toInt(),
                data[3].toInt(), data[4].toString(), data[5].toString()
            };
            presetCombo->addItem(name, QVariant::fromValue(preset));
        }
    }
    settings.endArray();

    // Restore last selected preset or default to Medium
    QString lastPreset = settings.value("LastSelectedPreset", "Medium").toString();
    int idx = presetCombo->findText(lastPreset);
    if (idx < 0) idx = 2; // fallback to Medium index
    presetCombo->setCurrentIndex(idx);
    updateFieldsFromPreset(idx);
}

void QualitySelector::updateFieldsFromPreset(int index) {
    if (index < 0) return;
    if (presetCombo->itemData(index).canConvert<QualityPreset>()) {
        QualityPreset data = presetCombo->itemData(index).value<QualityPreset>();
        
        // Limit output layers to 3000 max for FH6
        int outputLayers = qMin(3000, data.maxShapes);
        spinOutputLayers->setValue(outputLayers);
        
        spinMaxResolution->setValue(data.maxResolution);
        spinRandomSamples->setValue(data.randomSamples);
        spinMutateSamples->setValue(data.mutateSamples);
        editCheckpoints->setText(data.checkpoints);
        comboPreprocess->setCurrentText(data.preprocessMode);
    }
}

void QualitySelector::onPresetChanged(int index) {
    if (!checkUseCustom->isChecked()) {
        updateFieldsFromPreset(index);
    }
    if (index >= 0) {
        QSettings settings("ForzaGenerator", "PainterGenerator");
        settings.setValue("LastSelectedPreset", presetCombo->itemText(index));
    }
}

void QualitySelector::onUseCustomToggled(bool checked) {
    spinOutputLayers->setEnabled(checked);
    spinMaxResolution->setEnabled(checked);
    spinRandomSamples->setEnabled(checked);
    spinMutateSamples->setEnabled(checked);
    editCheckpoints->setEnabled(checked);
    comboPreprocess->setEnabled(checked);
    btnSavePreset->setEnabled(checked);
    btnLoadPreset->setEnabled(checked);
}

void QualitySelector::onSavePresetClicked() {
    bool ok;
    QString name = QInputDialog::getText(this, "Save Preset", "Preset Name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        QualityPreset preset{
            spinOutputLayers->value(),
            spinMaxResolution->value(),
            spinRandomSamples->value(),
            spinMutateSamples->value(),
            editCheckpoints->text(),
            comboPreprocess->currentText()
        };
        
        // For backwards compatibility in QSettings, save as QList<QVariant>
        QList<QVariant> listData = {
            preset.maxShapes, preset.maxResolution, preset.randomSamples,
            preset.mutateSamples, preset.checkpoints, preset.preprocessMode
        };
        
        QSettings settings("ForzaGenerator", "PainterGenerator");
        settings.beginGroup("SavedPresets");
        QList<QVariant> savedList = settings.value("list").toList();
        
        QVariantMap presetMap;
        presetMap["name"] = name;
        presetMap["data"] = QVariant::fromValue(listData);
        savedList.append(presetMap);
        
        settings.setValue("list", savedList);
        settings.endGroup();
        
        // Re-save array format
        settings.beginWriteArray("CustomPresets");
        for (int i = 0; i < savedList.size(); ++i) {
            settings.setArrayIndex(i);
            QVariantMap m = savedList[i].toMap();
            settings.setValue("name", m["name"]);
            settings.setValue("data", m["data"]);
        }
        settings.endArray();
        
        presetCombo->addItem(name, QVariant::fromValue(preset));
        presetCombo->setCurrentIndex(presetCombo->count() - 1);
        QMessageBox::information(this, "Success", "Preset saved successfully.");
    }
}

void QualitySelector::onLoadPresetClicked() {
    updateFieldsFromPreset(presetCombo->currentIndex());
}

int QualitySelector::getRandomSamples() const {
    return spinRandomSamples->value();
}

int QualitySelector::getMutateSamples() const {
    return spinMutateSamples->value();
}

int QualitySelector::getMaxShapes() const {
    return spinOutputLayers->value();
}

int QualitySelector::getMaxResolution() const {
    return spinMaxResolution->value();
}

QString QualitySelector::getCheckpoints() const {
    return editCheckpoints->text();
}

void QualitySelector::setMaxShapes(int count) {
    if (count > 0 && count <= 3000) {
        spinOutputLayers->setValue(count);
        
        // Also force custom mode to be enabled so the user sees the value
        checkUseCustom->setChecked(true);
    }
}

void QualitySelector::setCurrentPresetName(const QString& name) {
    int idx = presetCombo->findText(name);
    if (idx >= 0) {
        presetCombo->setCurrentIndex(idx);
        updateFieldsFromPreset(idx);
    }
}

QString QualitySelector::currentPresetName() const {
    if (checkUseCustom->isChecked()) {
        return "custom";
    }
    return presetCombo->currentText();
}

void QualitySelector::setCustomSettings(const QualityPreset& preset) {
    checkUseCustom->setChecked(true);
    onUseCustomToggled(true);
    spinOutputLayers->setValue(preset.maxShapes);
    spinMaxResolution->setValue(preset.maxResolution);
    spinRandomSamples->setValue(preset.randomSamples);
    spinMutateSamples->setValue(preset.mutateSamples);
    editCheckpoints->setText(preset.checkpoints);
    comboPreprocess->setCurrentText(preset.preprocessMode);
}

QString QualitySelector::getPreprocessMode() const {
    return comboPreprocess->currentText();
}
