#include "MenuItemDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>

MenuItemDialog::MenuItemDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Menu Item");
    resize(480, 420);

    auto* outer = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_type = new QComboBox(this);
    m_type->addItem("MIN_MAX", static_cast<int>(MenuItemType::MinMax));
    m_type->addItem("MULTI_CHOICE", static_cast<int>(MenuItemType::MultiChoice));
    m_type->addItem("AUDIO_VIEW_INTENSITY_STEREO", static_cast<int>(MenuItemType::AudioViewIntensityStereo));
    m_type->addItem("AUDIO_VIEW_INTENSITY_MONO", static_cast<int>(MenuItemType::AudioViewIntensityMono));
    m_type->setToolTip(
        "MIN_MAX        — a slider with min/max/step (e.g. delay in ms,\n"
        "                 frequency in Hz, intensity in %).\n"
        "MULTI_CHOICE   — a list of named options (e.g. mode = Pulse / Fade /\n"
        "                 Constant). The user cycles through them on-screen.\n"
        "AUDIO_VIEW_*   — decorative VU-meter on the LCD. Requires\n"
        "                 audio_processing_mode = AUDIO_INTENSITY in Config.");
    form->addRow("Type:", m_type);

    m_id = new QSpinBox(this);
    m_id->setRange(1, 99);
    m_id->setToolTip(
        "Numeric ID used by your script to identify this item.\n"
        "Inside MinMaxChange(menu_id, val) you'll write\n"
        "  if (menu_id == <this ID>) then ... end\n"
        "IDs must be unique across all menu items.");
    form->addRow("ID:", m_id);

    m_group = new QSpinBox(this);
    m_group->setRange(0, 99);
    m_group->setToolTip(
        "Group number — items with the same group are shown together on\n"
        "screen. Leave at 0 unless you have many items and want to organise\n"
        "them.");
    form->addRow("Group:", m_group);

    m_title = new QLineEdit(this);
    m_title->setToolTip(
        "Label shown on the LCD. Keep it short (≤ 14 chars).\n"
        "It also becomes the auto-generated Lua variable name in your\n"
        "script — \"Delay (ms)\" → variable _delay_ms.");
    form->addRow("Title:", m_title);

    outer->addLayout(form);

    m_stack = new QStackedWidget(this);

    // --- MIN_MAX page ---
    auto* minMaxPage = new QWidget(this);
    auto* mmForm = new QFormLayout(minMaxPage);
    m_min = new QSpinBox(this);
    m_min->setRange(-100000, 100000);
    m_min->setToolTip("Lowest value the user can dial in.");
    m_max = new QSpinBox(this);
    m_max->setRange(-100000, 100000);
    m_max->setToolTip("Highest value the user can dial in.\n"
        "Common ceilings:\n"
        "  Frequency:    250 Hz (above feels harsh)\n"
        "  Pulse width:  200 µs (above is intense)\n"
        "  Power:       1000   (the firmware caps here anyway)\n"
        "  Delay:      10000 ms (zc.DelayMs maximum)");
    m_step = new QSpinBox(this);
    m_step->setRange(1, 100000);
    m_step->setToolTip(
        "How much the value jumps each time the user turns the encoder.\n"
        "Typical: 1 for fine control, 10/50/100 for coarse sliders,\n"
        "         5 Hz for frequency, 10 µs for pulse width.");
    m_uom = new QLineEdit(this);
    m_uom->setPlaceholderText("e.g. ms, Hz, us, %");
    m_uom->setToolTip(
        "Unit of measure — appended to the value on the LCD.\n"
        "Examples: \"ms\" (delay), \"Hz\" (frequency), \"us\" (pulse width),\n"
        "          \"%\" (percentage), \"\" (dimensionless).");
    m_default = new QSpinBox(this);
    m_default->setRange(-100000, 100000);
    m_default->setToolTip(
        "Initial value when the pattern is loaded. MUST be inside [Min, Max].\n"
        "Tip for safety: pick a default that's MILD, not maximal — the user\n"
        "should choose to crank it up, not start there.");
    mmForm->addRow("Min:", m_min);
    mmForm->addRow("Max:", m_max);
    mmForm->addRow("Increment step:", m_step);
    mmForm->addRow("Unit (uom):", m_uom);
    mmForm->addRow("Default:", m_default);
    m_stack->addWidget(minMaxPage);

    // --- MULTI_CHOICE page ---
    auto* mcPage = new QWidget(this);
    auto* mcLayout = new QVBoxLayout(mcPage);
    mcLayout->addWidget(new QLabel("Choices:", this));
    m_choices = new QTableWidget(0, 2, this);
    m_choices->setHorizontalHeaderLabels({"Choice ID", "Description"});
    m_choices->horizontalHeader()->setStretchLastSection(true);
    m_choices->verticalHeader()->setVisible(false);
    mcLayout->addWidget(m_choices);
    auto* btnRow = new QHBoxLayout();
    auto* addBtn = new QPushButton("Add", this);
    auto* removeBtn = new QPushButton("Remove", this);
    btnRow->addWidget(addBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addStretch();
    mcLayout->addLayout(btnRow);
    m_stack->addWidget(mcPage);

    // --- AUDIO pages (empty) ---
    m_stack->addWidget(new QWidget(this));
    m_stack->addWidget(new QWidget(this));

    outer->addWidget(m_stack, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outer->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_type, qOverload<int>(&QComboBox::currentIndexChanged), this, &MenuItemDialog::onTypeChanged);
    connect(addBtn, &QPushButton::clicked, this, &MenuItemDialog::addChoice);
    connect(removeBtn, &QPushButton::clicked, this, &MenuItemDialog::removeChoice);

    onTypeChanged(0);
}

void MenuItemDialog::onTypeChanged(int index) {
    m_stack->setCurrentIndex(index);
}

void MenuItemDialog::addChoice() {
    int row = m_choices->rowCount();
    m_choices->insertRow(row);
    auto* idItem = new QTableWidgetItem(QString::number(row + 1));
    m_choices->setItem(row, 0, idItem);
    m_choices->setItem(row, 1, new QTableWidgetItem(""));
}

void MenuItemDialog::removeChoice() {
    int row = m_choices->currentRow();
    if (row >= 0) m_choices->removeRow(row);
}

void MenuItemDialog::setItem(const MenuItem& item) {
    m_type->setCurrentIndex(m_type->findData(static_cast<int>(item.type)));
    m_id->setValue(item.id);
    m_group->setValue(item.group);
    m_title->setText(item.title);
    m_min->setValue(item.min);
    m_max->setValue(item.max);
    m_step->setValue(item.incrementStep);
    m_uom->setText(item.uom);
    m_default->setValue(item.defaultValue);

    m_choices->setRowCount(0);
    for (const auto& c : item.choices) {
        int row = m_choices->rowCount();
        m_choices->insertRow(row);
        m_choices->setItem(row, 0, new QTableWidgetItem(QString::number(c.choiceId)));
        m_choices->setItem(row, 1, new QTableWidgetItem(c.description));
    }
}

MenuItem MenuItemDialog::item() const {
    MenuItem it;
    it.type = static_cast<MenuItemType>(m_type->currentData().toInt());
    it.id = m_id->value();
    it.group = m_group->value();
    it.title = m_title->text();
    it.min = m_min->value();
    it.max = m_max->value();
    it.incrementStep = m_step->value();
    it.uom = m_uom->text();
    it.defaultValue = m_default->value();

    for (int row = 0; row < m_choices->rowCount(); ++row) {
        MultiChoiceOption c;
        auto* idItem = m_choices->item(row, 0);
        auto* descItem = m_choices->item(row, 1);
        c.choiceId = idItem ? idItem->text().toInt() : (row + 1);
        c.description = descItem ? descItem->text() : QString();
        it.choices.push_back(c);
    }
    return it;
}
