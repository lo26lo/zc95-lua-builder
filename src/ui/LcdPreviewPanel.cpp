#include "LcdPreviewPanel.h"

#include <QPainter>
#include <QFont>
#include <QSet>

LcdPreviewPanel::LcdPreviewPanel(QWidget* parent) : QWidget(parent) {
    setMinimumSize(280, 220);
}

void LcdPreviewPanel::setItems(const QVector<MenuItem>& items, const QString& patternName,
                               const QString& softButtonLabel) {
    m_items = items;
    m_patternName = patternName;
    m_softButton = softButtonLabel;
    // Drop overrides for menu_ids that no longer exist (item was deleted).
    QSet<int> validIds;
    for (const auto& mi : items) validIds.insert(mi.id);
    QList<int> toRemove;
    for (auto it = m_liveValues.begin(); it != m_liveValues.end(); ++it) {
        if (!validIds.contains(it.key())) toRemove.append(it.key());
    }
    for (int id : toRemove) m_liveValues.remove(id);
    update();
}

void LcdPreviewPanel::setLiveValue(int menuId, int value) {
    m_liveValues[menuId] = value;
    update();
}

void LcdPreviewPanel::clearLiveValues() {
    if (m_liveValues.isEmpty()) return;
    m_liveValues.clear();
    update();
}

void LcdPreviewPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // ZC95-style green/black LCD-ish look.
    const QColor bg("#1d2b18");
    const QColor fg("#9bd76e");
    const QColor dim("#4f7a36");

    QRect screen = rect().adjusted(8, 8, -8, -8);
    p.fillRect(screen, bg);
    p.setPen(QPen(dim, 2));
    p.drawRect(screen);

    QFont mono("Consolas");
    if (!QFontInfo(mono).fixedPitch()) mono.setFamily("Courier New");
    mono.setPixelSize(12);
    p.setFont(mono);
    p.setPen(fg);

    int x = screen.left() + 6;
    int y = screen.top() + 6;
    int w = screen.width() - 12;

    // Title bar: pattern name (prefixed U: like the device shows for user scripts)
    QString title = "U: " + (m_patternName.isEmpty() ? "(unnamed)" : m_patternName);
    p.drawText(QRect(x, y, w, 14), Qt::AlignLeft, title);
    p.setPen(QPen(dim, 1));
    p.drawLine(x, y + 16, x + w, y + 16);
    p.setPen(fg);

    y += 22;

    // Soft button label (top-left of footer area)
    int footerY = screen.bottom() - 18;
    p.setPen(QPen(dim, 1));
    p.drawLine(x, footerY - 2, x + w, footerY - 2);
    p.setPen(fg);
    if (!m_softButton.isEmpty()) {
        QString sb = "[" + m_softButton + "]";
        p.drawText(QRect(x, footerY, w, 14), Qt::AlignLeft, sb);
    }

    // Menu items: render between title and footer.
    int areaBottom = footerY - 6;
    int rowH = 22;

    if (m_items.isEmpty()) {
        p.setPen(dim);
        p.drawText(QRect(x, y, w, 20), Qt::AlignCenter, "(no menu items)");
        return;
    }

    int maxRows = (areaBottom - y) / rowH;
    int shown = qMin(m_items.size(), maxRows);

    for (int i = 0; i < shown; ++i) {
        const MenuItem& mi = m_items[i];
        int rowY = y + i * rowH;
        QRect rowRect(x, rowY, w, rowH - 4);

        QString title = mi.title.isEmpty() ? QString("(item %1)").arg(mi.id) : mi.title;
        p.drawText(QRect(rowRect.left(), rowRect.top(), w, 12), Qt::AlignLeft, title);

        if (mi.type == MenuItemType::MinMax) {
            // Use the live override if present, otherwise the form default.
            int displayed = m_liveValues.value(mi.id, mi.defaultValue);
            // Bar graph: fill ratio = (displayed - min) / (max - min)
            int barX = rowRect.left();
            int barY = rowRect.top() + 12;
            int barW = w - 60;
            int barH = 8;
            p.setPen(dim);
            p.drawRect(barX, barY, barW, barH);
            int range = qMax(1, mi.max - mi.min);
            double ratio = double(displayed - mi.min) / range;
            ratio = qBound(0.0, ratio, 1.0);
            p.fillRect(QRect(barX + 1, barY + 1, int((barW - 2) * ratio), barH - 2), fg);
            p.setPen(fg);
            QString val = QString::number(displayed) + mi.uom;
            p.drawText(QRect(barX + barW + 4, barY - 1, 56, 10), Qt::AlignLeft, val);
        } else if (mi.type == MenuItemType::MultiChoice) {
            // Live override = currently-selected choice_id.
            QString label;
            if (mi.choices.isEmpty()) {
                label = "(no choices)";
            } else {
                int liveId = m_liveValues.value(mi.id, mi.choices.first().choiceId);
                bool found = false;
                for (const auto& c : mi.choices) {
                    if (c.choiceId == liveId) { label = c.description; found = true; break; }
                }
                if (!found) label = mi.choices.first().description;
            }
            p.drawText(QRect(rowRect.left() + 12, rowRect.top() + 12, w - 12, 10),
                       Qt::AlignLeft, "< " + label + " >");
        } else if (mi.type == MenuItemType::AudioViewIntensityStereo
                || mi.type == MenuItemType::AudioViewIntensityMono) {
            // Mock waveform
            int wfX = rowRect.left();
            int wfY = rowRect.top() + 12;
            int wfW = w;
            int wfH = 8;
            p.setPen(QPen(dim, 1));
            p.drawLine(wfX, wfY + wfH / 2, wfX + wfW, wfY + wfH / 2);
            p.setPen(QPen(fg, 1));
            for (int xi = 0; xi < wfW; xi += 3) {
                int amp = (xi * 7) % 5 - 2;
                p.drawLine(wfX + xi, wfY + wfH / 2 - amp, wfX + xi, wfY + wfH / 2 + amp);
            }
        }
    }

    if (m_items.size() > shown) {
        p.setPen(dim);
        p.drawText(QRect(x, areaBottom - 12, w, 12), Qt::AlignRight,
                   QString("+%1 more").arg(m_items.size() - shown));
    }
}
