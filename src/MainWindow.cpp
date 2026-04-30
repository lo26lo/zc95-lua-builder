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
#include "codegen/LuaGenerator.h"
#include "codegen/LuaParser.h"
#include "codegen/Linter.h"

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

MainWindow::MainWindow() {
    setWindowTitle("ZC95 Lua Builder");
    resize(1500, 900);
    setAcceptDrops(true);

    buildUi();
    buildMenus();
    connectSignals();
    loadSettings();

    if (m_currentFile.isEmpty()) {
        newScript();
    }
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
    m_syncBadge = new QLabel("", this);
    m_syncBadge->setStyleSheet("padding: 2px 8px; border-radius: 3px;");
    statusBar()->addPermanentWidget(m_syncBadge);
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    auto* newAct = fileMenu->addAction("&New", this, &MainWindow::newScript, QKeySequence::New);
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

    auto* simMenu = menuBar()->addMenu("&Simulator");
    simMenu->addAction("&Load editor into simulator", this,
        &MainWindow::loadCurrentEditorIntoSim, QKeySequence("Ctrl+R"));

    auto* presetMenu = menuBar()->addMenu("&Presets");
    presetMenu->addAction("Toggle", this, [this]() { loadPreset("toggle"); });
    presetMenu->addAction("Fire", this, [this]() { loadPreset("fire"); });
    presetMenu->addAction("Waves (skeleton)", this, [this]() { loadPreset("waves"); });
    presetMenu->addAction("Audio", this, [this]() { loadPreset("audio"); });
    presetMenu->addSeparator();
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

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, &MainWindow::about);

    auto* tb = addToolBar("Main");
    tb->setObjectName("MainToolBar");
    tb->addAction(newAct);
    tb->addAction(openAct);
    tb->addAction(saveAct);
    tb->addSeparator();
    tb->addAction(regenAct);
    tb->addAction(parseAct);
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
        // Live-refresh the LCD preview as the user edits the form.
        ScriptConfig snap = collectConfig();
        m_lcdPreview->setItems(snap.menuItems, snap.name, snap.softButtonLabel);
        statusBar()->showMessage("Form changed — press Ctrl+G to regenerate code", 4000);
    };
    connect(m_configPanel, &ConfigPanel::changed, this, formChanged);
    connect(m_menuItemsPanel, &MenuItemsPanel::changed, this, formChanged);
    connect(m_functionsPanel, &FunctionsPanel::changed, this, formChanged);

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
    m_lcdPreview->setItems(c.menuItems, c.name, c.softButtonLabel);
    m_suppressDirty = false;
}

void MainWindow::setDirty(bool dirty) {
    m_dirty = dirty;
    QString base = m_currentFile.isEmpty() ? "untitled" : QFileInfo(m_currentFile).fileName();
    setWindowTitle(QString("ZC95 Lua Builder — %1%2").arg(base, dirty ? " *" : ""));
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
        e->accept();
    } else {
        e->ignore();
    }
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
    if (m_simPanel) m_simPanel->loadSourceSilent(source);
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
}

void MainWindow::saveSettings() {
    QSettings s("zc95", "lua-builder");
    s.setValue("mainwindow/geometry", saveGeometry());
    s.setValue("mainwindow/state", saveState());
    s.setValue("mainwindow/splitter", m_horizSplitter->saveState());
    s.setValue("recentFiles", m_recentFiles);
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
    if (m_simPanel) m_simPanel->loadSourceSilent(source);
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
    if (m_simPanel) m_simPanel->loadSourceSilent(m_editor->toPlainText());
    statusBar()->showMessage("Loaded preset: " + presetName, 3000);
}
