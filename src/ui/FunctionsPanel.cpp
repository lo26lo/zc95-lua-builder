#include "FunctionsPanel.h"

#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>

static QCheckBox* makeBox(const QString& label, const QString& tip, QWidget* parent) {
    auto* cb = new QCheckBox(label, parent);
    cb->setToolTip(tip);
    return cb;
}

FunctionsPanel::FunctionsPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);

    auto* group = new QGroupBox("Special Functions to include", this);
    auto* layout = new QVBoxLayout(group);
    layout->addWidget(new QLabel("Tick the callbacks your script needs. Stubs will be generated.", this));

    m_setup = makeBox("Setup() — runs once before Loop", "If absent, all channels default to full power", this);
    m_loop = makeBox("Loop(time_ms) — runs continuously [required]", "Mandatory. time_ms is ms since power-on (float).", this);
    m_loop->setChecked(true);
    m_loop->setEnabled(false);

    m_minMax = makeBox("MinMaxChange(menu_id, val)", "Called when a MIN_MAX menu item changes", this);
    m_multiChoice = makeBox("MultiChoiceChange(menu_id, choice_id)", "Called when a MULTI_CHOICE menu item changes", this);
    m_softButton = makeBox("SoftButton(pushed)", "Top-left soft button. Set Config.soft_button label too.", this);
    m_externalTrigger = makeBox("ExternalTrigger(socket, part, active)", "Trigger inputs (TRIGGER1/2, parts A/B)", this);
    m_btKeypress = makeBox("BluetoothRemoteKeypress(key)", "Requires bluetooth_remote_passthrough = true", this);
    m_btHid = makeBox("BluetoothHidEvent(usage_page, usage, value)", "Raw HID events from custom BT devices", this);
    m_audioIntensity = makeBox("AudioIntensityChange(L, R, virt)", "Requires audio_processing_mode = AUDIO_INTENSITY", this);

    layout->addWidget(m_setup);
    layout->addWidget(m_loop);
    layout->addWidget(m_minMax);
    layout->addWidget(m_multiChoice);
    layout->addWidget(m_softButton);
    layout->addWidget(m_externalTrigger);
    layout->addWidget(m_btKeypress);
    layout->addWidget(m_btHid);
    layout->addWidget(m_audioIntensity);

    outer->addWidget(group);
    outer->addStretch();

    for (auto* cb : {m_setup, m_loop, m_minMax, m_multiChoice, m_softButton,
                     m_externalTrigger, m_btKeypress, m_btHid, m_audioIntensity}) {
        connect(cb, &QCheckBox::toggled, this, &FunctionsPanel::changed);
    }
}

void FunctionsPanel::load(const EnabledFunctions& f) {
    m_setup->setChecked(f.setup);
    m_loop->setChecked(true); // always
    m_minMax->setChecked(f.minMaxChange);
    m_multiChoice->setChecked(f.multiChoiceChange);
    m_softButton->setChecked(f.softButton);
    m_externalTrigger->setChecked(f.externalTrigger);
    m_btKeypress->setChecked(f.bluetoothRemoteKeypress);
    m_btHid->setChecked(f.bluetoothHidEvent);
    m_audioIntensity->setChecked(f.audioIntensityChange);
}

void FunctionsPanel::save(EnabledFunctions& f) const {
    f.setup = m_setup->isChecked();
    f.loop = true;
    f.minMaxChange = m_minMax->isChecked();
    f.multiChoiceChange = m_multiChoice->isChecked();
    f.softButton = m_softButton->isChecked();
    f.externalTrigger = m_externalTrigger->isChecked();
    f.bluetoothRemoteKeypress = m_btKeypress->isChecked();
    f.bluetoothHidEvent = m_btHid->isChecked();
    f.audioIntensityChange = m_audioIntensity->isChecked();
}
