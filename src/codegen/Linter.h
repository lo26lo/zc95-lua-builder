#pragma once

#include "../model/ScriptConfig.h"
#include <QString>
#include <QVector>

enum class IssueSeverity {
    Info,
    Warning,
    Error
};

struct Issue {
    IssueSeverity severity = IssueSeverity::Warning;
    int line = -1;       // 1-based line in editor source, -1 if unknown
    QString message;
    QString fixHint;     // optional human-readable fix suggestion

    QString severityText() const;
};

class Linter {
public:
    // Lint a script: combines structural issues from the form (config) and
    // textual issues from the editor source (channel/freq/pulse-width range
    // checks, triphase usage without flag, etc.).
    static QVector<Issue> lint(const ScriptConfig& config, const QString& source);

private:
    static void lintConfig(const ScriptConfig& config, QVector<Issue>& out);
    static void lintSource(const ScriptConfig& config, const QString& source, QVector<Issue>& out);

    // Strip comments and strings, keep line numbering.
    static QString sanitizeForLint(const QString& source);
};
