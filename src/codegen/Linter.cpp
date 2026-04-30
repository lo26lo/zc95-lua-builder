#include "Linter.h"

#include <QRegularExpression>
#include <QSet>
#include <climits>

QString Issue::severityText() const {
    switch (severity) {
        case IssueSeverity::Info: return "info";
        case IssueSeverity::Warning: return "warn";
        case IssueSeverity::Error: return "error";
    }
    return "?";
}

QString Linter::sanitizeForLint(const QString& source) {
    QString out = source;
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
            for (int k = i; k < j && k < out.length(); ++k) {
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

void Linter::lintConfig(const ScriptConfig& config, QVector<Issue>& out) {
    if (config.name.trimmed().isEmpty()) {
        out.push_back({IssueSeverity::Warning, -1, "Pattern name is empty.",
                       "Set a name in the Config tab."});
    }

    // Unique IDs (defensive — UI also enforces this)
    QSet<int> seen;
    for (const auto& mi : config.menuItems) {
        if (seen.contains(mi.id)) {
            out.push_back({IssueSeverity::Error, -1,
                QString("Duplicate menu_item id %1 (\"%2\").").arg(mi.id).arg(mi.title),
                "Each menu item needs a unique id."});
        }
        seen.insert(mi.id);

        if (mi.title.trimmed().isEmpty()) {
            out.push_back({IssueSeverity::Warning, -1,
                QString("Menu item id %1 has empty title.").arg(mi.id), {}});
        }
        if (mi.type == MenuItemType::MinMax) {
            if (mi.min >= mi.max) {
                out.push_back({IssueSeverity::Error, -1,
                    QString("Menu item id %1: min (%2) >= max (%3).").arg(mi.id).arg(mi.min).arg(mi.max), {}});
            }
            if (mi.defaultValue < mi.min || mi.defaultValue > mi.max) {
                out.push_back({IssueSeverity::Warning, -1,
                    QString("Menu item id %1: default (%2) outside [%3..%4].")
                        .arg(mi.id).arg(mi.defaultValue).arg(mi.min).arg(mi.max), {}});
            }
            if (mi.incrementStep <= 0) {
                out.push_back({IssueSeverity::Error, -1,
                    QString("Menu item id %1: increment_step must be > 0.").arg(mi.id), {}});
            }
        } else if (mi.type == MenuItemType::MultiChoice) {
            if (mi.choices.isEmpty()) {
                out.push_back({IssueSeverity::Warning, -1,
                    QString("MULTI_CHOICE id %1 has no choices.").arg(mi.id), {}});
            }
            QSet<int> cids;
            for (const auto& c : mi.choices) {
                if (cids.contains(c.choiceId)) {
                    out.push_back({IssueSeverity::Error, -1,
                        QString("MULTI_CHOICE id %1: duplicate choice_id %2.").arg(mi.id).arg(c.choiceId), {}});
                }
                cids.insert(c.choiceId);
            }
        }
    }

    // Cross-checks between Config and enabled functions
    bool hasAudioView = std::any_of(config.menuItems.begin(), config.menuItems.end(),
        [](const MenuItem& m) {
            return m.type == MenuItemType::AudioViewIntensityStereo
                || m.type == MenuItemType::AudioViewIntensityMono;
        });

    if (config.functions.audioIntensityChange && config.audioMode != AudioMode::AudioIntensity) {
        out.push_back({IssueSeverity::Error, -1,
            "AudioIntensityChange() is enabled but audio_processing_mode is OFF.",
            "Set audio_processing_mode = AUDIO_INTENSITY in the Config tab."});
    }
    if (hasAudioView && config.audioMode != AudioMode::AudioIntensity) {
        out.push_back({IssueSeverity::Warning, -1,
            "An AUDIO_VIEW_INTENSITY_* menu item is present but audio_processing_mode is OFF.",
            "It will not display anything useful unless audio is enabled."});
    }
    if (config.functions.bluetoothRemoteKeypress && !config.bluetoothRemotePassthrough) {
        out.push_back({IssueSeverity::Warning, -1,
            "BluetoothRemoteKeypress() is enabled but bluetooth_remote_passthrough is false.",
            "Tick \"Bluetooth remote passthrough\" in Config, otherwise this callback is never called."});
    }
    if (config.functions.softButton && config.softButtonLabel.trimmed().isEmpty()) {
        out.push_back({IssueSeverity::Warning, -1,
            "SoftButton() is enabled but soft_button label is empty.",
            "Set the label in the Config tab so the button is visible on screen."});
    }
}

void Linter::lintSource(const ScriptConfig& config, const QString& source, QVector<Issue>& out) {
    if (source.isEmpty()) return;
    QString clean = sanitizeForLint(source);

    auto lineOf = [&](int idx) {
        int line = 1;
        for (int i = 0; i < idx && i < clean.length(); ++i) {
            if (clean[i] == '\n') ++line;
        }
        return line;
    };

    // Range check helper: scan a regex like zc.SetPower(<num>, <num>) and check args.
    auto scan = [&](const QString& pattern, std::function<void(const QRegularExpressionMatch&, int)> check) {
        QRegularExpression re(pattern);
        auto it = re.globalMatch(clean);
        while (it.hasNext()) {
            auto m = it.next();
            check(m, lineOf(m.capturedStart()));
        }
    };

    // zc.SetPower(channel, power): channel 1-4, power 0-1000
    scan(R"(zc\.SetPower\s*\(\s*(-?\d+)\s*,\s*(-?\d+)\s*\))", [&](const QRegularExpressionMatch& m, int line) {
        int ch = m.captured(1).toInt();
        int pw = m.captured(2).toInt();
        if (ch < 1 || ch > 4) out.push_back({IssueSeverity::Error, line,
            QString("zc.SetPower channel %1 out of range (1-4).").arg(ch), {}});
        if (pw < 0 || pw > 1000) out.push_back({IssueSeverity::Error, line,
            QString("zc.SetPower power %1 out of range (0-1000).").arg(pw), {}});
    });

    // zc.SetFrequency(channel, hz): hz 1-300
    scan(R"(zc\.SetFrequency\s*\(\s*(-?\d+)\s*,\s*(-?\d+)\s*\))", [&](const QRegularExpressionMatch& m, int line) {
        int ch = m.captured(1).toInt();
        int hz = m.captured(2).toInt();
        if (ch < 1 || ch > 4) out.push_back({IssueSeverity::Error, line,
            QString("zc.SetFrequency channel %1 out of range (1-4).").arg(ch), {}});
        if (hz < 1 || hz > 300) out.push_back({IssueSeverity::Error, line,
            QString("zc.SetFrequency %1 Hz out of range (1-300).").arg(hz), {}});
    });

    // zc.SetPulseWidth(channel, pos, neg): pos/neg 0-255
    scan(R"(zc\.SetPulseWidth\s*\(\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\))",
         [&](const QRegularExpressionMatch& m, int line) {
        int ch = m.captured(1).toInt();
        int pos = m.captured(2).toInt();
        int neg = m.captured(3).toInt();
        if (ch < 1 || ch > 4) out.push_back({IssueSeverity::Error, line,
            QString("zc.SetPulseWidth channel %1 out of range (1-4).").arg(ch), {}});
        if (pos < 0 || pos > 255) out.push_back({IssueSeverity::Error, line,
            QString("zc.SetPulseWidth pos %1 out of range (0-255 us).").arg(pos), {}});
        if (neg < 0 || neg > 255) out.push_back({IssueSeverity::Error, line,
            QString("zc.SetPulseWidth neg %1 out of range (0-255 us).").arg(neg), {}});
    });

    // ChannelOn/Off/PulseMs channel range
    scan(R"(zc\.(ChannelOn|ChannelOff)\s*\(\s*(-?\d+)\s*\))", [&](const QRegularExpressionMatch& m, int line) {
        int ch = m.captured(2).toInt();
        if (ch < 1 || ch > 4) out.push_back({IssueSeverity::Error, line,
            QString("zc.%1 channel %2 out of range (1-4).").arg(m.captured(1)).arg(ch), {}});
    });
    scan(R"(zc\.ChannelPulseMs\s*\(\s*(-?\d+)\s*,\s*(-?\d+)\s*\))",
         [&](const QRegularExpressionMatch& m, int line) {
        int ch = m.captured(1).toInt();
        int dur = m.captured(2).toInt();
        if (ch < 1 || ch > 4) out.push_back({IssueSeverity::Error, line,
            QString("zc.ChannelPulseMs channel %1 out of range (1-4).").arg(ch), {}});
        if (dur < 0) out.push_back({IssueSeverity::Error, line,
            QString("zc.ChannelPulseMs duration %1 must be >= 0.").arg(dur), {}});
    });

    // AccIoWrite line 1-3
    scan(R"(zc\.AccIoWrite\s*\(\s*(-?\d+))", [&](const QRegularExpressionMatch& m, int line) {
        int n = m.captured(1).toInt();
        if (n < 1 || n > 3) out.push_back({IssueSeverity::Error, line,
            QString("zc.AccIoWrite line %1 out of range (1-3).").arg(n), {}});
    });

    // DelayMs 0-10000
    scan(R"(zc\.DelayMs\s*\(\s*(-?\d+)\s*\))", [&](const QRegularExpressionMatch& m, int line) {
        int n = m.captured(1).toInt();
        if (n < 0 || n > 10000) out.push_back({IssueSeverity::Error, line,
            QString("zc.DelayMs %1 ms out of range (0-10000).").arg(n), {}});
    });

    // LinkChannels offset 0-100
    scan(R"(zc\.LinkChannels\s*\(\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\))",
         [&](const QRegularExpressionMatch& m, int line) {
        int lead = m.captured(1).toInt();
        int linked = m.captured(2).toInt();
        int off = m.captured(3).toInt();
        if (lead < 1 || lead > 4) out.push_back({IssueSeverity::Error, line,
            QString("zc.LinkChannels lead channel %1 out of range (1-4).").arg(lead), {}});
        if (linked < 0 || linked > 4) out.push_back({IssueSeverity::Error, line,
            QString("zc.LinkChannels linked channel %1 out of range (0-4, 0=unlink).").arg(linked), {}});
        if (off < 0 || off > 100) out.push_back({IssueSeverity::Error, line,
            QString("zc.LinkChannels offset %1 out of range (0-100%%).").arg(off), {}});
    });

    // Triphase flag check
    bool usesTriphase = clean.contains(QRegularExpression(R"(\bzc\.(EnableTriphase|LinkChannels)\b)"));
    if (usesTriphase && !config.allowTriphase) {
        // Find the first occurrence to get a line.
        QRegularExpression re(R"(\bzc\.(EnableTriphase|LinkChannels)\b)");
        auto m = re.match(clean);
        int line = m.hasMatch() ? lineOf(m.capturedStart()) : -1;
        out.push_back({IssueSeverity::Error, line,
            "Triphase function used but allow_triphase is not set.",
            "Tick \"Allow triphase\" in the Config tab."});
    }

    // Detect callbacks that reference menu_id values not present in menu_items.
    QSet<int> validIds;
    for (const auto& mi : config.menuItems) validIds.insert(mi.id);

    auto scanMenuIdUse = [&](const QString& pattern) {
        QRegularExpression re(pattern);
        auto it = re.globalMatch(clean);
        while (it.hasNext()) {
            auto m = it.next();
            int id = m.captured(1).toInt();
            if (!validIds.contains(id)) {
                out.push_back({IssueSeverity::Warning, lineOf(m.capturedStart()),
                    QString("menu_id %1 referenced but no matching menu_item.").arg(id), {}});
            }
        }
    };
    // Patterns like "if (menu_id == 1)" or "if menu_id == 1 then"
    scanMenuIdUse(R"(menu_id\s*==\s*(\d+))");

    // SetMenuOption argument
    scanMenuIdUse(R"(zc\.SetMenuOption\s*\(\s*(\d+)\s*,)");

    // Lua 5.1 idioms. The embedded simulator polyfills these so scripts run,
    // but flag as Info so users know it's a pre-5.2 pattern.
    {
        QRegularExpression re(R"(\bmodule\s*\()");
        auto m = re.match(clean);
        if (m.hasMatch()) {
            out.push_back({IssueSeverity::Info, lineOf(m.capturedStart()),
                "Use of module() — Lua 5.1 idiom (simulator polyfills it).",
                "Modern style: `local M = {} ... return M`."});
        }
    }
    {
        QRegularExpression re(R"(\bpackage\.seeall\b)");
        auto m = re.match(clean);
        if (m.hasMatch()) {
            out.push_back({IssueSeverity::Info, lineOf(m.capturedStart()),
                "package.seeall — Lua 5.1 idiom (simulator polyfills it).", {}});
        }
    }

    // require("ettot") and other libs: the simulator now bundles ettot via
    // Qt resources, but other on-device libs may still be missing.
    {
        QRegularExpression re(R"(\brequire\s*\(?\s*[\"']([^\"']+)[\"'])");
        auto it = re.globalMatch(clean);
        while (it.hasNext()) {
            auto m = it.next();
            QString lib = m.captured(1);
            if (lib == "ettot") continue;  // bundled, no warning needed
            out.push_back({IssueSeverity::Info, lineOf(m.capturedStart()),
                QString("require(\"%1\") — provided by the ZC95 firmware but not by the embedded simulator.").arg(lib),
                "On-device this loads the ZC95 helper lib; the simulator may need a stub."});
        }
    }

    // Loop is mandatory — warn if missing.
    if (!clean.contains(QRegularExpression(R"(\bfunction\s+Loop\s*\()"))) {
        out.push_back({IssueSeverity::Warning, -1,
            "No top-level function Loop(time_ms) found.",
            "Loop() is the mandatory entry point that runs continuously."});
    }

    // -----------------------------------------------------------------
    // Safety rules — these are about user comfort and electrical safety,
    // not Lua correctness. They lean to the conservative side and can
    // be silenced by tightening the offending value.
    // -----------------------------------------------------------------

    // S1. SetFrequency above 250 Hz — comfort warning.
    {
        QRegularExpression re(R"(zc\.SetFrequency\s*\(\s*(-?\d+)\s*,\s*(\d+)\s*\))");
        auto it = re.globalMatch(clean);
        while (it.hasNext()) {
            auto m = it.next();
            int hz = m.captured(2).toInt();
            if (hz > 250 && hz <= 300) {
                out.push_back({IssueSeverity::Warning, lineOf(m.capturedStart()),
                    QString("zc.SetFrequency(%1) — frequencies above 250 Hz can feel harsh.").arg(hz),
                    "Consider exposing this as a MIN_MAX menu item so the user can tune down."});
            }
        }
    }

    // S2. SetPulseWidth above 200 µs — intensity warning.
    {
        QRegularExpression re(R"(zc\.SetPulseWidth\s*\(\s*(-?\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\))");
        auto it = re.globalMatch(clean);
        while (it.hasNext()) {
            auto m = it.next();
            int pos = m.captured(2).toInt();
            int neg = m.captured(3).toInt();
            int wmax = qMax(pos, neg);
            if (wmax > 200 && wmax <= 255) {
                out.push_back({IssueSeverity::Warning, lineOf(m.capturedStart()),
                    QString("zc.SetPulseWidth(%1, %2) — pulse widths above 200 µs deliver a lot of charge.").arg(pos).arg(neg),
                    "Combined with high power this can be very intense. Start lower and ramp up."});
            }
        }
    }

    // S3. SetPower(*, 1000) hard-coded in Setup — no headroom for the user.
    // Also flag SetPower(*, X) with X >= 800 if no MIN_MAX menu item exists.
    {
        bool hasMinMaxMenu = std::any_of(config.menuItems.begin(), config.menuItems.end(),
            [](const MenuItem& m) { return m.type == MenuItemType::MinMax; });

        QRegularExpression re(R"(zc\.SetPower\s*\(\s*(-?\d+)\s*,\s*(\d+)\s*\))");
        auto it = re.globalMatch(clean);
        while (it.hasNext()) {
            auto m = it.next();
            int pwr = m.captured(2).toInt();
            if (pwr >= 800 && !hasMinMaxMenu) {
                out.push_back({IssueSeverity::Warning, lineOf(m.capturedStart()),
                    QString("zc.SetPower(*, %1) is hard-coded high and there's no MIN_MAX menu to dial it down.").arg(pwr),
                    "Add a MIN_MAX item (\"Intensity\" 0-1000) and use the value here instead of a literal."});
                break;  // one warning is enough — don't spam
            }
        }
    }

    // S4. Kill-switch detection — at least one of:
    //   • SoftButton callback ticked,
    //   • ExternalTrigger callback ticked,
    //   • A MULTI_CHOICE menu item that looks like an off/on switch
    //     (we don't inspect the choices' wording — too fragile — but
    //      at least the script can be paused via the front-panel knob
    //      if a MULTI_CHOICE exists at all).
    // The front-panel power dial is always the ultimate kill-switch on
    // real hardware, so this is informational, not an error.
    {
        const auto& f = config.functions;
        bool hasMultiChoice = std::any_of(config.menuItems.begin(), config.menuItems.end(),
            [](const MenuItem& m) { return m.type == MenuItemType::MultiChoice; });
        bool hasSwitchableMode = f.softButton || f.externalTrigger || hasMultiChoice;
        if (!hasSwitchableMode) {
            out.push_back({IssueSeverity::Warning, -1,
                "No software kill-switch in this pattern.",
                "Tick SoftButton in Functions and label it in Config — gives you a one-tap stop. "
                "(The front-panel power dial is always available as a hardware fallback.)"});
        }
    }

    // S5. EnableTriphase / LinkChannels usage when allow_triphase is set — info only.
    {
        QRegularExpression re(R"(\bzc\.EnableTriphase\s*\(\s*true\s*\))");
        auto m = re.match(clean);
        if (m.hasMatch() && config.allowTriphase) {
            out.push_back({IssueSeverity::Info, lineOf(m.capturedStart()),
                "Triphase enabled — channel isolation is OFF, currents from different channels can combine.",
                "Pay extra attention to electrode placement. See ZC95 safety docs."});
        }
    }

    // S6. Loop body looks empty (no zc.* calls in the file at all) — info.
    {
        bool hasLoopFn = clean.contains(QRegularExpression(R"(\bfunction\s+Loop\s*\()"));
        bool anyZcCall = clean.contains(QRegularExpression(R"(\bzc\.[A-Za-z_]+\s*\()"));
        if (hasLoopFn && !anyZcCall) {
            out.push_back({IssueSeverity::Info, -1,
                "No zc.* calls anywhere in the script.",
                "The pattern won't drive any channel — did you forget to write the body?"});
        }
    }

    // S7. Loop calls SetPower(*, X>=800) on every tick — likely a runaway.
    // Heuristic: if SetPower with a high literal occurs INSIDE a function
    // named Loop and there's no surrounding `if`, that's a smell.
    // We do a naive scan: for each high SetPower, walk backward in the
    // sanitized text to the most recent `function Loop(`. If found and
    // there's no `if `/`elseif ` token between Loop's start and the call,
    // we warn.
    {
        QRegularExpression callRe(R"(zc\.SetPower\s*\(\s*-?\d+\s*,\s*(\d+)\s*\))");
        QRegularExpression loopStartRe(R"(\bfunction\s+Loop\s*\()");
        QRegularExpression loopEndRe(R"(\bend\b)");
        auto loopMatch = loopStartRe.match(clean);
        if (loopMatch.hasMatch()) {
            int loopStart = loopMatch.capturedEnd();
            // Find the matching `end` — naive: walk forward through
            // function/if/for/while/do depth.
            int depth = 1;
            int i = loopStart;
            QRegularExpression openersRe(R"(\b(function|if|for|while|do|repeat)\b)");
            int loopEnd = clean.length();
            while (i < clean.length()) {
                auto m1 = openersRe.match(clean, i);
                auto m2 = loopEndRe.match(clean, i);
                int p1 = m1.hasMatch() ? m1.capturedStart() : INT_MAX;
                int p2 = m2.hasMatch() ? m2.capturedStart() : INT_MAX;
                if (p1 == INT_MAX && p2 == INT_MAX) break;
                if (p1 < p2) { ++depth; i = m1.capturedEnd(); }
                else { --depth; i = m2.capturedEnd(); if (depth == 0) { loopEnd = m2.capturedStart(); break; } }
            }

            QString loopBody = clean.mid(loopStart, loopEnd - loopStart);
            auto it = callRe.globalMatch(loopBody);
            while (it.hasNext()) {
                auto m = it.next();
                int pwr = m.captured(1).toInt();
                if (pwr < 800) continue;
                // Look at the preceding 80 characters for an `if `/`elseif `
                int callPos = m.capturedStart();
                int lookback = qMax(0, callPos - 80);
                QString prefix = loopBody.mid(lookback, callPos - lookback);
                if (!prefix.contains(QRegularExpression(R"(\b(if|elseif)\b)"))) {
                    int absPos = loopStart + callPos;
                    out.push_back({IssueSeverity::Warning, lineOf(absPos),
                        QString("Unconditional zc.SetPower(*, %1) inside Loop — runs every tick.").arg(pwr),
                        "Wrap it in `if` so it only runs on state changes, or move it to Setup()."});
                    break;
                }
            }
        }
    }
}

QVector<Issue> Linter::lint(const ScriptConfig& config, const QString& source) {
    QVector<Issue> issues;
    lintConfig(config, issues);
    lintSource(config, source, issues);
    return issues;
}
