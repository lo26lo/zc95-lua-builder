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
    m_stepMs = new QSpinBox(this);
    m_stepMs->setRange(1, 1000);
    m_stepMs->setValue(20);
    m_stepMs->setSuffix(" ms/tick");
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
