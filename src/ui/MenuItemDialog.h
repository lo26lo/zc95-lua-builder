#pragma once

#include "../model/MenuItem.h"
#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

class MenuItemDialog : public QDialog {
    Q_OBJECT
public:
    explicit MenuItemDialog(QWidget* parent = nullptr);

    void setItem(const MenuItem& item);
    MenuItem item() const;

private slots:
    void onTypeChanged(int index);
    void addChoice();
    void removeChoice();

private:
    QComboBox* m_type;
    QSpinBox* m_id;
    QSpinBox* m_group;
    QLineEdit* m_title;

    // MIN_MAX
    QStackedWidget* m_stack;
    QSpinBox* m_min;
    QSpinBox* m_max;
    QSpinBox* m_step;
    QLineEdit* m_uom;
    QSpinBox* m_default;

    // MULTI_CHOICE
    QTableWidget* m_choices;
};
