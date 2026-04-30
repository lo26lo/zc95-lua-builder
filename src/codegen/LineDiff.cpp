#include "LineDiff.h"

#include <QHash>

namespace {

// Compute the LCS length matrix for two line lists. Standard O(N*M).
// We only use this for "small" inputs (typical scripts < 500 lines), so
// quadratic is fine.
QVector<QVector<int>> lcsTable(const QStringList& a, const QStringList& b) {
    const int n = a.size();
    const int m = b.size();
    QVector<QVector<int>> t(n + 1, QVector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (a[i - 1] == b[j - 1]) t[i][j] = t[i - 1][j - 1] + 1;
            else t[i][j] = qMax(t[i - 1][j], t[i][j - 1]);
        }
    }
    return t;
}

}  // namespace

QVector<LineDiff::Hunk> LineDiff::diff(const QString& before, const QString& after) {
    QStringList a = before.split('\n');
    QStringList b = after.split('\n');

    auto t = lcsTable(a, b);
    QVector<Hunk> hunks;
    int i = a.size();
    int j = b.size();
    while (i > 0 && j > 0) {
        if (a[i - 1] == b[j - 1]) {
            hunks.prepend({Op::Same, a[i - 1]});
            --i; --j;
        } else if (t[i - 1][j] >= t[i][j - 1]) {
            hunks.prepend({Op::Removed, a[i - 1]});
            --i;
        } else {
            hunks.prepend({Op::Added, b[j - 1]});
            --j;
        }
    }
    while (i > 0) { hunks.prepend({Op::Removed, a[i - 1]}); --i; }
    while (j > 0) { hunks.prepend({Op::Added, b[j - 1]}); --j; }
    return hunks;
}

QString LineDiff::toHtml(const QVector<Hunk>& hunks) {
    QString html;
    html += "<style>"
            "div.line { font-family: Consolas, monospace; padding: 1px 6px; "
            "           white-space: pre; }"
            "div.same    { color: #888; }"
            "div.added   { background: #14361b; color: #b3e8b8; }"
            "div.removed { background: #3b1414; color: #e8b3b3; "
            "              text-decoration: line-through; }"
            "</style>";
    for (const auto& h : hunks) {
        QString cls = "same";
        QString prefix = "  ";
        if (h.op == Op::Added)   { cls = "added";   prefix = "+ "; }
        if (h.op == Op::Removed) { cls = "removed"; prefix = "- "; }
        html += QString("<div class='line %1'>%2%3</div>")
                    .arg(cls, prefix, h.line.toHtmlEscaped());
    }
    return html;
}
