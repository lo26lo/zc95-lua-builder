#pragma once

#include <QString>
#include <QVector>

struct ApiEntry {
    QString name;          // e.g. "zc.ChannelOn"
    QString signature;     // e.g. "zc.ChannelOn(channel)"
    QString description;
    QStringList parameters;
    QString example;
};

class ApiDoc {
public:
    static const QVector<ApiEntry>& entries();

    // Look up by exact name. Returns nullptr if not found.
    static const ApiEntry* find(const QString& name);
};
