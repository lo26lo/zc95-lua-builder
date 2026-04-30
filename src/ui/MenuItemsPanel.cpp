#include "MenuItemsPanel.h"
#include "MenuItemDialog.h"

#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMessageBox>

MenuItemsPanel::MenuItemsPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    auto* group = new QGroupBox("Menu Items", this);
    auto* layout = new QVBoxLayout(group);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto* row = new QHBoxLayout();
    auto* add = new QPushButton("Add…", this);
    auto* edit = new QPushButton("Edit…", this);
    auto* dup = new QPushButton("Duplicate", this);
    auto* rm = new QPushButton("Remove", this);
    auto* up = new QPushButton("↑", this);
    auto* down = new QPushButton("↓", this);
    row->addWidget(add);
    row->addWidget(edit);
    row->addWidget(dup);
    row->addWidget(rm);
    row->addStretch();
    row->addWidget(up);
    row->addWidget(down);
    layout->addLayout(row);

    outer->addWidget(group);

    connect(add, &QPushButton::clicked, this, &MenuItemsPanel::addItem);
    connect(edit, &QPushButton::clicked, this, &MenuItemsPanel::editItem);
    connect(dup, &QPushButton::clicked, this, &MenuItemsPanel::duplicateItem);
    connect(rm, &QPushButton::clicked, this, &MenuItemsPanel::removeItem);
    connect(up, &QPushButton::clicked, this, &MenuItemsPanel::moveUp);
    connect(down, &QPushButton::clicked, this, &MenuItemsPanel::moveDown);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &MenuItemsPanel::editItem);
}

void MenuItemsPanel::load(const QVector<MenuItem>& items) {
    m_items = items;
    refresh();
}

void MenuItemsPanel::refresh() {
    m_list->clear();
    for (const auto& item : m_items) {
        m_list->addItem(item.summary());
    }
}

int MenuItemsPanel::currentRow() const {
    return m_list->currentRow();
}

int MenuItemsPanel::nextFreeId() const {
    int id = 1;
    while (idIsTaken(id, -1)) ++id;
    return id;
}

bool MenuItemsPanel::idIsTaken(int id, int ignoreIndex) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (i == ignoreIndex) continue;
        if (m_items[i].id == id) return true;
    }
    return false;
}

void MenuItemsPanel::addItem() {
    MenuItemDialog dlg(this);
    MenuItem fresh;
    fresh.id = nextFreeId();
    dlg.setItem(fresh);
    while (dlg.exec() == QDialog::Accepted) {
        MenuItem result = dlg.item();
        if (idIsTaken(result.id, -1)) {
            QMessageBox::warning(this, "Duplicate ID",
                QString("ID %1 is already used by another menu item. Please choose a different ID.").arg(result.id));
            dlg.setItem(result);
            continue;
        }
        m_items.push_back(result);
        refresh();
        emit changed();
        return;
    }
}

void MenuItemsPanel::editItem() {
    int row = currentRow();
    if (row < 0) return;
    MenuItemDialog dlg(this);
    dlg.setItem(m_items[row]);
    while (dlg.exec() == QDialog::Accepted) {
        MenuItem result = dlg.item();
        if (idIsTaken(result.id, row)) {
            QMessageBox::warning(this, "Duplicate ID",
                QString("ID %1 is already used by another menu item. Please choose a different ID.").arg(result.id));
            dlg.setItem(result);
            continue;
        }
        m_items[row] = result;
        refresh();
        m_list->setCurrentRow(row);
        emit changed();
        return;
    }
}

void MenuItemsPanel::duplicateItem() {
    int row = currentRow();
    if (row < 0) return;
    MenuItem copy = m_items[row];
    copy.id = nextFreeId();
    m_items.insert(row + 1, copy);
    refresh();
    m_list->setCurrentRow(row + 1);
    emit changed();
}

void MenuItemsPanel::removeItem() {
    int row = currentRow();
    if (row < 0) return;
    m_items.remove(row);
    refresh();
    emit changed();
}

void MenuItemsPanel::moveUp() {
    int row = currentRow();
    if (row <= 0) return;
    std::swap(m_items[row], m_items[row - 1]);
    refresh();
    m_list->setCurrentRow(row - 1);
    emit changed();
}

void MenuItemsPanel::moveDown() {
    int row = currentRow();
    if (row < 0 || row >= m_items.size() - 1) return;
    std::swap(m_items[row], m_items[row + 1]);
    refresh();
    m_list->setCurrentRow(row + 1);
    emit changed();
}
