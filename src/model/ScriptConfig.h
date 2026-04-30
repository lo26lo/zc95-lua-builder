#pragma once

#include "MenuItem.h"
#include <QString>
#include <QVector>

enum class AudioMode {
    Off,
    AudioIntensity
};

struct EnabledFunctions {
    bool setup = false;
    bool loop = true;
    bool minMaxChange = false;
    bool multiChoiceChange = false;
    bool softButton = false;
    bool externalTrigger = false;
    bool bluetoothRemoteKeypress = false;
    bool bluetoothHidEvent = false;
    bool audioIntensityChange = false;
};

struct ScriptConfig {
    QString name = "MyPattern";
    AudioMode audioMode = AudioMode::Off;
    QString softButtonLabel;        // empty => no soft_button entry
    int loopFreqHz = 0;              // 0 => default (omitted)
    bool allowTriphase = false;
    bool bluetoothRemotePassthrough = false;

    QVector<MenuItem> menuItems;
    EnabledFunctions functions;

    static QString audioModeToString(AudioMode m);
};
