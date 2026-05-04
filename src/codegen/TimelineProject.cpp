#include "TimelineProject.h"

#include <QJsonDocument>

QString TimelineParam::toLuaExpr() const {
    if (kind == Literal) return QString::number(literalValue);
    return variableName.isEmpty() ? QString::number(literalValue) : variableName;
}

QJsonObject TimelineParam::toJson() const {
    QJsonObject o;
    o["kind"] = (kind == Literal) ? "lit" : "var";
    if (kind == Literal) o["v"] = literalValue;
    else o["name"] = variableName;
    return o;
}

TimelineParam TimelineParam::fromJson(const QJsonObject& o) {
    TimelineParam p;
    p.kind = (o.value("kind").toString() == "var") ? Variable : Literal;
    if (p.kind == Literal) p.literalValue = o.value("v").toInt();
    else p.variableName = o.value("name").toString();
    return p;
}

QJsonObject TimelineEvent::toJson() const {
    QJsonObject o;
    o["t"]      = (int)type;
    o["ch"]     = channel;
    o["start"]  = startMs;
    o["dur"]    = durationMs;
    o["pwr"]    = power.toJson();
    o["freq"]   = frequency.toJson();
    o["pw"]     = pulseWidth.toJson();
    if (!comment.isEmpty()) o["c"] = comment;
    return o;
}

TimelineEvent TimelineEvent::fromJson(const QJsonObject& o) {
    TimelineEvent e;
    e.type        = (Type)o.value("t").toInt(0);
    e.channel     = o.value("ch").toInt(1);
    e.startMs     = o.value("start").toDouble(0);
    e.durationMs  = o.value("dur").toDouble(100);
    e.power       = TimelineParam::fromJson(o.value("pwr").toObject());
    e.frequency   = TimelineParam::fromJson(o.value("freq").toObject());
    e.pulseWidth  = TimelineParam::fromJson(o.value("pw").toObject());
    e.comment     = o.value("c").toString();
    return e;
}

QJsonObject TimelineProject::toJson() const {
    QJsonObject o;
    o["v"]        = 1;            // schema version
    o["loop"]     = loop;
    o["cycleMs"]  = cycleMs;
    QJsonArray arr;
    for (const auto& e : events) arr.append(e.toJson());
    o["events"] = arr;
    return o;
}

TimelineProject TimelineProject::fromJson(const QJsonObject& o) {
    TimelineProject p;
    p.loop    = o.value("loop").toBool(false);
    p.cycleMs = o.value("cycleMs").toDouble(10000);
    QJsonArray arr = o.value("events").toArray();
    for (const auto& v : arr) {
        if (v.isObject()) p.events.append(TimelineEvent::fromJson(v.toObject()));
    }
    return p;
}

QString TimelineProject::toJsonString() const {
    QJsonDocument doc(toJson());
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

TimelineProject TimelineProject::fromJsonString(const QString& s) {
    QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8());
    if (!doc.isObject()) return TimelineProject{};
    return fromJson(doc.object());
}
