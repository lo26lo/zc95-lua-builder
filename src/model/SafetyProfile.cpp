#include "SafetyProfile.h"

#include <QSettings>
#include <QCryptographicHash>

namespace {
// Salt baked into the binary. Doesn't add real cryptographic strength
// (the binary is publicly readable) but stops trivial dictionary
// attacks against the QSettings dump.
constexpr const char* kSalt = "zc95-safety-profile-v1";

bool g_initialised = false;
SafetyProfile g_profile;
}

SafetyProfile& SafetyProfile::current() {
    if (!g_initialised) {
        g_profile.load();
        g_initialised = true;
    }
    return g_profile;
}

void SafetyProfile::load() {
    QSettings s("zc95", "lua-builder");
    s.beginGroup("safety");
    maxPower            = s.value("maxPower", 1000).toInt();
    maxFrequencyHz      = s.value("maxFreqHz", 300).toInt();
    maxPulseWidthUs     = s.value("maxPulseWidthUs", 255).toInt();
    maxPulseDurationMs  = s.value("maxPulseDurationMs", 10000).toInt();
    locked              = s.value("locked", false).toBool();
    pinHash             = s.value("pinHash").toString();
    s.endGroup();

    // Sanity bounds.
    if (maxPower < 0)             maxPower = 0;
    if (maxPower > 1000)          maxPower = 1000;
    if (maxFrequencyHz < 1)       maxFrequencyHz = 1;
    if (maxFrequencyHz > 300)     maxFrequencyHz = 300;
    if (maxPulseWidthUs < 0)      maxPulseWidthUs = 0;
    if (maxPulseWidthUs > 255)    maxPulseWidthUs = 255;
    if (maxPulseDurationMs < 0)   maxPulseDurationMs = 0;
    if (maxPulseDurationMs > 10000) maxPulseDurationMs = 10000;
}

void SafetyProfile::save() const {
    QSettings s("zc95", "lua-builder");
    s.beginGroup("safety");
    s.setValue("maxPower",           maxPower);
    s.setValue("maxFreqHz",          maxFrequencyHz);
    s.setValue("maxPulseWidthUs",    maxPulseWidthUs);
    s.setValue("maxPulseDurationMs", maxPulseDurationMs);
    s.setValue("locked",             locked);
    s.setValue("pinHash",            pinHash);
    s.endGroup();
}

QString SafetyProfile::hashPin(const QString& pin) {
    QByteArray data = pin.toUtf8();
    data.append(kSalt);
    return QString::fromUtf8(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool SafetyProfile::verifyPin(const QString& pin) const {
    if (pinHash.isEmpty()) return true;   // no PIN set — verify trivially
    return hashPin(pin) == pinHash;
}

void SafetyProfile::setPin(const QString& pin) {
    pinHash = pin.isEmpty() ? QString() : hashPin(pin);
}

bool SafetyProfile::isActive() const {
    return locked
        || maxPower < 1000
        || maxFrequencyHz < 300
        || maxPulseWidthUs < 255
        || maxPulseDurationMs < 10000;
}

int SafetyProfile::clampPower(int v) const          { return qBound(0, v, maxPower); }
int SafetyProfile::clampFrequency(int v) const      { return qBound(1, v, maxFrequencyHz); }
int SafetyProfile::clampPulseWidth(int v) const     { return qBound(0, v, maxPulseWidthUs); }
int SafetyProfile::clampPulseDuration(int v) const  { return qBound(0, v, maxPulseDurationMs); }
