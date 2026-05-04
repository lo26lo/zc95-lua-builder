#include "SafetyProfileDialog.h"
#include "../model/SafetyProfile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialogButtonBox>

SafetyProfileDialog::SafetyProfileDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Safety profile");
    resize(560, 520);

    auto* outer = new QVBoxLayout(this);

    auto* hint = new QLabel(
        "<p>Caps applied at the simulator level — any <code>zc.SetPower</code> / "
        "<code>SetFrequency</code> / <code>SetPulseWidth</code> / "
        "<code>ChannelPulseMs</code> argument that exceeds the cap is silently "
        "clamped, with a <code>[SAFETY]</code> log line. The linter also "
        "promotes the matching warnings to errors when locked.</p>"
        "<p style='color:#888;'>The PIN is hashed (SHA-256 with a static salt) "
        "and stored in QSettings. It exists to prevent casual changes by "
        "someone borrowing your device — not to resist a determined attacker.</p>",
        this);
    hint->setWordWrap(true);
    outer->addWidget(hint);

    // Caps group
    auto* capsBox = new QGroupBox("Maximum allowed values", this);
    auto* capsForm = new QFormLayout(capsBox);

    m_powerSpin = new QSpinBox(capsBox);
    m_powerSpin->setRange(0, 1000);
    m_powerSpin->setSingleStep(50);
    m_powerSpin->setSuffix(" / 1000");
    m_powerSpin->setToolTip(
        "Cap on zc.SetPower. The device's hardware max is 1000.\n"
        "Recommended starting cap for first-time users: 400-500.");
    capsForm->addRow("Power:", m_powerSpin);

    m_freqSpin = new QSpinBox(capsBox);
    m_freqSpin->setRange(1, 300);
    m_freqSpin->setSingleStep(10);
    m_freqSpin->setSuffix(" Hz");
    m_freqSpin->setToolTip(
        "Cap on zc.SetFrequency. Hardware max is 300 Hz.\n"
        "Beyond 250 Hz feels harsh — 200 Hz is a sane safety ceiling.");
    capsForm->addRow("Frequency:", m_freqSpin);

    m_widthSpin = new QSpinBox(capsBox);
    m_widthSpin->setRange(0, 255);
    m_widthSpin->setSingleStep(10);
    m_widthSpin->setSuffix(" µs");
    m_widthSpin->setToolTip(
        "Cap on zc.SetPulseWidth (per phase). Hardware max is 255 µs.\n"
        "Above 200 µs delivers a lot of charge — 180 µs is conservative.");
    capsForm->addRow("Pulse width:", m_widthSpin);

    m_durSpin = new QSpinBox(capsBox);
    m_durSpin->setRange(0, 10000);
    m_durSpin->setSingleStep(100);
    m_durSpin->setSuffix(" ms");
    m_durSpin->setToolTip(
        "Cap on zc.ChannelPulseMs duration. Hardware max is 10000 ms.\n"
        "Most patterns use < 500 ms.");
    capsForm->addRow("Pulse duration:", m_durSpin);

    outer->addWidget(capsBox);

    // Lock group
    auto* lockBox = new QGroupBox("Lock", this);
    auto* lockLay = new QVBoxLayout(lockBox);

    m_lockCheck = new QCheckBox("Lock with a PIN (forces beginner mode, "
                                "promotes safety warnings to errors)", lockBox);
    lockLay->addWidget(m_lockCheck);

    auto* pinForm = new QFormLayout();
    m_pinLabel = new QLabel("Set a 4-8 digit PIN:", lockBox);
    lockLay->addWidget(m_pinLabel);
    m_pin1Edit = new QLineEdit(lockBox);
    m_pin1Edit->setEchoMode(QLineEdit::Password);
    m_pin1Edit->setPlaceholderText("PIN");
    m_pin1Edit->setMaxLength(16);
    pinForm->addRow("Choose PIN:", m_pin1Edit);
    m_pin2Edit = new QLineEdit(lockBox);
    m_pin2Edit->setEchoMode(QLineEdit::Password);
    m_pin2Edit->setPlaceholderText("Confirm");
    m_pin2Edit->setMaxLength(16);
    pinForm->addRow("Confirm PIN:", m_pin2Edit);
    lockLay->addLayout(pinForm);

    auto* unlockRow = new QHBoxLayout();
    m_status = new QLabel(lockBox);
    m_status->setWordWrap(true);
    unlockRow->addWidget(m_status, 1);
    m_unlockBtn = new QPushButton("Unlock to edit…", lockBox);
    unlockRow->addWidget(m_unlockBtn);
    lockLay->addLayout(unlockRow);

    outer->addWidget(lockBox);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outer->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_lockCheck, &QCheckBox::toggled, this, &SafetyProfileDialog::onLockToggled);
    connect(m_unlockBtn, &QPushButton::clicked, this, &SafetyProfileDialog::onUnlockClicked);

    // Load current values.
    const auto& sp = SafetyProfile::current();
    m_powerSpin->setValue(sp.maxPower);
    m_freqSpin->setValue(sp.maxFrequencyHz);
    m_widthSpin->setValue(sp.maxPulseWidthUs);
    m_durSpin->setValue(sp.maxPulseDurationMs);
    m_lockCheck->blockSignals(true);
    m_lockCheck->setChecked(sp.locked);
    m_lockCheck->blockSignals(false);

    // If currently locked, the user has to unlock before editing anything.
    m_unlocked = !sp.locked;
    refreshLockState();
}

void SafetyProfileDialog::refreshLockState() {
    bool editable = m_unlocked;
    m_powerSpin->setEnabled(editable);
    m_freqSpin->setEnabled(editable);
    m_widthSpin->setEnabled(editable);
    m_durSpin->setEnabled(editable);
    m_lockCheck->setEnabled(editable);

    bool wantLock = m_lockCheck->isChecked();
    m_pinLabel->setVisible(editable && wantLock && !SafetyProfile::current().locked);
    m_pin1Edit->setVisible(editable && wantLock && !SafetyProfile::current().locked);
    m_pin2Edit->setVisible(editable && wantLock && !SafetyProfile::current().locked);
    m_unlockBtn->setVisible(SafetyProfile::current().locked && !m_unlocked);

    if (SafetyProfile::current().locked && !m_unlocked) {
        m_status->setText("<b style='color:#ffb86b;'>🔒 Profile is currently locked.</b> "
                          "Click <b>Unlock to edit…</b> and enter the PIN to change anything.");
    } else if (m_lockCheck->isChecked() && !SafetyProfile::current().locked) {
        m_status->setText("<i style='color:#9ce29c;'>Will lock on OK — "
                          "set a PIN above.</i>");
    } else if (!m_lockCheck->isChecked() && SafetyProfile::current().locked) {
        m_status->setText("<i style='color:#ffd166;'>Will unlock on OK.</i>");
    } else {
        m_status->setText("");
    }
}

void SafetyProfileDialog::onLockToggled(bool) {
    refreshLockState();
}

void SafetyProfileDialog::onUnlockClicked() {
    if (tryUnlock()) {
        m_unlocked = true;
        refreshLockState();
    }
}

bool SafetyProfileDialog::tryUnlock() {
    bool ok = false;
    QString pin = QInputDialog::getText(this, "Unlock safety profile",
        "Enter PIN:", QLineEdit::Password, QString(), &ok);
    if (!ok) return false;
    if (!SafetyProfile::current().verifyPin(pin)) {
        QMessageBox::warning(this, "Wrong PIN",
            "That PIN doesn't match. The profile remains locked.");
        return false;
    }
    return true;
}

void SafetyProfileDialog::accept() {
    auto& sp = SafetyProfile::current();

    if (sp.locked && !m_unlocked) {
        // Should not happen — controls are disabled. Defensive.
        QDialog::reject();
        return;
    }

    bool wasLocked = sp.locked;
    bool wantLock  = m_lockCheck->isChecked();

    // Validate PIN entry only if transitioning unlocked → locked.
    if (wantLock && !wasLocked) {
        QString p1 = m_pin1Edit->text();
        QString p2 = m_pin2Edit->text();
        if (p1.length() < 4) {
            QMessageBox::warning(this, "PIN too short",
                "Choose a PIN of at least 4 characters.");
            return;
        }
        if (p1 != p2) {
            QMessageBox::warning(this, "PINs don't match",
                "The two PIN fields differ. Type the same PIN twice.");
            return;
        }
        sp.setPin(p1);
    }
    if (!wantLock && wasLocked) {
        // Unlocking — wipe the PIN.
        sp.setPin(QString());
    }

    sp.maxPower            = m_powerSpin->value();
    sp.maxFrequencyHz      = m_freqSpin->value();
    sp.maxPulseWidthUs     = m_widthSpin->value();
    sp.maxPulseDurationMs  = m_durSpin->value();
    sp.locked              = wantLock;
    sp.save();

    QDialog::accept();
}
