#include "model/ScriptConfig.h"
#include "codegen/LuaGenerator.h"
#include "codegen/LuaParser.h"
#include "codegen/Linter.h"
#include "sim/LuaRuntime.h"

#include <QtTest/QtTest>
#include <QFile>

class TestRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void emptyConfigRoundtrip();
    void minMaxItemRoundtrip();
    void multiChoiceItemRoundtrip();
    void mixedItemsRoundtrip();
    void allFunctionsRoundtrip();
    void escapedStringsRoundtrip();
    void parserHandlesComments();
    void linterDetectsAudioMismatch();
    void linterDetectsTriphaseWithoutFlag();
    void linterDetectsOutOfRangePower();
    void variableNamingHelpers();
    void officialScriptsParse_data();
    void officialScriptsParse();
    void officialScriptsRoundtrip_data();
    void officialScriptsRoundtrip();
    void mergePreservesFunctionBodies();
    void mergeReplacesConfigBlock();
    void mergeAppendsNewFunctionStubs();
    void mergeFallsBackWhenNoConfigBlock();
    void mergeOnOfficialScriptKeepsLogic();

    // Simulator end-to-end smoke tests.
    void officialScriptsLoadInSimulator_data();
    void officialScriptsLoadInSimulator();
    void officialScriptsSetupRuns_data();
    void officialScriptsSetupRuns();
    void officialScriptsLoopRuns_data();
    void officialScriptsLoopRuns();
    void officialScriptsResolveConfig_data();
    void officialScriptsResolveConfig();
    void officialScriptsExerciseCallbacks_data();
    void officialScriptsExerciseCallbacks();
};

// Helper used by the simulator smoke tests.
static QString loadResourceText(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    return QString::fromUtf8(f.readAll());
}

static const QStringList& officialScriptList() {
    static const QStringList names = {
        "climb", "combo", "intense", "orgasm", "phasing2", "random2",
        "rhythm", "stroke", "tens", "torment", "trifade", "waves"
    };
    return names;
}

static void assertSameConfig(const ScriptConfig& a, const ScriptConfig& b) {
    QCOMPARE(a.name, b.name);
    QCOMPARE((int)a.audioMode, (int)b.audioMode);
    QCOMPARE(a.softButtonLabel, b.softButtonLabel);
    QCOMPARE(a.loopFreqHz, b.loopFreqHz);
    QCOMPARE(a.allowTriphase, b.allowTriphase);
    QCOMPARE(a.bluetoothRemotePassthrough, b.bluetoothRemotePassthrough);
    QCOMPARE(a.menuItems.size(), b.menuItems.size());
    for (int i = 0; i < a.menuItems.size(); ++i) {
        const auto& x = a.menuItems[i];
        const auto& y = b.menuItems[i];
        QCOMPARE((int)x.type, (int)y.type);
        QCOMPARE(x.title, y.title);
        QCOMPARE(x.id, y.id);
        QCOMPARE(x.group, y.group);
        if (x.type == MenuItemType::MinMax) {
            QCOMPARE(x.min, y.min);
            QCOMPARE(x.max, y.max);
            QCOMPARE(x.incrementStep, y.incrementStep);
            QCOMPARE(x.uom, y.uom);
            QCOMPARE(x.defaultValue, y.defaultValue);
        } else if (x.type == MenuItemType::MultiChoice) {
            QCOMPARE(x.choices.size(), y.choices.size());
            for (int j = 0; j < x.choices.size(); ++j) {
                QCOMPARE(x.choices[j].choiceId, y.choices[j].choiceId);
                QCOMPARE(x.choices[j].description, y.choices[j].description);
            }
        }
    }
    QCOMPARE(a.functions.setup, b.functions.setup);
    QCOMPARE(a.functions.minMaxChange, b.functions.minMaxChange);
    QCOMPARE(a.functions.multiChoiceChange, b.functions.multiChoiceChange);
    QCOMPARE(a.functions.softButton, b.functions.softButton);
    QCOMPARE(a.functions.externalTrigger, b.functions.externalTrigger);
    QCOMPARE(a.functions.bluetoothRemoteKeypress, b.functions.bluetoothRemoteKeypress);
    QCOMPARE(a.functions.bluetoothHidEvent, b.functions.bluetoothHidEvent);
    QCOMPARE(a.functions.audioIntensityChange, b.functions.audioIntensityChange);
}

void TestRoundtrip::emptyConfigRoundtrip() {
    ScriptConfig c;
    c.name = "Empty";
    QString lua = LuaGenerator::generate(c);
    auto r = LuaParser::parse(lua);
    QVERIFY(r.ok);
    assertSameConfig(c, r.config);
}

void TestRoundtrip::minMaxItemRoundtrip() {
    ScriptConfig c;
    c.name = "MM";
    MenuItem mi;
    mi.type = MenuItemType::MinMax;
    mi.id = 1;
    mi.title = "Delay";
    mi.min = 100; mi.max = 2000; mi.incrementStep = 100;
    mi.uom = "ms"; mi.defaultValue = 500;
    c.menuItems.push_back(mi);

    QString lua = LuaGenerator::generate(c);
    auto r = LuaParser::parse(lua);
    QVERIFY(r.ok);
    assertSameConfig(c, r.config);
}

void TestRoundtrip::multiChoiceItemRoundtrip() {
    ScriptConfig c;
    c.name = "MC";
    MenuItem mi;
    mi.type = MenuItemType::MultiChoice;
    mi.id = 7;
    mi.title = "Mode";
    mi.choices = { {1, "Pulse"}, {2, "Constant"}, {3, "Random"} };
    c.menuItems.push_back(mi);

    QString lua = LuaGenerator::generate(c);
    auto r = LuaParser::parse(lua);
    QVERIFY(r.ok);
    assertSameConfig(c, r.config);
}

void TestRoundtrip::mixedItemsRoundtrip() {
    ScriptConfig c;
    c.name = "Mixed";
    c.softButtonLabel = "Fire";
    c.loopFreqHz = 200;
    c.allowTriphase = true;
    c.audioMode = AudioMode::AudioIntensity;

    MenuItem a;
    a.type = MenuItemType::MinMax;
    a.id = 1; a.title = "Speed"; a.min = 1; a.max = 10; a.incrementStep = 1;
    a.uom = "x"; a.defaultValue = 5;
    c.menuItems.push_back(a);

    MenuItem b;
    b.type = MenuItemType::MultiChoice;
    b.id = 2; b.title = "Mode";
    b.choices = { {1, "A"}, {2, "B"} };
    c.menuItems.push_back(b);

    MenuItem v;
    v.type = MenuItemType::AudioViewIntensityMono;
    v.id = 3; v.title = "Audio";
    c.menuItems.push_back(v);

    QString lua = LuaGenerator::generate(c);
    auto r = LuaParser::parse(lua);
    QVERIFY(r.ok);
    assertSameConfig(c, r.config);
}

void TestRoundtrip::allFunctionsRoundtrip() {
    ScriptConfig c;
    c.name = "AllFns";
    c.functions.setup = true;
    c.functions.minMaxChange = true;
    c.functions.multiChoiceChange = true;
    c.functions.softButton = true;
    c.functions.externalTrigger = true;
    c.functions.bluetoothRemoteKeypress = true;
    c.functions.bluetoothHidEvent = true;
    c.functions.audioIntensityChange = true;
    c.audioMode = AudioMode::AudioIntensity;
    c.bluetoothRemotePassthrough = true;
    c.softButtonLabel = "Go";

    QString lua = LuaGenerator::generate(c);
    auto r = LuaParser::parse(lua);
    QVERIFY(r.ok);
    assertSameConfig(c, r.config);
}

void TestRoundtrip::escapedStringsRoundtrip() {
    ScriptConfig c;
    c.name = "Has \"quotes\" and \\ backslash";
    MenuItem mi;
    mi.type = MenuItemType::MinMax;
    mi.id = 1; mi.title = "Title with 'apostrophe'"; mi.min = 0; mi.max = 100;
    mi.incrementStep = 1; mi.uom = ""; mi.defaultValue = 0;
    c.menuItems.push_back(mi);

    QString lua = LuaGenerator::generate(c);
    auto r = LuaParser::parse(lua);
    QVERIFY(r.ok);
    assertSameConfig(c, r.config);
}

void TestRoundtrip::parserHandlesComments() {
    QString lua = R"LUA(
-- header comment
Config = {
    name = "Commented", -- inline
    --[[ block
       comment ]]
    audio_processing_mode = "OFF"
}
function Loop(t) end
)LUA";
    auto r = LuaParser::parse(lua);
    QVERIFY(r.ok);
    QCOMPARE(r.config.name, QString("Commented"));
    QCOMPARE((int)r.config.audioMode, (int)AudioMode::Off);
}

void TestRoundtrip::linterDetectsAudioMismatch() {
    ScriptConfig c;
    c.name = "x";
    c.functions.audioIntensityChange = true;
    c.audioMode = AudioMode::Off;
    auto issues = Linter::lint(c, "function Loop(t) end");
    bool found = false;
    for (const auto& i : issues) {
        if (i.severity == IssueSeverity::Error
            && i.message.contains("AudioIntensityChange")) found = true;
    }
    QVERIFY(found);
}

void TestRoundtrip::linterDetectsTriphaseWithoutFlag() {
    ScriptConfig c;
    c.name = "x";
    c.allowTriphase = false;
    QString src = R"(
Config = { name = "x", audio_processing_mode = "OFF" }
function Loop(t)
    zc.LinkChannels(1, 2, 0)
end
)";
    auto issues = Linter::lint(c, src);
    bool found = false;
    for (const auto& i : issues) {
        if (i.message.contains("allow_triphase")) found = true;
    }
    QVERIFY(found);
}

void TestRoundtrip::linterDetectsOutOfRangePower() {
    ScriptConfig c;
    c.name = "x";
    QString src = R"(
function Loop(t)
    zc.SetPower(5, 2000)
end
)";
    auto issues = Linter::lint(c, src);
    int errors = 0;
    for (const auto& i : issues) {
        if (i.severity == IssueSeverity::Error
            && i.message.contains("SetPower")) ++errors;
    }
    QVERIFY(errors >= 2);  // channel out of range AND power out of range
}

void TestRoundtrip::variableNamingHelpers() {
    MenuItem mi;
    mi.title = "Delay (ms)";
    mi.id = 1;
    QCOMPARE(LuaGenerator::variableNameFor(mi), QString("_delay_ms"));
    mi.title = "  Multi -- Word  ";
    QCOMPARE(LuaGenerator::variableNameFor(mi), QString("_multi_word"));
    mi.title = "";
    QCOMPARE(LuaGenerator::variableNameFor(mi), QString("_menu_1"));
}

static const QStringList kOfficialScripts = {
    "climb", "combo", "intense", "orgasm", "phasing2", "random2",
    "rhythm", "stroke", "tens", "torment", "trifade", "waves"
};

void TestRoundtrip::officialScriptsParse_data() {
    QTest::addColumn<QString>("name");
    for (const QString& n : kOfficialScripts) QTest::newRow(n.toUtf8().constData()) << n;
}

void TestRoundtrip::officialScriptsParse() {
    QFETCH(QString, name);
    QFile f(":/presets/" + name + ".lua");
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable("Cannot open resource for " + name));
    QString src = QString::fromUtf8(f.readAll());
    auto r = LuaParser::parse(src);
    QVERIFY2(r.ok, qPrintable("Parse failed for " + name + ": " + r.warning));
    QVERIFY2(!r.config.name.isEmpty(),
             qPrintable("Empty config name in " + name));
}

void TestRoundtrip::officialScriptsRoundtrip_data() {
    QTest::addColumn<QString>("name");
    for (const QString& n : kOfficialScripts) QTest::newRow(n.toUtf8().constData()) << n;
}

void TestRoundtrip::officialScriptsRoundtrip() {
    QFETCH(QString, name);
    QFile f(":/presets/" + name + ".lua");
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString src = QString::fromUtf8(f.readAll());

    // Parse the original — extract just the Config block via the model.
    auto r1 = LuaParser::parse(src);
    QVERIFY2(r1.ok, qPrintable("Initial parse failed for " + name));

    // Regenerate Lua from the model only (no logic body), then parse again.
    QString regen = LuaGenerator::generate(r1.config);
    auto r2 = LuaParser::parse(regen);
    QVERIFY2(r2.ok, qPrintable("Reparse of regenerated script failed for " + name));

    // The Config block should round-trip 1:1.
    assertSameConfig(r1.config, r2.config);
}

void TestRoundtrip::mergePreservesFunctionBodies() {
    QString original = R"LUA(
Config = {
    name = "Old",
    audio_processing_mode = "OFF"
}

function Loop(time_ms)
    -- USER LOGIC: do not lose this
    zc.ChannelOn(1)
    if time_ms > 1000 then
        zc.SetPower(1, 800)
    end
end
)LUA";

    ScriptConfig c;
    c.name = "New";
    c.functions.loop = true;

    QString merged = LuaGenerator::mergeIntoSource(c, original);
    // New name is now in the file.
    QVERIFY(merged.contains("name = \"New\""));
    QVERIFY(!merged.contains("name = \"Old\""));
    // The user's function body is preserved verbatim.
    QVERIFY(merged.contains("-- USER LOGIC: do not lose this"));
    QVERIFY(merged.contains("zc.SetPower(1, 800)"));
    // No duplicate Loop function.
    int firstLoop = merged.indexOf("function Loop");
    int secondLoop = merged.indexOf("function Loop", firstLoop + 1);
    QCOMPARE(secondLoop, -1);
}

void TestRoundtrip::mergeReplacesConfigBlock() {
    QString original = R"LUA(
Config = {
    name = "Old",
    audio_processing_mode = "OFF",
    menu_items = {
        {
            type = "MIN_MAX",
            title = "OldOnly",
            id = 1,
            min = 0, max = 10, increment_step = 1, uom = "", default = 5
        }
    }
}

function Loop(t) end
)LUA";

    ScriptConfig c;
    c.name = "Renamed";
    c.functions.loop = true;
    MenuItem mi;
    mi.type = MenuItemType::MultiChoice;
    mi.id = 7;
    mi.title = "Mode";
    mi.choices = { {1, "A"}, {2, "B"} };
    c.menuItems.push_back(mi);

    QString merged = LuaGenerator::mergeIntoSource(c, original);
    QVERIFY(merged.contains("name = \"Renamed\""));
    // Old menu item is gone, replaced by the new one.
    QVERIFY(!merged.contains("OldOnly"));
    QVERIFY(merged.contains("title = \"Mode\""));
    QVERIFY(merged.contains("description = \"A\""));
}

void TestRoundtrip::mergeAppendsNewFunctionStubs() {
    QString original = R"LUA(
Config = { name = "x", audio_processing_mode = "OFF" }
function Loop(t)
    -- existing
end
)LUA";

    ScriptConfig c;
    c.name = "x";
    c.functions.loop = true;
    c.functions.softButton = true;     // new — should be appended
    c.functions.externalTrigger = true;// new — should be appended

    QString merged = LuaGenerator::mergeIntoSource(c, original);
    QVERIFY(merged.contains("-- existing"));            // Loop body kept
    QVERIFY(merged.contains("function SoftButton"));    // appended
    QVERIFY(merged.contains("function ExternalTrigger"));
    // Loop is not duplicated.
    int firstLoop = merged.indexOf("function Loop");
    int secondLoop = merged.indexOf("function Loop", firstLoop + 1);
    QCOMPARE(secondLoop, -1);
}

void TestRoundtrip::mergeFallsBackWhenNoConfigBlock() {
    QString original = "-- just a comment, no Config\nprint(1)\n";
    ScriptConfig c;
    c.name = "Fresh";
    QString merged = LuaGenerator::mergeIntoSource(c, original);
    // Falls back to full generation.
    QVERIFY(merged.contains("Config = {"));
    QVERIFY(merged.contains("name = \"Fresh\""));
}

void TestRoundtrip::mergeOnOfficialScriptKeepsLogic() {
    QFile f(":/presets/intense.lua");
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString src = QString::fromUtf8(f.readAll());

    auto r = LuaParser::parse(src);
    QVERIFY(r.ok);

    // Re-merge with the same config — only Config block should change shape;
    // the toggle logic in Loop must remain.
    QString merged = LuaGenerator::mergeIntoSource(r.config, src);
    QVERIFY(merged.contains("_chan3_on = false"));
    QVERIFY(merged.contains("zc.ChannelOff(3)"));
    QVERIFY(merged.contains("zc.ChannelOn(4)"));
    // The merged source still parses cleanly.
    auto r2 = LuaParser::parse(merged);
    QVERIFY(r2.ok);
    assertSameConfig(r.config, r2.config);
}

// =====================================================================
// Simulator end-to-end smoke tests
//
// These run each of the 12 official scripts through the embedded
// LuaRuntime and check it doesn't blow up. Each script is exercised in
// isolation so a regression in one never masks failures in another.
// =====================================================================

void TestRoundtrip::officialScriptsLoadInSimulator_data() {
    QTest::addColumn<QString>("name");
    for (const QString& n : officialScriptList()) QTest::newRow(n.toUtf8()) << n;
}

void TestRoundtrip::officialScriptsLoadInSimulator() {
    QFETCH(QString, name);
    QString src = loadResourceText(":/presets/" + name + ".lua");
    QVERIFY2(!src.isEmpty(), qPrintable("missing resource: " + name));

    LuaRuntime rt;
    QString err;
    QVERIFY2(rt.loadScript(src, &err),
             qPrintable(QString("script %1 failed to load: %2").arg(name, err)));
}

void TestRoundtrip::officialScriptsSetupRuns_data() {
    QTest::addColumn<QString>("name");
    for (const QString& n : officialScriptList()) QTest::newRow(n.toUtf8()) << n;
}

void TestRoundtrip::officialScriptsSetupRuns() {
    QFETCH(QString, name);
    QString src = loadResourceText(":/presets/" + name + ".lua");
    LuaRuntime rt;
    QVERIFY(rt.loadScript(src));
    QString err;
    QVERIFY2(rt.callSetup(&err),
             qPrintable(QString("Setup() of %1 failed: %2").arg(name, err)));
}

void TestRoundtrip::officialScriptsLoopRuns_data() {
    QTest::addColumn<QString>("name");
    for (const QString& n : officialScriptList()) QTest::newRow(n.toUtf8()) << n;
}

void TestRoundtrip::officialScriptsLoopRuns() {
    QFETCH(QString, name);
    QString src = loadResourceText(":/presets/" + name + ".lua");
    LuaRuntime rt;
    QVERIFY(rt.loadScript(src));
    QString err;
    QVERIFY(rt.callSetup(&err));

    // Run 100 ticks of Loop at 20ms intervals = 2 seconds simulated.
    const int kTicks = 100;
    const double kStepMs = 20.0;
    for (int i = 1; i <= kTicks; ++i) {
        bool ok = rt.callLoop(i * kStepMs, &err);
        if (!ok) {
            QFAIL(qPrintable(QString("Loop() of %1 crashed at tick %2: %3")
                                 .arg(name).arg(i).arg(err)));
        }
        rt.updatePulses();
    }
}

void TestRoundtrip::officialScriptsResolveConfig_data() {
    QTest::addColumn<QString>("name");
    for (const QString& n : officialScriptList()) QTest::newRow(n.toUtf8()) << n;
}

void TestRoundtrip::officialScriptsResolveConfig() {
    QFETCH(QString, name);
    QString src = loadResourceText(":/presets/" + name + ".lua");
    LuaRuntime rt;
    QVERIFY(rt.loadScript(src));

    ScriptConfig resolved;
    QString warn;
    QVERIFY2(rt.extractScriptConfig(resolved, &warn),
             qPrintable(QString("could not extract Config from %1: %2").arg(name, warn)));

    // Every official script defines a non-empty name and at least one menu item.
    QVERIFY(!resolved.name.isEmpty());
    QVERIFY(!resolved.menuItems.isEmpty());

    // Every menu item should have a unique non-zero ID once resolved
    // through Lua (regression test for the regex parser bug that left
    // every id at 0 because they were expressed as MenuId.X).
    QSet<int> ids;
    for (const auto& mi : resolved.menuItems) {
        QVERIFY2(mi.id > 0,
                 qPrintable(QString("%1: menu_item \"%2\" has id 0").arg(name, mi.title)));
        QVERIFY2(!ids.contains(mi.id),
                 qPrintable(QString("%1: duplicate menu_item id %2").arg(name).arg(mi.id)));
        ids.insert(mi.id);
    }
}

void TestRoundtrip::officialScriptsExerciseCallbacks_data() {
    QTest::addColumn<QString>("name");
    for (const QString& n : officialScriptList()) QTest::newRow(n.toUtf8()) << n;
}

void TestRoundtrip::officialScriptsExerciseCallbacks() {
    QFETCH(QString, name);
    QString src = loadResourceText(":/presets/" + name + ".lua");
    LuaRuntime rt;
    QVERIFY(rt.loadScript(src));
    QVERIFY(rt.callSetup());

    ScriptConfig cfg;
    QVERIFY(rt.extractScriptConfig(cfg));

    QString err;
    // For every MIN_MAX item, drive min / default / max.
    // For every MULTI_CHOICE item, drive every choice_id.
    // After each, run a few Loop ticks and ensure the script still works.
    double t = 0;
    auto runTicks = [&](int n) {
        for (int i = 0; i < n; ++i) {
            t += 20.0;
            bool ok = rt.callLoop(t, &err);
            if (!ok) {
                QFAIL(qPrintable(QString("%1: Loop crashed during exercise: %2")
                                     .arg(name, err)));
            }
            rt.updatePulses();
        }
    };

    for (const auto& mi : cfg.menuItems) {
        if (mi.type == MenuItemType::MinMax) {
            for (int v : { mi.min, mi.defaultValue, mi.max }) {
                bool ok = rt.callMinMaxChange(mi.id, v, &err);
                if (!ok) {
                    QFAIL(qPrintable(QString("%1: MinMaxChange(%2,%3) crashed: %4")
                                         .arg(name).arg(mi.id).arg(v).arg(err)));
                }
                runTicks(3);
            }
        } else if (mi.type == MenuItemType::MultiChoice) {
            for (const auto& c : mi.choices) {
                bool ok = rt.callMultiChoiceChange(mi.id, c.choiceId, &err);
                if (!ok) {
                    QFAIL(qPrintable(QString("%1: MultiChoiceChange(%2,%3) crashed: %4")
                                         .arg(name).arg(mi.id).arg(c.choiceId).arg(err)));
                }
                runTicks(3);
            }
        }
    }

    // Soft button + external trigger if defined.
    rt.callSoftButton(true, &err);   runTicks(1);
    rt.callSoftButton(false, &err);  runTicks(1);
    rt.callExternalTrigger("TRIGGER1", "A", true, &err);  runTicks(1);
    rt.callExternalTrigger("TRIGGER1", "A", false, &err); runTicks(1);
}

QTEST_MAIN(TestRoundtrip)
#include "test_roundtrip.moc"
