#pragma once

#include "../codegen/TimelineProject.h"
#include "../sim/ChannelEvent.h"
#include <QWidget>
#include <QStack>

class QPaintEvent;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class QContextMenuEvent;

// Interactive 4-lane timeline editor.
//
// Tool modes (set via setTool, exposed by the wrapping panel):
//   • Select  — click to select / drag to move existing events. Click on
//               an empty lane = deselect.
//   • Pulse   — drag on empty lane = create a Pulse with that duration.
//   • On      — single click on empty lane = create a ChannelOn at t.
//   • Off     — single click on empty lane = create a ChannelOff at t.
//
// Common interactions (all tools):
//   • Click on existing event = select.
//   • Drag selected event body = move (cross-channel allowed).
//   • Drag right edge of a Pulse = resize duration.
//   • Right-click on event = convert / delete menu.
//   • Wheel = zoom around cursor (Ctrl+Wheel = pan).
//   • Delete / Backspace = remove selected.
//   • ← → = nudge selected by one snap step (Ctrl+← → for 10×).
//   • Ctrl+D = duplicate selected.
//   • Ctrl+Z / Ctrl+Y = undo / redo.
//   • Esc = deselect.
//
// Snap: events are quantised to `snapMs()` (default 100 ms). Hold
// Shift while dragging or nudging to bypass the snap.
class TimelineEditorWidget : public QWidget {
    Q_OBJECT
public:
    enum Tool { ToolSelect, ToolPulse, ToolOn, ToolOff };

    explicit TimelineEditorWidget(QWidget* parent = nullptr);

    const TimelineProject& project() const { return m_project; }
    // Programmatic load — resets undo history and selection.
    void setProject(const TimelineProject& p);
    // Replace the project but KEEP undo / redo / selection / view state.
    // Use after pushUndoSnapshot() for changes that must be undoable.
    void applyProjectKeepHistory(const TimelineProject& p);

    int  selectedIndex() const { return m_selected; }
    void setSelectedIndex(int idx);

    Tool tool() const { return m_tool; }
    void setTool(Tool t);

    double snapMs() const { return m_snapMs; }
    void   setSnapMs(double ms);

    // Ratio cycleMs / viewSpanMs (1.0 = full cycle visible, 2.0 = 2× zoom).
    double zoomLevel() const;
    void zoomIn();
    void zoomOut();
    void fitToCycle();

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    void undo();
    void redo();

    // Push the current m_project onto the undo stack. External callers
    // (e.g. the property panel) MUST call this *before* mutating the
    // project so the change can be rolled back.
    void pushUndoSnapshot();

    void deleteSelected();
    void duplicateSelected();

    // Read-only "ghost" overlay rendered behind editable events. Used to
    // visualise what a dynamic script (tens.lua, climb.lua, …) actually
    // does on the channels — those scripts compute schedules at runtime
    // so they have no JSON sentinel to read. The overlay is a snapshot
    // captured by MainWindow running the script in a sandboxed runtime;
    // it does not refresh when the user moves a slider.
    void setCapturedEvents(const QVector<ChannelEvent>& events);
    void clearCapturedEvents();
    bool hasCapturedEvents() const { return !m_capturedEvents.isEmpty(); }

signals:
    // Project mutated (added / moved / resized / deleted / converted /
    // loop / cycle changed). Wrapper marks the document dirty.
    void projectChanged();
    // Selection changed (so the property panel can re-render).
    void selectionChanged(int newIndex);
    // Tool changed (canvas → palette buttons).
    void toolChanged(Tool t);
    // Undo / redo stack mutated (canvas → undo/redo buttons).
    void undoStateChanged();
    // View span changed (canvas → zoom indicator).
    void zoomChanged();
    // Pointer hover time, in ms within the cycle. -1 when leaving.
    void cursorTimeChanged(double tMs);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    int    laneTop(int channel) const;
    int    laneHeight() const;
    int    chartLeft() const;
    int    chartRight() const;
    int    chartTop() const;
    int    chartBottom() const;
    double xToTime(int x) const;
    int    timeToX(double tMs) const;
    int    yToChannel(int y) const;

    enum HitZone { HitNone, HitBody, HitRightEdge };
    HitZone hitTest(const QPoint& pos, int* outIndex) const;

    QRect eventRect(const TimelineEvent& e) const;

    void emitChanged();
    void updateHoverCursor(int laneAtPointer);

    // Snap a time value. Bypassed when Shift is held in `mods`.
    double snapTime(double t, Qt::KeyboardModifiers mods) const;

    void drawEmptyHint(QPainter& p);
    void drawEventLabel(QPainter& p, const TimelineEvent& e, const QRect& r);
    void drawCapturedOverlay(QPainter& p);

    TimelineProject m_project;
    int             m_selected = -1;

    Tool   m_tool   = ToolPulse;
    double m_snapMs = 100.0;

    // View state.
    double m_viewStartMs = 0;
    double m_viewSpanMs  = 10000;

    // Drag state.
    enum DragMode { DragNone, DragCreate, DragMove, DragResize };
    DragMode m_drag = DragNone;
    QPoint   m_dragStart;
    int      m_dragChannel = 0;
    double   m_dragOriginalStart = 0;
    double   m_dragOriginalDuration = 0;
    int      m_hoverLane = -1;
    // For undo: snapshot taken before a drag begins; pushed on release
    // only if the project actually mutated.
    TimelineProject m_dragBaseline;
    bool            m_dragChangedProject = false;

    // Undo / redo (stack of full project snapshots).
    QStack<TimelineProject> m_undoStack;
    QStack<TimelineProject> m_redoStack;
    static constexpr int kMaxUndoDepth = 100;

    // Read-only ghost overlay (see setCapturedEvents).
    QVector<ChannelEvent> m_capturedEvents;
};
