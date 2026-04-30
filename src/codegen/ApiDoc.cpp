#include "ApiDoc.h"

const QVector<ApiEntry>& ApiDoc::entries() {
    static const QVector<ApiEntry> data = {
        {"zc.ChannelOn", "zc.ChannelOn(channel)",
         "Switch a channel ON until ChannelOff is called, using the previously "
         "set frequency, pulse width and power level (or defaults).",
         {"channel: 1-4"},
         "zc.ChannelOn(1)"},

        {"zc.ChannelOff", "zc.ChannelOff(channel)",
         "Switch a channel OFF.",
         {"channel: 1-4"},
         "zc.ChannelOff(1)"},

        {"zc.ChannelPulseMs", "zc.ChannelPulseMs(channel, duration_ms)",
         "Pulse a channel ON for the specified number of milliseconds.",
         {"channel: 1-4", "duration_ms: milliseconds"},
         "zc.ChannelPulseMs(1, 100)"},

        {"zc.SetPower", "zc.SetPower(channel, power)",
         "Set output power 0-1000. Scaled by the front-panel dial — e.g. dial "
         "at 50% with power=500 produces 25%. If Setup() is absent, all "
         "channels default to 1000.",
         {"channel: 1-4", "power: 0-1000"},
         "zc.SetPower(1, 1000)"},

        {"zc.SetFrequency", "zc.SetFrequency(channel, hz)",
         "Set output frequency. Defaults to 150Hz.",
         {"channel: 1-4", "hz: 1-300"},
         "zc.SetFrequency(1, 150)"},

        {"zc.SetPulseWidth", "zc.SetPulseWidth(channel, pos_us, neg_us)",
         "Set pulse width. Defaults to 150us. For symmetric pulses (most "
         "patterns), pos and neg should be equal.",
         {"channel: 1-4", "pos_us: 0-255", "neg_us: 0-255"},
         "zc.SetPulseWidth(1, 150, 150)"},

        {"zc.SetMenuOption", "zc.SetMenuOption(menu_id, value)",
         "Programmatically change a menu setting. The change is async — the "
         "MinMaxChange/MultiChoiceChange callback may fire after Loop() runs "
         "several more times.",
         {"menu_id: id of a menu_items entry",
          "value: must match the configured min/max or choice_id"},
         "zc.SetMenuOption(1, 500)"},

        {"zc.DelayMs", "zc.DelayMs(ms)",
         "Sleep for the specified number of milliseconds. Other events "
         "(MinMaxChange, SoftButton, etc.) still fire during the delay.",
         {"ms: 0-10000"},
         "zc.DelayMs(100)"},

        {"zc.EnableTriphase", "zc.EnableTriphase(enabled)",
         "Enable or disable triphase mode. Requires allow_triphase = true in "
         "the Config block. With triphase enabled, the ZC95 will no longer "
         "prevent overlapping pulses on different channels.",
         {"enabled: true / false"},
         "zc.EnableTriphase(true)"},

        {"zc.LinkChannels", "zc.LinkChannels(lead, linked, offset_pct)",
         "Link a channel to a lead channel. A pulse on the linked channel is "
         "generated to overlap with the lead channel: 0% = simultaneous, "
         "100% = linked starts as lead finishes. Once linked, do not control "
         "the linked channel directly (only its power level).",
         {"lead: 1-4", "linked: 1-4 (0 = unlink)", "offset_pct: 0-100"},
         "zc.LinkChannels(1, 2, 0)"},

        {"zc.AccIoWrite", "zc.AccIoWrite(line, state)",
         "Set one of the 3 accessory port I/O lines high (3.3V) or low. "
         "Default state from power-on is HIGH. Lines source only a few mA — "
         "use a logic-level MOSFET for anything more than an LED.",
         {"line: 1-3", "state: true (high) / false (low)"},
         "zc.AccIoWrite(1, true)"},

        {"print", "print(text)",
         "Print to serial output (prefixed with [LUA]) or to the GUI debug "
         "window if running remotely.",
         {"text: any value (auto-converted to string)"},
         "print(\"hello\")"},

        // Callbacks
        {"Setup", "function Setup()",
         "Called once when the pattern starts, before Loop. Use it for "
         "initial configuration (power, frequency, pulse width). If Setup "
         "is not defined, all channels default to power=1000.",
         {},
         "function Setup()\n    zc.SetPower(1, 800)\nend"},

        {"Loop", "function Loop(time_ms)",
         "Called periodically. Mandatory. time_ms is ms since power-on (float, "
         "microsecond precision since v1.7). Default rate: every ~50us if "
         "empty, ~4000us if complex. Use loop_freq_hz in Config to throttle.",
         {"time_ms: ms since power-on"},
         "function Loop(time_ms)\n    -- ...\nend"},

        {"MinMaxChange", "function MinMaxChange(menu_id, min_max_val)",
         "Called when a MIN_MAX menu item is changed.",
         {"menu_id: id from menu_items", "min_max_val: new value"},
         "function MinMaxChange(menu_id, min_max_val)\n"
         "    if (menu_id == 1) then _delay = min_max_val end\n"
         "end"},

        {"MultiChoiceChange", "function MultiChoiceChange(menu_id, choice_id)",
         "Called when a MULTI_CHOICE menu item is changed.",
         {"menu_id: id from menu_items", "choice_id: id of the selected choice"},
         "function MultiChoiceChange(menu_id, choice_id)\n"
         "    if (menu_id == 2) then _mode = choice_id end\n"
         "end"},

        {"SoftButton", "function SoftButton(pushed)",
         "Called for the top-left soft button. Set Config.soft_button = "
         "\"label\" to make the button visible.",
         {"pushed: true on press, false on release"},
         "function SoftButton(pushed)\n"
         "    if (pushed) then zc.ChannelOn(1) else zc.ChannelOff(1) end\n"
         "end"},

        {"ExternalTrigger", "function ExternalTrigger(socket, part, active)",
         "Called when an external trigger fires. With a stereo TRS cable, "
         "shorting Tip-Sleeve = part A (LED green), Tip-Ring = part B (LED red).",
         {"socket: \"TRIGGER1\" or \"TRIGGER2\"",
          "part: \"A\" or \"B\"",
          "active: true on trigger, false on release"},
         "function ExternalTrigger(socket, part, active)\n"
         "    -- ...\nend"},

        {"BluetoothRemoteKeypress", "function BluetoothRemoteKeypress(key)",
         "Called for each keypress from a paired bluetooth remote. Requires "
         "bluetooth_remote_passthrough = true in Config. Single event "
         "per press (no separate release).",
         {"key: KEY_BUTTON, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, "
          "KEY_SHUTTER, KEY_UNKNOWN"},
         "function BluetoothRemoteKeypress(key)\n"
         "    if (key == \"KEY_UP\") then zc.ChannelOn(1) end\n"
         "end"},

        {"BluetoothHidEvent", "function BluetoothHidEvent(usage_page, usage, value)",
         "Receives raw HID events from a paired BT device. For advanced use.",
         {"usage_page: HID usage page", "usage: HID usage", "value: event value"},
         ""},

        {"AudioIntensityChange", "function AudioIntensityChange(left, right, virt)",
         "Receive audio intensity. Values are 0-255 — scale to 0-1000 if used "
         "to modulate power. Requires audio_processing_mode = AUDIO_INTENSITY.",
         {"left: left channel intensity 0-255",
          "right: right channel intensity 0-255",
          "virt: experimental virtual channel — usually ignore"},
         "function AudioIntensityChange(L, R, V)\n"
         "    zc.SetPower(3, L * 4)\n"
         "end"},
    };
    return data;
}

const ApiEntry* ApiDoc::find(const QString& name) {
    for (const auto& e : entries()) {
        if (e.name == name) return &e;
    }
    return nullptr;
}
