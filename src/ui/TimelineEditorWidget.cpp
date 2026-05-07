#include "TimelineEditorWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kLeftMargin   = 40;
constexpr int kRightMargin  = 8;
constexpr int kTopMargin    = 24;
constexpr int kBottomMargin = 22;
constexpr int kLaneCount    = 4;
constexpr int kEdgeGrabPx   = 6;
constexpr double kMinPulseMs = 5;
constexpr double kMinSpanMs  = 50.0;
constexpr double kMaxSpanMs  = 600000.0;
constexpr double kZoomStep   = 0.65;       // < 1 = zoom in
}

TimelineEditorWidget::TimelineEditorWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(600, 220);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_viewSpanMs = m_project.cycleMs;
}

void TimelineEditorWidget::setProject(const TimelineProject& p) {
    m_project = p;
    m_selected = -1;
    m_viewStartMs = 0;
    m_viewSpanMs = qMax(100.0, p.cycleMs);
    // Programmatic load → reset history.
    m_undoStack.clear();
    m_redoStack.clear();
    emit selectionChanged(-1);
    emit undoStateChanged();
    emit zoomChanged();
    update();
}

void TimelineEditorWidget::setCapturedEvents(const QVector<ChannelEvent>& events) {
    m_capturedEvents = events;
    update();
}

void TimelineEditorWidget::clearCapturedEvents() {
    if (m_capturedEvents.isEmpty()) return;
    m_capturedEvents.clear();
    update();
}

void TimelineEditorWidget::applyProjectKeepHistory(const TimelineProject& p) {
    m_project = p;
    if (m_selected >= m_project.events.size()) {
        m_selected = -1;
        emit selectionChanged(-1);
    }
    emit projectChanged();
    update();
}

void TimelineEditorWidget::setSelectedIndex(int idx) {
    if (idx < 0 || idx >= m_project.events.size()) idx = -1;
    if (m_selected == idx) return;
    m_selected = idx;
    emit selectionChanged(idx);
    update();
}

void TimelineEditorWidget::setTool(Tool t) {
    if (m_tool == t) return;
    m_tool = t;
    emit toolChanged(t);
    updateHoverCursor(m_hoverLane);
    update();
}

void TimelineEditorWidget::setSnapMs(double ms) {
    if (qFuzzyCompare(m_snapMs, ms)) return;
    m_snapMs = qMax(0.0, ms);
    update();
}

double TimelineEditorWidget::zoomLevel() const {
    if (m_viewSpanMs <= 0) return 1.0;
    return m_project.cycleMs / m_viewSpanMs;
}

void TimelineEditorWidget::zoomIn() {
    double pivotT = m_viewStartMs + m_viewSpanMs / 2.0;
    double newSpan = qBound(kMinSpanMs, m_viewSpanMs * kZoomStep, kMaxSpanMs);
    m_viewStartMs = pivotT - newSpan / 2.0;
    m_viewSpanMs  = newSpan;
    m_viewStartMs = qMax(0.0, m_viewStartMs);
    emit zoomChanged();
    update();
}

void TimelineEditorWidget::zoomOut() {
    double pivotT = m_viewStartMs + m_viewSpanMs / 2.0;
    double newSpan = qBound(kMinSpanMs, m_viewSpanMs / kZoomStep, kMaxSpanMs);
    m_viewStartMs = pivotT - newSpan / 2.0;
    m_viewSpanMs  = newSpan;
    m_viewStartMs = qMax(0.0, m_viewStartMs);
    emit zoomChanged();
    update();
}

void TimelineEditorWidget::fitToCycle() {
    m_viewStartMs = 0;
    m_viewSpanMs  = qMax(kMinSpanMs, m_project.cycleMs);
    emit zoomChanged();
    update();
}

void TimelineEditorWidget::pushUndoSnapshot() {
    m_undoStack.push(m_project);
    while (m_undoStack.size() > kMaxUndoDepth) m_undoStack.removeFirst();
    m_redoStack.clear();
    emit undoStateChanged();
}

void TimelineEditorWidget::undo() {
    if (m_undoStack.isEmpty()) return;
    m_redoStack.push(m_project);
    m_project = m_undoStack.pop();
    if (m_selected >= m_project.events.size()) {
        m_selected = -1;
        emit selectionChanged(-1);
    }
    emit projectChanged();
    emit undoStateChanged();
    update();
}

void TimelineEditorWidget::redo() {
    if (m_redoStack.isEmpty()) return;
    m_undoStack.push(m_project);
    m_project = m_redoStack.pop();
    if (m_selected >= m_project.events.size()) {
        m_selected = -1;
        emit selectionChanged(-1);
    }
    emit projectChanged();
    emit undoStateChanged();
    update();
}

void TimelineEditorWidget::deleteSelected() {
    if (m_selected < 0 || m_selected >= m_project.events.size()) return;
    pushUndoSnapshot();
    m_project.events.remove(m_selected);
    m_selected = -1;
    emit selectionChanged(-1);
    emitChanged();
}

void TimelineEditorWidget::duplicateSelected() {
    if (m_selected < 0 || m_selected >= m_project.events.size()) return;
    pushUndoSnapshot();
    TimelineEvent copy = m_project.events[m_selected];
    // Offset the duplicate by one snap step (or 100 ms if snap is off)
    // so it doesn't sit exactly on top of the original.
    double offset = (m_snapMs > 0) ? m_snapMs : 100.0;
    copy.startMs = qMax(0.0, copy.startMs + offset);
    m_project.events.append(copy);
    m_selected = m_project.events.size() - 1;
    emit selectionChanged(m_selected);
    emitChanged();
}

void TimelineEditorWidget::emitChanged() {
    emit projectChanged();
    update();
}

double TimelineEditorWidget::snapTime(double t, Qt::KeyboardModifiers mods) const {
    if (m_snapMs <= 0) return qMax(0.0, t);
    if (mods & Qt::ShiftModifier) return qMax(0.0, t);
    return qMax(0.0, std::round(t / m_snapMs) * m_snapMs);
}

int TimelineEditorWidget::laneHeight() const {
    return (height() - kTopMargin - kBottomMargin) / kLaneCount;
}
int TimelineEditorWidget::laneTop(int ch) const {
    return kTopMargin + (ch - 1) * laneHeight();
}
int TimelineEditorWidget::chartLeft() const   { return kLeftMargin; }
int TimelineEditorWidget::chartRight() const  { return width() - kRightMargin; }
int TimelineEditorWidget::chartTop() const    { return kTopMargin; }
int TimelineEditorWidget::chartBottom() const { return height() - kBottomMargin; }

double TimelineEditorWidget::xToTime(int x) const {
    int chartW = chartRight() - chartLeft();
    if (chartW <= 0) return 0;
    double rel = double(x - chartLeft()) / chartW;
    return m_viewStartMs + rel * m_viewSpanMs;
}
int TimelineEditorWidget::timeToX(double tMs) const {
    int chartW = chartRight() - chartLeft();
    double rel = (tMs - m_viewStartMs) / qMax(1.0, m_viewSpanMs);
    return chartLeft() + int(rel * chartW);
}

int TimelineEditorWidget::yToChannel(int y) const {
    if (y < kTopMargin || y >= height() - kBottomMargin) return -1;
    int lh = laneHeight();
    if (lh <= 0) return -1;
    int ch = (y - kTopMargin) / lh + 1;
    return (ch >= 1 && ch <= kLaneCount) ? ch : -1;
}

QRect TimelineEditorWidget::eventRect(const TimelineEvent& e) const {
    int x1 = timeToX(e.startMs);
    int x2 = timeToX(e.startMs + (e.type == TimelineEvent::Pulse ? e.durationMs : qMax(50.0, e.durationMs)));
    int yTop = laneTop(e.channel) + 4;
    int h = laneHeight() - 8;
    return QRect(x1, yTop, qMax(2, x2 - x1), h);
}

TimelineEditorWidget::HitZone TimelineEditorWidget::hitTest(const QPoint& pos, int* outIndex) const {
    for (int i = m_project.events.size() - 1; i >= 0; --i) {
        QRect r = eventRect(m_project.events[i]);
        if (!r.contains(pos)) continue;
        if (outIndex) *outIndex = i;
        if (pos.x() >= r.right() - kEdgeGrabPx
            && m_project.events[i].type == TimelineEvent::Pulse) {
            return HitRightEdge;
        }
        return HitBody;
    }
    return HitNone;
}

void TimelineEditorWidget::updateHoverCursor(int laneAtPointer) {
    if (m_drag != DragNone) return;
    if (laneAtPointer < 0) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    switch (m_tool) {
        case ToolSelect: setCursor(Qt::ArrowCursor);      break;
        case ToolPulse:  setCursor(Qt::CrossCursor);      break;
        case ToolOn:     setCursor(Qt::PointingHandCursor); break;
        case ToolOff:    setCursor(Qt::PointingHandCursor); break;
    }
}

void TimelineEditorWidget::drawEmptyHint(QPainter& p) {
    p.save();
    p.setPen(QColor("#666"));
    QFont f("Segoe UI");
    f.setPointSize(11);
    f.setItalic(true);
    p.setFont(f);
    QString line1, line2;
    switch (m_tool) {
        case ToolSelect:
            line1 = "Select tool: pick Pulse / On / Off from the palette,";
            line2 = "then click on a lane to add an event.";
            break;
        case ToolPulse:
            line1 = "Pulse tool: click-drag on a lane (CH1–CH4) to create";
            line2 = "a pulse. Hold Shift to bypass snap.";
            break;
        case ToolOn:
            line1 = "ChannelOn tool: click on a lane to drop a channel";
            line2 = "start at that instant.";
            break;
        case ToolOff:
            line1 = "ChannelOff tool: click on a lane to drop a channel";
            line2 = "stop at that instant.";
            break;
    }
    int y = (chartTop() + chartBottom()) / 2;
    p.drawText(QRect(chartLeft(), y - 16, chartRight() - chartLeft(), 18),
               Qt::AlignHCenter, line1);
    p.drawText(QRect(chartLeft(), y + 4, chartRight() - chartLeft(), 18),
               Qt::AlignHCenter, line2);
    p.restore();
}

void TimelineEditorWidget::drawEventLabel(QPainter& p, const TimelineEvent& e, const QRect& r) {
    if (r.width() <= 28) return;
    QString lbl;
    switch (e.type) {
        case TimelineEvent::Pulse: lbl = QString("%1 ms").arg((int)e.durationMs); break;
        case TimelineEvent::On:    lbl = "ON";  break;
        case TimelineEvent::Off:   lbl = "OFF"; break;
    }
    // White text with a dark thin halo for legibility on any base colour.
    p.save();
    QFont f = p.font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(0, 0, 0, 180));
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            p.drawText(r.translated(dx, dy), Qt::AlignCenter, lbl);
        }
    }
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, lbl);
    p.restore();
}

void TimelineEditorWidget::drawCapturedOverlay(QPainter& p) {
    // Reconstruct on/off segments per channel by walking the captured
    // event stream, exactly the same way the simulator timeline does.
    // ChannelPulseMs becomes a sealed segment of param1 ms; ChannelOn /
    // ChannelOff bracket sustained segments. We use the same colour
    // language as the simulator (green = sustained, orange = pulse) but
    // dimmed and semi-transparent so the user's editable events stay
    // dominant.
    struct Seg { double start; double end; bool isPulse; };
    QVector<Seg> segs[kLaneCount];
    bool on[kLaneCount] = {false, false, false, false};
    double onSince[kLaneCount] = {0, 0, 0, 0};
    bool isPulse[kLaneCount] = {false, false, false, false};

    double maxT = 0;
    for (const auto& e : m_capturedEvents) {
        if (e.timeMs > maxT) maxT = e.timeMs;
        if (e.channel < 1 || e.channel > kLaneCount) continue;
        int idx = e.channel - 1;
        if (e.type == ChannelEventType::ChannelOn) {
            if (on[idx]) segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx]});
            on[idx] = true;
            onSince[idx] = e.timeMs;
            isPulse[idx] = false;
        } else if (e.type == ChannelEventType::ChannelOff) {
            if (on[idx]) {
                segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx]});
                on[idx] = false;
            }
        } else if (e.type == ChannelEventType::ChannelPulseMs) {
            segs[idx].push_back({e.timeMs, e.timeMs + e.param1, true});
            if (e.timeMs + e.param1 > maxT) maxT = e.timeMs + e.param1;
        }
    }
    // Close any still-on segment at maxT (the capture ends "now").
    for (int i = 0; i < kLaneCount; ++i) {
        if (on[i]) segs[i].push_back({onSince[i], maxT, isPulse[i]});
    }

    p.save();
    // Slim band centred on the lane mid-line. Editable events are taller,
    // so they visually stand on top of the ghost trace.
    const QColor sustainCol(78, 201, 110, 110);   // dim green, ~43% alpha
    const QColor pulseCol  (240, 160, 32, 150);   // dim orange, ~59% alpha
    int lh = laneHeight();
    int barH = qMax(4, lh / 4);
    for (int ch = 1; ch <= kLaneCount; ++ch) {
        int yMid = laneTop(ch) + lh / 2;
        int yTop = yMid - barH / 2;
        for (const auto& s : segs[ch - 1]) {
            int x1 = timeToX(s.start);
            int x2 = timeToX(s.end);
            if (x2 < chartLeft() || x1 > chartRight()) continue;
            x1 = qMax(x1, chartLeft());
            x2 = qMin(x2, chartRight());
            int w = qMax(2, x2 - x1);
            p.fillRect(QRect(x1, yTop, w, barH), s.isPulse ? pulseCol : sustainCol);
        }
    }
    // "Snapshot @ Xs" hint in the lower-right corner so the user knows
    // the trace isn't live.
    if (maxT > 0) {
        p.setPen(QColor(150, 150, 150, 200));
        QFont f("Segoe UI");
        f.setPointSize(8);
        f.setItalic(true);
        p.setFont(f);
        QString lbl = QString("Sim snapshot · %1 s · gray = script behaviour (read-only)")
                          .arg(maxT / 1000.0, 0, 'f', 1);
        p.drawText(QRect(chartLeft(), chartBottom() - 30, chartRight() - chartLeft(), 14),
                   Qt::AlignRight | Qt::AlignVCenter, lbl);
    }
    p.restore();
}

void TimelineEditorWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor("#1a1a1a"));

    // Header (cycle / loop / event count + tool name).
    p.setPen(QColor("#aaa"));
    p.setFont(QFont("Segoe UI", 8));
    const char* toolName = "Select";
    switch (m_tool) {
        case ToolSelect: toolName = "Select"; break;
        case ToolPulse:  toolName = "Pulse";  break;
        case ToolOn:     toolName = "ChannelOn";  break;
        case ToolOff:    toolName = "ChannelOff"; break;
    }
    QString header = QString("Cycle: %1 ms · Loop: %2 · %3 event%4 · Tool: %5 · Snap: %6")
                         .arg((int)m_project.cycleMs)
                         .arg(m_project.loop ? "ON" : "off")
                         .arg(m_project.events.size())
                         .arg(m_project.events.size() != 1 ? "s" : "")
                         .arg(toolName)
                         .arg(m_snapMs > 0 ? QString("%1 ms").arg((int)m_snapMs) : QString("off"));
    p.drawText(QRect(chartLeft(), 4, chartRight() - chartLeft(), 16),
               Qt::AlignLeft | Qt::AlignVCenter, header);

    // Lanes.
    int lh = laneHeight();
    QFont mono("Consolas");
    if (!QFontInfo(mono).fixedPitch()) mono.setFamily("Courier New");
    mono.setPointSize(9);
    mono.setBold(true);
    p.setFont(mono);
    for (int ch = 1; ch <= kLaneCount; ++ch) {
        int yTop = laneTop(ch);
        QColor laneBg = (ch % 2 == 0) ? QColor("#1f1f1f") : QColor("#1a1a1a");
        if (m_hoverLane == ch && m_drag == DragNone && m_tool != ToolSelect) {
            laneBg = QColor("#252a30");  // subtle highlight on hover
        }
        p.fillRect(QRect(chartLeft(), yTop, chartRight() - chartLeft(), lh), laneBg);
        p.setPen(QColor("#ccc"));
        p.drawText(QRect(0, yTop, kLeftMargin - 4, lh), Qt::AlignVCenter | Qt::AlignRight,
                   QString("CH%1").arg(ch));
        p.setPen(QPen(QColor("#2c2c2c"), 1, Qt::DashLine));
        p.drawLine(chartLeft(), yTop + lh / 2, chartRight(), yTop + lh / 2);
    }

    // Snap grid (light vertical ticks at every snap step, only when zoomed in enough).
    if (m_snapMs > 0) {
        double pxPerMs = double(chartRight() - chartLeft()) / qMax(1.0, m_viewSpanMs);
        if (pxPerMs * m_snapMs >= 6) {  // don't draw if < 6 px between gridlines
            p.setPen(QColor("#262626"));
            double t0 = std::floor(m_viewStartMs / m_snapMs) * m_snapMs;
            for (double t = t0; t <= m_viewStartMs + m_viewSpanMs; t += m_snapMs) {
                int x = timeToX(t);
                if (x < chartLeft() || x > chartRight()) continue;
                p.drawLine(x, chartTop(), x, chartBottom());
            }
        }
    }

    // Time axis + ticks.
    int axisY = chartBottom();
    p.setPen(QColor("#888"));
    p.setFont(QFont("Segoe UI", 8));
    p.drawLine(chartLeft(), axisY, chartRight(), axisY);
    int ticks = 10;
    for (int i = 0; i <= ticks; ++i) {
        int x = chartLeft() + i * (chartRight() - chartLeft()) / ticks;
        p.drawLine(x, axisY, x, axisY + 3);
        double t = m_viewStartMs + i * m_viewSpanMs / ticks;
        QString lbl = (t >= 1000) ? QString::number(t / 1000.0, 'f', 1) + "s"
                                  : QString::number((int)t) + "ms";
        p.drawText(QRect(x - 30, axisY + 4, 60, 14), Qt::AlignHCenter, lbl);
    }

    // End-of-cycle marker (if loop ON).
    if (m_project.loop && m_project.cycleMs >= m_viewStartMs
        && m_project.cycleMs <= m_viewStartMs + m_viewSpanMs) {
        int x = timeToX(m_project.cycleMs);
        p.setPen(QPen(QColor("#aa6666"), 1, Qt::DashLine));
        p.drawLine(x, chartTop(), x, chartBottom());
    }

    // Captured ghost overlay — drawn before editable events so the user's
    // events overlap on top. Only visible if MainWindow has pushed a
    // capture (via "Capture from Sim").
    if (!m_capturedEvents.isEmpty()) {
        drawCapturedOverlay(p);
    }

    // Empty state hint when no events yet — but skip it when we have a
    // capture overlay, otherwise the hint sits on top of meaningful data
    // and is misleading.
    if (m_project.events.isEmpty() && m_capturedEvents.isEmpty()) {
        drawEmptyHint(p);
    }

    // Events.
    for (int i = 0; i < m_project.events.size(); ++i) {
        const auto& e = m_project.events[i];
        QRect r = eventRect(e);
        if (r.right() < chartLeft() || r.left() > chartRight()) continue;
        QColor base;
        switch (e.type) {
            case TimelineEvent::Pulse: base = QColor("#f0a020"); break;
            case TimelineEvent::On:    base = QColor("#4ec96e"); break;
            case TimelineEvent::Off:   base = QColor("#cc4040"); break;
        }
        p.fillRect(r, base);
        if (i == m_selected) {
            p.setPen(QPen(QColor("#ffd400"), 2));
        } else {
            p.setPen(base.darker(160));
        }
        p.drawRect(r);
        drawEventLabel(p, e, r);
    }
}

void TimelineEditorWidget::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if (e->button() != Qt::LeftButton) return;

    int idx = -1;
    HitZone hz = hitTest(e->pos(), &idx);
    if (hz != HitNone) {
        // Clicking an existing event = select + start drag (regardless of tool).
        setSelectedIndex(idx);
        m_dragStart = e->pos();
        m_drag = (hz == HitRightEdge) ? DragResize : DragMove;
        m_dragOriginalStart    = m_project.events[idx].startMs;
        m_dragOriginalDuration = m_project.events[idx].durationMs;
        m_dragBaseline = m_project;
        m_dragChangedProject = false;
        return;
    }

    // Empty area inside a lane → behaviour depends on the active tool.
    int ch = yToChannel(e->pos().y());
    if (ch < 0) return;

    double t = snapTime(xToTime(e->pos().x()), e->modifiers());

    if (m_tool == ToolSelect) {
        // Deselect on empty click.
        setSelectedIndex(-1);
        return;
    }

    if (m_tool == ToolPulse) {
        pushUndoSnapshot();
        TimelineEvent ne;
        ne.type        = TimelineEvent::Pulse;
        ne.channel     = ch;
        ne.startMs     = qMax(0.0, t);
        ne.durationMs  = kMinPulseMs;
        m_project.events.append(ne);
        m_selected = m_project.events.size() - 1;
        emit selectionChanged(m_selected);
        m_drag = DragCreate;
        m_dragStart = e->pos();
        m_dragChannel = ch;
        m_dragBaseline = m_project;     // ignored (already pushed)
        m_dragChangedProject = true;
        emitChanged();
        return;
    }

    // ToolOn / ToolOff — single click places the event, no drag.
    pushUndoSnapshot();
    TimelineEvent ne;
    ne.type    = (m_tool == ToolOn) ? TimelineEvent::On : TimelineEvent::Off;
    ne.channel = ch;
    ne.startMs = qMax(0.0, t);
    ne.durationMs = 0;   // visual width clamped to 50 ms in eventRect
    m_project.events.append(ne);
    m_selected = m_project.events.size() - 1;
    emit selectionChanged(m_selected);
    emitChanged();
}

void TimelineEditorWidget::mouseMoveEvent(QMouseEvent* e) {
    // Always emit cursor time so the panel can display it.
    int laneNow = yToChannel(e->pos().y());
    if (laneNow != m_hoverLane) {
        m_hoverLane = laneNow;
        update();   // for hover lane highlight
    }
    if (e->pos().x() >= chartLeft() && e->pos().x() <= chartRight()) {
        emit cursorTimeChanged(qMax(0.0, xToTime(e->pos().x())));
    } else {
        emit cursorTimeChanged(-1);
    }

    if (m_drag == DragNone) {
        // Hover cursor: existing-event hits always win (move / resize).
        int idx = -1;
        HitZone hz = hitTest(e->pos(), &idx);
        if (hz == HitRightEdge)      setCursor(Qt::SizeHorCursor);
        else if (hz == HitBody)      setCursor(Qt::SizeAllCursor);
        else                         updateHoverCursor(laneNow);
        return;
    }

    if (m_selected < 0 || m_selected >= m_project.events.size()) return;
    auto& ev = m_project.events[m_selected];
    double tCur  = xToTime(e->pos().x());
    double tStart = xToTime(m_dragStart.x());
    double dt = tCur - tStart;

    if (m_drag == DragCreate) {
        double endT = qMax(ev.startMs + kMinPulseMs, snapTime(tCur, e->modifiers()));
        ev.durationMs = endT - ev.startMs;
        m_dragChangedProject = true;
    } else if (m_drag == DragMove) {
        double newStart = snapTime(m_dragOriginalStart + dt, e->modifiers());
        ev.startMs = qMax(0.0, newStart);
        int ch = yToChannel(e->pos().y());
        if (ch > 0) ev.channel = ch;
        m_dragChangedProject = true;
    } else if (m_drag == DragResize) {
        double newEnd = snapTime(ev.startMs + m_dragOriginalDuration + dt, e->modifiers());
        ev.durationMs = qMax(kMinPulseMs, newEnd - ev.startMs);
        m_dragChangedProject = true;
    }
    emitChanged();
}

void TimelineEditorWidget::mouseReleaseEvent(QMouseEvent*) {
    if (m_drag == DragNone) return;
    DragMode mode = m_drag;
    m_drag = DragNone;
    if (mode == DragMove || mode == DragResize) {
        // Push the pre-drag snapshot if the project actually changed.
        // Compare events by serialising — cheap enough for the typical
        // ~10–50 events on a timeline.
        if (m_dragChangedProject &&
            m_dragBaseline.toJsonString() != m_project.toJsonString()) {
            // Push the BASELINE so undo restores the pre-drag state.
            // We push current then swap: pushUndoSnapshot pushes current,
            // but we want baseline. Easiest: temporarily restore baseline,
            // push, then re-apply current.
            TimelineProject after = m_project;
            m_project = m_dragBaseline;
            pushUndoSnapshot();
            m_project = after;
        }
    }
    // ToolCreate already pushed undo on press.
    m_dragChangedProject = false;
    emitChanged();
}

void TimelineEditorWidget::contextMenuEvent(QContextMenuEvent* e) {
    int idx = -1;
    HitZone hz = hitTest(e->pos(), &idx);
    if (hz == HitNone) return;
    setSelectedIndex(idx);

    QMenu menu(this);
    QAction* aPulse = menu.addAction("Convert to Pulse");
    QAction* aOn    = menu.addAction("Convert to ChannelOn");
    QAction* aOff   = menu.addAction("Convert to ChannelOff");
    menu.addSeparator();
    QAction* aDup   = menu.addAction("Duplicate (Ctrl+D)");
    QAction* aDel   = menu.addAction("Delete");
    QAction* picked = menu.exec(e->globalPos());
    if (!picked) return;
    if (picked == aDel) { deleteSelected(); return; }
    if (picked == aDup) { duplicateSelected(); return; }
    pushUndoSnapshot();
    auto& ev = m_project.events[idx];
    if      (picked == aPulse) ev.type = TimelineEvent::Pulse;
    else if (picked == aOn)    ev.type = TimelineEvent::On;
    else if (picked == aOff)   ev.type = TimelineEvent::Off;
    emitChanged();
}

void TimelineEditorWidget::wheelEvent(QWheelEvent* e) {
    double factor = (e->angleDelta().y() > 0) ? 0.85 : 1.18;
    if (e->modifiers() & Qt::ControlModifier) {
        // Pan
        m_viewStartMs = qMax(0.0, m_viewStartMs + (factor - 1.0) * m_viewSpanMs);
    } else {
        // Zoom around cursor
        double pivotT = xToTime(int(e->position().x()));
        double newSpan = qBound(kMinSpanMs, m_viewSpanMs * factor, kMaxSpanMs);
        m_viewStartMs = pivotT - (pivotT - m_viewStartMs) * (newSpan / m_viewSpanMs);
        m_viewSpanMs  = newSpan;
        m_viewStartMs = qMax(0.0, m_viewStartMs);
    }
    emit zoomChanged();
    update();
}

void TimelineEditorWidget::keyPressEvent(QKeyEvent* e) {
    // Undo / redo first (work even with no selection).
    if (e->matches(QKeySequence::Undo)) { undo(); return; }
    if (e->matches(QKeySequence::Redo)) { redo(); return; }

    // Tool switching.
    if (e->modifiers() == Qt::NoModifier) {
        if (e->key() == Qt::Key_S) { setTool(ToolSelect); return; }
        if (e->key() == Qt::Key_P) { setTool(ToolPulse);  return; }
        if (e->key() == Qt::Key_O) { setTool(ToolOn);     return; }
        if (e->key() == Qt::Key_F) { setTool(ToolOff);    return; }
    }

    if (e->key() == Qt::Key_Escape) {
        setSelectedIndex(-1);
        return;
    }

    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        deleteSelected();
        return;
    }

    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_D) {
        duplicateSelected();
        return;
    }

    // Arrow nudge (only with a selection).
    if ((e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) &&
        m_selected >= 0 && m_selected < m_project.events.size()) {
        double step = (m_snapMs > 0) ? m_snapMs : 10.0;
        if (e->modifiers() & Qt::ControlModifier) step *= 10.0;
        double dir = (e->key() == Qt::Key_Right) ? +1.0 : -1.0;
        pushUndoSnapshot();
        auto& ev = m_project.events[m_selected];
        ev.startMs = qMax(0.0, ev.startMs + dir * step);
        emitChanged();
        return;
    }

    QWidget::keyPressEvent(e);
}

void TimelineEditorWidget::leaveEvent(QEvent* e) {
    m_hoverLane = -1;
    emit cursorTimeChanged(-1);
    update();
    QWidget::leaveEvent(e);
}
