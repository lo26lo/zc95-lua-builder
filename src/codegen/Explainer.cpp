#include "Explainer.h"

#include <QRegularExpression>

namespace {

// Try to convert a line of Lua into plain English. Returns "" if no
// confident match — caller leaves the column blank.
QString explainLine(const QString& raw) {
    QString line = raw.trimmed();
    if (line.isEmpty()) return "";
    if (line.startsWith("--")) return "(comment)";

    // -- Function definitions ------------------------------------------------
    {
        QRegularExpression re(R"(^function\s+(\w+)\s*\(([^)]*)\))");
        auto m = re.match(line);
        if (m.hasMatch()) {
            QString name = m.captured(1);
            QString args = m.captured(2).trimmed();
            if (name == "Setup")
                return "Function called ONCE before Loop starts. Used for initial setup.";
            if (name == "Loop")
                return "Mandatory main loop, called repeatedly. Argument: time_ms (ms since power-on).";
            if (name == "MinMaxChange")
                return "Called whenever the user moves a MIN_MAX slider on the device.";
            if (name == "MultiChoiceChange")
                return "Called whenever the user changes a MULTI_CHOICE option on the device.";
            if (name == "SoftButton")
                return "Called when the soft button is pressed (true) / released (false).";
            if (name == "ExternalTrigger")
                return "Called when an external trigger (TRIGGER1/2 part A/B) fires.";
            if (name == "BluetoothRemoteKeypress")
                return "Called for each Bluetooth remote keypress.";
            if (name == "BluetoothHidEvent")
                return "Called for raw Bluetooth HID events.";
            if (name == "AudioIntensityChange")
                return "Called with audio intensity (L, R, virt, each 0-255).";
            if (args.isEmpty())
                return QString("Defines a helper function called \"%1\".").arg(name);
            return QString("Defines a helper function \"%1\" taking (%2).").arg(name, args);
        }
    }

    // -- Config = { ----------------------------------------------------------
    if (line.startsWith("Config")) {
        QRegularExpression re(R"(^Config\s*=\s*\{)");
        if (re.match(line).hasMatch())
            return "Begins the Config table — pattern metadata read by the firmware.";
    }

    // -- zc.* calls ----------------------------------------------------------
    {
        QRegularExpression re(R"(zc\.(\w+)\s*\(([^)]*)\))");
        auto m = re.match(line);
        if (m.hasMatch()) {
            QString fn = m.captured(1);
            QString args = m.captured(2).trimmed();
            QStringList parts;
            for (const QString& p : args.split(',')) parts << p.trimmed();
            auto arg = [&](int i) { return i < parts.size() ? parts[i] : QString("?"); };

            if (fn == "ChannelOn")
                return QString("Switch channel %1 ON until ChannelOff is called.").arg(arg(0));
            if (fn == "ChannelOff")
                return QString("Switch channel %1 OFF.").arg(arg(0));
            if (fn == "ChannelPulseMs")
                return QString("Pulse channel %1 ON for %2 ms.").arg(arg(0), arg(1));
            if (fn == "SetPower")
                return QString("Set power on channel %1 to %2 (range 0-1000, scaled by the front-panel dial).").arg(arg(0), arg(1));
            if (fn == "SetFrequency")
                return QString("Set frequency on channel %1 to %2 Hz.").arg(arg(0), arg(1));
            if (fn == "SetPulseWidth")
                return QString("Set pulse width on channel %1: pos=%2 µs, neg=%3 µs.").arg(arg(0), arg(1), arg(2));
            if (fn == "SetMenuOption")
                return QString("Programmatically set menu item %1 to %2 (async — callback fires later).").arg(arg(0), arg(1));
            if (fn == "DelayMs")
                return QString("Sleep for %1 ms (other callbacks still fire).").arg(arg(0));
            if (fn == "EnableTriphase")
                return QString("Enable triphase mode (%1) — channel isolation OFF.").arg(arg(0));
            if (fn == "LinkChannels")
                return QString("Link channel %2 to lead %1 with offset %3%.").arg(arg(0), arg(1), arg(2));
            if (fn == "AccIoWrite")
                return QString("Set accessory I/O line %1 to %2.").arg(arg(0), arg(1));
            return QString("Calls zc.%1(%2).").arg(fn, args);
        }
    }

    // -- print --------------------------------------------------------------
    {
        QRegularExpression re(R"(^print\s*\()");
        if (re.match(line).hasMatch())
            return "Print to the simulator log / device serial output.";
    }

    // -- Control flow -------------------------------------------------------
    {
        // for ch = 1, 4 do
        QRegularExpression re(R"(^for\s+(\w+)\s*=\s*([^,]+),\s*([^,]+)(?:,\s*(\S+))?\s*(do)?)");
        auto m = re.match(line);
        if (m.hasMatch()) {
            QString var = m.captured(1);
            QString lo = m.captured(2);
            QString hi = m.captured(3);
            QString step = m.captured(4);
            if (step.isEmpty()) {
                return QString("Loop with %1 going from %2 to %3.").arg(var, lo, hi);
            }
            return QString("Loop with %1 going from %2 to %3 by %4.").arg(var, lo, hi, step);
        }
    }
    {
        // while X do
        QRegularExpression re(R"(^while\s+(.+?)\s+do)");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("While %1 is true, repeat the block.").arg(m.captured(1));
    }
    {
        // if menu_id == N then
        QRegularExpression re(R"(^if\s*\(?\s*menu_id\s*==\s*(\d+))");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("Check if the changed menu item is id %1.").arg(m.captured(1));
    }
    {
        // elseif menu_id == N then
        QRegularExpression re(R"(^elseif\s*\(?\s*menu_id\s*==\s*(\d+))");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("Otherwise, check if it's menu id %1.").arg(m.captured(1));
    }
    {
        // if pushed then  (typical SoftButton body)
        QRegularExpression re(R"(^if\s+pushed\b)");
        if (re.match(line).hasMatch())
            return "Branch when the soft button is pressed (pushed = true).";
    }
    {
        // if active then  (typical ExternalTrigger body)
        QRegularExpression re(R"(^if\s+active\b)");
        if (re.match(line).hasMatch())
            return "Branch when the trigger fires (active = true).";
    }
    {
        // if time_ms > X
        QRegularExpression re(R"(^if\s*\(?\s*time_ms\s*[<>]=?\s*(.+?)\s*\)?\s*then)");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("Time-gated check (compares time_ms with %1).").arg(m.captured(1));
    }

    if (line == "else") return "Otherwise…";
    if (line == "end")  return "End of the previous block.";
    if (line == "do")   return "Begin the loop body.";
    if (line == "then") return "(then — begin the if branch)";
    if (line == "return") return "Return from the function.";
    if (line == "break")  return "Exit the enclosing loop.";

    // -- Local variable declaration -----------------------------------------
    {
        QRegularExpression re(R"(^local\s+([\w,\s]+?)\s*=\s*(.+))");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("Local variable %1 set to %2.").arg(m.captured(1).trimmed(), m.captured(2).trimmed());
    }
    {
        QRegularExpression re(R"(^local\s+(\w+)\s*$)");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("Declares a local variable \"%1\" (uninitialised).").arg(m.captured(1));
    }

    // -- Bare assignment to a known menu var ---------------------------------
    {
        QRegularExpression re(R"(^(_\w+)\s*=\s*(.+))");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("Set the global \"%1\" to %2.").arg(m.captured(1), m.captured(2).trimmed());
    }

    // -- require ------------------------------------------------------------
    {
        QRegularExpression re(R"(require\s*\(?\s*[\"']([^\"']+)[\"'])");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("Load helper library \"%1\".").arg(m.captured(1));
    }

    // -- module(...) (Lua 5.1) ----------------------------------------------
    {
        QRegularExpression re(R"(module\s*\(\s*[\"'](\w+)[\"'])");
        auto m = re.match(line);
        if (m.hasMatch())
            return QString("Declares this file as the \"%1\" module (Lua 5.1 idiom).").arg(m.captured(1));
    }

    return "";
}

}  // namespace

QVector<Explainer::Line> Explainer::explain(const QString& source) {
    QVector<Line> out;
    const QStringList raw = source.split('\n');
    for (const QString& r : raw) {
        out.push_back({r, explainLine(r)});
    }
    return out;
}

QString Explainer::toHtml(const QVector<Line>& lines) {
    QString html;
    html += "<style>"
            "table { border-collapse: collapse; font-family: Consolas, monospace; }"
            "td.code { background:#1e1e1e; color:#d4d4d4; padding: 2px 8px; "
            "          white-space: pre; vertical-align: top; }"
            "td.note { color:#9bc4d8; padding: 2px 12px; vertical-align: top; "
            "          font-family: Segoe UI, sans-serif; font-size: 9pt; }"
            "td.lineno { color:#666; padding: 2px 6px; text-align: right; }"
            "tr:nth-child(odd) td.code { background:#252526; }"
            "</style>";
    html += "<table>";
    int n = 1;
    for (const auto& l : lines) {
        html += "<tr>";
        html += QString("<td class='lineno'>%1</td>").arg(n++);
        html += QString("<td class='code'>%1</td>").arg(l.code.toHtmlEscaped());
        html += QString("<td class='note'>%1</td>").arg(l.note.toHtmlEscaped());
        html += "</tr>";
    }
    html += "</table>";
    return html;
}
