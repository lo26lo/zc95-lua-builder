#include "LuaRuntime.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <QByteArray>
#include <QFile>

namespace {
constexpr const char* kSelfRegistryKey = "zc95_LuaRuntime_self";
}

LuaRuntime* LuaRuntime::self(lua_State* L) {
    lua_pushstring(L, kSelfRegistryKey);
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* p = static_cast<LuaRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return p;
}

LuaRuntime::LuaRuntime(QObject* parent) : QObject(parent) {
    L = luaL_newstate();
    luaL_openlibs(L);

    // Stash this in the Lua registry so static callbacks can find us.
    lua_pushstring(L, kSelfRegistryKey);
    lua_pushlightuserdata(L, this);
    lua_settable(L, LUA_REGISTRYINDEX);

    registerApi();
    installCompat();
    installResourceSearcher();
}

LuaRuntime::~LuaRuntime() {
    if (L) lua_close(L);
}

void LuaRuntime::resetState() {
    m_state = SimState();
    m_events.clear();
    m_output.clear();
    m_currentTimeMs = 0;
}

void LuaRuntime::registerApi() {
    // Build a global "zc" table with our API.
    lua_newtable(L);
    auto reg = [&](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };
    reg("ChannelOn", api_ChannelOn);
    reg("ChannelOff", api_ChannelOff);
    reg("ChannelPulseMs", api_ChannelPulseMs);
    reg("SetPower", api_SetPower);
    reg("SetFrequency", api_SetFrequency);
    reg("SetPulseWidth", api_SetPulseWidth);
    reg("SetMenuOption", api_SetMenuOption);
    reg("DelayMs", api_DelayMs);
    reg("EnableTriphase", api_EnableTriphase);
    reg("LinkChannels", api_LinkChannels);
    reg("AccIoWrite", api_AccIoWrite);
    lua_setglobal(L, "zc");

    // Override print to capture output.
    lua_pushcfunction(L, api_print);
    lua_setglobal(L, "print");
}

// ---------------------------------------------------------------
// Lua 5.1 compatibility shim: scripts shipped with ZC95 (notably
// the bundled `ettot.lua`) use `module(name, package.seeall)`,
// which was removed in Lua 5.2+. The polyfill re-creates just
// enough of that behavior to make those scripts load.
// ---------------------------------------------------------------
void LuaRuntime::installCompat() {
    static const char* kCompat = R"LUA(
if not module then
  function module(name, ...)
    local M = package.loaded[name] or rawget(_G, name) or {}
    M._NAME    = name
    M._M       = M
    M._PACKAGE = (name:match("(.-)[^%.]+$")) or ""
    package.loaded[name] = M
    rawset(_G, name, M)
    -- Apply modifiers like package.seeall
    for _, mod in ipairs({...}) do
      if type(mod) == "function" then mod(M) end
    end
    -- Rebind the calling chunk's _ENV to the module table so
    -- bare `function foo() end` and `bar = 1` end up inside M.
    local f = debug.getinfo(2, "f").func
    local i = 1
    while true do
      local n = debug.getupvalue(f, i)
      if not n then break end
      if n == "_ENV" then
        debug.setupvalue(f, i, M)
        return
      end
      i = i + 1
    end
  end
end

if not package.seeall then
  function package.seeall(t)
    local mt = getmetatable(t)
    if not mt then mt = {}; setmetatable(t, mt) end
    mt.__index = _G
  end
end
)LUA";
    if (luaL_dostring(L, kCompat) != LUA_OK) {
        // Should never happen, but stay defensive.
        m_output += QString("[INTERNAL] compat shim failed: %1\n")
                        .arg(QString::fromUtf8(lua_tostring(L, -1)));
        lua_pop(L, 1);
    }
}

// ---------------------------------------------------------------
// Install a custom package.searcher that resolves `require("x")`
// against the embedded Qt resource ":/presets/lib/x.lua".
// Inserted at index 2, just after package.preload.
// ---------------------------------------------------------------
void LuaRuntime::installResourceSearcher() {
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    lua_getfield(L, -1, "searchers");
    if (!lua_istable(L, -1)) { lua_pop(L, 2); return; }

    int n = (int)lua_rawlen(L, -1);
    // Shift existing entries up by one to make room at index 2.
    for (int i = n; i >= 2; --i) {
        lua_rawgeti(L, -1, i);
        lua_rawseti(L, -2, i + 1);
    }
    lua_pushcfunction(L, api_resourceSearcher);
    lua_rawseti(L, -2, 2);

    lua_pop(L, 2); // pop searchers, package
}

int LuaRuntime::api_resourceSearcher(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    QString modName = QString::fromUtf8(name);
    // Allow dotted names: "foo.bar" -> "foo/bar.lua"
    QString rel = modName;
    rel.replace('.', '/');

    const QStringList candidates = {
        QString(":/presets/lib/%1.lua").arg(rel),
        QString(":/presets/lib/%1/init.lua").arg(rel),
        QString(":/presets/%1.lua").arg(rel),
    };

    for (const QString& p : candidates) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QByteArray bytes = f.readAll();
        QString chunkName = "@qrc" + p;
        QByteArray cn = chunkName.toUtf8();
        if (luaL_loadbuffer(L, bytes.constData(), bytes.size(), cn.constData()) != LUA_OK) {
            return lua_error(L);
        }
        QByteArray pUtf8 = p.toUtf8();
        lua_pushstring(L, pUtf8.constData());
        return 2; // function + hint
    }
    lua_pushfstring(L, "\n\tno embedded resource for module '%s'", name);
    return 1;
}

bool LuaRuntime::loadScript(const QString& source, QString* error) {
    resetState();
    QByteArray bytes = source.toUtf8();
    if (luaL_loadbuffer(L, bytes.constData(), bytes.size(), "script") != LUA_OK) {
        if (error) *error = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        if (error) *error = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool LuaRuntime::callOptional(const char* name, int nargs, int nresults, QString* error) {
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        // Not present — pop and discard already-pushed args.
        lua_pop(L, 1 + nargs);
        return true;
    }
    // Move function to before its args: function is at top, args were pushed
    // before getglobal. So actually, args were already on stack. Caller must
    // push args first then call this. We need to reorder: insert function.
    // We'll require caller to push args BEFORE calling this with right order.
    // Below path is used like: lua_pushXxx(args), call this with name.
    // But getglobal pushed function on top — we need it before args. Move it.
    lua_insert(L, -(nargs + 1));
    if (lua_pcall(L, nargs, nresults, 0) != LUA_OK) {
        if (error) *error = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool LuaRuntime::callRequired(const char* name, int nargs, int nresults, QString* error) {
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1 + nargs);
        if (error) *error = QString("function %1 not defined").arg(name);
        return false;
    }
    lua_insert(L, -(nargs + 1));
    if (lua_pcall(L, nargs, nresults, 0) != LUA_OK) {
        if (error) *error = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool LuaRuntime::callSetup(QString* error) {
    return callOptional("Setup", 0, 0, error);
}

bool LuaRuntime::callLoop(double time_ms, QString* error) {
    setCurrentTimeMs(time_ms);
    lua_pushnumber(L, time_ms);
    return callRequired("Loop", 1, 0, error);
}

bool LuaRuntime::callMinMaxChange(int menuId, int val, QString* error) {
    lua_pushinteger(L, menuId);
    lua_pushinteger(L, val);
    return callOptional("MinMaxChange", 2, 0, error);
}

bool LuaRuntime::callMultiChoiceChange(int menuId, int choiceId, QString* error) {
    lua_pushinteger(L, menuId);
    lua_pushinteger(L, choiceId);
    return callOptional("MultiChoiceChange", 2, 0, error);
}

bool LuaRuntime::callSoftButton(bool pushed, QString* error) {
    lua_pushboolean(L, pushed);
    return callOptional("SoftButton", 1, 0, error);
}

bool LuaRuntime::callExternalTrigger(const QString& socket, const QString& part,
                                     bool active, QString* error) {
    lua_pushstring(L, socket.toUtf8().constData());
    lua_pushstring(L, part.toUtf8().constData());
    lua_pushboolean(L, active);
    return callOptional("ExternalTrigger", 3, 0, error);
}

QVector<ChannelEvent> LuaRuntime::takeEvents() {
    QVector<ChannelEvent> out;
    out.swap(m_events);
    return out;
}

QString LuaRuntime::takeOutput() {
    QString s;
    s.swap(m_output);
    return s;
}

void LuaRuntime::updatePulses() {
    for (int i = 0; i < 4; ++i) {
        auto& ch = m_state.channels[i];
        if (ch.on && ch.pulseEndsAtMs >= 0 && m_currentTimeMs >= ch.pulseEndsAtMs) {
            // Capture the end time BEFORE resetting it — otherwise the
            // emitted ChannelOff event ends up with timeMs = -1 and the
            // timeline draws the segment all the way back to t=0.
            double endAt = ch.pulseEndsAtMs;
            ch.on = false;
            ch.pulseEndsAtMs = -1;
            ChannelEvent e;
            e.timeMs = endAt;
            e.type = ChannelEventType::ChannelOff;
            e.channel = i + 1;
            m_events.push_back(e);
        }
    }
}

// ---- API implementations ----

int LuaRuntime::api_ChannelOn(lua_State* L) {
    auto* self_ = self(L);
    int ch = (int)luaL_checkinteger(L, 1);
    if (ch >= 1 && ch <= 4) {
        auto& s = self_->m_state.channels[ch - 1];
        s.on = true;
        s.pulseEndsAtMs = -1;
    }
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::ChannelOn;
    e.channel = ch;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_ChannelOff(lua_State* L) {
    auto* self_ = self(L);
    int ch = (int)luaL_checkinteger(L, 1);
    if (ch >= 1 && ch <= 4) {
        auto& s = self_->m_state.channels[ch - 1];
        s.on = false;
        s.pulseEndsAtMs = -1;
    }
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::ChannelOff;
    e.channel = ch;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_ChannelPulseMs(lua_State* L) {
    auto* self_ = self(L);
    int ch = (int)luaL_checkinteger(L, 1);
    int dur = (int)luaL_checkinteger(L, 2);
    if (ch >= 1 && ch <= 4) {
        auto& s = self_->m_state.channels[ch - 1];
        s.on = true;
        s.pulseEndsAtMs = self_->m_currentTimeMs + dur;
    }
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::ChannelPulseMs;
    e.channel = ch;
    e.param1 = dur;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_SetPower(lua_State* L) {
    auto* self_ = self(L);
    int ch = (int)luaL_checkinteger(L, 1);
    int pw = (int)luaL_checkinteger(L, 2);
    if (ch >= 1 && ch <= 4) self_->m_state.channels[ch - 1].power = pw;
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::SetPower;
    e.channel = ch;
    e.param1 = pw;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_SetFrequency(lua_State* L) {
    auto* self_ = self(L);
    int ch = (int)luaL_checkinteger(L, 1);
    int hz = (int)luaL_checkinteger(L, 2);
    if (ch >= 1 && ch <= 4) self_->m_state.channels[ch - 1].frequencyHz = hz;
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::SetFrequency;
    e.channel = ch;
    e.param1 = hz;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_SetPulseWidth(lua_State* L) {
    auto* self_ = self(L);
    int ch = (int)luaL_checkinteger(L, 1);
    int pos = (int)luaL_checkinteger(L, 2);
    int neg = (int)luaL_checkinteger(L, 3);
    if (ch >= 1 && ch <= 4) {
        self_->m_state.channels[ch - 1].pulseWidthPosUs = pos;
        self_->m_state.channels[ch - 1].pulseWidthNegUs = neg;
    }
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::SetPulseWidth;
    e.channel = ch;
    e.param1 = pos;
    e.param2 = neg;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_SetMenuOption(lua_State* L) {
    auto* self_ = self(L);
    int id = (int)luaL_checkinteger(L, 1);
    int val = (int)luaL_checkinteger(L, 2);
    // We don't actually re-fire MinMaxChange asynchronously in the simulator;
    // log the call so the user sees what would happen on the device.
    self_->m_output += QString("[SetMenuOption] id=%1 value=%2\n").arg(id).arg(val);
    return 0;
}

int LuaRuntime::api_DelayMs(lua_State* L) {
    auto* self_ = self(L);
    int ms = (int)luaL_checkinteger(L, 1);
    self_->m_currentTimeMs += ms;
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::DelayMs;
    e.param1 = ms;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_EnableTriphase(lua_State* L) {
    auto* self_ = self(L);
    bool en = lua_toboolean(L, 1);
    self_->m_state.triphaseEnabled = en;
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::EnableTriphase;
    e.param1 = en ? 1 : 0;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_LinkChannels(lua_State* L) {
    auto* self_ = self(L);
    int lead = (int)luaL_checkinteger(L, 1);
    int linked = (int)luaL_checkinteger(L, 2);
    int off = (int)luaL_checkinteger(L, 3);
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::LinkChannels;
    e.channel = lead;
    e.param1 = linked;
    e.param2 = off;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_AccIoWrite(lua_State* L) {
    auto* self_ = self(L);
    int line = (int)luaL_checkinteger(L, 1);
    bool state = lua_toboolean(L, 2);
    if (line >= 1 && line <= 3) self_->m_state.accIo[line - 1] = state;
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::AccIoWrite;
    e.channel = line;
    e.param1 = state ? 1 : 0;
    self_->m_events.push_back(e);
    return 0;
}

int LuaRuntime::api_print(lua_State* L) {
    auto* self_ = self(L);
    int n = lua_gettop(L);
    QString line;
    for (int i = 1; i <= n; ++i) {
        if (i > 1) line += "\t";
        if (lua_isstring(L, i)) {
            line += QString::fromUtf8(lua_tostring(L, i));
        } else {
            // Use tostring conversion via Lua
            lua_getglobal(L, "tostring");
            lua_pushvalue(L, i);
            lua_call(L, 1, 1);
            line += QString::fromUtf8(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    self_->m_output += "[LUA] " + line + "\n";
    ChannelEvent e;
    e.timeMs = self_->m_currentTimeMs;
    e.type = ChannelEventType::Print;
    e.textParam = line;
    self_->m_events.push_back(e);
    return 0;
}
