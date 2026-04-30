#include "SnippetsPanel.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>

namespace {
struct Snippet {
    QString label;
    QString tip;
    QString code;
};

struct SnippetGroup {
    QString title;
    QVector<Snippet> items;
};

const QVector<SnippetGroup>& snippetGroups() {
    static const QVector<SnippetGroup> groups = {
        {"zc.* API", {
            {"ChannelOn", "Switch a channel on until ChannelOff is called",
             "zc.ChannelOn(1)"},
            {"ChannelOff", "Switch a channel off",
             "zc.ChannelOff(1)"},
            {"ChannelPulseMs", "Pulse a channel on for N ms",
             "zc.ChannelPulseMs(1, 100)"},
            {"SetPower", "Set power 0-1000 (scaled by front-panel dial)",
             "zc.SetPower(1, 1000)"},
            {"SetFrequency", "Set output frequency 1-300 Hz",
             "zc.SetFrequency(1, 150)"},
            {"SetPulseWidth", "Pos & neg pulse width 0-255 us (usually equal)",
             "zc.SetPulseWidth(1, 150, 150)"},
            {"SetMenuOption", "Programmatically change a menu option (async)",
             "zc.SetMenuOption(1, 500)"},
            {"DelayMs", "Sleep for N ms (callbacks still fire during)",
             "zc.DelayMs(100)"},
            {"EnableTriphase", "Enables triphase mode (requires allow_triphase = true)",
             "zc.EnableTriphase(true)"},
            {"LinkChannels", "Link linked-channel to lead-channel with offset 0-100%",
             "zc.LinkChannels(1, 2, 0)"},
            {"AccIoWrite", "Set accessory I/O line (1-3) high/low",
             "zc.AccIoWrite(1, true)"},
            {"print (debug)", "Print to serial / debug window",
             "print(\"debug\")"},
        }},
        {"Patterns idiomatiques", {
            {"Init 4 channels", "Set up all 4 channels with default power.",
             "for chan = 1, 4, 1\n"
             "do\n"
             "    zc.ChannelOn(chan)\n"
             "    zc.SetPower(chan, 1000)\n"
             "    zc.SetFrequency(chan, 50)\n"
             "    zc.SetPulseWidth(chan, 150, 150)\n"
             "end\n"},
            {"Toggle every N ms", "Run a block every _delay_ms in Loop().",
             "_last_toggle_ms = _last_toggle_ms or 0\n"
             "if (time_ms > _last_toggle_ms + _delay_ms)\n"
             "then\n"
             "    -- toggle / step here\n"
             "    _last_toggle_ms = time_ms\n"
             "end\n"},
            {"244 Hz tick", "Run a function at ~244 Hz inside Loop() (Loop runs ~600/s).",
             "_last_244 = _last_244 or 0\n"
             "local hz244 = math.floor(time_ms / (1000/244))\n"
             "if hz244 ~= _last_244 then\n"
             "    _last_244 = hz244\n"
             "    -- per-tick code here\n"
             "end\n"},
            {"Triangle wave modulator",
             "Smoothly modulate a value between [low, high] over `period` ms.",
             "local low = 60   -- lower bound\n"
             "local high = 100 -- upper bound\n"
             "local period = 5000\n"
             "local span = high - low\n"
             "local v = ((span / math.pi) * math.asin(math.sin((2 * math.pi / period) * time_ms))) + (span / 2) + low\n"
             "-- v now triangle-waves between low..high\n"},
            {"Sine wave modulator",
             "Smoothly modulate a value between [low, high] using a sine.",
             "local low = 60\n"
             "local high = 100\n"
             "local period = 5000\n"
             "local mid = (low + high) / 2\n"
             "local amp = (high - low) / 2\n"
             "local v = mid + amp * math.sin((2 * math.pi / period) * time_ms)\n"},
            {"Mode/MenuId enum tables",
             "Named constants for menu items + multi-choice options.",
             "Mode = { OFF = 1, BURST = 2, CONSTANT = 3 }\n"
             "MenuId = { MODE = 1, FREQ = 2 }\n"},
            {"Triphase fade cycle",
             "Cycle the triphase offset 0-100-0 over a configurable duration.",
             "function OffsetCycle()\n"
             "    local offset = 0\n"
             "    if _progress_percent < 50 then\n"
             "        offset = 100 - (_progress_percent * 2)\n"
             "    else\n"
             "        offset = (_progress_percent - 50) * 2\n"
             "    end\n"
             "    zc.LinkChannels(1, 2, offset)\n"
             "end\n"
             "\n"
             "function UpdateStepProgress(time_ms)\n"
             "    _progress_percent = ((time_ms - _step_start_time_ms) / _duration_ms) * 100\n"
             "    if _progress_percent > 100 then\n"
             "        _progress_percent = 0\n"
             "        _step_start_time_ms = time_ms\n"
             "    end\n"
             "end\n"},
            {"Burst loop",
             "Trigger a short pulse on all 4 channels at burst frequency.",
             "_burst_next_ms = _burst_next_ms or 0\n"
             "if (_burst_next_ms == 0) then\n"
             "    _burst_next_ms = time_ms + ((1 / _burst_freq_hz) * 1000)\n"
             "elseif (time_ms > _burst_next_ms) then\n"
             "    zc.ChannelPulseMs(1, _burst_duration_ms)\n"
             "    zc.ChannelPulseMs(2, _burst_duration_ms)\n"
             "    zc.ChannelPulseMs(3, _burst_duration_ms)\n"
             "    zc.ChannelPulseMs(4, _burst_duration_ms)\n"
             "    _burst_next_ms = _burst_next_ms + ((1 / _burst_freq_hz) * 1000)\n"
             "end\n"},
            {"SetFreq helper",
             "Apply a frequency to all 4 channels at once.",
             "function SetFreq(freq_hz)\n"
             "    zc.SetFrequency(1, freq_hz)\n"
             "    zc.SetFrequency(2, freq_hz)\n"
             "    zc.SetFrequency(3, freq_hz)\n"
             "    zc.SetFrequency(4, freq_hz)\n"
             "end\n"},
            {"SetWidth helper (mono/bi)",
             "Apply pulse width to all 4 channels, supporting mono/bi mode.",
             "function SetWidth(pulse_width_us)\n"
             "    local neg_wid = (_pulse_type == PulseType.MONO) and 1 or pulse_width_us\n"
             "    zc.SetPulseWidth(1, pulse_width_us, neg_wid)\n"
             "    zc.SetPulseWidth(2, pulse_width_us, neg_wid)\n"
             "    zc.SetPulseWidth(3, pulse_width_us, neg_wid)\n"
             "    zc.SetPulseWidth(4, pulse_width_us, neg_wid)\n"
             "end\n"},
        }},
    };
    return groups;
}
}

SnippetsPanel::SnippetsPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel("Click to insert at cursor.", this));

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* container = new QWidget(scroll);
    auto* listLayout = new QVBoxLayout(container);
    listLayout->setContentsMargins(0, 0, 0, 0);

    for (const auto& g : snippetGroups()) {
        auto* group = new QGroupBox(g.title, container);
        auto* gLayout = new QVBoxLayout(group);
        for (const auto& s : g.items) {
            auto* btn = new QPushButton(s.label, group);
            btn->setToolTip(s.tip + "\n\n" + s.code);
            btn->setStyleSheet("text-align: left; padding: 6px;");
            connect(btn, &QPushButton::clicked, this, [this, code = s.code]() {
                emit snippetRequested(code + "\n");
            });
            gLayout->addWidget(btn);
        }
        listLayout->addWidget(group);
    }
    listLayout->addStretch();
    scroll->setWidget(container);

    outer->addWidget(scroll);
}
