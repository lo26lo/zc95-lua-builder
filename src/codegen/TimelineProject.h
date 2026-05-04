#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

// A single parameter slot on a timeline event. Can be either a literal
// integer (e.g. power=800) or a reference to a Lua variable defined by
// the form's MIN_MAX menu items (e.g. power=_intensity, evaluated at
// runtime). The variable name is stored verbatim and emitted as-is in
// the generated Lua — no escaping; we trust the form's name sanitiser.
struct TimelineParam {
    enum Kind { Literal, Variable };
    Kind kind = Literal;
    int literalValue = 0;
    QString variableName;          // e.g. "_intensity"

    // Lua expression: "800" or "_intensity"
    QString toLuaExpr() const;

    QJsonObject toJson() const;
    static TimelineParam fromJson(const QJsonObject& o);

    static TimelineParam ofLit(int v)         { TimelineParam p; p.kind = Literal; p.literalValue = v; return p; }
    static TimelineParam ofVar(const QString& n) { TimelineParam p; p.kind = Variable; p.variableName = n; return p; }
};

// One placed event on the timeline editor.
//   Pulse  — a short on-then-off rectangle (uses durationMs).
//   On     — turn channel on. Until matched by an Off event the channel
//            stays driven. (durationMs ignored — visualised by extending
//            to next Off or to cycle end).
//   Off    — turn channel off.
struct TimelineEvent {
    enum Type { Pulse = 0, On = 1, Off = 2 };
    Type type = Pulse;
    int channel = 1;               // 1..4
    double startMs = 0;
    double durationMs = 100;       // only meaningful for Pulse
    TimelineParam power     = TimelineParam::ofLit(800);
    TimelineParam frequency = TimelineParam::ofLit(150);
    TimelineParam pulseWidth = TimelineParam::ofLit(150);
    QString comment;               // optional — for the user's notes

    QJsonObject toJson() const;
    static TimelineEvent fromJson(const QJsonObject& o);
};

// The full project the timeline editor is editing. Serialised to JSON
// and embedded in the generated .lua script inside a sentinel comment
// block so the editor can re-load its own output.
struct TimelineProject {
    QVector<TimelineEvent> events;
    bool   loop   = false;         // wrap time_ms modulo cycleMs?
    double cycleMs = 10000;        // length of one cycle (used for loop and viewport)

    QJsonObject toJson() const;
    static TimelineProject fromJson(const QJsonObject& o);

    // Serialise to compact JSON (one line) for embedding.
    QString toJsonString() const;
    static TimelineProject fromJsonString(const QString& s);
};
