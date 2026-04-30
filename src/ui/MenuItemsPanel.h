#pragma once

#include "../model/MenuItem.h"
#include <QWidget>
#include <QVector>

class QListWidget;

class MenuItemsPanel : public QWidget {
    Q_OBJECT
public:
    explicit MenuItemsPanel(QWidget* parent = nullptr);

    void load(const QVector<MenuItem>& items);
    QVector<MenuItem> items() const { return m_items; }

signals:
    void changed();
    // Emitted when the user clicks the ▶ Test button on a menu item.
    // MainWindow handles it by switching to the Simulator tab and
    // poking the corresponding callback to verify the script reacts.
    void testItemRequested(int row);

private slots:
    void addItem();
    void editItem();
    void removeItem();
    void duplicateItem();
    void moveUp();
    void moveDown();
    void testItem();

private:
    void refresh();
    int currentRow() const;
    int nextFreeId() const;
    bool idIsTaken(int id, int ignoreIndex) const;

    QListWidget* m_list;
    QVector<MenuItem> m_items;
};
