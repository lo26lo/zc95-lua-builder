#pragma once

#include "../sim/ChannelEvent.h"
#include <QWidget>

class LuaRuntime;
class TimelineWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QTimer;

class SimulatorPanel : public QWidget {
    Q_OBJECT
public:
    explicit SimulatorPanel(QWidget* parent = nullptr);

    // Load source into the runtime. Returns true on success.
    bool loadSource(const QString& source);
    // Same as loadSource but only logs errors (no "[OK] Script loaded." spam).
    bool loadSourceSilent(const QString& source);

    bool isLoaded() const { return m_loaded; }

public slots:
    void play();
    void pause();
    void reset();
    void step();
    void triggerSoftButton(bool pushed);
    void triggerExternalA();

signals:
    void log(const QString& msg);
    // Emitted when the user pressed Run/Step but no script is loaded.
    // MainWindow reacts by feeding the current editor source.
    void needsScript();

private slots:
    void onTimerTick();

private:
    void appendLogs(const QString& s);
    void refreshState();

    LuaRuntime* m_runtime;
    TimelineWidget* m_timeline;
    QPlainTextEdit* m_log;

    QPushButton* m_playBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_resetBtn;
    QPushButton* m_stepBtn;
    QPushButton* m_softBtn;
    QPushButton* m_trigBtn;
    QDoubleSpinBox* m_speedFactor;
    QSpinBox* m_stepMs;
    QLabel* m_clock;
    QLabel* m_channelStates[4];

    QTimer* m_timer;
    bool m_loaded = false;
    bool m_setupCalled = false;
    QVector<ChannelEvent> m_allEvents;
};
