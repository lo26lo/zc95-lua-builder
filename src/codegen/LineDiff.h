#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Tiny line-level diff for the "show what changed" dialog after Ctrl+G.
// Not a real LCS — uses a hash-based myers-style longest common subsequence
// approximation that works well for the structural changes smart-merge
// produces (Config block replacement + function appends).
class LineDiff {
public:
    enum class Op { Same, Added, Removed };
    struct Hunk {
        Op op;
        QString line;
    };

    static QVector<Hunk> diff(const QString& before, const QString& after);

    // Render as HTML — green added, red removed, gray same.
    static QString toHtml(const QVector<Hunk>& hunks);
};
