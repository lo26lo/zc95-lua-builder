#include "SimulatorPanel.h"
#include "TimelineWidget.h"
#include "../sim/LuaRuntime.h"

#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QTimer>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSlider>
#include <QComboBox>
#include <QToolButton>

SimulatorPanel::SimulatorPanel(QWidget* parent) : QWidget(parent) {
    m_runtime = new LuaRuntime(this);

    auto* outer = new QVBoxLayout(this);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    m_playBtn = new QPushButton("▶ Run", this);
    m_pauseBtn = new QPushButton("⏸ Pause", this);
    m_resetBtn = new QPushButton("⟲ Reset", this);
    m_stepBtn = new QPushButton("Step", this);
    m_softBtn = new QPushButton("Soft Btn", this);
    m_trigBtn = new QPushButton("Trigger 1A", this);
    m_speedFactor = new QDoubleSpinBox(this);
    m_speedFactor->setRange(0.1, 100.0);
    m_speedFactor->setValue(1.0);
    m_speedFactor->setPrefix("speed ×");
    m_speedFactor->setToolTip(
        "Multiplier on simulated time. 1.0 = real-time, 5.0 = 5× faster.\n"
        "This is the ONLY control that changes the playback speed of the\n"
        "timeline — Menu controls sliders below change the script's\n"
        "parameters, not the simulator speed.");
    m_stepMs = new QSpinBox(this);
    m_stepMs->setRange(1, 1000);
    m_stepMs->setValue(20);
    m_stepMs->setSuffix(" ms/tick");
    m_stepMs->setToolTip(
        "Simulated milliseconds advanced per tick. Smaller = finer\n"
        "resolution, slower wallclock progress. 20 ms is a good default.");
    m_clock = new QLabel("t = 0.000s", this);
    m_clock->setStyleSheet("font-family: monospace;");

    toolbar->addWidget(m_playBtn);
    toolbar->addWidget(m_pauseBtn);
    toolbar->addWidget(m_resetBtn);
    toolbar->addWidget(m_stepBtn);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_softBtn);
    toolbar->addWidget(m_trigBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_speedFactor);
    toolbar->addWidget(m_stepMs);
    toolbar->addWidget(m_clock);
    outer->addLayout(toolbar);

    // Channel state lights
    auto* states = new QGroupBox("Channels", this);
    auto* statesLayout = new QGridLayout(states);
    for (int i = 0; i < 4; ++i) {
        auto* lbl = new QLabel(QString("CH%1").arg(i + 1), this);
        m_channelStates[i] = new QLabel("OFF", this);
        m_channelStates[i]->setStyleSheet("font-family: monospace; padding: 4px 8px; background:#333; border-radius:3px;");
        statesLayout->addWidget(lbl, 0, i);
        statesLayout->addWidget(m_channelStates[i], 1, i);
    }
    outer->addWidget(states);

    // Menu controls — sliders for MIN_MAX, combos for MULTI_CHOICE.
    // Populated lazily by setMenuItems(); hidden when no items.
    m_menuGroup = new QGroupBox("Menu controls (live) — change SCRIPT parameters, not simulator speed", this);
    m_menuGroup->setToolTip(
        "Move a slider / pick a choice and the simulator fires the matching\n"
        "MinMaxChange / MultiChoiceChange callback in real time. Lets you\n"
        "test how your script reacts to the user turning the device knob —\n"
        "without recompiling or reflashing.\n\n"
        "These sliders DO NOT change the simulator's playback speed.\n"
        "For that, use \"speed ×N\" and \"N ms/tick\" at the top.");
    m_menuLayout = new QVBoxLayout(m_menuGroup);
    m_menuLayout->setContentsMargins(8, 8, 8, 8);
    m_menuLayout->setSpacing(4);
    m_menuGroup->hide();
    outer->addWidget(m_menuGroup);

    // Timeline
    m_timeline = new TimelineWidget(this);
    outer->addWidget(m_timeline, 1);

    // Log
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFont(QFont("Consolas", 9));
    m_log->setMaximumHeight(160);
    m_log->setStyleSheet("background:#1e1e1e; color:#d4d4d4;");
    outer->addWidget(m_log);

    m_timer = new QTimer(this);
    m_timer->setInterval(50);
    connect(m_timer, &QTimer::timeout, this, &SimulatorPanel::onTimerTick);

    connect(m_playBtn, &QPushButton::clicked, this, &SimulatorPanel::play);
    connect(m_pauseBtn, &QPushButton::clicked, this, &SimulatorPanel::pause);
    connect(m_resetBtn, &QPushButton::clicked, this, &SimulatorPanel::reset);
    connect(m_stepBtn, &QPushButton::clicked, this, &SimulatorPanel::step);
    connect(m_softBtn, &QPushButton::pressed, this, [this]() { triggerSoftButton(true); });
    connect(m_softBtn, &QPushButton::released, this, [this]() { triggerSoftButton(false); });
    connect(m_trigBtn, &QPushButton::clicked, this, &SimulatorPanel::triggerExternalA);
}

bool SimulatorPanel::loadSource(const QString& source) {
    pause();
    QString err;
    if (!m_runtime->loadScript(source, &err)) {
        appendLogs("[ERROR] Load failed: " + err);
        m_loaded = false;
        return false;
    }
    m_loaded = true;
    m_setupCalled = false;
    m_allEvents.clear();
    m_timeline->clear();
    appendLogs("[OK] Script loaded.");
    appendLogs(m_runtime->takeOutput());
    refreshState();
    return true;
}

bool SimulatorPanel::loadSourceSilent(const QString& source) {
    pause();
    QString err;
    if (!m_runtime->loadScript(source, &err)) {
        // Stay silent on success, but still report errors so the user knows.
        appendLogs("[ERROR] Load failed: " + err);
        m_loaded = false;
        return false;
    }
    m_loaded = true;
    m_setupCalled = false;
    m_allEvents.clear();
    m_timeline->clear();
    // Drain any print() output the script may have produced at top level
    // but do NOT emit our own "[OK] Script loaded." line.
    QString topOut = m_runtime->takeOutput();
    if (!topOut.trimmed().isEmpty()) appendLogs(topOut);
    refreshState();
    return true;
}

void SimulatorPanel::play() {
    if (!m_loaded) {
        emit needsScript();   // give MainWindow a chance to feed editor source
    }
    if (!m_loaded) {
        appendLogs("[INFO] No script loaded. Open a preset or paste code in the editor.");
        return;
    }
    if (!m_setupCalled) {
        QString err;
        if (!m_runtime->callSetup(&err)) {
            appendLogs("[ERROR] Setup() failed: " + err);
        }
        m_setupCalled = true;
        appendLogs(m_runtime->takeOutput());
        m_allEvents += m_runtime->takeEvents();
    }
    m_timer->start();
}

void SimulatorPanel::pause() {
    m_timer->stop();
}

void SimulatorPanel::reset() {
    pause();
    m_setupCalled = false;
    m_allEvents.clear();
    m_runtime->setCurrentTimeMs(0);
    m_timeline->clear();
    m_clock->setText("t = 0.000s");
    appendLogs("[INFO] Reset.");
    refreshState();
}

void SimulatorPanel::step() {
    if (!m_loaded) {
        emit needsScript();
    }
    if (!m_loaded) return;
    if (!m_setupCalled) {
        QString err;
        m_runtime->callSetup(&err);
        m_setupCalled = true;
        appendLogs(m_runtime->takeOutput());
        m_allEvents += m_runtime->takeEvents();
    }
    onTimerTick();
}

void SimulatorPanel::onTimerTick() {
    if (!m_loaded) { pause(); return; }
    int dt = m_stepMs->value();
    double newT = m_runtime->currentTimeMs() + dt * m_speedFactor->value();

    QString err;
    if (!m_runtime->callLoop(newT, &err)) {
        appendLogs("[ERROR] Loop() failed: " + err);
        pause();
    }
    m_runtime->updatePulses();

    appendLogs(m_runtime->takeOutput());
    m_allEvents += m_runtime->takeEvents();

    // Trim event history to keep things manageable.
    if (m_allEvents.size() > 5000) {
        m_allEvents.remove(0, m_allEvents.size() - 5000);
    }

    m_timeline->setEvents(m_allEvents, m_runtime->currentTimeMs());
    m_clock->setText(QString("t = %1s").arg(m_runtime->currentTimeMs() / 1000.0, 0, 'f', 3));
    refreshState();
}

void SimulatorPanel::triggerSoftButton(bool pushed) {
    if (!m_loaded) return;
    QString err;
    if (!m_runtime->callSoftButton(pushed, &err)) appendLogs("[ERROR] SoftButton: " + err);
    appendLogs(m_runtime->takeOutput());
    m_allEvents += m_runtime->takeEvents();
    refreshState();
}

void SimulatorPanel::triggerExternalA() {
    if (!m_loaded) return;
    QString err;
    m_runtime->callExternalTrigger("TRIGGER1", "A", true, &err);
    m_runtime->callExternalTrigger("TRIGGER1", "A", false, &err);
    appendLogs(m_runtime->takeOutput());
    m_allEvents += m_runtime->takeEvents();
    refreshState();
}

void SimulatorPanel::appendLogs(const QString& s) {
    if (s.trimmed().isEmpty()) return;
    m_log->appendPlainText(s.trimmed());
    emit log(s);
}

void SimulatorPanel::refreshState() {
    const auto& s = m_runtime->state();
    for (int i = 0; i < 4; ++i) {
        const auto& ch = s.channels[i];
        QString text = ch.on
            ? QString("ON  pwr=%1  %2Hz  %3us")
                  .arg(ch.power).arg(ch.frequencyHz).arg(ch.pulseWidthPosUs)
            : QString("OFF pwr=%1").arg(ch.power);
        m_channelStates[i]->setText(text);
        m_channelStates[i]->setStyleSheet(QString(
            "font-family: monospace; padding: 4px 8px; background:%1; color:%2; border-radius:3px;")
            .arg(ch.on ? "#3a5" : "#333", ch.on ? "#fff" : "#bbb"));
    }
}

bool SimulatorPanel::resolveScriptConfig(ScriptConfig& out, QString* warning) const {
    if (!m_loaded || !m_runtime) {
        if (warning) *warning = "No script loaded.";
        return false;
    }
    return m_runtime->extractScriptConfig(out, warning);
}

bool SimulatorPanel::testMenuItemDrive(const MenuItem& item) {
    if (!m_loaded) {
        appendLogs("[INFO] Test: no script loaded.");
        return false;
    }
    // Make sure Setup() has run.
    if (!m_setupCalled) {
        QString err;
        m_runtime->callSetup(&err);
        m_setupCalled = true;
        appendLogs(m_runtime->takeOutput());
        m_allEvents += m_runtime->takeEvents();
    }

    auto snapshot = [&]() {
        const auto& s = m_runtime->state();
        QString out;
        for (int i = 0; i < 4; ++i) {
            out += QString("CH%1:%2/%3/%4/%5;")
                       .arg(i + 1)
                       .arg(s.channels[i].on ? '1' : '0')
                       .arg(s.channels[i].power)
                       .arg(s.channels[i].frequencyHz)
                       .arg(s.channels[i].pulseWidthPosUs);
        }
        return out;
    };
    QString before = snapshot();

    auto runFew = [&](int ticks) {
        QString err;
        for (int i = 0; i < ticks; ++i) {
            m_runtime->callLoop(m_runtime->currentTimeMs() + 20.0, &err);
            m_runtime->updatePulses();
        }
        appendLogs(m_runtime->takeOutput());
        m_allEvents += m_runtime->takeEvents();
        refreshState();
    };

    appendLogs(QString("[TEST] Driving menu_id %1 (\"%2\")…").arg(item.id).arg(item.title));

    QString err;
    if (item.type == MenuItemType::MinMax) {
        QVector<int> values = { item.min, item.defaultValue, item.max };
        for (int v : values) {
            m_runtime->callMinMaxChange(item.id, v, &err);
            runFew(5);
        }
    } else if (item.type == MenuItemType::MultiChoice) {
        for (const auto& c : item.choices) {
            m_runtime->callMultiChoiceChange(item.id, c.choiceId, &err);
            runFew(5);
        }
    } else {
        appendLogs("[TEST] No interactive control for this item type.");
        return false;
    }

    QString after = snapshot();
    bool reacted = (before != after) || !m_allEvents.isEmpty();
    appendLogs(reacted
        ? "[TEST] Pattern reacted to the value changes — looks wired."
        : "[TEST] Pattern did NOT react. Check that MinMaxChange/MultiChoiceChange "
          "is defined AND that it handles this menu_id.");
    return reacted;
}

void SimulatorPanel::setMenuItems(const QVector<MenuItem>& items) {
    // Skip rebuild if nothing actually changed (avoid resetting sliders the
    // user is mid-drag).
    if (items.size() == m_currentMenuItems.size()) {
        bool same = true;
        for (int i = 0; i < items.size(); ++i) {
            const auto& a = items[i];
            const auto& b = m_currentMenuItems[i];
            if (a.id != b.id || a.type != b.type || a.title != b.title
                || a.min != b.min || a.max != b.max
                || a.incrementStep != b.incrementStep
                || a.defaultValue != b.defaultValue
                || a.choices.size() != b.choices.size()) {
                same = false; break;
            }
        }
        if (same) return;
    }
    m_currentMenuItems = items;

    // Wipe existing widgets in the layout.
    while (QLayoutItem* it = m_menuLayout->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }

    int liveCount = 0;
    for (const auto& mi : items) {
        if (mi.type == MenuItemType::MinMax) {
            // [Title (uom)]   |———O———|   [value]
            auto* row = new QWidget(m_menuGroup);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);

            QString labelText = mi.title;
            if (!mi.uom.isEmpty()) labelText += " (" + mi.uom + ")";
            auto* label = new QLabel(labelText, row);
            label->setMinimumWidth(140);
            label->setToolTip(QString(
                "menu_id %1 — fires MinMaxChange(%1, value) on every change.\n"
                "Range: %2..%3, step %4. Default: %5.")
                .arg(mi.id).arg(mi.min).arg(mi.max).arg(mi.incrementStep).arg(mi.defaultValue));

            auto* slider = new QSlider(Qt::Horizontal, row);
            slider->setRange(mi.min, mi.max);
            slider->setSingleStep(mi.incrementStep);
            slider->setPageStep(mi.incrementStep * 10);
            slider->setValue(mi.defaultValue);

            auto* valLabel = new QLabel(QString::number(mi.defaultValue), row);
            valLabel->setMinimumWidth(48);
            valLabel->setStyleSheet("font-family: monospace;");
            valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            h->addWidget(label);
            h->addWidget(slider, 1);
            h->addWidget(valLabel);
            m_menuLayout->addWidget(row);

            int menuId = mi.id;
            connect(slider, &QSlider::valueChanged, this, [this, menuId, valLabel](int v) {
                valLabel->setText(QString::number(v));
                emit liveMenuValueChanged(menuId, v);   // mirror on LCD preview
                if (!m_loaded) return;
                QString err;
                if (!m_runtime->callMinMaxChange(menuId, v, &err)) {
                    appendLogs(QString("[ERROR] MinMaxChange(%1,%2): %3").arg(menuId).arg(v).arg(err));
                }
                appendLogs(m_runtime->takeOutput());
                m_allEvents += m_runtime->takeEvents();
                refreshState();
            });
            // Initial fire so the LCD preview matches the slider's
            // starting position even before the user touches it.
            emit liveMenuValueChanged(menuId, slider->value());
            ++liveCount;
        } else if (mi.type == MenuItemType::MultiChoice) {
            auto* row = new QWidget(m_menuGroup);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);

            auto* label = new QLabel(mi.title, row);
            label->setMinimumWidth(140);
            label->setToolTip(QString(
                "menu_id %1 — fires MultiChoiceChange(%1, choice_id) on every change.")
                .arg(mi.id));

            auto* combo = new QComboBox(row);
            for (const auto& c : mi.choices) {
                combo->addItem(c.description, c.choiceId);
            }
            h->addWidget(label);
            h->addWidget(combo, 1);
            m_menuLayout->addWidget(row);

            int menuId = mi.id;
            connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this, combo, menuId](int) {
                    int cid = combo->currentData().toInt();
                    emit liveMenuValueChanged(menuId, cid);
                    if (!m_loaded) return;
                    QString err;
                    if (!m_runtime->callMultiChoiceChange(menuId, cid, &err)) {
                        appendLogs(QString("[ERROR] MultiChoiceChange(%1,%2): %3").arg(menuId).arg(cid).arg(err));
                    }
                    appendLogs(m_runtime->takeOutput());
                    m_allEvents += m_runtime->takeEvents();
                    refreshState();
                });
            if (combo->count() > 0) {
                emit liveMenuValueChanged(menuId, combo->currentData().toInt());
            }
            ++liveCount;
        }
        // AUDIO_VIEW_INTENSITY_* types have no interactive control.
    }

    m_menuGroup->setVisible(liveCount > 0);
}
