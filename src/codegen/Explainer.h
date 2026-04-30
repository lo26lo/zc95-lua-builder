#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Best-effort plain-English line-by-line explanation of a ZC95 Lua script.
// Used by the "Explain" dialog to help users who don't read Lua fluently
// understand what their script (or an official preset) does.
//
// The explainer is INTENTIONALLY naive — it pattern-matches on common
// idioms (`zc.*(args)`, `if menu_id == N`, `for ch = 1, 4`, …) rather
// than running a real Lua parser. Lines it can't recognise get an empty
// note rather than a guess.
class Explainer {
public:
    struct Line {
        QString code;     // original line, verbatim
        QString note;     // human description (may be empty)
    };

    // Annotate every line of `source`. Returns a vector parallel to the
    // source's split-by-newline.
    static QVector<Line> explain(const QString& source);

    // Convenience: render the explanation as a side-by-side HTML table
    // ready to drop into a QTextBrowser / QMessageBox.
    static QString toHtml(const QVector<Line>& lines);
};
