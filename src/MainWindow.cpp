#include "MainWindow.h"

#include "ui/ConfigPanel.h"
#include "ui/MenuItemsPanel.h"
#include "ui/FunctionsPanel.h"
#include "ui/SnippetsPanel.h"
#include "ui/LuaEditor.h"
#include "ui/FindReplaceBar.h"
#include "ui/IssuesPanel.h"
#include "ui/ApiDocPanel.h"
#include "ui/LcdPreviewPanel.h"
#include "ui/SimulatorPanel.h"
#include "ui/WizardDialog.h"
#include "codegen/LuaGenerator.h"
#include "codegen/LuaParser.h"
#include "codegen/Linter.h"
#include "codegen/Explainer.h"
#include "codegen/LineDiff.h"
#include "sim/LuaRuntime.h"

#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QLabel>
#include <QVBoxLayout>
#include <QTextCursor>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QCoreApplication>
#include <QDateTime>
#include <QRegularExpression>
#include <QDialog>
#include <QTextBrowser>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCheckBox>

MainWindow::MainWindow() {
    setWindowTitle("ZC95 Lua Builder  ⚠ EXPERIMENTAL — UNTESTED ON HARDWARE");
    resize(1500, 900);
    setAcceptDrops(true);

    buildUi();
    buildMenus();
    connectSignals();
    loadSettings();

    // Set up the autosave file paths (one per process — keeps several
    // simultaneous instances from clobbering each other's drafts).
    {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/zc95-lua-builder";
        QDir().mkpath(dir);
        qint64 pid = QCoreApplication::applicationPid();
        m_autosavePath     = QString("%1/autosave-%2.lua").arg(dir).arg(pid);
        m_autosaveMetaPath = QString("%1/autosave-%2.meta").arg(dir).arg(pid);
    }

    // Look for any leftover autosave from a previous (crashed?) run before
    // we overwrite our own slot.
    offerAutosaveRecovery();

    if (m_currentFile.isEmpty()) {
        newScript();
    }

    // Kick off the autosave timer (30 s).
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setInterval(30 * 1000);
    connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::writeAutosave);
    m_autosaveTimer->start();

    statusBar()->showMessage("Ready");
}

void MainWindow::buildUi() {
    m_horizSplitter = new QSplitter(Qt::Horizontal, this);

    // ----- Left side: form tabs -----
    m_leftTabs = new QTabWidget(this);
    m_configPanel = new ConfigPanel(this);
    m_menuItemsPanel = new MenuItemsPanel(this);
    m_functionsPanel = new FunctionsPanel(this);
    m_snippetsPanel = new SnippetsPanel(this);
    m_apiDocPanel = new ApiDocPanel(this);
    m_lcdPreview = new LcdPreviewPanel(this);
    m_leftTabs->addTab(m_configPanel, "Config");
    m_leftTabs->addTab(m_menuItemsPanel, "Menu Items");
    m_leftTabs->addTab(m_functionsPanel, "Functions");
    m_leftTabs->addTab(m_snippetsPanel, "Snippets");
    m_leftTabs->addTab(m_apiDocPanel, "API Help");
    m_leftTabs->addTab(m_lcdPreview, "LCD Preview");

    // ----- Center: editor + find bar in a vertical container -----
    auto* editorContainer = new QWidget(this);
    auto* editorLayout = new QVBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    m_editor = new LuaEditor(editorContainer);
    m_findBar = new FindReplaceBar(m_editor, editorContainer);
    m_findBar->hide();
    editorLayout->addWidget(m_findBar);
    editorLayout->addWidget(m_editor, 1);

    // ----- Right side: editor on top, simulator tab next to editor -----
    m_rightTabs = new QTabWidget(this);
    m_simPanel = new SimulatorPanel(this);
    m_rightTabs->addTab(editorContainer, "Editor");
    m_rightTabs->addTab(m_simPanel, "Simulator");

    m_horizSplitter->addWidget(m_leftTabs);
    m_horizSplitter->addWidget(m_rightTabs);
    m_horizSplitter->setStretchFactor(0, 0);
    m_horizSplitter->setStretchFactor(1, 1);
    m_horizSplitter->setSizes({440, 1060});

    setCentralWidget(m_horizSplitter);

    // ----- Issues dock at the bottom -----
    m_issuesPanel = new IssuesPanel(this);
    m_issuesDock = new QDockWidget("Issues", this);
    m_issuesDock->setObjectName("IssuesDock");
    m_issuesDock->setWidget(m_issuesPanel);
    m_issuesDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_issuesDock);

    // ----- Status bar widgets -----
    m_beginnerBadge = new QLabel("", this);
    m_beginnerBadge->setStyleSheet(
        "padding: 2px 8px; border-radius: 3px; background:#3b4d68; color:#a8c5e8;");
    m_beginnerBadge->setText("● beginner mode");
    m_beginnerBadge->setToolTip(
        "Beginner mode is on. Toggle in View → Beginner mode.");
    m_beginnerBadge->hide();   // shown only when beginner mode is on
    statusBar()->addPermanentWidget(m_beginnerBadge);

    m_syncBadge = new QLabel("", this);
    m_syncBadge->setStyleSheet("padding: 2px 8px; border-radius: 3px;");
    statusBar()->addPermanentWidget(m_syncBadge);
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    auto* newAct = fileMenu->addAction("&New", this, &MainWindow::newScript, QKeySequence::New);
    auto* wizardAct = fileMenu->addAction("New from &wizard…", this,
        &MainWindow::newFromWizard, QKeySequence("Ctrl+Shift+N"));
    wizardAct->setStatusTip("Walk through 5 questions and generate a starting pattern.");
    auto* openAct = fileMenu->addAction("&Open…", this, &MainWindow::openScript, QKeySequence::Open);
    auto* saveAct = fileMenu->addAction("&Save", this, [this]() { saveScript(); }, QKeySequence::Save);
    auto* saveAsAct = fileMenu->addAction("Save &As…", this, [this]() { saveScriptAs(); }, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu("&Recent Files");
    rebuildRecentFilesMenu();
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction("&Find…", this, [this]() { m_findBar->showFind(); }, QKeySequence::Find);
    editMenu->addAction("&Replace…", this, [this]() { m_findBar->showReplace(); }, QKeySequence::Replace);
    editMenu->addAction("Find &Next", this, [this]() { m_findBar->findNext(); }, QKeySequence::FindNext);
    editMenu->addAction("Find Pre&vious", this, [this]() { m_findBar->findPrev(); }, QKeySequence::FindPrevious);

    auto* generateMenu = menuBar()->addMenu("&Generate");
    auto* regenAct = generateMenu->addAction("&Regenerate code from form (smart merge)", this,
        &MainWindow::regenerate, QKeySequence("Ctrl+G"));
    regenAct->setStatusTip("Replace just the Config block; keep your function bodies.");
    generateMenu->addAction("Regenerate code from &scratch (overwrite)", this,
        &MainWindow::regenerateFromScratch, QKeySequence("Ctrl+Shift+Alt+G"));
    auto* parseAct = generateMenu->addAction("Re&parse form from editor", this,
        &MainWindow::parseEditorIntoForm, QKeySequence("Ctrl+Shift+G"));
    generateMenu->addSeparator();
    generateMenu->addAction("&Lint", this, &MainWindow::runLinter, QKeySequence("Ctrl+L"));
    auto* preflightAct = generateMenu->addAction("&Pre-flight check", this,
        &MainWindow::runPreflight, QKeySequence("Ctrl+Shift+P"));
    preflightAct->setStatusTip(
        "Run the linter AND a 1-second simulator dry-run to score the script.");

    auto* simMenu = menuBar()->addMenu("&Simulator");
    simMenu->addAction("&Load editor into simulator", this,
        &MainWindow::loadCurrentEditorIntoSim, QKeySequence("Ctrl+R"));

    auto* presetMenu = menuBar()->addMenu("&Presets");
    auto* quickToggle = presetMenu->addAction("Toggle", this, [this]() { loadPreset("toggle"); });
    auto* quickFire = presetMenu->addAction("Fire", this, [this]() { loadPreset("fire"); });
    auto* quickWaves = presetMenu->addAction("Waves (skeleton)", this, [this]() { loadPreset("waves"); });
    auto* quickAudio = presetMenu->addAction("Audio", this, [this]() { loadPreset("audio"); });
    auto* quickSep = presetMenu->addSeparator();
    // We hide the quick presets in beginner mode (they're skeletons —
    // less polished than the official scripts).
    m_quickPresetActions = { quickToggle, quickFire, quickWaves, quickAudio, quickSep };
    auto* officialMenu = presetMenu->addMenu("Official scripts");
    const QStringList officials = {
        "climb", "combo", "intense", "orgasm", "phasing2", "random2",
        "rhythm", "stroke", "tens", "torment", "trifade", "waves"
    };
    for (const QString& name : officials) {
        QString label = name.left(1).toUpper() + name.mid(1);
        QString resPath = ":/presets/" + name + ".lua";
        officialMenu->addAction(label, this, [this, resPath]() {
            loadOfficialScript(resPath);
        });
    }

    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_issuesDock->toggleViewAction());
    viewMenu->addSeparator();
    m_beginnerAction = viewMenu->addAction("&Beginner mode");
    m_beginnerAction->setCheckable(true);
    m_beginnerAction->setStatusTip(
        "Hide advanced options (triphase, BT HID, audio, …). Recommended "
        "if you're new to ZC95 Lua scripting.");
    connect(m_beginnerAction, &QAction::toggled, this, &MainWindow::setBeginnerMode);

    auto* showDiffAct = viewMenu->addAction("Show &diff after regenerate");
    showDiffAct->setCheckable(true);
    {
        QSettings s("zc95", "lua-builder");
        showDiffAct->setChecked(s.value("showRegenDiff", true).toBool());
    }
    connect(showDiffAct, &QAction::toggled, this, [](bool on) {
        QSettings s("zc95", "lua-builder");
        s.setValue("showRegenDiff", on);
    });

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&Explain this script…", this, &MainWindow::explainScript,
                        QKeySequence("Ctrl+Shift+E"));
    helpMenu->addSeparator();
    helpMenu->addAction("&About", this, &MainWindow::about);

    auto* tb = addToolBar("Main");
    tb->setObjectName("MainToolBar");
    tb->addAction(newAct);
    tb->addAction(openAct);
    tb->addAction(saveAct);
    tb->addSeparator();
    tb->addAction(regenAct);
    tb->addAction(parseAct);
    tb->addSeparator();
    tb->addAction(preflightAct);
}

void MainWindow::connectSignals() {
    connect(m_snippetsPanel, &SnippetsPanel::snippetRequested,
            m_editor, &LuaEditor::insertSnippet);
    connect(m_apiDocPanel, &ApiDocPanel::insertRequested,
            m_editor, &LuaEditor::insertSnippet);

    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_suppressDirty) return;
        setDirty(true);
        updateSyncIndicator();
    });

    auto formChanged = [this]() {
        if (m_suppressDirty) return;
        setDirty(true);
        updateSyncIndicator();
        // Live-refresh the LCD preview AND the simulator's live controls
        // as the user edits the form.
        ScriptConfig snap = collectConfig();
        m_lcdPreview->setItems(snap.menuItems, snap.name, snap.softButtonLabel);
        if (m_simPanel) m_simPanel->setMenuItems(snap.menuItems);
        statusBar()->showMessage("Form changed — press Ctrl+G to regenerate code", 4000);
    };
    connect(m_configPanel, &ConfigPanel::changed, this, formChanged);
    connect(m_menuItemsPanel, &MenuItemsPanel::changed, this, formChanged);
    connect(m_functionsPanel, &FunctionsPanel::changed, this, formChanged);

    connect(m_menuItemsPanel, &MenuItemsPanel::testItemRequested,
            this, &MainWindow::testMenuItem);

    connect(m_issuesPanel, &IssuesPanel::jumpToLine, this, [this](int line) {
        if (line <= 0) return;
        QTextCursor c = m_editor->textCursor();
        c.movePosition(QTextCursor::Start);
        c.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
        m_editor->setTextCursor(c);
        m_editor->setFocus();
        m_rightTabs->setCurrentIndex(0);  // bring editor tab forward
    });

    connect(m_simPanel, &SimulatorPanel::log, this, [this](const QString& msg) {
        statusBar()->showMessage(msg.left(120), 4000);
    });

    // If the user presses Run/Step in the simulator without ever calling
    // "Load editor into simulator", silently feed it the current editor.
    connect(m_simPanel, &SimulatorPanel::needsScript, this, [this]() {
        m_simPanel->loadSourceSilent(m_editor->toPlainText());
    });

    // Mirror the simulator's live sliders / combos onto the LCD preview.
    connect(m_simPanel, &SimulatorPanel::liveMenuValueChanged, this,
            [this](int menuId, int value) {
                if (m_lcdPreview) m_lcdPreview->setLiveValue(menuId, value);
            });

    // When switching to the Simulator tab, refresh the runtime from the
    // editor if the user has never loaded anything yet. This is cheap and
    // avoids the "no script loaded" trap on first run.
    connect(m_rightTabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (m_rightTabs->widget(idx) == m_simPanel && !m_simPanel->isLoaded()) {
            m_simPanel->loadSourceSilent(m_editor->toPlainText());
        }
    });
}

ScriptConfig MainWindow::collectConfig() const {
    ScriptConfig c;
    m_configPanel->save(c);
    c.menuItems = m_menuItemsPanel->items();
    m_functionsPanel->save(c.functions);
    return c;
}

void MainWindow::applyConfigToForm(const ScriptConfig& c) {
    m_suppressDirty = true;
    m_configPanel->load(c);
    m_menuItemsPanel->load(c.menuItems);
    m_functionsPanel->load(c.functions);
    if (m_lcdPreview) m_lcdPreview->clearLiveValues();
    m_lcdPreview->setItems(c.menuItems, c.name, c.softButtonLabel);
    if (m_simPanel) m_simPanel->setMenuItems(c.menuItems);
    m_suppressDirty = false;
}

void MainWindow::setDirty(bool dirty) {
    m_dirty = dirty;
    QString base = m_currentFile.isEmpty() ? "untitled" : QFileInfo(m_currentFile).fileName();
    setWindowTitle(QString("ZC95 Lua Builder  ⚠ EXPERIMENTAL — %1%2")
                       .arg(base, dirty ? " *" : ""));
}

void MainWindow::setCurrentFile(const QString& path) {
    m_currentFile = path;
    setDirty(false);
    if (!path.isEmpty()) addToRecentFiles(path);
    updateSyncIndicator();
}

bool MainWindow::maybeSave() {
    if (!m_dirty) return true;
    auto ret = QMessageBox::warning(this, "Unsaved changes",
        "The script has unsaved changes. Save before continuing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save) return saveScript();
    if (ret == QMessageBox::Cancel) return false;
    return true;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (maybeSave()) {
        saveSettings();
        cleanupAutosave();   // graceful exit — drop our draft
        e->accept();
    } else {
        e->ignore();
    }
}

void MainWindow::writeAutosave() {
    // Only autosave if there's something to save AND it's actually dirty
    // (no point writing identical bytes every 30 s otherwise).
    if (!m_dirty) return;
    if (m_autosavePath.isEmpty()) return;
    QString src = m_editor ? m_editor->toPlainText() : QString();
    if (src.trimmed().isEmpty()) return;

    QFile f(m_autosavePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out << src;
    f.close();

    // Companion .meta — original path + timestamp.
    QFile meta(m_autosaveMetaPath);
    if (meta.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream mout(&meta);
        mout << "originalPath=" << m_currentFile << "\n";
        mout << "savedAt=" << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    }
}

void MainWindow::offerAutosaveRecovery() {
    // Scan the autosave dir for leftover files NOT belonging to our PID.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                  + "/zc95-lua-builder";
    QDir d(dir);
    if (!d.exists()) return;
    qint64 myPid = QCoreApplication::applicationPid();
    QStringList autosaves = d.entryList(QStringList() << "autosave-*.lua",
                                        QDir::Files, QDir::Time);
    for (const QString& fn : autosaves) {
        // Extract PID from filename
        QRegularExpression re(R"(autosave-(\d+)\.lua)");
        auto m = re.match(fn);
        if (!m.hasMatch()) continue;
        qint64 pid = m.captured(1).toLongLong();
        if (pid == myPid) continue;
        // If a process with that PID is still alive, skip — it's another
        // running instance, not a crash leftover. (Cheap test: try opening
        // for write; if locked, another process may hold it. But Windows
        // rarely locks files by other PIDs. Skip the check; worst case the
        // user gets a recover dialog they can ignore.)
        QString path = d.absoluteFilePath(fn);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream in(&f);
        QString src = in.readAll();
        f.close();
        if (src.trimmed().isEmpty()) {
            d.remove(fn);
            continue;
        }

        // Read meta if available
        QString origPath, savedAt;
        QString metaPath = path;
        metaPath.replace(".lua", ".meta");
        QFile meta(metaPath);
        if (meta.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream min(&meta);
            while (!min.atEnd()) {
                QString line = min.readLine();
                if (line.startsWith("originalPath=")) origPath = line.mid(13);
                else if (line.startsWith("savedAt=")) savedAt = line.mid(8);
            }
            meta.close();
        }

        QString msg = "An unsaved draft was found from a previous session.\n\n";
        if (!origPath.isEmpty()) msg += "Original file: " + origPath + "\n";
        if (!savedAt.isEmpty())  msg += "Last saved:    " + savedAt + "\n";
        msg += "\nRecover the draft into the editor?";

        auto ans = QMessageBox::question(this, "Recover unsaved draft", msg,
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Discard,
            QMessageBox::Yes);

        if (ans == QMessageBox::Yes) {
            m_suppressDirty = true;
            if (m_editor) m_editor->setPlainText(src);
            m_suppressDirty = false;
            // Apply the parsed Config to the form so the LCD preview etc. are correct.
            auto parsed = LuaParser::parse(src);
            if (parsed.ok) applyConfigToForm(parsed.config);
            setCurrentFile(origPath);   // may be empty (was "untitled")
            setDirty(true);             // it's still unsaved relative to disk
            updateSyncIndicator();
            if (m_simPanel) m_simPanel->loadSourceSilent(src);
            statusBar()->showMessage("Recovered draft from previous session.", 5000);
            // Don't delete it yet — let writeAutosave overwrite it once the
            // user does something. Or let the user save explicitly.
            return;  // Only recover the most recent one.
        } else {
            // Discard or No — drop this autosave so we don't keep asking.
            d.remove(fn);
            QFile::remove(metaPath);
            if (ans == QMessageBox::Discard) return;
        }
    }
}

void MainWindow::fixupFormFromSimulator() {
    if (!m_simPanel || !m_simPanel->isLoaded()) return;
    ScriptConfig resolved;
    QString warn;
    if (!m_simPanel->resolveScriptConfig(resolved, &warn)) return;
    if (resolved.menuItems.isEmpty()) return;

    // Pull the form's current items, merge in the Lua-resolved IDs and
    // ranges WITHOUT clobbering anything else. We match by position
    // (index) — the menu_items array order is preserved through the
    // regex parser, so item N from the regex matches item N from Lua.
    QVector<MenuItem> formItems = m_menuItemsPanel->items();
    if (formItems.size() != resolved.menuItems.size()) {
        // Item count mismatch — trust Lua entirely.
        formItems = resolved.menuItems;
    } else {
        for (int i = 0; i < formItems.size(); ++i) {
            const auto& src = resolved.menuItems[i];
            // Always overwrite the IDs and choice IDs (those are what
            // the regex parser gets wrong). Keep the form's other
            // fields if they were correctly parsed.
            formItems[i].id = src.id;
            formItems[i].group = src.group;
            // Type & ranges — the form might be wrong if defaults
            // referenced variables. Trust Lua.
            formItems[i].type          = src.type;
            formItems[i].min           = src.min;
            formItems[i].max           = src.max;
            formItems[i].incrementStep = src.incrementStep;
            formItems[i].uom           = src.uom;
            formItems[i].defaultValue  = src.defaultValue;
            // For MULTI_CHOICE: replace choices entirely.
            if (src.type == MenuItemType::MultiChoice) {
                formItems[i].choices = src.choices;
            }
            // Keep the form's title (it's regex-readable from string literal).
            if (src.title.isEmpty() == false && formItems[i].title.isEmpty()) {
                formItems[i].title = src.title;
            }
        }
    }

    bool prev = m_suppressDirty;
    m_suppressDirty = true;
    m_menuItemsPanel->load(formItems);
    if (m_lcdPreview) {
        m_lcdPreview->clearLiveValues();
        ScriptConfig snap = collectConfig();
        m_lcdPreview->setItems(snap.menuItems, snap.name, snap.softButtonLabel);
    }
    if (m_simPanel) m_simPanel->setMenuItems(formItems);
    m_suppressDirty = prev;
}

void MainWindow::cleanupAutosave() {
    if (!m_autosavePath.isEmpty()) QFile::remove(m_autosavePath);
    if (!m_autosaveMetaPath.isEmpty()) QFile::remove(m_autosaveMetaPath);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) {
        for (const QUrl& u : e->mimeData()->urls()) {
            if (u.toLocalFile().endsWith(".lua", Qt::CaseInsensitive)) {
                e->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* e) {
    if (!e->mimeData()->hasUrls()) return;
    for (const QUrl& u : e->mimeData()->urls()) {
        QString path = u.toLocalFile();
        if (path.endsWith(".lua", Qt::CaseInsensitive)) {
            if (!maybeSave()) return;
            loadFile(path);
            return;
        }
    }
}

void MainWindow::newScript() {
    if (!maybeSave()) return;
    ScriptConfig fresh;
    applyConfigToForm(fresh);
    m_suppressDirty = true;
    m_editor->setPlainText(LuaGenerator::generate(fresh));
    m_suppressDirty = false;
    setCurrentFile(QString());
    if (m_simPanel) m_simPanel->loadSourceSilent(m_editor->toPlainText());
}

void MainWindow::newFromWizard() {
    if (!maybeSave()) return;
    WizardDialog wiz(this);
    if (wiz.exec() != QDialog::Accepted) return;

    ScriptConfig cfg = wiz.config();
    QString src = wiz.generatedSource();

    applyConfigToForm(cfg);
    m_suppressDirty = true;
    m_editor->setPlainText(src);
    m_suppressDirty = false;
    setCurrentFile(QString());
    setDirty(true);
    updateSyncIndicator();
    if (m_simPanel) m_simPanel->loadSourceSilent(src);
    statusBar()->showMessage(
        "Wizard generated a script — read the comments, run Pre-flight, and try it in the simulator.",
        8000);
}

void MainWindow::regenerate() {
    ScriptConfig cfg = collectConfig();
    QString existing = m_editor->toPlainText();
    QString code = LuaGenerator::mergeIntoSource(cfg, existing);

    QTextCursor c = m_editor->textCursor();
    int pos = c.position();
    m_suppressDirty = true;
    m_editor->setPlainText(code);
    // Try to keep cursor near where it was.
    QTextCursor nc = m_editor->textCursor();
    nc.setPosition(qMin(pos, code.length()));
    m_editor->setTextCursor(nc);
    m_suppressDirty = false;
    setDirty(true);
    updateSyncIndicator();

    // Offer a diff if the user wants to see what changed (controlled by
    // a QSettings flag — first call shows it; "don't show again" silences
    // it forever).
    QSettings s("zc95", "lua-builder");
    bool showDiff = s.value("showRegenDiff", true).toBool();
    if (showDiff && existing.trimmed() != code.trimmed()) {
        auto hunks = LineDiff::diff(existing, code);
        bool hasChanges = std::any_of(hunks.begin(), hunks.end(),
            [](const LineDiff::Hunk& h) { return h.op != LineDiff::Op::Same; });
        if (hasChanges) {
            QDialog dlg(this);
            dlg.setWindowTitle("Regenerated — what changed?");
            dlg.resize(900, 600);
            auto* outer = new QVBoxLayout(&dlg);
            auto* hint = new QLabel(
                "<b>Smart-merge result.</b> Red lines were removed, green lines were "
                "added. Function bodies you wrote are preserved verbatim.",
                &dlg);
            hint->setWordWrap(true);
            hint->setStyleSheet("padding: 6px; background:#2d2d30; color:#ddd;");
            outer->addWidget(hint);
            auto* browser = new QTextBrowser(&dlg);
            browser->setHtml(LineDiff::toHtml(hunks));
            outer->addWidget(browser, 1);
            auto* row = new QHBoxLayout();
            auto* dontShow = new QCheckBox("Don't show this dialog again", &dlg);
            row->addWidget(dontShow);
            row->addStretch();
            auto* close = new QPushButton("Close", &dlg);
            row->addWidget(close);
            outer->addLayout(row);
            connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
            dlg.exec();
            if (dontShow->isChecked()) s.setValue("showRegenDiff", false);
        }
    }

    statusBar()->showMessage("Regenerated Config block (function bodies preserved)", 3000);
}

void MainWindow::regenerateFromScratch() {
    auto reply = QMessageBox::warning(this, "Regenerate from scratch",
        "This will overwrite the editor with a freshly generated script — all "
        "function bodies you've written will be lost.\n\nContinue?",
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (reply != QMessageBox::Yes) return;
    QString code = LuaGenerator::generate(collectConfig());
    m_suppressDirty = true;
    m_editor->setPlainText(code);
    m_suppressDirty = false;
    setDirty(true);
    updateSyncIndicator();
    statusBar()->showMessage("Regenerated code from scratch (overwritten)", 2000);
}

void MainWindow::parseEditorIntoForm() {
    auto result = LuaParser::parse(m_editor->toPlainText());
    if (!result.ok) {
        QMessageBox::warning(this, "Parse error",
            "Could not parse Config block:\n" + result.warning);
        return;
    }
    applyConfigToForm(result.config);
    updateSyncIndicator();
    if (!result.warning.isEmpty()) {
        statusBar()->showMessage("Form repopulated (note: " + result.warning + ")", 5000);
    } else {
        statusBar()->showMessage("Form repopulated from editor", 3000);
    }
}

void MainWindow::runLinter() {
    auto issues = Linter::lint(collectConfig(), m_editor->toPlainText());
    m_issuesPanel->setIssues(issues);
    m_issuesDock->show();
    m_issuesDock->raise();
    statusBar()->showMessage(QString("Linter: %1 issue(s)").arg(issues.size()), 3000);
}

void MainWindow::runPreflight() {
    // 1) Lint
    ScriptConfig cfg = collectConfig();
    QString src = m_editor->toPlainText();
    auto issues = Linter::lint(cfg, src);
    m_issuesPanel->setIssues(issues);
    m_issuesDock->show();
    m_issuesDock->raise();

    int errors = 0, warnings = 0, infos = 0;
    for (const auto& i : issues) {
        switch (i.severity) {
            case IssueSeverity::Error: ++errors; break;
            case IssueSeverity::Warning: ++warnings; break;
            case IssueSeverity::Info: ++infos; break;
        }
    }

    // 2) Smoke-run in a throwaway Lua runtime: load + Setup + 50 Loop ticks @ 20ms.
    QString simErr;
    bool loaded = false, setupOk = false, loopOk = true;
    {
        LuaRuntime rt;
        loaded = rt.loadScript(src, &simErr);
        if (loaded) {
            setupOk = rt.callSetup(&simErr);
            if (setupOk) {
                for (int i = 1; i <= 50; ++i) {
                    if (!rt.callLoop(i * 20.0, &simErr)) {
                        loopOk = false;
                        break;
                    }
                    rt.updatePulses();
                }
            }
        }
    }

    // 3) Score and verdict.
    int score = 100;
    score -= 30 * errors;
    score -= 8  * warnings;
    if (!loaded) score -= 50;
    if (!setupOk) score -= 20;
    if (!loopOk) score -= 30;
    score = qBound(0, score, 100);

    QString verdict;
    QString color;
    if (errors > 0 || !loaded || !setupOk || !loopOk) {
        verdict = "❌  Not ready";
        color = "#ff6b6b";
    } else if (warnings > 0) {
        verdict = "⚠  Mostly OK";
        color = "#ffd166";
    } else {
        verdict = "✓  Looks good";
        color = "#6cd47a";
    }

    QString html;
    html += QString("<h2 style='color:%1; margin-top:0;'>%2 — Confidence %3%</h2>")
                .arg(color, verdict).arg(score);

    html += "<table cellpadding='4' cellspacing='0' style='font-family:Consolas;'>";
    auto row = [&](const QString& label, const QString& val, const QString& col) {
        html += QString("<tr><td>%1</td><td style='color:%2;'>%3</td></tr>")
                    .arg(label, col, val.toHtmlEscaped());
    };
    row("Lint errors",   QString::number(errors),   errors > 0 ? "#ff6b6b" : "#6cd47a");
    row("Lint warnings", QString::number(warnings), warnings > 0 ? "#ffd166" : "#6cd47a");
    row("Lint info",     QString::number(infos),    "#88ccff");
    row("Loads in Lua",  loaded ? "yes" : "no",     loaded ? "#6cd47a" : "#ff6b6b");
    row("Setup() runs",  loaded ? (setupOk ? "yes" : "FAILED") : "—",
                         setupOk ? "#6cd47a" : (loaded ? "#ff6b6b" : "#888"));
    row("Loop() x50",    setupOk ? (loopOk ? "yes" : "CRASHED") : "—",
                         loopOk && setupOk ? "#6cd47a" : (setupOk ? "#ff6b6b" : "#888"));
    html += "</table>";

    if (!loaded || !setupOk || !loopOk) {
        html += "<h3 style='color:#ff6b6b;'>Simulator error</h3>";
        html += "<pre style='background:#1e1e1e; padding:6px; color:#ddd;'>"
                + simErr.toHtmlEscaped() + "</pre>";
    }

    if (errors == 0 && warnings == 0 && loaded && setupOk && loopOk) {
        html += "<p>Nothing flagged. The script loads cleanly and runs a "
                "1-second simulation without crashing.</p>"
                "<p><b>Reminder:</b> the simulator validates <i>logic</i>, "
                "not electrical safety. Always start a real-hardware session "
                "with the front-panel dial at zero and ramp up gradually.</p>";
    } else if (errors > 0) {
        html += "<p>Fix the <b>errors</b> before flashing. Double-click an "
                "issue in the bottom panel to jump to its line.</p>";
    } else {
        html += "<p>The warnings won't prevent the script from running but "
                "should be reviewed — they typically catch comfort or "
                "safety issues.</p>";
    }

    QMessageBox box(this);
    box.setWindowTitle("Pre-flight check");
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();

    statusBar()->showMessage(
        QString("Pre-flight: %1 — %2 err / %3 warn").arg(verdict).arg(errors).arg(warnings), 5000);
}

void MainWindow::testMenuItem(int row) {
    auto items = m_menuItemsPanel->items();
    if (row < 0 || row >= items.size()) return;
    const MenuItem& mi = items[row];

    // Make sure the simulator has the latest script loaded.
    if (!m_simPanel->isLoaded()) {
        m_simPanel->loadSourceSilent(m_editor->toPlainText());
    }
    // Switch to the Simulator tab so the user sees what's happening.
    m_rightTabs->setCurrentWidget(m_simPanel);

    bool reacted = m_simPanel->testMenuItemDrive(mi);
    if (reacted) {
        statusBar()->showMessage(
            QString("Test of \"%1\" — pattern reacted ✓").arg(mi.title), 5000);
    } else {
        statusBar()->showMessage(
            QString("Test of \"%1\" — no reaction. Did you wire menu_id %2 in MinMaxChange/MultiChoiceChange?")
                .arg(mi.title).arg(mi.id), 8000);
    }
}

void MainWindow::loadCurrentEditorIntoSim() {
    if (m_simPanel->loadSource(m_editor->toPlainText())) {
        m_rightTabs->setCurrentWidget(m_simPanel);
        statusBar()->showMessage("Script loaded into simulator. Press Run.", 3000);
    } else {
        statusBar()->showMessage("Simulator: load failed (see log)", 4000);
    }
}

void MainWindow::openScript() {
    if (!maybeSave()) return;
    QString path = QFileDialog::getOpenFileName(this, "Open Lua script", QString(),
                                                "Lua scripts (*.lua)");
    if (path.isEmpty()) return;
    loadFile(path);
}

bool MainWindow::loadFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Open", "Could not open file: " + f.errorString());
        return false;
    }
    QTextStream in(&f);
    QString source = in.readAll();

    m_suppressDirty = true;
    m_editor->setPlainText(source);
    m_suppressDirty = false;

    auto result = LuaParser::parse(source);
    if (result.ok) {
        applyConfigToForm(result.config);
        if (!result.warning.isEmpty()) {
            statusBar()->showMessage("Opened (note: " + result.warning + ")", 5000);
        } else {
            statusBar()->showMessage("Opened " + path, 3000);
        }
    } else {
        QMessageBox::warning(this, "Open",
            "Loaded file but could not parse Config block:\n" + result.warning);
    }
    setCurrentFile(path);
    if (m_simPanel && m_simPanel->loadSourceSilent(source)) {
        fixupFormFromSimulator();
    }
    return true;
}

bool MainWindow::saveScript() {
    if (m_currentFile.isEmpty()) return saveScriptAs();
    QFile f(m_currentFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save", "Could not save: " + f.errorString());
        return false;
    }
    QTextStream out(&f);
    out << m_editor->toPlainText();
    setCurrentFile(m_currentFile);
    statusBar()->showMessage("Saved " + m_currentFile, 3000);
    return true;
}

bool MainWindow::saveScriptAs() {
    QString path = QFileDialog::getSaveFileName(this, "Save Lua script",
                                                m_currentFile.isEmpty() ? "pattern.lua" : m_currentFile,
                                                "Lua scripts (*.lua)");
    if (path.isEmpty()) return false;
    m_currentFile = path;
    return saveScript();
}

void MainWindow::explainScript() {
    QString src = m_editor ? m_editor->toPlainText() : QString();
    if (src.trimmed().isEmpty()) {
        QMessageBox::information(this, "Explain",
            "The editor is empty — nothing to explain. Load a preset or "
            "generate a script first.");
        return;
    }
    auto lines = Explainer::explain(src);
    QString html = Explainer::toHtml(lines);

    QDialog dlg(this);
    dlg.setWindowTitle("Explain this script");
    dlg.resize(1100, 700);
    auto* outer = new QVBoxLayout(&dlg);
    auto* hint = new QLabel(
        "<b>Plain-English explanation</b> — line by line. The right column is a best-effort, "
        "naive translation of common idioms. Lines that don't match any pattern are left blank.",
        &dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet("padding: 6px; background:#2d2d30; color:#ddd;");
    outer->addWidget(hint);

    auto* browser = new QTextBrowser(&dlg);
    browser->setOpenExternalLinks(false);
    browser->setHtml(html);
    outer->addWidget(browser, 1);

    auto* btn = new QPushButton("Close", &dlg);
    connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    auto* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(btn);
    outer->addLayout(row);

    dlg.exec();
}

void MainWindow::about() {
    QMessageBox::about(this, "About",
        "<h3>ZC95 Lua Builder</h3>"
        "<p>Visual editor for ZC95 Lua pattern scripts.</p>"
        "<ul>"
        "<li><b>Ctrl+G</b> — regenerate code from form</li>"
        "<li><b>Ctrl+Shift+G</b> — reparse form from editor</li>"
        "<li><b>Ctrl+L</b> — run linter</li>"
        "<li><b>Ctrl+R</b> — load script into simulator</li>"
        "<li><b>Ctrl+F</b> / <b>Ctrl+H</b> — find / replace</li>"
        "<li><b>Ctrl+Space</b> — autocomplete in editor</li>"
        "</ul>");
}

void MainWindow::openRecentFile() {
    auto* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    QString path = act->data().toString();
    if (path.isEmpty()) return;
    if (!maybeSave()) return;
    if (!QFile::exists(path)) {
        QMessageBox::information(this, "Recent file",
            "File no longer exists; removing from recent list.\n" + path);
        m_recentFiles.removeAll(path);
        rebuildRecentFilesMenu();
        return;
    }
    loadFile(path);
}

void MainWindow::clearRecentFiles() {
    m_recentFiles.clear();
    rebuildRecentFilesMenu();
}

void MainWindow::addToRecentFiles(const QString& path) {
    QString abs = QFileInfo(path).absoluteFilePath();
    m_recentFiles.removeAll(abs);
    m_recentFiles.prepend(abs);
    while (m_recentFiles.size() > kMaxRecentFiles) m_recentFiles.removeLast();
    rebuildRecentFilesMenu();
}

void MainWindow::rebuildRecentFilesMenu() {
    if (!m_recentMenu) return;
    m_recentMenu->clear();
    if (m_recentFiles.isEmpty()) {
        auto* empty = m_recentMenu->addAction("(none)");
        empty->setEnabled(false);
        return;
    }
    int n = 1;
    for (const QString& path : m_recentFiles) {
        QString label = QString("&%1  %2").arg(n++).arg(QFileInfo(path).fileName());
        auto* act = m_recentMenu->addAction(label, this, &MainWindow::openRecentFile);
        act->setData(path);
        act->setToolTip(path);
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction("&Clear list", this, &MainWindow::clearRecentFiles);
}

void MainWindow::updateSyncIndicator() {
    if (!m_syncBadge) return;
    QString src = m_editor->toPlainText();
    QString merged = LuaGenerator::mergeIntoSource(collectConfig(), src);
    bool inSync = (merged.trimmed() == src.trimmed());
    if (inSync) {
        m_syncBadge->setText("● in sync");
        m_syncBadge->setStyleSheet(
            "padding: 2px 8px; border-radius: 3px; background:#234c2a; color:#9ee2a0;");
    } else {
        m_syncBadge->setText("● form/code differ");
        m_syncBadge->setStyleSheet(
            "padding: 2px 8px; border-radius: 3px; background:#5a3a1f; color:#f0c891;");
    }
}

void MainWindow::loadSettings() {
    QSettings s("zc95", "lua-builder");
    QByteArray geom = s.value("mainwindow/geometry").toByteArray();
    QByteArray state = s.value("mainwindow/state").toByteArray();
    if (!geom.isEmpty()) restoreGeometry(geom);
    if (!state.isEmpty()) restoreState(state);
    QByteArray sp = s.value("mainwindow/splitter").toByteArray();
    if (!sp.isEmpty()) m_horizSplitter->restoreState(sp);
    m_recentFiles = s.value("recentFiles").toStringList();
    rebuildRecentFilesMenu();

    // Beginner mode — default OFF for first-time users; persisted afterwards.
    bool beginner = s.value("beginnerMode", false).toBool();
    if (m_beginnerAction) {
        m_beginnerAction->blockSignals(true);
        m_beginnerAction->setChecked(beginner);
        m_beginnerAction->blockSignals(false);
    }
    setBeginnerMode(beginner);
}

void MainWindow::saveSettings() {
    QSettings s("zc95", "lua-builder");
    s.setValue("mainwindow/geometry", saveGeometry());
    s.setValue("mainwindow/state", saveState());
    s.setValue("mainwindow/splitter", m_horizSplitter->saveState());
    s.setValue("recentFiles", m_recentFiles);
    s.setValue("beginnerMode", m_beginnerMode);
}

void MainWindow::setBeginnerMode(bool beginner) {
    m_beginnerMode = beginner;
    if (m_configPanel) m_configPanel->setBeginnerMode(beginner);
    if (m_functionsPanel) m_functionsPanel->setBeginnerMode(beginner);
    if (m_beginnerBadge) m_beginnerBadge->setVisible(beginner);
    for (QAction* a : m_quickPresetActions) {
        if (a) a->setVisible(!beginner);
    }
    if (beginner) {
        statusBar()->showMessage(
            "Beginner mode ON — advanced controls hidden. Toggle in View → Beginner mode.", 5000);
    } else {
        statusBar()->showMessage("Beginner mode OFF — all controls visible.", 3000);
    }
}

void MainWindow::loadOfficialScript(const QString& resourcePath) {
    if (!maybeSave()) return;
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Preset",
            "Could not load preset: " + resourcePath);
        return;
    }
    QTextStream in(&f);
    QString source = in.readAll();

    m_suppressDirty = true;
    m_editor->setPlainText(source);
    m_suppressDirty = false;

    auto result = LuaParser::parse(source);
    if (result.ok) {
        applyConfigToForm(result.config);
    }
    setCurrentFile(QString());
    setDirty(true);
    updateSyncIndicator();
    if (m_simPanel && m_simPanel->loadSourceSilent(source)) {
        fixupFormFromSimulator();
    }
    statusBar()->showMessage("Loaded official script: " + resourcePath, 3000);
}

void MainWindow::loadPreset(const QString& presetName) {
    if (!maybeSave()) return;

    ScriptConfig c;
    c.functions.loop = true;

    if (presetName == "toggle") {
        c.name = "Toggle";
        c.functions.minMaxChange = true;
        c.functions.multiChoiceChange = true;
        MenuItem mm;
        mm.type = MenuItemType::MinMax;
        mm.id = 1;
        mm.title = "Delay";
        mm.min = 100; mm.max = 2000; mm.incrementStep = 100;
        mm.uom = "ms"; mm.defaultValue = 500;
        c.menuItems.push_back(mm);
        MenuItem mc;
        mc.type = MenuItemType::MultiChoice;
        mc.id = 2;
        mc.title = "Output";
        mc.choices = { {1, "Pulse"}, {2, "Constant"} };
        c.menuItems.push_back(mc);
    } else if (presetName == "fire") {
        c.name = "Fire";
        c.softButtonLabel = "Fire";
        c.functions.softButton = true;
        c.functions.externalTrigger = true;
    } else if (presetName == "waves") {
        c.name = "Waves";
        c.functions.setup = true;
        c.functions.minMaxChange = true;
        for (int i = 1; i <= 4; ++i) {
            MenuItem mm;
            mm.type = MenuItemType::MinMax;
            mm.id = i;
            mm.title = QString("Cycle ch%1").arg(i);
            mm.min = 1; mm.max = 30; mm.incrementStep = 1;
            mm.uom = "s"; mm.defaultValue = 5;
            c.menuItems.push_back(mm);
        }
    } else if (presetName == "audio") {
        c.name = "Audio";
        c.audioMode = AudioMode::AudioIntensity;
        c.functions.setup = true;
        c.functions.audioIntensityChange = true;
        MenuItem view;
        view.type = MenuItemType::AudioViewIntensityMono;
        view.id = 1;
        view.title = "Audio";
        c.menuItems.push_back(view);
    }

    applyConfigToForm(c);
    m_suppressDirty = true;
    m_editor->setPlainText(LuaGenerator::generate(c));
    m_suppressDirty = false;
    setCurrentFile(QString());
    setDirty(true);
    updateSyncIndicator();
    if (m_simPanel && m_simPanel->loadSourceSilent(m_editor->toPlainText())) {
        fixupFormFromSimulator();
    }
    statusBar()->showMessage("Loaded preset: " + presetName, 3000);
}
