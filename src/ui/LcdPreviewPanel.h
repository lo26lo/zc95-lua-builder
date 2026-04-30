#pragma once

#include "../model/MenuItem.h"
#include <QWidget>
#include <QVector>
#include <QMap>

class LcdPreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit LcdPreviewPanel(QWidget* parent = nullptr);

    void setItems(const QVector<MenuItem>& items, const QString& patternName,
                  const QString& softButtonLabel);

    // Override the displayed value for a single menu_id without rebuilding
    // the whole item list. Used to mirror the simulator's live sliders /
    // combos onto the LCD preview in real time.
    //   For MIN_MAX:    pass the slider value.
    //   For MULTI_CHOICE: pass the selected choice_id.
    void setLiveValue(int menuId, int value);
    void clearLiveValues();

protected:
    void paintEvent(QPaintEvent* e) override;
    QSize sizeHint() const override { return QSize(280, 220); }

private:
    QVector<MenuItem> m_items;
    QString m_patternName;
    QString m_softButton;
    QMap<int, int> m_liveValues;   // menu_id -> override
};
