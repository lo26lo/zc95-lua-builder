#pragma once

#include "model/ScriptConfig.h"
#include <QMainWindow>
#include <QStringList>
#include <QList>

class ConfigPanel;
class MenuItemsPanel;
class FunctionsPanel;
class SnippetsPanel;
class LuaEditor;
class FindReplaceBar;
class IssuesPanel;
class ApiDocPanel;
class LcdPreviewPanel;
class SimulatorPanel;
class QSplitter;
class QTabWidget;
class QLabel;
class QMenu;
class QDockWidget;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void newScript();
    void newFromWizard();
    void openScript();
    bool saveScript();
    bool saveScriptAs();
    void regenerate();           // smart merge — preserves function bodies
    void regenerateFromScratch();// full overwrite (legacy behavior)
    void parseEditorIntoForm();
    void runLinter();
    void runPreflight();
    void explainScript();
    void loadCurrentEditorIntoSim();
    void testMenuItem(int row);
    void loadPreset(const QString& presetName);
    void loadOfficialScript(const QString& resourcePath);
    void about();
    void openRecentFile();
    void clearRecentFiles();
    void setBeginnerMode(bool beginner);
    void writeAutosave();
    void offerAutosaveRecovery();
    // After loading a script into the simulator, refresh the form's menu
    // item IDs from Lua's resolved Config (the regex parser can't evaluate
    // identifiers like MenuId.FREQ, so this fills in the right integers).
    void fixupFormFromSimulator();

private:
    void buildUi();
    void buildMenus();
    void connectSignals();
    void loadSettings();
    void saveSettings();

    ScriptConfig collectConfig() const;
    void applyConfigToForm(const ScriptConfig& c);

    bool maybeSave();
    void setDirty(bool dirty);
    void setCurrentFile(const QString& path);

    bool loadFile(const QString& path);

    // Recent files
    void addToRecentFiles(const QString& path);
    void rebuildRecentFilesMenu();

    // Sync indicator
    void updateSyncIndicator();

    ConfigPanel* m_configPanel = nullptr;
    MenuItemsPanel* m_menuItemsPanel = nullptr;
    FunctionsPanel* m_functionsPanel = nullptr;
    SnippetsPanel* m_snippetsPanel = nullptr;
    ApiDocPanel* m_apiDocPanel = nullptr;
    LcdPreviewPanel* m_lcdPreview = nullptr;
    SimulatorPanel* m_simPanel = nullptr;
    LuaEditor* m_editor = nullptr;
    FindReplaceBar* m_findBar = nullptr;
    IssuesPanel* m_issuesPanel = nullptr;

    QSplitter* m_horizSplitter = nullptr;
    QTabWidget* m_leftTabs = nullptr;
    QTabWidget* m_rightTabs = nullptr;
    QDockWidget* m_issuesDock = nullptr;
    QLabel* m_syncBadge = nullptr;

    QMenu* m_recentMenu = nullptr;
    QStringList m_recentFiles;
    static constexpr int kMaxRecentFiles = 8;

    QString m_currentFile;
    bool m_dirty = false;
    bool m_suppressDirty = false;
    bool m_beginnerMode = false;
    QAction* m_beginnerAction = nullptr;
    QLabel* m_beginnerBadge = nullptr;
    QList<QAction*> m_quickPresetActions;  // hidden in beginner mode

    // Autosave / draft recovery.
    class QTimer* m_autosaveTimer = nullptr;
    QString m_autosavePath;       // <tempdir>/zc95-lua-builder/autosave-<pid>.lua
    QString m_autosaveMetaPath;   // companion .meta file
    void cleanupAutosave();
};
