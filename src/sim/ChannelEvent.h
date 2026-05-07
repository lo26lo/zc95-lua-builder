#pragma once

#include <QString>
#include <QVector>

enum class ChannelEventType {
    ChannelOn,
    ChannelOff,
    ChannelPulseMs,   // duration in param1 ms
    SetPower,         // power 0-1000 in param1
    SetFrequency,     // hz in param1
    SetPulseWidth,    // pos us in param1, neg us in param2
    EnableTriphase,   // enabled in param1 (0/1)
    LinkChannels,     // lead in channel field, linked in param1, offset% in param2
    AccIoWrite,       // line in channel field, state in param1 (0/1)
    Print,            // text in textParam
    DelayMs,          // ms in param1
    UserInput,        // user touched a control: short label in textParam,
                      // category code in param1 (0=menu, 1=soft button,
                      // 2=external trigger, 3=reset/setup marker).
};

struct ChannelEvent {
    double timeMs = 0;
    ChannelEventType type;
    int channel = 0;        // 1-4 for channel events; for AccIoWrite/LinkChannels: see comments above
    int param1 = 0;
    int param2 = 0;
    QString textParam;      // for Print
};

struct ChannelState {
    bool on = false;
    int power = 1000;       // default if no Setup()
    int frequencyHz = 150;
    int pulseWidthPosUs = 150;
    int pulseWidthNegUs = 150;
    double pulseEndsAtMs = -1; // for ChannelPulseMs auto-off
};

struct SimState {
    ChannelState channels[4];
    bool triphaseEnabled = false;
    bool accIo[3] = { true, true, true };  // default HIGH per docs
};
