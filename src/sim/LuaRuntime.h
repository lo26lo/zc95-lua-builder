#pragma once

#include "ChannelEvent.h"
#include "../model/ScriptConfig.h"
#include <QObject>
#include <QString>
#include <QVector>

struct lua_State;

class LuaRuntime : public QObject {
    Q_OBJECT
public:
    explicit LuaRuntime(QObject* parent = nullptr);
    ~LuaRuntime();

    // Reset interpreter and load source. Returns false on syntax/runtime error
    // during chunk load; the error is placed in *error.
    bool loadScript(const QString& source, QString* error = nullptr);

    // Read the Config global as a ScriptConfig. Must be called AFTER
    // loadScript. Returns true on success — fills `out` with the
    // Lua-evaluated values, including any menu_item IDs that were
    // expressed as identifiers (MenuId.FREQ etc.) instead of literals.
    // The regex parser can't resolve those; this method can.
    bool extractScriptConfig(ScriptConfig& out, QString* warning = nullptr) const;

    bool callSetup(QString* error = nullptr);                    // optional
    bool callLoop(double time_ms, QString* error = nullptr);     // mandatory
    bool callMinMaxChange(int menuId, int val, QString* error = nullptr);
    bool callMultiChoiceChange(int menuId, int choiceId, QString* error = nullptr);
    bool callSoftButton(bool pushed, QString* error = nullptr);
    bool callExternalTrigger(const QString& socket, const QString& part, bool active,
                             QString* error = nullptr);

    // Drain captured events (cleared after returning).
    QVector<ChannelEvent> takeEvents();
    QString takeOutput();

    // Snapshot of internal state (channels, triphase, acc IO).
    const SimState& state() const { return m_state; }

    void setCurrentTimeMs(double t) { m_currentTimeMs = t; }
    double currentTimeMs() const { return m_currentTimeMs; }

    // Auto-end ChannelPulseMs that have expired (called between Loop iterations).
    void updatePulses();

private:
    bool callOptional(const char* name, int nargs, int nresults, QString* error);
    bool callRequired(const char* name, int nargs, int nresults, QString* error);

    void registerApi();
    void installCompat();          // Lua 5.1 module()/package.seeall polyfill
    void installResourceSearcher();// Resolve require() against :/presets/lib/
    void resetState();

    // Lua C callbacks (forwarded to instance via upvalue / registry).
    static int api_ChannelOn(lua_State* L);
    static int api_ChannelOff(lua_State* L);
    static int api_ChannelPulseMs(lua_State* L);
    static int api_SetPower(lua_State* L);
    static int api_SetFrequency(lua_State* L);
    static int api_SetPulseWidth(lua_State* L);
    static int api_SetMenuOption(lua_State* L);
    static int api_DelayMs(lua_State* L);
    static int api_EnableTriphase(lua_State* L);
    static int api_LinkChannels(lua_State* L);
    static int api_AccIoWrite(lua_State* L);
    static int api_print(lua_State* L);
    static int api_resourceSearcher(lua_State* L);

    static LuaRuntime* self(lua_State* L);

    lua_State* L = nullptr;
    SimState m_state;
    QVector<ChannelEvent> m_events;
    QString m_output;
    double m_currentTimeMs = 0;
};
