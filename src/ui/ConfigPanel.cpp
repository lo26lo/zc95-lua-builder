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
    m_form = form;

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText("e.g. Toggle");
    m_name->setToolTip(
        "Name shown on the device LCD when this pattern is selected.\n"
        "Keep it short (≤ 10 chars) so it fits on the screen.\n"
        "Example: \"Climb\", \"Fire\", \"Slow rise\".");
    form->addRow("Name:", m_name);

    m_audioMode = new QComboBox(this);
    m_audioMode->addItem("OFF", static_cast<int>(AudioMode::Off));
    m_audioMode->addItem("AUDIO_INTENSITY", static_cast<int>(AudioMode::AudioIntensity));
    m_audioMode->setToolTip(
        "OFF — no audio input is processed (default, what you want 99% of the time).\n"
        "AUDIO_INTENSITY — the device samples a stereo audio jack and fires the\n"
        "  AudioIntensityChange(L, R, virt) callback. Tick that callback in\n"
        "  Functions, otherwise this setting does nothing.");
    form->addRow("Audio mode:", m_audioMode);

    m_softButton = new QLineEdit(this);
    m_softButton->setPlaceholderText("Empty = no soft button");
    m_softButton->setToolTip(
        "Label shown next to the top-left soft button on the device.\n"
        "Empty = the button is hidden and SoftButton() never fires.\n"
        "Tip: a soft button is the simplest way to give yourself a\n"
        "kill-switch — bind it to ChannelOff in your script.");
    form->addRow("Soft button label:", m_softButton);

    m_loopFreq = new QSpinBox(this);
    m_loopFreq->setRange(0, 400);
    m_loopFreq->setSpecialValueText("default (as fast as possible)");
    m_loopFreq->setToolTip(
        "How often Loop(time_ms) is called per second.\n"
        "  0   — device default (~600 Hz, runs as fast as it can).\n"
        "  10  — 10 calls per second, plenty for slow fades.\n"
        "  60  — smooth modulation (good for sine/triangle waves).\n"
        "  244 — high-rate, used by some official scripts for fine control.\n"
        "Throttling lowers CPU load on the device and is rarely needed.");
    form->addRow("Loop frequency (Hz):", m_loopFreq);

    m_allowTriphase = new QCheckBox("Allow triphase (channel isolation disabled!)", this);
    m_allowTriphase->setToolTip(
        "DANGER ZONE.\n\n"
        "Triphase mode lets pulses on different channels overlap. The device\n"
        "normally prevents this for safety — currents from two channels can\n"
        "combine through your body in unintended paths.\n\n"
        "Only tick this if your script genuinely uses zc.EnableTriphase(true)\n"
        "or zc.LinkChannels(...). Otherwise leave OFF.\n\n"
        "When ON, your electrode placement matters more — read the ZC95\n"
        "safety documentation before using triphase patterns.");
    form->addRow("", m_allowTriphase);

    m_btPassthrough = new QCheckBox("Bluetooth remote passthrough", this);
    m_btPassthrough->setToolTip(
        "Enable to receive BluetoothRemoteKeypress(key) events from a\n"
        "paired Bluetooth remote (e.g. a camera shutter remote).\n"
        "Leave OFF unless you know you need it — the firmware uses BT for\n"
        "other things otherwise.");
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

void ConfigPanel::setBeginnerMode(bool beginner) {
    if (!m_form) return;
    // Find each row by its field widget and toggle the whole row.
    auto hideRow = [&](QWidget* field) {
        int row;
        QFormLayout::ItemRole role;
        m_form->getWidgetPosition(field, &row, &role);
        if (row >= 0) m_form->setRowVisible(row, !beginner);
    };
    hideRow(m_loopFreq);
    hideRow(m_allowTriphase);
    hideRow(m_btPassthrough);
    // In beginner mode, force-clear the dangerous flags so they don't
    // silently persist hidden.
    if (beginner) {
        if (m_allowTriphase->isChecked()) m_allowTriphase->setChecked(false);
        if (m_btPassthrough->isChecked()) m_btPassthrough->setChecked(false);
    }
}
