#include "LuaParser.h"

#include <QRegularExpression>
#include <QStringList>

QString LuaParser::stripComments(const QString& source) {
    QString out = source;
    int i = 0;
    while (i < out.length()) {
        QChar c = out[i];
        // Skip strings to avoid stripping fake comments inside them.
        if (c == '"' || c == '\'') {
            int end = skipString(out, i);
            i = end;
            continue;
        }
        if (c == '-' && i + 1 < out.length() && out[i + 1] == '-') {
            // Long comment --[[ ... ]] (not handling --[==[ etc., rare in our scripts)
            if (i + 3 < out.length() && out[i + 2] == '[' && out[i + 3] == '[') {
                int end = out.indexOf("]]", i + 4);
                int stop = (end < 0) ? out.length() : end + 2;
                for (int j = i; j < stop; ++j) {
                    if (out[j] != '\n') out[j] = ' ';
                }
                i = stop;
                continue;
            }
            // Line comment until newline
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

int LuaParser::skipString(const QString& text, int start) {
    if (start >= text.length()) return start;
    QChar quote = text[start];
    if (quote != '"' && quote != '\'') return start;
    int i = start + 1;
    while (i < text.length()) {
        QChar c = text[i];
        if (c == '\\' && i + 1 < text.length()) {
            i += 2;
            continue;
        }
        if (c == quote) return i + 1;
        ++i;
    }
    return text.length();
}

int LuaParser::findMatchingBrace(const QString& text, int openPos) {
    if (openPos < 0 || openPos >= text.length() || text[openPos] != '{') return -1;
    int depth = 0;
    int i = openPos;
    while (i < text.length()) {
        QChar c = text[i];
        if (c == '"' || c == '\'') {
            i = skipString(text, i);
            continue;
        }
        if (c == '{') ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0) return i;
        }
        ++i;
    }
    return -1;
}

QVector<QPair<QString, QString>> LuaParser::topLevelPairs(const QString& tableInner) {
    QVector<QPair<QString, QString>> result;
    int i = 0;
    int n = tableInner.length();

    auto skipWs = [&](int& p) {
        while (p < n && tableInner[p].isSpace()) ++p;
    };

    while (i < n) {
        skipWs(i);
        if (i >= n) break;

        // Read identifier (key).
        int keyStart = i;
        while (i < n) {
            QChar c = tableInner[i];
            if (c.isLetterOrNumber() || c == '_') ++i;
            else break;
        }
        QString key = tableInner.mid(keyStart, i - keyStart);
        if (key.isEmpty()) {
            // Not a key=value pair, skip one char
            ++i;
            continue;
        }
        skipWs(i);
        if (i >= n || tableInner[i] != '=') {
            // Could be a positional table entry, skip until comma at top-level.
            // Find next top-level comma to advance.
            int depth = 0;
            while (i < n) {
                QChar c = tableInner[i];
                if (c == '"' || c == '\'') { i = skipString(tableInner, i); continue; }
                if (c == '{') ++depth;
                else if (c == '}') --depth;
                else if (c == ',' && depth == 0) { ++i; break; }
                ++i;
            }
            continue;
        }
        ++i; // consume '='
        skipWs(i);

        // Read value until top-level comma (or end).
        int valStart = i;
        int depth = 0;
        while (i < n) {
            QChar c = tableInner[i];
            if (c == '"' || c == '\'') { i = skipString(tableInner, i); continue; }
            if (c == '{') ++depth;
            else if (c == '}') {
                if (depth == 0) break;
                --depth;
            } else if (c == ',' && depth == 0) {
                break;
            }
            ++i;
        }
        QString val = tableInner.mid(valStart, i - valStart).trimmed();
        if (i < n && tableInner[i] == ',') ++i;

        result.push_back({key, val});
    }
    return result;
}

QVector<QString> LuaParser::topLevelBraceGroups(const QString& tableInner) {
    QVector<QString> groups;
    int n = tableInner.length();
    int i = 0;
    while (i < n) {
        if (tableInner[i] == '{') {
            int end = findMatchingBrace(tableInner, i);
            if (end < 0) break;
            groups.push_back(tableInner.mid(i + 1, end - i - 1));
            i = end + 1;
        } else if (tableInner[i] == '"' || tableInner[i] == '\'') {
            i = skipString(tableInner, i);
        } else {
            ++i;
        }
    }
    return groups;
}

QString LuaParser::unquote(const QString& value) {
    QString v = value.trimmed();
    if (v.length() >= 2 && (v.front() == '"' || v.front() == '\'') && v.back() == v.front()) {
        QString inner = v.mid(1, v.length() - 2);
        QString out;
        out.reserve(inner.size());
        for (int i = 0; i < inner.length(); ++i) {
            QChar c = inner[i];
            if (c == '\\' && i + 1 < inner.length()) {
                QChar n = inner[i + 1];
                if (n == 'n') out += '\n';
                else if (n == 't') out += '\t';
                else out += n;
                ++i;
            } else {
                out += c;
            }
        }
        return out;
    }
    return v;
}

QVector<MultiChoiceOption> LuaParser::parseChoices(const QString& choicesBody) {
    QVector<MultiChoiceOption> result;
    auto groups = topLevelBraceGroups(choicesBody);
    for (const QString& g : groups) {
        MultiChoiceOption c;
        for (const auto& kv : topLevelPairs(g)) {
            if (kv.first == "choice_id") c.choiceId = kv.second.toInt();
            else if (kv.first == "description") c.description = unquote(kv.second);
        }
        result.push_back(c);
    }
    return result;
}

MenuItem LuaParser::parseMenuItem(const QString& itemBody) {
    MenuItem item;
    for (const auto& kv : topLevelPairs(itemBody)) {
        const QString& k = kv.first;
        const QString& v = kv.second;
        if (k == "type") {
            QString s = unquote(v);
            if (s == "MIN_MAX") item.type = MenuItemType::MinMax;
            else if (s == "MULTI_CHOICE") item.type = MenuItemType::MultiChoice;
            else if (s == "AUDIO_VIEW_INTENSITY_STEREO") item.type = MenuItemType::AudioViewIntensityStereo;
            else if (s == "AUDIO_VIEW_INTENSITY_MONO") item.type = MenuItemType::AudioViewIntensityMono;
        }
        else if (k == "title") item.title = unquote(v);
        else if (k == "id") item.id = v.toInt();
        else if (k == "group") item.group = v.toInt();
        else if (k == "min") item.min = v.toInt();
        else if (k == "max") item.max = v.toInt();
        else if (k == "increment_step") item.incrementStep = v.toInt();
        else if (k == "uom") item.uom = unquote(v);
        else if (k == "default") item.defaultValue = v.toInt();
        else if (k == "choices") {
            // v includes the surrounding braces
            QString trimmed = v.trimmed();
            if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
                trimmed = trimmed.mid(1, trimmed.length() - 2);
            }
            item.choices = parseChoices(trimmed);
        }
    }
    return item;
}

LuaParseResult LuaParser::parse(const QString& source) {
    LuaParseResult result;
    QString text = stripComments(source);

    // Locate Config block: "Config" identifier followed by '=' then '{'.
    QRegularExpression configRe(R"(\bConfig\s*=\s*\{)");
    auto m = configRe.match(text);
    if (!m.hasMatch()) {
        result.ok = true;
        result.warning = "No Config = {...} block found.";
        // Still detect functions
    } else {
        int braceOpen = m.capturedEnd() - 1;  // position of '{'
        int braceClose = findMatchingBrace(text, braceOpen);
        if (braceClose < 0) {
            result.ok = false;
            result.warning = "Config block has unbalanced braces.";
            return result;
        }
        QString inner = text.mid(braceOpen + 1, braceClose - braceOpen - 1);
        ScriptConfig& c = result.config;
        for (const auto& kv : topLevelPairs(inner)) {
            const QString& k = kv.first;
            const QString& v = kv.second;
            if (k == "name") c.name = unquote(v);
            else if (k == "audio_processing_mode") {
                QString s = unquote(v);
                c.audioMode = (s == "AUDIO_INTENSITY") ? AudioMode::AudioIntensity : AudioMode::Off;
            }
            else if (k == "soft_button") c.softButtonLabel = unquote(v);
            else if (k == "loop_freq_hz") c.loopFreqHz = v.toInt();
            else if (k == "allow_triphase") c.allowTriphase = (v.trimmed().compare("true", Qt::CaseInsensitive) == 0);
            else if (k == "bluetooth_remote_passthrough") c.bluetoothRemotePassthrough = (v.trimmed().compare("true", Qt::CaseInsensitive) == 0);
            else if (k == "menu_items") {
                QString trimmed = v.trimmed();
                if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
                    trimmed = trimmed.mid(1, trimmed.length() - 2);
                }
                for (const QString& g : topLevelBraceGroups(trimmed)) {
                    c.menuItems.push_back(parseMenuItem(g));
                }
            }
        }
    }

    // Detect top-level function definitions.
    auto hasFn = [&](const QString& name) {
        QRegularExpression re("\\bfunction\\s+" + QRegularExpression::escape(name) + "\\s*\\(");
        return re.match(text).hasMatch();
    };
    EnabledFunctions& f = result.config.functions;
    f.setup = hasFn("Setup");
    // Loop() is mandatory by design, but reflect what the file actually has —
    // otherwise the FunctionsPanel will lie about a Loop it doesn't see, and
    // the linter's "missing Loop()" warning will never fire after parsing.
    f.loop = hasFn("Loop");
    f.minMaxChange = hasFn("MinMaxChange");
    f.multiChoiceChange = hasFn("MultiChoiceChange");
    f.softButton = hasFn("SoftButton");
    f.externalTrigger = hasFn("ExternalTrigger");
    f.bluetoothRemoteKeypress = hasFn("BluetoothRemoteKeypress");
    f.bluetoothHidEvent = hasFn("BluetoothHidEvent");
    f.audioIntensityChange = hasFn("AudioIntensityChange");

    result.ok = true;
    return result;
}
