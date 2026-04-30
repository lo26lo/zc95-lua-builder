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

    m_setup = makeBox("Setup() — runs once before Loop",
        "Tick this to set initial channel state (power, frequency,\n"
        "pulse width) BEFORE the pattern starts running.\n\n"
        "If you don't tick it, every channel defaults to:\n"
        "  power=1000, frequency=150 Hz, pulse_width=150 µs.\n"
        "That's quite aggressive — Setup is the place to tone it down.", this);
    m_loop = makeBox("Loop(time_ms) — runs continuously [required]",
        "MANDATORY. The heart of every pattern.\n"
        "Called repeatedly with time_ms = milliseconds since power-on.\n"
        "Use the time argument to schedule events (\"every 500 ms do X\")\n"
        "rather than relying on counting ticks.", this);
    m_loop->setChecked(true);
    m_loop->setEnabled(false);

    m_minMax = makeBox("MinMaxChange(menu_id, val)",
        "Called when the user moves a MIN_MAX slider on the device LCD.\n"
        "Tick this if you have any MIN_MAX menu items — otherwise the\n"
        "user can't influence your pattern at runtime.\n\n"
        "If you tick it, the smart-merge will pre-fill the function with\n"
        "an if/elseif chain mapping each menu_id to its variable.", this);
    m_multiChoice = makeBox("MultiChoiceChange(menu_id, choice_id)",
        "Called when the user changes a MULTI_CHOICE option on the LCD.\n"
        "Same idea as MinMaxChange — tick it whenever you have at least\n"
        "one MULTI_CHOICE menu item.", this);
    m_softButton = makeBox("SoftButton(pushed)",
        "Soft button is THE simplest kill-switch. Highly recommended.\n"
        "When ticked, set the label in Config → \"Soft button label\"\n"
        "(otherwise the button is invisible).\n\n"
        "Typical body:\n"
        "  if pushed then\n"
        "    -- emergency: turn everything off\n"
        "    for c=1,4 do zc.ChannelOff(c) end\n"
        "  end", this);
    m_externalTrigger = makeBox("ExternalTrigger(socket, part, active)",
        "External 3.5mm trigger inputs (TRIGGER1, TRIGGER2; parts A/B).\n"
        "Tick this if you've wired a footswitch / pushbutton / sensor\n"
        "to the device. Receives an event each time the line is shorted.", this);
    m_btKeypress = makeBox("BluetoothRemoteKeypress(key)",
        "Receive button presses from a paired Bluetooth remote\n"
        "(e.g. a camera shutter remote).\n\n"
        "ALSO requires Config → \"Bluetooth remote passthrough\" ticked,\n"
        "otherwise this callback never fires.", this);
    m_btHid = makeBox("BluetoothHidEvent(usage_page, usage, value)",
        "Raw HID events from a paired BT device — advanced.\n"
        "If you don't already know what HID usage pages are, leave this\n"
        "unchecked.", this);
    m_audioIntensity = makeBox("AudioIntensityChange(L, R, virt)",
        "Receives audio level (0-255 per channel) from the audio jack.\n\n"
        "ALSO requires Config → Audio mode = AUDIO_INTENSITY, otherwise\n"
        "the callback is never called.\n\n"
        "Typical body: scale L/R into power values to make the pattern\n"
        "react to music volume.", this);

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

void FunctionsPanel::setBeginnerMode(bool beginner) {
    // Hide the advanced callbacks. Force-uncheck them so the generator
    // doesn't emit stubs for callbacks the beginner can't see.
    auto hide = [&](QCheckBox* cb) {
        cb->setVisible(!beginner);
        if (beginner && cb->isChecked()) cb->setChecked(false);
    };
    hide(m_externalTrigger);
    hide(m_btKeypress);
    hide(m_btHid);
    hide(m_audioIntensity);
}
