#include "TimelineWidget.h"

#include <QPainter>
#include <QPen>

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 160);
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

    double tEnd = m_currentTimeMs;
    double tStart = tEnd - m_windowMs;
    if (tStart < 0) tStart = 0;
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

    // Reconstruct on/off per channel by walking events in time order.
    struct Segment { double start; double end; bool isPulse; };
    QVector<Segment> segs[4];
    bool on[4] = {false, false, false, false};
    double onSince[4] = {0, 0, 0, 0};
    bool isPulse[4] = {false, false, false, false};

    for (const auto& e : m_events) {
        if (e.channel < 1 || e.channel > 4) continue;
        int idx = e.channel - 1;
        if (e.type == ChannelEventType::ChannelOn) {
            if (on[idx]) {
                segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx]});
            }
            on[idx] = true;
            onSince[idx] = e.timeMs;
            isPulse[idx] = false;
        } else if (e.type == ChannelEventType::ChannelOff) {
            if (on[idx]) {
                segs[idx].push_back({onSince[idx], e.timeMs, isPulse[idx]});
                on[idx] = false;
            }
        } else if (e.type == ChannelEventType::ChannelPulseMs) {
            // Treated as a sealed segment
            segs[idx].push_back({e.timeMs, e.timeMs + e.param1, true});
        }
    }
    // Close any still-on segment at currentTime
    for (int i = 0; i < 4; ++i) {
        if (on[i]) segs[i].push_back({onSince[i], tEnd, isPulse[i]});
    }

    // Draw segments
    for (int i = 0; i < 4; ++i) {
        int yTop = r.top() + 4 + i * laneH;
        int barY = yTop + laneH / 2 - 8;
        int barH = 14;
        for (const auto& s : segs[i]) {
            if (s.end < tStart || s.start > tEnd) continue;
            int x1 = xOf(s.start);
            int x2 = xOf(s.end);
            int w = qMax(2, x2 - x1);
            QColor col = s.isPulse ? QColor("#f0a020") : QColor("#4ec96e");
            p.fillRect(QRect(x1, barY, w, barH), col);
            p.setPen(col.darker(150));
            p.drawRect(QRect(x1, barY, w, barH));
        }
    }

    // Vertical "now" line
    p.setPen(QPen(QColor("#aa6666"), 1, Qt::DashLine));
    p.drawLine(xOf(tEnd), r.top() + 2, xOf(tEnd), axisY);
}
