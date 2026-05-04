#pragma once

#include <QString>

// Process-wide safety caps for the simulator and the linter. The user
// can lock the profile with a PIN to prevent casual changes (e.g. when
// lending the device). When locked:
//   • the simulator clamps every zc.SetPower / SetFrequency / SetPulseWidth
//     / ChannelPulseMs argument to the configured maximum,
//   • the linter promotes the matching safety warnings to ERRORS,
//   • the View → Beginner mode toggle is forced ON and read-only,
//   • the unlock requires re-entering the PIN.
//
// PINs are stored as a salted SHA-256 hash in QSettings. This is meant
// to keep an honest user honest, not to resist a hostile attacker —
// the binary itself is readable.
class SafetyProfile {
public:
    int maxPower         = 1000;     // 0..1000
    int maxFrequencyHz   = 300;      // 0..300
    int maxPulseWidthUs  = 255;      // 0..255
    int maxPulseDurationMs = 10000;  // 0..10000 — caps zc.ChannelPulseMs

    bool    locked         = false;
    QString pinHash;                 // hex-encoded SHA-256(pin || kSalt)

    // Singleton: one profile per process. Loaded from QSettings on
    // first access. Mutations persist immediately.
    static SafetyProfile& current();

    // Persistence.
    void load();
    void save() const;

    // PIN helpers.
    static QString hashPin(const QString& pin);
    bool verifyPin(const QString& pin) const;
    void setPin(const QString& pin);     // does NOT call save()

    // True iff *any* cap is below the device-physical maximum, OR if
    // locked. Used to decide whether to show the status-bar badge.
    bool isActive() const;

    // Convenience — clamp a value, returning the new (possibly capped)
    // value. The simulator and linter use these.
    int clampPower(int v) const;
    int clampFrequency(int v) const;
    int clampPulseWidth(int v) const;
    int clampPulseDuration(int v) const;
};
