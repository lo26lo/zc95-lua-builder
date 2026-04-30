#include "ConfigPanel.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>

ConfigPanel::ConfigPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);

    auto* group = new QGroupBox("Pattern Config", this);
    auto* form = new QFormLayout(group);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText("e.g. Toggle");
    form->addRow("Name:", m_name);

    m_audioMode = new QComboBox(this);
    m_audioMode->addItem("OFF", static_cast<int>(AudioMode::Off));
    m_audioMode->addItem("AUDIO_INTENSITY", static_cast<int>(AudioMode::AudioIntensity));
    form->addRow("Audio mode:", m_audioMode);

    m_softButton = new QLineEdit(this);
    m_softButton->setPlaceholderText("Empty = no soft button");
    form->addRow("Soft button label:", m_softButton);

    m_loopFreq = new QSpinBox(this);
    m_loopFreq->setRange(0, 400);
    m_loopFreq->setSpecialValueText("default (as fast as possible)");
    form->addRow("Loop frequency (Hz):", m_loopFreq);

    m_allowTriphase = new QCheckBox("Allow triphase (channel isolation disabled!)", this);
    form->addRow("", m_allowTriphase);

    m_btPassthrough = new QCheckBox("Bluetooth remote passthrough", this);
    form->addRow("", m_btPassthrough);

    outer->addWidget(group);
    outer->addStretch();

    connect(m_name, &QLineEdit::textChanged, this, &ConfigPanel::changed);
    connect(m_softButton, &QLineEdit::textChanged, this, &ConfigPanel::changed);
    connect(m_audioMode, qOverload<int>(&QComboBox::currentIndexChanged), this, &ConfigPanel::changed);
    connect(m_loopFreq, qOverload<int>(&QSpinBox::valueChanged), this, &ConfigPanel::changed);
    connect(m_allowTriphase, &QCheckBox::toggled, this, &ConfigPanel::changed);
    connect(m_btPassthrough, &QCheckBox::toggled, this, &ConfigPanel::changed);
}

void ConfigPanel::load(const ScriptConfig& config) {
    m_name->setText(config.name);
    m_audioMode->setCurrentIndex(m_audioMode->findData(static_cast<int>(config.audioMode)));
    m_softButton->setText(config.softButtonLabel);
    m_loopFreq->setValue(config.loopFreqHz);
    m_allowTriphase->setChecked(config.allowTriphase);
    m_btPassthrough->setChecked(config.bluetoothRemotePassthrough);
}

void ConfigPanel::save(ScriptConfig& config) const {
    config.name = m_name->text();
    config.audioMode = static_cast<AudioMode>(m_audioMode->currentData().toInt());
    config.softButtonLabel = m_softButton->text();
    config.loopFreqHz = m_loopFreq->value();
    config.allowTriphase = m_allowTriphase->isChecked();
    config.bluetoothRemotePassthrough = m_btPassthrough->isChecked();
}
