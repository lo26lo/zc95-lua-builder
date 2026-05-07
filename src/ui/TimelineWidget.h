#pragma once

#include "../sim/ChannelEvent.h"
#include <QWidget>

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setEvents(const QVector<ChannelEvent>& events, double currentTimeMs);
    void clear();
    void setWindowMs(double ms) { m_windowMs = ms; update(); }

protected:
    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    QSize sizeHint() const override { return QSize(600, 180); }

private:
    struct Segment { double start; double end; bool isPulse; int power; };
    struct Marker  { double timeMs; int category; QString label; };

    void recomputeCache();
    QPair<double, double> visibleWindow() const;

    QVector<ChannelEvent> m_events;
    QVector<Segment> m_segs[4];
    QVector<Marker> m_markers;
    double m_currentTimeMs = 0;
    double m_windowMs = 5000.0;  // visible time window
};
