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

WizardDialog::WizardDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("New pattern wizard");
    resize(620, 520);

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

void WizardDialog::buildTypePage() {
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->addWidget(new QLabel(
        "<p>What kind of pattern do you want?</p>"
        "<p style='color:#888;'>Each option generates a starting script "
        "you can refine later. You can always come back and pick another.</p>", page));

    m_typeGroup = new QButtonGroup(page);

    auto add = [&](Type t, const QString& title, const QString& desc, bool checked = false) {
        auto* rb = new QRadioButton(title, page);
        rb->setStyleSheet("font-weight: bold;");
        rb->setChecked(checked);
        m_typeGroup->addButton(rb, t);
        lay->addWidget(rb);
        auto* d = new QLabel(desc, page);
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
    m_stack->addWidget(page);
}

void WizardDialog::buildIntensityPage() {
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->addWidget(new QLabel(
        "<p>Maximum intensity ceiling for this pattern</p>"
        "<p style='color:#888;'>Sets the <b>cap</b>. The actual output is "
        "still scaled by the front-panel dial. Picking <b>Gentle</b> means "
        "even at the dial's maximum the pattern stays mild.</p>", page));

    m_intGroup = new QButtonGroup(page);

    auto add = [&](Intensity i, const QString& title, const QString& desc, bool checked = false) {
        auto* rb = new QRadioButton(title, page);
        rb->setStyleSheet("font-weight: bold;");
        rb->setChecked(checked);
        m_intGroup->addButton(rb, i);
        lay->addWidget(rb);
        auto* d = new QLabel(desc, page);
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
    m_stack->addWidget(page);
}

void WizardDialog::buildCyclePage() {
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->addWidget(new QLabel(
        "<p>How long should one cycle of the pattern last?</p>"
        "<p style='color:#888;'>For a Fade: time from low to high to low again.<br>"
        "For a Burst: time between burst groups.<br>"
        "For a Pulse: time between individual pulses.</p>", page));

    auto* h = new QHBoxLayout();
    h->addWidget(new QLabel("Cycle duration (seconds):", page));
    m_cycleSec = new QSpinBox(page);
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
        "&nbsp;&nbsp;20-60 s — slow build-up</p>", page);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    lay->addStretch();
    m_stack->addWidget(page);
}

void WizardDialog::buildChannelsPage() {
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->addWidget(new QLabel(
        "<p>Which channels should the pattern drive?</p>"
        "<p style='color:#888;'>The ZC95 has 4 independent output channels. "
        "Most beginners start with 1 or 2 channels.</p>", page));

    auto* row = new QHBoxLayout();
    for (int i = 0; i < 4; ++i) {
        m_chBoxes[i] = new QCheckBox(QString("CH%1").arg(i + 1), page);
        m_chBoxes[i]->setChecked(i < 1);  // CH1 by default
        m_chBoxes[i]->setStyleSheet("font-size: 12pt; padding: 8px;");
        row->addWidget(m_chBoxes[i]);
    }
    row->addStretch();
    lay->addLayout(row);

    auto* hint = new QLabel(
        "<p style='color:#aaa;'>Tip: when in doubt, pick CH1 only. You can "
        "always edit the script afterwards to add more channels.</p>", page);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    lay->addStretch();
    m_stack->addWidget(page);
}

void WizardDialog::buildKillSwitchPage() {
    auto* page = new QWidget(m_stack);
    auto* lay = new QVBoxLayout(page);
    lay->addWidget(new QLabel(
        "<p>Add a kill-switch?</p>"
        "<p style='color:#888;'>A soft button on the device LCD lets you "
        "stop the output instantly with one tap. <b>Highly recommended</b> "
        "— always have a way to abort.</p>", page));

    m_killSwitch = new QCheckBox("Yes, add a STOP soft button", page);
    m_killSwitch->setChecked(true);
    m_killSwitch->setStyleSheet("font-size: 12pt; padding: 8px;");
    lay->addWidget(m_killSwitch);

    auto* hint = new QLabel(
        "<p style='color:#aaa;'>The front-panel dial is always available "
        "as a hardware kill-switch (turn it to zero). The soft button is "
        "an additional one-tap alternative.</p>", page);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    lay->addStretch();
    m_stack->addWidget(page);
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
