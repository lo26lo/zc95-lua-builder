#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class LuaHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit LuaHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<Rule> m_rules;
    QTextCharFormat m_multiLineCommentFormat;
    QRegularExpression m_commentStart;
    QRegularExpression m_commentEnd;
};
