#include "LuaHighlighter.h"

LuaHighlighter::LuaHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    Rule rule;

    // Lua keywords
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(86, 156, 214));
    keywordFormat.setFontWeight(QFont::Bold);
    const QStringList keywords = {
        "\\band\\b", "\\bbreak\\b", "\\bdo\\b", "\\belse\\b", "\\belseif\\b",
        "\\bend\\b", "\\bfalse\\b", "\\bfor\\b", "\\bfunction\\b", "\\bgoto\\b",
        "\\bif\\b", "\\bin\\b", "\\blocal\\b", "\\bnil\\b", "\\bnot\\b",
        "\\bor\\b", "\\brepeat\\b", "\\breturn\\b", "\\bthen\\b", "\\btrue\\b",
        "\\buntil\\b", "\\bwhile\\b"
    };
    for (const QString& kw : keywords) {
        rule.pattern = QRegularExpression(kw);
        rule.format = keywordFormat;
        m_rules.append(rule);
    }

    // zc.* and known top-level callbacks
    QTextCharFormat zcFormat;
    zcFormat.setForeground(QColor(220, 220, 170));
    rule.pattern = QRegularExpression("\\bzc\\.[A-Za-z_]+\\b");
    rule.format = zcFormat;
    m_rules.append(rule);

    QTextCharFormat callbackFormat;
    callbackFormat.setForeground(QColor(78, 201, 176));
    callbackFormat.setFontWeight(QFont::Bold);
    const QStringList callbacks = {
        "\\bSetup\\b", "\\bLoop\\b", "\\bMinMaxChange\\b", "\\bMultiChoiceChange\\b",
        "\\bSoftButton\\b", "\\bExternalTrigger\\b", "\\bBluetoothRemoteKeypress\\b",
        "\\bBluetoothHidEvent\\b", "\\bAudioIntensityChange\\b", "\\bConfig\\b",
        "\\bprint\\b"
    };
    for (const QString& cb : callbacks) {
        rule.pattern = QRegularExpression(cb);
        rule.format = callbackFormat;
        m_rules.append(rule);
    }

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(181, 206, 168));
    rule.pattern = QRegularExpression("\\b[0-9]+(\\.[0-9]+)?\\b");
    rule.format = numberFormat;
    m_rules.append(rule);

    // Strings
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(206, 145, 120));
    rule.pattern = QRegularExpression("\"([^\"\\\\]|\\\\.)*\"");
    rule.format = stringFormat;
    m_rules.append(rule);
    rule.pattern = QRegularExpression("'([^'\\\\]|\\\\.)*'");
    rule.format = stringFormat;
    m_rules.append(rule);

    // Single-line comment
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(106, 153, 85));
    commentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression("--[^\\n]*");
    rule.format = commentFormat;
    m_rules.append(rule);

    // Multi-line comments --[[ ... ]]
    m_multiLineCommentFormat = commentFormat;
    m_commentStart = QRegularExpression("--\\[\\[");
    m_commentEnd = QRegularExpression("\\]\\]");
}

void LuaHighlighter::highlightBlock(const QString& text) {
    for (const Rule& rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1) {
        auto m = m_commentStart.match(text);
        startIndex = m.hasMatch() ? m.capturedStart() : -1;
    }

    while (startIndex >= 0) {
        auto endMatch = m_commentEnd.match(text, startIndex);
        int endIndex = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, m_multiLineCommentFormat);
        auto next = m_commentStart.match(text, startIndex + commentLength);
        startIndex = next.hasMatch() ? next.capturedStart() : -1;
    }
}
