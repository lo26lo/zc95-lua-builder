#include "TimelineWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPolygon>
#include <QToolTip>

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 160);
    setMouseTracking(true);
    setToolTip(
        "Each lane shows what the SCRIPT did on that channel.\n"
        "  ▮ green   = sustained ON (between ChannelOn / ChannelOff)\n"
        "  ▮ orange  = ChannelPulseMs (single fixed-duration pulse)\n"
        "  ▼ yellow cursor = simulated time \"now\".\n"
        "  ┊ dashed marker = user input (slider, button, trigger)\n\n"
        "Hover a bar for segment details. The pulse PATTERN you see is\n"
        "decided by the script's logic. Moving a Menu controls slider\n"
        "stamps a marker on the timeline AND fires MinMaxChange so you\n"
        "can correlate input with reaction.\n\n"
        "To change the SIMULATOR speed, use the \"speed ×N\" and\n"
        "\"N ms/tick\" controls above.");
}

void TimelineWidget::setEvents(const QVector<ChannelEvent>& events, double currentTimeMs) {
    m_events = events;
    m_currentTimeMs = currentTimeMs;
    recomputeCache();
    update();
}

void TimelineWidget::clear() {
    m_events.clear();
    for (int i = 0; i < 4; ++i) m_segs[i].clear();
    m_markers.clear();
    m_currentTimeMs = 0;
    update();
}

QPair<double, double> TimelineWidget::visibleWindow() const {
    // Fixed-window-then-slide behaviour:
    //   • t < windowMs     → window is [0, windowMs], cursor advances
    //                        from left to right inside it.
    //   • t ≥ windowMs     → window slides so its right edge is "now".
    if (m_currentTimeMs < m_windowMs) return {0.0, m_windowMs};
    return {m_currentTimeMs - m_windowMs, m_currentTimeMs};
}

void TimelineWidget::recomputeCache() {
    for (int i = 0; i < 4; ++i) m_segs[i].clear();
    m_markers.clear();

    // Reconstruct on/off + power per channel by walking events in time
    // order. Each segment records its power level so we can draw bars
    // whose height is proportional to the effective output (power/1000).
    bool on[4] = {false, false, false, false};
    double onSince[4] = {0, 0, 0, 0};
    bool isPulse[4] = {false, false, false, false};
    int curPower[4] = {1000, 1000, 1000, 1000};   // device default

    for (const auto& e : m_events) {
        if (e.type == ChannelEventType::UserInput) {
            m_markers.push_back({e.timeMs, e.param1, e.textParam});
            continue;
        }
        if (e.channel < 1 || e.channel > 4) continue;
        int idx = e.channel - 1;
        if (e.type == ChannelEventType::ChannelOn) {
            if (on[idx]) {
                m_segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx], curPower[idx]});
            }
            on[idx] = true;
            onSince[idx] = e.timeMs;
            isPulse[idx] = false;
        } else if (e.type == ChannelEventType::ChannelOff) {
            if (on[idx]) {
                m_segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx], curPower[idx]});
                on[idx] = false;
            }
        } else if (e.type == ChannelEventType::ChannelPulseMs) {
            // Treated as a sealed segment with the current power level.
            m_segs[idx].push_back({e.timeMs, e.timeMs + e.param1, true, curPower[idx]});
        } else if (e.type == ChannelEventType::SetPower) {
            // If the channel is currently ON, close the running segment
            // at this point and start a new one with the new power.
            // Otherwise just remember the power for the next ON.
            if (on[idx]) {
                m_segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx], curPower[idx]});
                onSince[idx] = e.timeMs;
            }
            curPower[idx] = e.param1;
        }
    }
    // Close any still-on segment at currentTime
    for (int i = 0; i < 4; ++i) {
        if (on[i]) m_segs[i].push_back({onSince[i], m_currentTimeMs, isPulse[i], curPower[i]});
    }
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect().adjusted(4, 4, -4, -4);
    p.fillRect(r, QColor("#1e1e1e"));
    p.setPen(QColor("#3c3c3c"));
    p.drawRect(r);

    const int leftMargin = 32;
    // Reserve a thin band at the very top for user-input marker labels
    // so they don't overlap the first channel's bars.
    const int labelBandH = 12;
    const int laneTop = r.top() + 4 + labelBandH;
    const int laneAreaH = r.height() - 24 - labelBandH;
    const int laneH = laneAreaH / 4;
    const int chartLeft = r.left() + leftMargin;
    const int chartRight = r.right() - 4;
    const int chartW = chartRight - chartLeft;

    auto window = visibleWindow();
    double tStart = window.first;
    double tEnd   = window.second;
    double span = qMax(1.0, tEnd - tStart);

    auto xOf = [&](double t) {
        double rel = (t - tStart) / span;
        rel = qBound(0.0, rel, 1.0);
        return chartLeft + int(rel * chartW);
    };

    // Channel labels and lanes
    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);
    for (int ch = 0; ch < 4; ++ch) {
        int yTop = laneTop + ch * laneH;
        int yMid = yTop + laneH / 2;
        p.setPen(QColor("#888"));
        p.drawText(QRect(r.left(), yTop, leftMargin - 6, laneH), Qt::AlignVCenter | Qt::AlignRight,
                   QString("CH%1").arg(ch + 1));
        p.setPen(QPen(QColor("#333"), 1, Qt::DashLine));
        p.drawLine(chartLeft, yMid, chartRight, yMid);
    }

    // Time axis
    int axisY = r.bottom() - 14;
    p.setPen(QColor("#666"));
    p.drawLine(chartLeft, axisY, chartRight, axisY);
    int ticks = 5;
    for (int i = 0; i <= ticks; ++i) {
        int x = chartLeft + i * chartW / ticks;
        p.drawLine(x, axisY, x, axisY + 3);
        double t = tStart + i * span / ticks;
        p.drawText(QRect(x - 30, axisY + 4, 60, 12), Qt::AlignHCenter,
                   QString::number(t / 1000.0, 'f', 1) + "s");
    }

    // Draw segments — bar HEIGHT is proportional to power (0..1000), so a
    // low-intensity sustained ON looks like a thin stripe centered on the
    // lane mid-line and full power looks like a fat bar. Adjacent
    // segments still merge cleanly because we don't draw outlines.
    for (int i = 0; i < 4; ++i) {
        int yTop = laneTop + i * laneH;
        int yMid = yTop + laneH / 2;
        const int kMaxBarH = 14;
        const int kMinBarH = 2;
        for (const auto& s : m_segs[i]) {
            if (s.end < tStart || s.start > tEnd) continue;
            int x1 = xOf(s.start);
            int x2 = xOf(s.end);
            int w = qMax(2, x2 - x1);
            int pwr = qBound(0, s.power, 1000);
            int barH = kMinBarH + (kMaxBarH - kMinBarH) * pwr / 1000;
            int barY = yMid - barH / 2;
            QColor col = s.isPulse ? QColor("#f0a020") : QColor("#4ec96e");
            p.fillRect(QRect(x1, barY, w, barH), col);
        }
    }

    // User-input markers — vertical dashed line spanning all lanes plus
    // a short label in the top band. Color codes the source so the eye
    // can distinguish "I moved a slider" from "I hit Soft Btn" at a
    // glance:
    //   0 = menu (cyan)        — sliders / multi-choice combos
    //   1 = soft button (magenta)
    //   2 = external trigger (warm orange)
    auto markerColor = [](int category) -> QColor {
        switch (category) {
            case 0:  return QColor("#4fc3f7");
            case 1:  return QColor("#e57aff");
            case 2:  return QColor("#ffb347");
            default: return QColor("#bbbbbb");
        }
    };
    int laneBottom = laneTop + 4 * laneH;
    for (const auto& m : m_markers) {
        if (m.timeMs < tStart || m.timeMs > tEnd) continue;
        int x = xOf(m.timeMs);
        QColor col = markerColor(m.category);
        p.setPen(QPen(col, 1, Qt::DashLine));
        p.drawLine(x, laneTop, x, laneBottom);
        // Label sits in the reserved band above the lanes. Drawn left-
        // aligned starting just to the right of the marker line so the
        // line itself stays readable.
        QColor labelCol = col;
        p.setPen(labelCol);
        QRect labelRect(x + 3, r.top() + 2, 140, labelBandH);
        p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, m.label);
    }

    // Vertical "now" cursor — designed to stay visible on top of dense
    // green segments. Four layers, all in cyan/yellow that contrasts
    // strongly against the green/orange channel bars.
    int nowX = xOf(m_currentTimeMs);
    int top = laneTop - 2;
    int bot = axisY;

    // Halo (wide, very translucent)
    p.setPen(QPen(QColor(255, 220, 0, 50), 12));
    p.drawLine(nowX, top, nowX, bot);

    // Main line — solid, bright yellow.
    p.setPen(QPen(QColor("#ffd400"), 3));
    p.drawLine(nowX, top, nowX, bot);

    // White inner core for contrast on saturated channels.
    p.setPen(QPen(QColor(255, 255, 255, 220), 1));
    p.drawLine(nowX, top, nowX, bot);

    // Top arrow head — bigger this time.
    {
        QPolygon tri;
        tri << QPoint(nowX - 7, top - 2)
            << QPoint(nowX + 7, top - 2)
            << QPoint(nowX,     top + 9);
        p.setPen(QPen(QColor(0, 0, 0, 200), 1));
        p.setBrush(QColor("#ffd400"));
        p.drawPolygon(tri);
    }
    // Bottom arrow head pointing up — sits on the time axis.
    {
        QPolygon tri;
        tri << QPoint(nowX - 7, bot + 2)
            << QPoint(nowX + 7, bot + 2)
            << QPoint(nowX,     bot - 7);
        p.setPen(QPen(QColor(0, 0, 0, 200), 1));
        p.setBrush(QColor("#ffd400"));
        p.drawPolygon(tri);
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* ev) {
    QRect r = rect().adjusted(4, 4, -4, -4);
    const int leftMargin = 32;
    const int labelBandH = 12;
    const int laneTop = r.top() + 4 + labelBandH;
    const int laneAreaH = r.height() - 24 - labelBandH;
    const int laneH = laneAreaH / 4;
    const int chartLeft = r.left() + leftMargin;
    const int chartRight = r.right() - 4;
    const int chartW = chartRight - chartLeft;

    int x = ev->pos().x();
    int y = ev->pos().y();
    if (x < chartLeft || x > chartRight) { QToolTip::hideText(); return; }

    auto window = visibleWindow();
    double tStart = window.first;
    double tEnd   = window.second;
    double span = qMax(1.0, tEnd - tStart);
    double tHover = tStart + double(x - chartLeft) * span / qMax(1, chartW);

    // 1. User-input markers take priority — they're thin so we accept a
    //    wide ±5 px hit zone.
    for (const auto& m : m_markers) {
        if (m.timeMs < tStart || m.timeMs > tEnd) continue;
        int mx = chartLeft + int((m.timeMs - tStart) / span * chartW);
        if (qAbs(x - mx) <= 5) {
            const char* kind =
                m.category == 0 ? "Menu input"
              : m.category == 1 ? "Soft button"
              : m.category == 2 ? "External trigger"
              : "User input";
            QToolTip::showText(ev->globalPosition().toPoint(),
                QString("%1\n%2\nt = %3 s")
                    .arg(kind, m.label)
                    .arg(m.timeMs / 1000.0, 0, 'f', 3),
                this);
            return;
        }
    }

    // 2. Channel segment under cursor.
    if (y >= laneTop && y < laneTop + 4 * laneH) {
        int ch = (y - laneTop) / laneH;
        if (ch >= 0 && ch < 4) {
            for (const auto& s : m_segs[ch]) {
                if (tHover >= s.start && tHover <= s.end) {
                    double durMs = s.end - s.start;
                    QToolTip::showText(ev->globalPosition().toPoint(),
                        QString("CH%1 — %2\nt %3 → %4 s   (%5 ms)\npower = %6 / 1000")
                            .arg(ch + 1)
                            .arg(s.isPulse ? "ChannelPulseMs" : "sustained ON")
                            .arg(s.start / 1000.0, 0, 'f', 3)
                            .arg(s.end / 1000.0,   0, 'f', 3)
                            .arg(durMs, 0, 'f', 0)
                            .arg(s.power),
                        this);
                    return;
                }
            }
            // Lane is empty at this time — show the time being hovered.
            QToolTip::showText(ev->globalPosition().toPoint(),
                QString("CH%1 — idle\nt = %2 s")
                    .arg(ch + 1).arg(tHover / 1000.0, 0, 'f', 3),
                this);
            return;
        }
    }

    QToolTip::hideText();
}

void TimelineWidget::leaveEvent(QEvent*) {
    QToolTip::hideText();
}
