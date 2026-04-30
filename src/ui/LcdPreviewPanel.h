#pragma once

#include "../model/MenuItem.h"
#include <QWidget>
#include <QVector>

class LcdPreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit LcdPreviewPanel(QWidget* parent = nullptr);

    void setItems(const QVector<MenuItem>& items, const QString& patternName,
                  const QString& softButtonLabel);

protected:
    void paintEvent(QPaintEvent* e) override;
    QSize sizeHint() const override { return QSize(280, 220); }

private:
    QVector<MenuItem> m_items;
    QString m_patternName;
    QString m_softButton;
};
