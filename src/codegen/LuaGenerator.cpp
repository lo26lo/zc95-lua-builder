#include "LuaGenerator.h"
#include <QStringList>
#include <QRegularExpression>

QString LuaGenerator::quote(const QString& s) {
    QString escaped = s;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return "\"" + escaped + "\"";
}

QString LuaGenerator::indent(int level) {
    return QString(level * 4, ' ');
}

QString LuaGenerator::variableNameFor(const MenuItem& item) {
    QString base = item.title.toLower();
    // Replace runs of non-alphanumerics with underscore
    QRegularExpression re("[^a-z0-9]+");
    base.replace(re, "_");
    while (base.startsWith('_')) base.remove(0, 1);
    while (base.endsWith('_')) base.chop(1);
    if (base.isEmpty()) base = QString("menu_%1").arg(item.id);
    return "_" + base;
}

// ---------------------------------------------------------------------------
// Smart merge helpers (private to this file)
// ---------------------------------------------------------------------------
namespace {

// Replace comments and string contents with spaces so structural scans can
// safely look for braces and identifiers. Preserves line layout.
QString sanitize(const QString& src) {
    QString out = src;
    int i = 0;
    while (i < out.length()) {
        QChar c = out[i];
        if (c == '"' || c == '\'') {
            QChar q = c;
            int j = i + 1;
            while (j < out.length()) {
                if (out[j] == '\\' && j + 1 < out.length()) { j += 2; continue; }
                if (out[j] == q) { ++j; break; }
                if (out[j] == '\n') break;
                ++j;
            }
            for (int k = i + 1; k < j - 1 && k < out.length(); ++k) {
                if (out[k] != '\n') out[k] = ' ';
            }
            i = j;
            continue;
        }
        if (c == '-' && i + 1 < out.length() && out[i + 1] == '-') {
            if (i + 3 < out.length() && out[i + 2] == '[' && out[i + 3] == '[') {
                int end = out.indexOf("]]", i + 4);
                int stop = (end < 0) ? out.length() : end + 2;
                for (int j = i; j < stop; ++j) if (out[j] != '\n') out[j] = ' ';
                i = stop;
                continue;
            }
            int end = out.indexOf('\n', i);
            int stop = (end < 0) ? out.length() : end;
            for (int j = i; j < stop; ++j) out[j] = ' ';
            i = stop;
            continue;
        }
        ++i;
    }
    return out;
}

int matchBrace(const QString& clean, int openPos) {
    if (openPos < 0 || openPos >= clean.length() || clean[openPos] != '{') return -1;
    int depth = 0;
    for (int i = openPos; i < clean.length(); ++i) {
        QChar c = clean[i];
        if (c == '{') ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return -1;
}

// Find the [start, end] character range of the existing Config block in
// `source` (start = position of `C` in "Config", end = position AFTER closing
// brace). Returns {-1, -1} if not found.
QPair<int, int> findConfigBlock(const QString& source) {
    QString clean = sanitize(source);
    QRegularExpression re(R"(\bConfig\s*=\s*\{)");
    auto m = re.match(clean);
    if (!m.hasMatch()) return {-1, -1};
    int start = m.capturedStart();
    int openBrace = clean.indexOf('{', start);
    int close = matchBrace(clean, openBrace);
    if (close < 0) return {-1, -1};
    return {start, close + 1};
}

bool hasFunctionDef(const QString& cleanSource, const QString& name) {
    QRegularExpression re(QString(R"(\bfunction\s+%1\s*\()").arg(name));
    return re.match(cleanSource).hasMatch();
}

}  // namespace

QString LuaGenerator::mergeIntoSource(const ScriptConfig& config,
                                      const QString& existingSource,
                                      bool smart) {
    if (existingSource.trimmed().isEmpty()) {
        return generate(config, smart);
    }

    auto range = findConfigBlock(existingSource);
    if (range.first < 0) {
        // No Config block detected — fall back to a full generation rather
        // than silently producing something that won't run on-device.
        return generate(config, smart);
    }

    QString newConfig = generateConfigBlock(config);
    if (newConfig.endsWith('\n')) newConfig.chop(1);

    QString merged = existingSource;
    merged.replace(range.first, range.second - range.first, newConfig);

    // Append stubs for any enabled function that isn't already defined.
    QString clean = sanitize(merged);
    struct FnSpec { bool enabled; const char* name; QString stub; };

    const auto& f = config.functions;
    QString minMaxBody = smart ? generateMinMaxChangeBody(config) : QString();
    QString multiBody  = smart ? generateMultiChoiceChangeBody(config) : QString();

    QVector<FnSpec> fns = {
        {f.setup, "Setup",
            "function Setup()\n"
            "    -- Called once when the pattern starts (before Loop).\n"
            "end\n"},
        {f.loop, "Loop",
            "function Loop(time_ms)\n"
            "    -- Called periodically. time_ms = ms since power-on.\n"
            "end\n"},
        {f.minMaxChange, "MinMaxChange",
            QString("function MinMaxChange(menu_id, min_max_val)\n")
                + (minMaxBody.isEmpty()
                    ? QString("    -- Called when a MIN_MAX menu item is changed.\n")
                    : minMaxBody)
                + "end\n"},
        {f.multiChoiceChange, "MultiChoiceChange",
            QString("function MultiChoiceChange(menu_id, choice_id)\n")
                + (multiBody.isEmpty()
                    ? QString("    -- Called when a MULTI_CHOICE menu item is changed.\n")
                    : multiBody)
                + "end\n"},
        {f.softButton, "SoftButton",
            "function SoftButton(pushed)\n"
            "    -- pushed = true on press, false on release.\n"
            "end\n"},
        {f.externalTrigger, "ExternalTrigger",
            "function ExternalTrigger(socket, part, active)\n"
            "end\n"},
        {f.bluetoothRemoteKeypress, "BluetoothRemoteKeypress",
            "function BluetoothRemoteKeypress(key)\n"
            "end\n"},
        {f.bluetoothHidEvent, "BluetoothHidEvent",
            "function BluetoothHidEvent(usage_page, usage, value)\n"
            "end\n"},
        {f.audioIntensityChange, "AudioIntensityChange",
            "function AudioIntensityChange(left_chan, right_chan, virt_chan)\n"
            "    -- left/right/virt 0-255. Scale to 0-1000 for power.\n"
            "end\n"},
    };

    QString appended;
    for (const auto& spec : fns) {
        if (!spec.enabled) continue;
        if (hasFunctionDef(clean, spec.name)) continue;
        if (appended.isEmpty() && !merged.endsWith("\n\n")) {
            appended += merged.endsWith("\n") ? "\n" : "\n\n";
        }
        appended += spec.stub;
        appended += "\n";
    }
    merged += appended;
    return merged;
}

QString LuaGenerator::generate(const ScriptConfig& config, bool smart) {
    QString out;
    out += "-- Generated by zc95-lua-builder\n";
    out += "-- Pattern: " + config.name + "\n\n";

    if (smart) {
        QString vars = generateVariableDeclarations(config);
        if (!vars.isEmpty()) out += vars + "\n";
    }

    out += generateConfigBlock(config);
    out += "\n";
    out += generateFunctions(config, smart);
    return out;
}

QString LuaGenerator::generateVariableDeclarations(const ScriptConfig& config) {
    QString out;
    for (const auto& mi : config.menuItems) {
        if (mi.type == MenuItemType::MinMax) {
            out += variableNameFor(mi) + " = " + QString::number(mi.defaultValue) + "\n";
        } else if (mi.type == MenuItemType::MultiChoice && !mi.choices.isEmpty()) {
            out += variableNameFor(mi) + " = " + QString::number(mi.choices.first().choiceId) + "\n";
        }
    }
    return out;
}

QString LuaGenerator::generateConfigBlock(const ScriptConfig& config) {
    QString out;
    out += "Config = {\n";
    out += indent(1) + "name = " + quote(config.name) + ",\n";
    out += indent(1) + "audio_processing_mode = " + quote(ScriptConfig::audioModeToString(config.audioMode)) + ",\n";

    if (!config.softButtonLabel.isEmpty()) {
        out += indent(1) + "soft_button = " + quote(config.softButtonLabel) + ",\n";
    }
    if (config.loopFreqHz > 0) {
        out += indent(1) + "loop_freq_hz = " + QString::number(config.loopFreqHz) + ",\n";
    }
    if (config.allowTriphase) {
        out += indent(1) + "allow_triphase = true,\n";
    }
    if (config.bluetoothRemotePassthrough) {
        out += indent(1) + "bluetooth_remote_passthrough = true,\n";
    }

    if (!config.menuItems.isEmpty()) {
        out += indent(1) + "menu_items = {\n";
        for (int i = 0; i < config.menuItems.size(); ++i) {
            out += generateMenuItem(config.menuItems[i]);
            if (i < config.menuItems.size() - 1) out += ",";
            out += "\n";
        }
        out += indent(1) + "}\n";
    } else {
        if (out.endsWith(",\n")) {
            out.chop(2);
            out += "\n";
        }
    }
    out += "}\n";
    return out;
}

QString LuaGenerator::generateMenuItem(const MenuItem& item) {
    QString out;
    out += indent(2) + "{\n";
    out += indent(3) + "type = " + quote(MenuItem::typeToString(item.type)) + ",\n";
    out += indent(3) + "title = " + quote(item.title) + ",\n";
    out += indent(3) + "id = " + QString::number(item.id) + ",\n";
    out += indent(3) + "group = " + QString::number(item.group);

    if (item.type == MenuItemType::MinMax) {
        out += ",\n";
        out += indent(3) + "min = " + QString::number(item.min) + ",\n";
        out += indent(3) + "max = " + QString::number(item.max) + ",\n";
        out += indent(3) + "increment_step = " + QString::number(item.incrementStep) + ",\n";
        out += indent(3) + "uom = " + quote(item.uom) + ",\n";
        out += indent(3) + "default = " + QString::number(item.defaultValue) + "\n";
    } else if (item.type == MenuItemType::MultiChoice) {
        out += ",\n";
        out += indent(3) + "choices = {\n";
        for (int i = 0; i < item.choices.size(); ++i) {
            const auto& c = item.choices[i];
            out += indent(4) + "{choice_id = " + QString::number(c.choiceId)
                + ", description = " + quote(c.description) + "}";
            if (i < item.choices.size() - 1) out += ",";
            out += "\n";
        }
        out += indent(3) + "}\n";
    } else {
        out += "\n";
    }
    out += indent(2) + "}";
    return out;
}

QString LuaGenerator::generateMinMaxChangeBody(const ScriptConfig& config) {
    QString out;
    bool first = true;
    for (const auto& mi : config.menuItems) {
        if (mi.type != MenuItemType::MinMax) continue;
        if (first) {
            out += indent(1) + "if (menu_id == " + QString::number(mi.id) + ") then\n";
            first = false;
        } else {
            out += indent(1) + "elseif (menu_id == " + QString::number(mi.id) + ") then\n";
        }
        out += indent(2) + variableNameFor(mi) + " = min_max_val\n";
    }
    if (!first) out += indent(1) + "end\n";
    return out;
}

QString LuaGenerator::generateMultiChoiceChangeBody(const ScriptConfig& config) {
    QString out;
    bool first = true;
    for (const auto& mi : config.menuItems) {
        if (mi.type != MenuItemType::MultiChoice) continue;
        if (first) {
            out += indent(1) + "if (menu_id == " + QString::number(mi.id) + ") then\n";
            first = false;
        } else {
            out += indent(1) + "elseif (menu_id == " + QString::number(mi.id) + ") then\n";
        }
        out += indent(2) + variableNameFor(mi) + " = choice_id\n";
    }
    if (!first) out += indent(1) + "end\n";
    return out;
}

QString LuaGenerator::generateFunctions(const ScriptConfig& config, bool smart) {
    QString out;
    const auto& f = config.functions;

    if (f.setup) {
        out += "function Setup()\n";
        out += indent(1) + "-- Called once when the pattern starts (before Loop).\n";
        out += indent(1) + "-- Set initial power level for each channel here, e.g.:\n";
        out += indent(1) + "-- zc.SetPower(1, 1000)\n";
        out += "end\n\n";
    }

    if (f.loop) {
        out += "function Loop(time_ms)\n";
        out += indent(1) + "-- Called periodically. time_ms = ms since power-on (float, microsecond precision).\n";
        out += "end\n\n";
    }

    if (f.minMaxChange) {
        out += "function MinMaxChange(menu_id, min_max_val)\n";
        if (smart) {
            QString body = generateMinMaxChangeBody(config);
            if (!body.isEmpty()) out += body;
            else out += indent(1) + "-- Called when a MIN_MAX menu item is changed.\n";
        } else {
            out += indent(1) + "-- Called when a MIN_MAX menu item is changed.\n";
        }
        out += "end\n\n";
    }

    if (f.multiChoiceChange) {
        out += "function MultiChoiceChange(menu_id, choice_id)\n";
        if (smart) {
            QString body = generateMultiChoiceChangeBody(config);
            if (!body.isEmpty()) out += body;
            else out += indent(1) + "-- Called when a MULTI_CHOICE menu item is changed.\n";
        } else {
            out += indent(1) + "-- Called when a MULTI_CHOICE menu item is changed.\n";
        }
        out += "end\n\n";
    }

    if (f.softButton) {
        out += "function SoftButton(pushed)\n";
        out += indent(1) + "-- pushed = true on press, false on release.\n";
        out += "end\n\n";
    }

    if (f.externalTrigger) {
        out += "function ExternalTrigger(socket, part, active)\n";
        out += indent(1) + "-- socket: \"TRIGGER1\" or \"TRIGGER2\"\n";
        out += indent(1) + "-- part:   \"A\" or \"B\"\n";
        out += indent(1) + "-- active: true on trigger, false on release\n";
        out += "end\n\n";
    }

    if (f.bluetoothRemoteKeypress) {
        out += "function BluetoothRemoteKeypress(key)\n";
        out += indent(1) + "-- key: KEY_BUTTON, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SHUTTER, KEY_UNKNOWN\n";
        out += "end\n\n";
    }

    if (f.bluetoothHidEvent) {
        out += "function BluetoothHidEvent(usage_page, usage, value)\n";
        out += indent(1) + "-- Raw HID events from a paired bluetooth device.\n";
        out += "end\n\n";
    }

    if (f.audioIntensityChange) {
        out += "function AudioIntensityChange(left_chan, right_chan, virt_chan)\n";
        out += indent(1) + "-- Values are 0-255. Scale to 0-1000 if used to modulate power.\n";
        out += "end\n\n";
    }

    return out;
}
