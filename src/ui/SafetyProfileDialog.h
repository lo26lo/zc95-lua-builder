#pragma once

#include <QDialog>

class QSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;

// Dialog for editing the process-wide SafetyProfile. Lets the user:
//   • Set caps for power / frequency / pulse width / pulse duration
//   • Lock the profile with a PIN (so they're prompted to unlock if
//     they ever try to raise the caps again)
//   • Unlock with the same PIN to make further changes
//
// Persistence is automatic — Apply writes to QSettings before closing.
class SafetyProfileDialog : public QDialog {
    Q_OBJECT
public:
    explicit SafetyProfileDialog(QWidget* parent = nullptr);

protected:
    void accept() override;

private slots:
    void onLockToggled(bool b);
    void onUnlockClicked();

private:
    void refreshLockState();
    bool tryUnlock();           // prompts for PIN if locked

    QSpinBox*    m_powerSpin = nullptr;
    QSpinBox*    m_freqSpin  = nullptr;
    QSpinBox*    m_widthSpin = nullptr;
    QSpinBox*    m_durSpin   = nullptr;

    QCheckBox*   m_lockCheck = nullptr;
    QLineEdit*   m_pin1Edit  = nullptr;
    QLineEdit*   m_pin2Edit  = nullptr;
    QLabel*      m_pinLabel  = nullptr;
    QPushButton* m_unlockBtn = nullptr;
    QLabel*      m_status    = nullptr;

    bool m_unlocked = true;     // false until verifyPin succeeds (only matters if profile was already locked)
};
