#pragma once

#include "../codegen/TimelineProject.h"
#include "../model/MenuItem.h"
#include "TimelineEditorWidget.h"
#include <QWidget>
#include <QVector>

class QCheckBox;
class QSpinBox;
class QPushButton;
class QToolButton;
class QButtonGroup;
class QComboBox;
class QLabel;
class QGroupBox;

// Wrapper around TimelineEditorWidget. Stacks (top → bottom):
//   1. Beta banner (yellow, fixed)
//   2. Project toolbar (Loop / Cycle ms / ← Lua / → Lua / Clear all)
//   3. Edit toolbar    (tool palette / snap / cursor time / undo-redo /
//                       zoom −/+/Fit + indicator)
//   4. Splitter: canvas | property panel for the selected event.
class TimelineEditorPanel : public QWidget {
    Q_OBJECT
public:
    explicit TimelineEditorPanel(QWidget* parent = nullptr);

    const TimelineProject& project() const;
    void setProject(const TimelineProject& p);

    // Provide the list of MIN_MAX variable names the user can reference
    // from event params (e.g. "_intensity", "_speed").
    void setAvailableVariables(const QStringList& names);

signals:
    void pushToLuaRequested();
    void pullFromLuaRequested();
    void projectChanged();

private slots:
    void onSelectionChanged(int idx);
    void onProjectChanged();
    void rebuildPropertyPanel();
    void onLoopToggled(bool b);
    void onCycleChanged(int v);
    void onClearAll();

    void onToolButtonClicked(int id);
    void onCanvasToolChanged(TimelineEditorWidget::Tool t);
    void onSnapChanged(int idx);
    void onCursorTimeChanged(double tMs);
    void onZoomChanged();
    void onUndoStateChanged();

private:
    QWidget* buildParamEditor(const TimelineParam& current,
                              std::function<void(const TimelineParam&)> apply,
                              const QString& label);

    TimelineEditorWidget* m_canvas = nullptr;

    // Project toolbar.
    QCheckBox*   m_loopCheck = nullptr;
    QSpinBox*    m_cycleSpin = nullptr;
    QPushButton* m_pushBtn   = nullptr;
    QPushButton* m_pullBtn   = nullptr;
    QPushButton* m_clearBtn  = nullptr;

    // Edit toolbar.
    QToolButton*  m_btnSelect = nullptr;
    QToolButton*  m_btnPulse  = nullptr;
    QToolButton*  m_btnOn     = nullptr;
    QToolButton*  m_btnOff    = nullptr;
    QButtonGroup* m_toolGroup = nullptr;
    QComboBox*    m_snapCombo = nullptr;
    QLabel*       m_timeLabel = nullptr;
    QPushButton*  m_undoBtn   = nullptr;
    QPushButton*  m_redoBtn   = nullptr;
    QPushButton*  m_zoomOut   = nullptr;
    QPushButton*  m_zoomIn    = nullptr;
    QPushButton*  m_zoomFit   = nullptr;
    QLabel*       m_zoomLabel = nullptr;

    QGroupBox*  m_propsBox = nullptr;
    QStringList m_availableVars;
};
