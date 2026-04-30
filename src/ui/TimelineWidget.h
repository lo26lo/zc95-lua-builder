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
    QSize sizeHint() const override { return QSize(600, 180); }

private:
    QVector<ChannelEvent> m_events;
    double m_currentTimeMs = 0;
    double m_windowMs = 5000.0;  // visible time window
};
