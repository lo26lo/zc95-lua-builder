#include "TimelineWidget.h"

#include <QPainter>
#include <QPen>
#include <QPolygon>

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 160);
    setToolTip(
        "Each lane shows what the SCRIPT did on that channel.\n"
        "  ▮ green   = sustained ON (between ChannelOn / ChannelOff)\n"
        "  ▮ orange  = ChannelPulseMs (single fixed-duration pulse)\n"
        "  ▼ yellow cursor = simulated time \"now\".\n\n"
        "The pulse PATTERN you see is decided by the script's logic.\n"
        "Moving a Menu controls slider changes the script's parameters\n"
        "(e.g. zc.SetFrequency value) but won't change the visual\n"
        "density unless the script ties its scheduling to that value.\n\n"
        "To change the SIMULATOR speed, use the \"speed ×N\" and\n"
        "\"N ms/tick\" controls above.");
}

void TimelineWidget::setEvents(const QVector<ChannelEvent>& events, double currentTimeMs) {
    m_events = events;
    m_currentTimeMs = currentTimeMs;
    update();
}

void TimelineWidget::clear() {
    m_events.clear();
    m_currentTimeMs = 0;
    update();
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect().adjusted(4, 4, -4, -4);
    p.fillRect(r, QColor("#1e1e1e"));
    p.setPen(QColor("#3c3c3c"));
    p.drawRect(r);

    const int leftMargin = 32;
    const int laneH = (r.height() - 24) / 4;
    const int chartLeft = r.left() + leftMargin;
    const int chartRight = r.right() - 4;
    const int chartW = chartRight - chartLeft;

    // Fixed-window-then-slide behaviour:
    //   • t < windowMs     → window is [0, windowMs], cursor advances
    //                        from left to right inside it.
    //   • t ≥ windowMs     → window slides so its right edge is "now".
    // This gives the user a visible "cursor moving" feedback during the
    // first few seconds, instead of the cursor pinned to the right edge.
    double tStart, tEnd;
    if (m_currentTimeMs < m_windowMs) {
        tStart = 0;
        tEnd   = m_windowMs;
    } else {
        tEnd   = m_currentTimeMs;
        tStart = tEnd - m_windowMs;
    }
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
        int yTop = r.top() + 4 + ch * laneH;
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

    // Reconstruct on/off + power per channel by walking events in time
    // order. Each segment records its power level so we can draw bars
    // whose height is proportional to the effective output (power/1000).
    struct Segment { double start; double end; bool isPulse; int power; };
    QVector<Segment> segs[4];
    bool on[4] = {false, false, false, false};
    double onSince[4] = {0, 0, 0, 0};
    bool isPulse[4] = {false, false, false, false};
    int curPower[4] = {1000, 1000, 1000, 1000};   // device default

    for (const auto& e : m_events) {
        if (e.channel < 1 || e.channel > 4) continue;
        int idx = e.channel - 1;
        if (e.type == ChannelEventType::ChannelOn) {
            if (on[idx]) {
                segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx], curPower[idx]});
            }
            on[idx] = true;
            onSince[idx] = e.timeMs;
            isPulse[idx] = false;
        } else if (e.type == ChannelEventType::ChannelOff) {
            if (on[idx]) {
                segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx], curPower[idx]});
                on[idx] = false;
            }
        } else if (e.type == ChannelEventType::ChannelPulseMs) {
            // Treated as a sealed segment with the current power level.
            segs[idx].push_back({e.timeMs, e.timeMs + e.param1, true, curPower[idx]});
        } else if (e.type == ChannelEventType::SetPower) {
            // If the channel is currently ON, close the running segment
            // at this point and start a new one with the new power.
            // Otherwise just remember the power for the next ON.
            if (on[idx]) {
                segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx], curPower[idx]});
                onSince[idx] = e.timeMs;
            }
            curPower[idx] = e.param1;
        }
    }
    // Close any still-on segment at currentTime
    for (int i = 0; i < 4; ++i) {
        if (on[i]) segs[i].push_back({onSince[i], m_currentTimeMs, isPulse[i], curPower[i]});
    }

    // Draw segments — bar HEIGHT is proportional to power (0..1000), so a
    // low-intensity sustained ON looks like a thin stripe centered on the
    // lane mid-line and full power looks like a fat bar. Adjacent
    // segments still merge cleanly because we don't draw outlines.
    for (int i = 0; i < 4; ++i) {
        int yTop = r.top() + 4 + i * laneH;
        int yMid = yTop + laneH / 2;
        const int kMaxBarH = 14;
        const int kMinBarH = 2;
        for (const auto& s : segs[i]) {
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

    // Vertical "now" cursor — designed to stay visible on top of dense
    // green segments. Four layers, all in cyan/yellow that contrasts
    // strongly against the green/orange channel bars:
    //   1. A 12px translucent yellow halo for the "glow".
    //   2. A 3px solid bright-yellow line as the actual marker.
    //   3. A 1px white core down the middle for max contrast.
    //   4. A big yellow triangle at the top (and a matching one at the
    //      bottom on the time axis) so the eye finds it instantly.
    // The cursor follows simulated "now", not the window's right edge.
    // While t < windowMs the window is fixed and the cursor walks from
    // left to right; afterwards the cursor sits at the right edge while
    // the window slides under it.
    int nowX = xOf(m_currentTimeMs);
    int top = r.top() + 2;
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
