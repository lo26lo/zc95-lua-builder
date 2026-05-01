#pragma once

#include "../sim/ChannelEvent.h"
#include "../model/MenuItem.h"
#include <QWidget>
#include <QVector>

class LuaRuntime;
class TimelineWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QTimer;
class QGroupBox;
class QVBoxLayout;

class SimulatorPanel : public QWidget {
    Q_OBJECT
public:
    explicit SimulatorPanel(QWidget* parent = nullptr);

    // Load source into the runtime. Returns true on success.
    bool loadSource(const QString& source);
    // Same as loadSource but only logs errors (no "[OK] Script loaded." spam).
    bool loadSourceSilent(const QString& source);

    bool isLoaded() const { return m_loaded; }

    // Rebuild the "Menu controls" group from the form's menu items.
    // Called by MainWindow whenever the form changes or a script is loaded.
    void setMenuItems(const QVector<MenuItem>& items);

    // Test helper: drive a single MIN_MAX item through min → default → max
    // (or a MULTI_CHOICE through all its choices), running a few Loop()
    // ticks between each step. Returns true if the script's channel state
    // changed at least once during the test (i.e. the item is wired).
    bool testMenuItemDrive(const MenuItem& item);

    // After a successful loadSource*, return the script's Config table
    // as a ScriptConfig, with menu_item IDs that were expressed as
    // identifiers (MenuId.FREQ, …) resolved to their real integer
    // values. Used by MainWindow to fix up the form whose regex parser
    // can't evaluate Lua expressions.
    bool resolveScriptConfig(struct ScriptConfig& out, QString* warning = nullptr) const;

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
    // Emitted when the user moves a live MIN_MAX slider or picks a
    // MULTI_CHOICE option. MainWindow forwards it to the LCD preview so
    // the on-screen value mirrors what the user is testing.
    //   For MIN_MAX:    value = the slider value
    //   For MULTI_CHOICE: value = the selected choice_id
    void liveMenuValueChanged(int menuId, int value);

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
    QString m_currentSource;     // cached so reset() can re-init the lua_State

    // Live menu controls (sliders for MIN_MAX, combos for MULTI_CHOICE).
    QGroupBox* m_menuGroup = nullptr;
    QVBoxLayout* m_menuLayout = nullptr;
    QVector<MenuItem> m_currentMenuItems;
};
