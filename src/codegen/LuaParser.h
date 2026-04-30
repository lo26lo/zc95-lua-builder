#pragma once

#include "../model/ScriptConfig.h"
#include <QString>

struct LuaParseResult {
    bool ok = false;
    ScriptConfig config;
    QString warning;  // non-fatal notes (e.g. "menu_items not found")
};

class LuaParser {
public:
    // Parse a Lua script source and extract the Config block + presence of
    // top-level callback functions. Tolerant: returns ok=true with warnings
    // when the Config block is partially missing, ok=false only on hard errors.
    static LuaParseResult parse(const QString& source);

private:
    // Strip Lua comments (-- to EOL, --[[ ... ]]), preserving line layout
    // by replacing comment chars with spaces. Strings are preserved.
    static QString stripComments(const QString& source);

    // Find position of the closing '}' that matches the '{' at openPos.
    // Returns -1 if not found. Skips over strings and nested braces.
    static int findMatchingBrace(const QString& text, int openPos);

    // Skip a Lua string starting at quote position; returns position after
    // closing quote. Handles \" and \\ escapes. If start is not a string,
    // returns start.
    static int skipString(const QString& text, int start);

    // Given the inner text of a Lua table { ... }, return a list of top-level
    // key/value pairs (e.g. name = "foo", menu_items = { ... }).
    // Values are kept as raw strings (with surrounding whitespace trimmed).
    static QVector<QPair<QString, QString>> topLevelPairs(const QString& tableInner);

    // Given the inner text of a Lua table list { {...}, {...}, ... } return
    // each top-level brace-group's inner content (without surrounding braces).
    static QVector<QString> topLevelBraceGroups(const QString& tableInner);

    // Strip surrounding quotes and unescape \" \\ for a Lua string literal.
    // If value is not a quoted string, returns it unchanged.
    static QString unquote(const QString& value);

    static MenuItem parseMenuItem(const QString& itemBody);
    static QVector<MultiChoiceOption> parseChoices(const QString& choicesBody);
};
