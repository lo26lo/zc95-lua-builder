#include "Linter.h"

#include <QRegularExpression>
#include <QSet>

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
}

QVector<Issue> Linter::lint(const ScriptConfig& config, const QString& source) {
    QVector<Issue> issues;
    lintConfig(config, issues);
    lintSource(config, source, issues);
    return issues;
}
