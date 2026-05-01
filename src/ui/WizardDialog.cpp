#include "WizardDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QFrame>

// Common HTML stylesheet for info panels.
static const char* kInfoStyle =
    "QLabel {"
    "  background: #1e2630;"
    "  color: #e0e6ed;"
    "  border: 1px solid #2d3a4a;"
    "  border-radius: 4px;"
    "  padding: 14px;"
    "  font-size: 10pt;"
    "}";

// Standard section header used inside info panels.
static QString infoSection(const QString& emoji, const QString& title, const QString& body) {
    return QString("<p style='margin: 0 0 4px 0;'><b style='color:#9bd1ff;'>%1 %2</b></p>"
                   "<p style='margin: 0 0 12px 0; color:#cbd5e0;'>%3</p>")
        .arg(emoji, title, body);
}
static QString infoWarning(const QString& body) {
    return QString("<p style='margin: 0 0 4px 0;'><b style='color:#ffb86b;'>⚠ Warning</b></p>"
                   "<p style='margin: 0 0 12px 0; color:#f5d8a8;'>%1</p>").arg(body);
}

WizardDialog::WizardDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("New pattern wizard");
    resize(980, 620);

    auto* outer = new QVBoxLayout(this);

    m_pageHeader = new QLabel(this);
    m_pageHeader->setStyleSheet("font-size: 14pt; font-weight: bold; padding: 8px;");
    outer->addWidget(m_pageHeader);

    m_stack = new QStackedWidget(this);
    outer->addWidget(m_stack, 1);

    buildTypePage();
    buildIntensityPage();
    buildCyclePage();
    buildChannelsPage();
    buildKillSwitchPage();
    buildSummaryPage();

    auto* navRow = new QHBoxLayout();
    m_prevBtn   = new QPushButton("◀ Back", this);
    m_nextBtn   = new QPushButton("Next ▶", this);
    m_finishBtn = new QPushButton("✓ Generate", this);
    auto* cancel = new QPushButton("Cancel", this);
    navRow->addWidget(cancel);
    navRow->addStretch();
    navRow->addWidget(m_prevBtn);
    navRow->addWidget(m_nextBtn);
    navRow->addWidget(m_finishBtn);
    outer->addLayout(navRow);

    connect(m_prevBtn,   &QPushButton::clicked, this, &WizardDialog::prev);
    connect(m_nextBtn,   &QPushButton::clicked, this, &WizardDialog::next);
    connect(m_finishBtn, &QPushButton::clicked, this, [this]() { produceResult(); accept(); });
    connect(cancel,      &QPushButton::clicked, this, &QDialog::reject);

    m_stack->setCurrentIndex(PType);
    updateNav();
}

QWidget* WizardDialog::buildSplitPage(QWidget* leftControls, QLabel*& outInfo) {
    auto* page = new QWidget(m_stack);
    auto* h = new QHBoxLayout(page);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(12);

    h->addWidget(leftControls, 55);

    outInfo = new QLabel(page);
    outInfo->setWordWrap(true);
    outInfo->setTextFormat(Qt::RichText);
    outInfo->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    outInfo->setStyleSheet(kInfoStyle);
    outInfo->setMinimumWidth(280);
    h->addWidget(outInfo, 45);

    return page;
}

void WizardDialog::buildTypePage() {
    auto* left = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(left);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(new QLabel(
        "<p>What kind of pattern do you want?</p>"
        "<p style='color:#888;'>Each option generates a starting script "
        "you can refine later. You can always come back and pick another.</p>", left));

    m_typeGroup = new QButtonGroup(left);

    auto add = [&](Type t, const QString& title, const QString& desc, bool checked = false) {
        auto* rb = new QRadioButton(title, left);
        rb->setStyleSheet("font-weight: bold;");
        rb->setChecked(checked);
        m_typeGroup->addButton(rb, t);
        lay->addWidget(rb);
        auto* d = new QLabel(desc, left);
        d->setWordWrap(true);
        d->setStyleSheet("color:#aaa; padding-left: 22px; padding-bottom: 8px;");
        lay->addWidget(d);
    };

    add(TPulse, "Pulse  (recommended for beginners)",
        "Short bursts on each channel at a regular interval. Easy to "
        "understand and feel — you adjust speed with a slider.", true);
    add(TConstant, "Constant",
        "Steady output at fixed frequency / pulse width. The simplest "
        "pattern of all — like a basic TENS unit.");
    add(TFade, "Fade in/out",
        "Gradually ramps power up and back down over the cycle duration. "
        "Good for warm-up.");
    add(TBurst, "Burst",
        "Several rapid pulses, then a pause, then again. Feels rhythmic.");
    add(TTens, "TENS-like",
        "Higher frequency, narrow pulse width — gentler tingling rather "
        "than thumping.");

    lay->addStretch();

    connect(m_typeGroup, &QButtonGroup::idToggled,
            this, [this](int, bool) { updateTypeInfo(); });

    m_stack->addWidget(buildSplitPage(left, m_typeInfo));
    updateTypeInfo();
}

void WizardDialog::updateTypeInfo() {
    Type t = (m_typeGroup && m_typeGroup->checkedId() >= 0)
                 ? Type(m_typeGroup->checkedId()) : TPulse;
    QString html;
    switch (t) {
    case TPulse:
        html += infoSection("🎯", "What it does",
            "Fires short on/off pulses at a regular interval (controlled by "
            "the <i>Speed</i> slider in the generated script).");
        html += infoSection("🎨", "Feeling",
            "Rhythmic taps. Each pulse is brief — you feel a pop, then nothing, "
            "then another pop. Speed slider controls how fast.");
        html += infoSection("💡", "Recommendation",
            "Best starting choice for newcomers. Easy to dial in: turn the "
            "front-panel knob slowly until you feel it, then adjust speed.");
        break;
    case TConstant:
        html += infoSection("🎯", "What it does",
            "Drives every selected channel ON continuously at a fixed "
            "frequency and pulse width. Once it's on, it stays on.");
        html += infoSection("🎨", "Feeling",
            "Steady vibration / buzz. Like a basic TENS unit at a clinic.");
        html += infoSection("💡", "Recommendation",
            "Pick this if you want maximum simplicity and predictability. "
            "Good for muscle stimulation or extended low-intensity sessions.");
        html += infoWarning(
            "Constant output at high power can cause electrode-site fatigue "
            "(skin irritation). Take breaks every 20-30 minutes.");
        break;
    case TFade:
        html += infoSection("🎯", "What it does",
            "Smoothly ramps power up to maximum and back down to zero over "
            "one cycle, using a sine wave. Repeats indefinitely.");
        html += infoSection("🎨", "Feeling",
            "A wave that builds, peaks, and recedes. Very gentle entry and "
            "exit — you barely notice it starting.");
        html += infoSection("💡", "Recommendation",
            "Excellent for warm-up sessions or for users who don't like "
            "sudden onsets. Set cycle duration to 10-30 s for slow fades.");
        break;
    case TBurst:
        html += infoSection("🎯", "What it does",
            "Groups of 4 quick pulses on every channel, then a pause, then "
            "another group. Period controlled by the <i>Speed</i> slider.");
        html += infoSection("🎨", "Feeling",
            "Rapid-fire double-tap-tap-tap, then silence, repeat. More "
            "intense than a single Pulse — your nerves perceive the group.");
        html += infoSection("💡", "Recommendation",
            "Skip on first session. Try Pulse first to learn what intensity "
            "you tolerate, then graduate to Burst.");
        html += infoWarning(
            "Burst patterns concentrate charge in a short window and can "
            "feel surprisingly strong. Always start the dial at zero.");
        break;
    case TTens:
        html += infoSection("🎯", "What it does",
            "High-frequency continuous output (150 Hz) with narrow pulse "
            "width (80 µs) — the classic TENS-style waveform.");
        html += infoSection("🎨", "Feeling",
            "A fine tingling, almost prickling sensation. Less thumpy than "
            "low-frequency patterns; more about surface stimulation.");
        html += infoSection("💡", "Recommendation",
            "Good for users who find low-frequency rumble uncomfortable, "
            "or for muscle relaxation / nerve stimulation contexts.");
        break;
    }
    m_typeInfo->setText(html);
}

void WizardDialog::buildIntensityPage() {
    auto* left = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(left);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(new QLabel(
        "<p>Maximum intensity ceiling for this pattern</p>"
        "<p style='color:#888;'>Sets the <b>cap</b>. The actual output is "
        "still scaled by the front-panel dial. Picking <b>Gentle</b> means "
        "even at the dial's maximum the pattern stays mild.</p>", left));

    m_intGroup = new QButtonGroup(left);

    auto add = [&](Intensity i, const QString& title, const QString& desc, bool checked = false) {
        auto* rb = new QRadioButton(title, left);
        rb->setStyleSheet("font-weight: bold;");
        rb->setChecked(checked);
        m_intGroup->addButton(rb, i);
        lay->addWidget(rb);
        auto* d = new QLabel(desc, left);
        d->setWordWrap(true);
        d->setStyleSheet("color:#aaa; padding-left: 22px; padding-bottom: 8px;");
        lay->addWidget(d);
    };

    add(IGentle, "Gentle  (max power 400 / 1000)",
        "Recommended for first sessions. Mild buzz / tingling.", true);
    add(IMedium, "Medium  (max power 700 / 1000)",
        "Noticeable, comfortable. Good general-purpose ceiling.");
    add(IStrong, "Strong  (max power 1000 / 1000)",
        "Full power available. Only pick this if you know what you're "
        "doing AND you'll start the session with the dial at zero.");

    lay->addStretch();

    connect(m_intGroup, &QButtonGroup::idToggled,
            this, [this](int, bool) { updateIntensityInfo(); });

    m_stack->addWidget(buildSplitPage(left, m_intensityInfo));
    updateIntensityInfo();
}

void WizardDialog::updateIntensityInfo() {
    Intensity i = (m_intGroup && m_intGroup->checkedId() >= 0)
                      ? Intensity(m_intGroup->checkedId()) : IGentle;
    QString html;
    switch (i) {
    case IGentle:
        html += infoSection("🎯", "What it does",
            "Caps the script's <code>SetPower</code> at 400/1000. Even with "
            "the front-panel dial maxed, the output stays moderate.");
        html += infoSection("🎨", "Feeling",
            "Pleasant buzz, never overwhelming. You feel it clearly but "
            "always in control.");
        html += infoSection("💡", "Recommendation",
            "<b>Always pick this for your first session.</b> You can change "
            "it later — but a gentle ceiling means you can't hurt yourself "
            "by accidentally cranking the dial.");
        break;
    case IMedium:
        html += infoSection("🎯", "What it does",
            "Caps power at 700/1000. About 70% of the device's electrical "
            "output capability.");
        html += infoSection("🎨", "Feeling",
            "Noticeable but comfortable. Most experienced users settle here "
            "for their day-to-day patterns.");
        html += infoSection("💡", "Recommendation",
            "Pick this once you know your tolerance from at least 2-3 "
            "Gentle sessions. Good general-purpose ceiling.");
        break;
    case IStrong:
        html += infoSection("🎯", "What it does",
            "No software cap — full 1000/1000 available. The front-panel "
            "dial is your ONLY moderation.");
        html += infoSection("🎨", "Feeling",
            "Strong sensations possible. Some pattern types (Burst, narrow "
            "pulse widths) at full power can be uncomfortable.");
        html += infoSection("💡", "Recommendation",
            "Only pick this if you've used the device for a while AND you "
            "always start the dial at zero. Best paired with a Gentle "
            "<i>Speed</i> setting on Burst patterns.");
        html += infoWarning(
            "Combining Strong intensity + Burst type + narrow pulse width "
            "(&lt; 100 µs) creates the most intense ZC95 outputs. Slow "
            "ramp-up is essential.");
        break;
    }
    m_intensityInfo->setText(html);
}

void WizardDialog::buildCyclePage() {
    auto* left = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(left);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(new QLabel(
        "<p>How long should one cycle of the pattern last?</p>"
        "<p style='color:#888;'>For a Fade: time from low to high to low again.<br>"
        "For a Burst: time between burst groups.<br>"
        "For a Pulse: time between individual pulses.</p>", left));

    auto* h = new QHBoxLayout();
    h->addWidget(new QLabel("Cycle duration (seconds):", left));
    m_cycleSec = new QSpinBox(left);
    m_cycleSec->setRange(1, 60);
    m_cycleSec->setValue(5);
    m_cycleSec->setSingleStep(1);
    m_cycleSec->setSuffix(" s");
    h->addWidget(m_cycleSec);
    h->addStretch();
    lay->addLayout(h);

    auto* hint = new QLabel(
        "<p style='color:#aaa;'>Typical values:<br>"
        "&nbsp;&nbsp;1-3 s — fast, energetic<br>"
        "&nbsp;&nbsp;5-10 s — comfortable, what most patterns use<br>"
        "&nbsp;&nbsp;20-60 s — slow build-up</p>", left);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    lay->addStretch();

    connect(m_cycleSec, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int) { updateCycleInfo(); });

    m_stack->addWidget(buildSplitPage(left, m_cycleInfo));
    updateCycleInfo();
}

void WizardDialog::updateCycleInfo() {
    int s = m_cycleSec ? m_cycleSec->value() : 5;
    QString html;
    QString feel, reco;
    if (s <= 3) {
        feel = QString("Very rapid. Pulses or fades happen %1 time(s) per second-ish.").arg(s == 1 ? "more than 1" : QString::number(s));
        reco = "Energetic, sharp. Combined with Burst type this becomes very intense — better with Gentle intensity.";
    } else if (s <= 10) {
        feel = "Comfortable rhythm. You can clearly perceive each cycle but it's not exhausting.";
        reco = "Sweet spot for most patterns. <b>Recommended for beginners.</b>";
    } else if (s <= 30) {
        feel = "Slow build-up. Each fade or burst feels deliberate, almost meditative.";
        reco = "Excellent for warm-up sessions or for users who like a long, gradual approach.";
    } else {
        feel = "Very slow. A single cycle takes nearly a minute. Useful only for Fade.";
        reco = "Niche choice — for extended Fade sessions where you want barely-perceptible changes.";
    }
    html += infoSection("⏱", "Currently",
        QString("<b style='color:#ffd400;'>%1 second%2</b> per cycle.").arg(s).arg(s > 1 ? "s" : ""));
    html += infoSection("🎨", "Feeling", feel);
    html += infoSection("💡", "Recommendation", reco);
    if (s == 1) {
        html += infoWarning(
            "1 second is the minimum and rather aggressive — combined with "
            "high power it can feel jarring. Try 3-5 s instead unless you "
            "specifically want a fast pattern.");
    }
    m_cycleInfo->setText(html);
}

void WizardDialog::buildChannelsPage() {
    auto* left = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(left);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(new QLabel(
        "<p>Which channels should the pattern drive?</p>"
        "<p style='color:#888;'>The ZC95 has 4 independent output channels. "
        "Most beginners start with 1 or 2 channels.</p>", left));

    auto* row = new QHBoxLayout();
    for (int i = 0; i < 4; ++i) {
        m_chBoxes[i] = new QCheckBox(QString("CH%1").arg(i + 1), left);
        m_chBoxes[i]->setChecked(i < 1);  // CH1 by default
        m_chBoxes[i]->setStyleSheet("font-size: 12pt; padding: 8px;");
        connect(m_chBoxes[i], &QCheckBox::toggled, this, [this](bool) { updateChannelsInfo(); });
        row->addWidget(m_chBoxes[i]);
    }
    row->addStretch();
    lay->addLayout(row);

    auto* hint = new QLabel(
        "<p style='color:#aaa;'>Tip: when in doubt, pick CH1 only. You can "
        "always edit the script afterwards to add more channels.</p>", left);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    lay->addStretch();
    m_stack->addWidget(buildSplitPage(left, m_channelsInfo));
    updateChannelsInfo();
}

void WizardDialog::updateChannelsInfo() {
    int count = 0;
    QStringList names;
    for (int i = 0; i < 4; ++i) {
        if (m_chBoxes[i] && m_chBoxes[i]->isChecked()) {
            ++count;
            names << QString("CH%1").arg(i + 1);
        }
    }
    QString html;
    html += infoSection("🎯", "Currently",
        count == 0
            ? "<i style='color:#ffb86b;'>No channel selected — at least one is required.</i>"
            : QString("<b style='color:#ffd400;'>%1 channel%2:</b> %3")
                  .arg(count).arg(count > 1 ? "s" : "").arg(names.join(", ")));

    if (count == 0) {
        html += infoSection("💡", "Recommendation",
            "Select at least CH1. The wizard will default to CH1 if you "
            "leave them all unchecked.");
    } else if (count == 1) {
        html += infoSection("🎨", "Feeling",
            "A single channel drives one electrode pair. Sensation localised "
            "to that area only.");
        html += infoSection("💡", "Recommendation",
            "<b>Best starting choice.</b> One channel = one variable to "
            "understand. Pair it with two electrodes placed close together "
            "(2-5 cm apart) for a focused sensation.");
    } else if (count == 2) {
        html += infoSection("🎨", "Feeling",
            "Two independent channels. With four electrodes you can stimulate "
            "two distinct areas simultaneously.");
        html += infoSection("💡", "Recommendation",
            "Common for paired placements (left/right, or two body zones). "
            "The pattern fires identical events on both — they don't "
            "alternate unless you edit the script.");
    } else if (count == 3) {
        html += infoSection("🎨", "Feeling",
            "Three channels. Larger total surface area engaged simultaneously.");
        html += infoSection("💡", "Recommendation",
            "Uncommon — most setups use 1, 2, or 4. Consider whether you "
            "really need 3, or whether 2 or 4 would be cleaner.");
    } else {
        html += infoSection("🎨", "Feeling",
            "All 4 channels active. Maximum surface area engaged.");
        html += infoSection("💡", "Recommendation",
            "Useful for whole-body or multi-zone setups. Make sure each "
            "channel has its own electrode pair — sharing an electrode "
            "between channels is electrically unsound.");
        html += infoWarning(
            "Driving 4 channels simultaneously at high power can be more "
            "intense than expected — total charge delivered scales with "
            "channel count. Start the dial low.");
    }
    m_channelsInfo->setText(html);
}

void WizardDialog::buildKillSwitchPage() {
    auto* left = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(left);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(new QLabel(
        "<p>Add a kill-switch?</p>"
        "<p style='color:#888;'>A soft button on the device LCD lets you "
        "stop the output instantly with one tap. <b>Highly recommended</b> "
        "— always have a way to abort.</p>", left));

    m_killSwitch = new QCheckBox("Yes, add a STOP soft button", left);
    m_killSwitch->setChecked(true);
    m_killSwitch->setStyleSheet("font-size: 12pt; padding: 8px;");
    lay->addWidget(m_killSwitch);
    connect(m_killSwitch, &QCheckBox::toggled, this, [this](bool) { updateKillSwitchInfo(); });

    auto* hint = new QLabel(
        "<p style='color:#aaa;'>The front-panel dial is always available "
        "as a hardware kill-switch (turn it to zero). The soft button is "
        "an additional one-tap alternative.</p>", left);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    lay->addStretch();
    m_stack->addWidget(buildSplitPage(left, m_killSwitchInfo));
    updateKillSwitchInfo();
}

void WizardDialog::updateKillSwitchInfo() {
    bool on = m_killSwitch && m_killSwitch->isChecked();
    QString html;
    if (on) {
        html += infoSection("🎯", "What it does",
            "Generates a <code>SoftButton(pushed)</code> Lua callback that, "
            "on press, calls <code>zc.ChannelOff(1..4)</code> and resets "
            "<code>_intensity = 0</code>.");
        html += infoSection("🎨", "How it feels",
            "One tap on the on-screen STOP button = output stops "
            "immediately. The button label appears in the LCD's top-left.");
        html += infoSection("💡", "Recommendation",
            "<b>Keep this enabled.</b> Two independent ways to stop output "
            "(soft button + hardware dial) is the gold standard. Costs you "
            "nothing.");
    } else {
        html += infoSection("🎯", "What it does",
            "Skips the soft-button generation. The pattern can only be "
            "stopped by turning the front-panel dial to zero.");
        html += infoWarning(
            "<b>Not recommended.</b> If for any reason the dial is "
            "out of reach or you panic, you have NO software override. "
            "The dial is your only kill mechanism.");
        html += infoSection("💡", "Recommendation",
            "Re-enable the soft button. Even if you never use it, having "
            "it cost nothing and may save the day.");
    }
    m_killSwitchInfo->setText(html);
}

void WizardDialog::buildSummaryPage() {
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    m_summaryLabel = new QLabel(page);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet(
        "background:#1e1e1e; color:#ddd; padding:12px; "
        "font-family: Consolas, monospace; font-size: 10pt;");
    m_summaryLabel->setTextFormat(Qt::RichText);
    lay->addWidget(m_summaryLabel);
    lay->addStretch();
    m_stack->addWidget(page);
}

void WizardDialog::updateNav() {
    int idx = m_stack->currentIndex();
    static const QStringList headers = {
        "Step 1 / 6 — Pattern type",
        "Step 2 / 6 — Intensity ceiling",
        "Step 3 / 6 — Cycle duration",
        "Step 4 / 6 — Channels",
        "Step 5 / 6 — Kill-switch",
        "Step 6 / 6 — Review",
    };
    if (idx >= 0 && idx < headers.size()) m_pageHeader->setText(headers[idx]);

    m_prevBtn->setEnabled(idx > 0);
    m_nextBtn->setVisible(idx < PageCount - 1);
    m_finishBtn->setVisible(idx == PageCount - 1);

    if (idx == PSummary) {
        // Refresh summary from current selections.
        QStringList chans;
        for (int i = 0; i < 4; ++i) {
            if (m_chBoxes[i] && m_chBoxes[i]->isChecked()) chans << QString("CH%1").arg(i + 1);
        }
        if (chans.isEmpty()) chans << "CH1";
        QString typeName;
        switch (Type(m_typeGroup->checkedId())) {
            case TConstant: typeName = "Constant"; break;
            case TPulse:    typeName = "Pulse"; break;
            case TFade:     typeName = "Fade in/out"; break;
            case TBurst:    typeName = "Burst"; break;
            case TTens:     typeName = "TENS-like"; break;
        }
        QString intensityName;
        switch (Intensity(m_intGroup->checkedId())) {
            case IGentle: intensityName = "Gentle (max 400)"; break;
            case IMedium: intensityName = "Medium (max 700)"; break;
            case IStrong: intensityName = "Strong (max 1000)"; break;
        }
        QString summary;
        summary += QString("<b>Type:</b> %1<br>").arg(typeName);
        summary += QString("<b>Intensity ceiling:</b> %1<br>").arg(intensityName);
        summary += QString("<b>Cycle duration:</b> %1 s<br>").arg(m_cycleSec->value());
        summary += QString("<b>Channels:</b> %1<br>").arg(chans.join(", "));
        summary += QString("<b>Kill-switch:</b> %1<br>")
                       .arg(m_killSwitch->isChecked() ? "Yes (soft button STOP)" : "No");
        summary += "<br><i>Click <b>Generate</b> to create the script. "
                   "It will replace whatever's currently in the editor.</i>";
        m_summaryLabel->setText(summary);
    }
}

void WizardDialog::next() {
    int idx = m_stack->currentIndex();
    if (idx == PType)        m_type      = Type(m_typeGroup->checkedId());
    if (idx == PIntensity)   m_intensity = Intensity(m_intGroup->checkedId());
    if (idx < PageCount - 1) m_stack->setCurrentIndex(idx + 1);
    updateNav();
}

void WizardDialog::prev() {
    int idx = m_stack->currentIndex();
    if (idx > 0) m_stack->setCurrentIndex(idx - 1);
    updateNav();
}

// Generate the ScriptConfig + Lua source from the wizard's choices.
void WizardDialog::produceResult() {
    m_type      = Type(m_typeGroup->checkedId());
    m_intensity = Intensity(m_intGroup->checkedId());

    QVector<int> channels;
    for (int i = 0; i < 4; ++i) {
        if (m_chBoxes[i] && m_chBoxes[i]->isChecked()) channels.append(i + 1);
    }
    if (channels.isEmpty()) channels.append(1);

    int cycleSec = m_cycleSec->value();
    bool killSwitch = m_killSwitch->isChecked();
    int maxPower = int(m_intensity);

    // ---- Build ScriptConfig ----
    ScriptConfig c;
    QString typeShort;
    switch (m_type) {
        case TConstant: c.name = "Constant";  typeShort = "constant"; break;
        case TPulse:    c.name = "Pulse";     typeShort = "pulse"; break;
        case TFade:     c.name = "Fade";      typeShort = "fade"; break;
        case TBurst:    c.name = "Burst";     typeShort = "burst"; break;
        case TTens:     c.name = "TENS";      typeShort = "tens"; break;
    }

    if (killSwitch) {
        c.softButtonLabel = "STOP";
        c.functions.softButton = true;
    }

    c.functions.setup = true;
    c.functions.loop = true;
    c.functions.minMaxChange = true;

    // One MIN_MAX item: intensity 0..maxPower, default 0 (safe).
    MenuItem intensityItem;
    intensityItem.type = MenuItemType::MinMax;
    intensityItem.id = 1;
    intensityItem.title = "Intensity";
    intensityItem.min = 0;
    intensityItem.max = maxPower;
    intensityItem.incrementStep = qMax(1, maxPower / 50);
    intensityItem.uom = "";
    intensityItem.defaultValue = 0;  // start at zero — safe default
    c.menuItems.append(intensityItem);

    // For pulse/burst, also expose a speed slider.
    if (m_type == TPulse || m_type == TBurst) {
        MenuItem speedItem;
        speedItem.type = MenuItemType::MinMax;
        speedItem.id = 2;
        speedItem.title = "Speed";
        speedItem.min = 1;
        speedItem.max = 30;
        speedItem.incrementStep = 1;
        speedItem.uom = "x";
        speedItem.defaultValue = 5;  // = 1× nominal cycle
        c.menuItems.append(speedItem);
    }

    m_result = c;

    // ---- Build the Lua source, with French comments for beginners ----
    QString s;
    s += "-- =====================================================\n";
    s += "-- Pattern généré par le \"New pattern wizard\".\n";
    s += "-- Type: " + c.name + " — Intensité max: " + QString::number(maxPower) + " — Cycle: "
       + QString::number(cycleSec) + " s\n";
    s += "-- Édite librement, mais lis les commentaires d'abord !\n";
    s += "-- =====================================================\n\n";

    s += "-- Variable globale liée au menu \"Intensity\" (id=1).\n";
    s += "-- Le firmware appelle MinMaxChange(1, valeur) à chaque mouvement\n";
    s += "-- du slider sur l'écran. On stocke la dernière valeur ici.\n";
    s += "_intensity = 0\n";

    if (m_type == TPulse || m_type == TBurst) {
        s += "\n-- Variable liée au menu \"Speed\" (id=2). 5 = vitesse nominale.\n";
        s += "_speed = 5\n";
    }

    s += "\n";

    // Config block — generated by the regular code path later in MainWindow,
    // but for the wizard we emit it inline so the script is self-contained.
    s += "Config = {\n";
    s += "    name = \"" + c.name + "\",\n";
    s += "    audio_processing_mode = \"OFF\",\n";
    if (killSwitch) s += "    soft_button = \"STOP\",\n";
    s += "    menu_items = {\n";
    s += "        {\n";
    s += "            type = \"MIN_MAX\",\n";
    s += "            title = \"Intensity\",\n";
    s += "            id = 1,\n";
    s += "            group = 0,\n";
    s += "            min = 0,\n";
    s += "            max = " + QString::number(maxPower) + ",\n";
    s += "            increment_step = " + QString::number(intensityItem.incrementStep) + ",\n";
    s += "            uom = \"\",\n";
    s += "            default = 0\n";
    s += "        }";
    if (m_type == TPulse || m_type == TBurst) {
        s += ",\n";
        s += "        {\n";
        s += "            type = \"MIN_MAX\",\n";
        s += "            title = \"Speed\",\n";
        s += "            id = 2,\n";
        s += "            group = 0,\n";
        s += "            min = 1,\n";
        s += "            max = 30,\n";
        s += "            increment_step = 1,\n";
        s += "            uom = \"x\",\n";
        s += "            default = 5\n";
        s += "        }";
    }
    s += "\n    }\n";
    s += "}\n\n";

    // Setup() — initial channel state.
    s += "function Setup()\n";
    s += "    -- Setup() est appelé UNE fois avant le démarrage de Loop().\n";
    s += "    -- On y configure l'état initial de chaque canal utilisé.\n";
    QString chanList;
    for (int ch : channels) {
        if (!chanList.isEmpty()) chanList += ", ";
        chanList += QString::number(ch);
    }
    s += "    -- Canaux actifs : " + chanList + "\n";
    for (int ch : channels) {
        s += QString("    zc.SetPower(%1, _intensity)        -- 0 au démarrage — l'utilisateur monte avec le slider\n").arg(ch);
        s += QString("    zc.SetFrequency(%1, 50)            -- 50 Hz = vibration douce\n").arg(ch);
        s += QString("    zc.SetPulseWidth(%1, 150, 150)     -- 150 µs = standard\n").arg(ch);
    }
    s += "end\n\n";

    // Loop() — type-specific body.
    s += "-- État interne de Loop pour ce pattern.\n";
    s += "_next_event_ms = 0\n\n";
    s += "function Loop(time_ms)\n";

    if (m_type == TConstant) {
        s += "    -- Pattern \"Constant\" : on allume tous les canaux et on les laisse.\n";
        s += "    -- (On ne le fait qu'une fois — sinon ça spamme.)\n";
        s += "    if _next_event_ms == 0 then\n";
        for (int ch : channels) {
            s += QString("        zc.ChannelOn(%1)\n").arg(ch);
        }
        s += "        _next_event_ms = -1   -- marqueur \"déjà fait\"\n";
        s += "    end\n";
    } else if (m_type == TPulse) {
        int baseInterval = (cycleSec * 1000) / 5;  // 5 = nominal speed factor
        s += QString("    -- Pattern \"Pulse\" : un coup court sur chaque canal toutes les N ms.\n");
        s += QString("    -- N = (durée du cycle) / (vitesse). Vitesse plus grande = plus rapide.\n");
        s += QString("    local interval_ms = (%1 * 1000) / _speed\n").arg(cycleSec);
        s += QString("    if time_ms > _next_event_ms then\n");
        for (int ch : channels) {
            s += QString("        zc.ChannelPulseMs(%1, 100)   -- pulse de 100 ms\n").arg(ch);
        }
        s += QString("        _next_event_ms = time_ms + interval_ms\n");
        s += QString("    end\n");
    } else if (m_type == TFade) {
        s += "    -- Pattern \"Fade\" : on calcule un facteur entre 0 et 1 qui suit\n";
        s += "    -- une demi-onde sinusoïdale sur le cycle, on l'applique à _intensity.\n";
        s += QString("    local cycle_ms = %1 * 1000\n").arg(cycleSec);
        s += "    local phase = (time_ms % cycle_ms) / cycle_ms      -- 0..1\n";
        s += "    local factor = 0.5 - 0.5 * math.cos(2 * math.pi * phase)  -- 0..1..0\n";
        s += "    local pwr = math.floor(_intensity * factor)\n";
        s += "    if time_ms > _next_event_ms then\n";
        for (int ch : channels) {
            s += QString("        zc.ChannelOn(%1)\n").arg(ch);
            s += QString("        zc.SetPower(%1, pwr)\n").arg(ch);
        }
        s += "        _next_event_ms = time_ms + 50    -- on rafraîchit toutes les 50 ms\n";
        s += "    end\n";
    } else if (m_type == TBurst) {
        s += "    -- Pattern \"Burst\" : 4 pulses rapides, puis pause, puis on recommence.\n";
        s += "    -- L'écart entre les bursts dépend du slider Speed.\n";
        s += QString("    local burst_period_ms = (%1 * 1000) / _speed\n").arg(cycleSec);
        s += "    if time_ms > _next_event_ms then\n";
        for (int ch : channels) {
            s += QString("        zc.ChannelPulseMs(%1, 30)\n").arg(ch);
        }
        s += "        zc.DelayMs(60)\n";
        for (int ch : channels) {
            s += QString("        zc.ChannelPulseMs(%1, 30)\n").arg(ch);
        }
        s += "        zc.DelayMs(60)\n";
        for (int ch : channels) {
            s += QString("        zc.ChannelPulseMs(%1, 30)\n").arg(ch);
        }
        s += "        zc.DelayMs(60)\n";
        for (int ch : channels) {
            s += QString("        zc.ChannelPulseMs(%1, 30)\n").arg(ch);
        }
        s += "        _next_event_ms = time_ms + burst_period_ms\n";
        s += "    end\n";
    } else if (m_type == TTens) {
        s += "    -- Pattern \"TENS-like\" : haute fréquence, pulse étroit. On allume\n";
        s += "    -- les canaux une fois et on laisse le device générer le signal.\n";
        s += "    if _next_event_ms == 0 then\n";
        for (int ch : channels) {
            s += QString("        zc.SetFrequency(%1, 150)\n").arg(ch);
            s += QString("        zc.SetPulseWidth(%1, 80, 80)    -- pulses étroits = picotement\n").arg(ch);
            s += QString("        zc.ChannelOn(%1)\n").arg(ch);
        }
        s += "        _next_event_ms = -1\n";
        s += "    end\n";
    }

    s += "end\n\n";

    // MinMaxChange handler — wires the sliders.
    s += "function MinMaxChange(menu_id, min_max_val)\n";
    s += "    -- Appelé quand l'utilisateur bouge un slider sur l'écran.\n";
    s += "    if (menu_id == 1) then\n";
    s += "        _intensity = min_max_val\n";
    s += "        -- On répercute immédiatement la nouvelle puissance sur les canaux.\n";
    for (int ch : channels) {
        s += QString("        zc.SetPower(%1, _intensity)\n").arg(ch);
    }
    s += "    end\n";
    if (m_type == TPulse || m_type == TBurst) {
        s += "    if (menu_id == 2) then\n";
        s += "        _speed = min_max_val\n";
        s += "    end\n";
    }
    s += "end\n";

    if (killSwitch) {
        s += "\nfunction SoftButton(pushed)\n";
        s += "    -- Bouton STOP : coupe immédiatement tous les canaux et\n";
        s += "    -- remet l'intensité à zéro pour qu'au relâchement rien ne reparte.\n";
        s += "    if pushed then\n";
        for (int i = 1; i <= 4; ++i) {
            s += QString("        zc.ChannelOff(%1)\n").arg(i);
            s += QString("        zc.SetPower(%1, 0)\n").arg(i);
        }
        s += "        _intensity = 0\n";
        s += "    end\n";
        s += "end\n";
    }

    m_source = s;
}
