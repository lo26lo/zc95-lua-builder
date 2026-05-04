#pragma once

#include "../model/ScriptConfig.h"
#include <QString>

class LuaGenerator {
public:
    // Generate a complete Lua script. If smart=true (default), declare
    // variables for each MIN_MAX/MULTI_CHOICE menu item and pre-fill the
    // MinMaxChange/MultiChoiceChange handlers to assign them.
    static QString generate(const ScriptConfig& config, bool smart = true);

    // Surgical merge: replace the existing `Config = {...}` block in
    // existingSource with one freshly generated from config, and append stubs
    // for any newly-enabled function that isn't already defined. Existing
    // function bodies are preserved verbatim.
    //
    // `removeFunctions`: list of top-level function names to delete from
    // the source entirely (signature + body). Use this for callbacks the
    // user just unticked in the Functions panel. Pass an empty list to
    // skip removals (default — keeps the historical "smart merge never
    // destroys code" guarantee).
    //
    // If existingSource has no Config block, falls back to generate().
    // Returns the merged source.
    static QString mergeIntoSource(const ScriptConfig& config,
                                   const QString& existingSource,
                                   bool smart = true,
                                   const QStringList& removeFunctions = {});

    // Find the [start, end) char range of a top-level function definition
    // (`function Name(...) ... end`) in `source`. Returns {-1,-1} if not
    // found. Comment / string contents are ignored. Public so MainWindow
    // can preview which ranges will be deleted.
    static QPair<int, int> findFunctionRange(const QString& source,
                                             const QString& name);

    // Helper: produce a Lua-safe identifier from a menu item title (snake_case
    // with leading underscore). e.g. "Delay (ms)" -> "_delay_ms".
    static QString variableNameFor(const MenuItem& item);

    // Public so tests can verify Config block emission directly.
    static QString generateConfigBlock(const ScriptConfig& config);

private:
    static QString generateMenuItem(const MenuItem& item);
    static QString generateVariableDeclarations(const ScriptConfig& config);
    static QString generateFunctions(const ScriptConfig& config, bool smart);
    static QString generateMinMaxChangeBody(const ScriptConfig& config);
    static QString generateMultiChoiceChangeBody(const ScriptConfig& config);
    static QString quote(const QString& s);
    static QString indent(int level);
};
