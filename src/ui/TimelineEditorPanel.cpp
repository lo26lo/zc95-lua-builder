#include "TimelineEditorPanel.h"
#include "TimelineEditorWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QFrame>
#include <QShortcut>
#include <QKeySequence>

namespace {
QToolButton* makeToolButton(const QString& label, const QString& tip,
                            const QString& shortcut, QButtonGroup* group, int id) {
    auto* b = new QToolButton();
    b->setText(label);
    b->setCheckable(true);
    b->setAutoRaise(false);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setMinimumWidth(78);
    b->setToolTip(QString("%1\nRaccourci : %2").arg(tip, shortcut));
    group->addButton(b, id);
    return b;
}
}  // namespace

TimelineEditorPanel::TimelineEditorPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ---- Beta banner ------------------------------------------------------
    auto* banner = new QLabel(
        "<b>⚠ Fonction expérimentale (Beta)</b> — round-trip stable, "
        "mais l'API peut encore évoluer. Toujours relire le Lua généré.",
        this);
    banner->setWordWrap(true);
    banner->setStyleSheet(
        "QLabel { background:#5a4a1a; color:#ffe2a8; "
        "         padding: 4px 10px; border-bottom: 1px solid #3d3013; }");
    outer->addWidget(banner);

    // ---- Project toolbar (Loop / Cycle / Lua sync / Clear) ----------------
    auto* projBar = new QHBoxLayout();
    projBar->setContentsMargins(8, 6, 8, 0);
    m_loopCheck = new QCheckBox("Loop", this);
    m_loopCheck->setToolTip(
        "When ticked, the generated Loop() wraps time_ms modulo the\n"
        "cycle duration so the pattern restarts automatically.\n"
        "Off = events fire once and stay fired.");
    projBar->addWidget(m_loopCheck);

    projBar->addWidget(new QLabel("Cycle (ms):", this));
    m_cycleSpin = new QSpinBox(this);
    m_cycleSpin->setRange(100, 600000);
    m_cycleSpin->setSingleStep(500);
    m_cycleSpin->setValue(10000);
    m_cycleSpin->setToolTip(
        "Length of one cycle. Used for the loop wrap and as the default\n"
        "viewport width.");
    projBar->addWidget(m_cycleSpin);

    projBar->addStretch();

    m_pullBtn = new QPushButton("← Read from Lua", this);
    m_pullBtn->setToolTip(
        "Re-read the timeline JSON sentinel from the current editor.\n"
        "Useful if you've manually edited the Lua and the timeline is stale.");
    projBar->addWidget(m_pullBtn);

    m_pushBtn = new QPushButton("→ Push to Lua", this);
    m_pushBtn->setToolTip(
        "Generate the Setup() / Loop() body from the current timeline\n"
        "and write it into the editor (replacing any previous timeline\n"
        "block — bracketed by the ZC95_TIMELINE_V1 sentinel).");
    m_pushBtn->setStyleSheet(
        "QPushButton { background:#3a5; color:#fff; padding: 4px 12px; "
        "              border-radius: 3px; font-weight: bold; } "
        "QPushButton:hover { background:#4c7; }");
    projBar->addWidget(m_pushBtn);

    m_clearBtn = new QPushButton("Clear all", this);
    projBar->addWidget(m_clearBtn);

    outer->addLayout(projBar);

    // ---- Edit toolbar (tools / snap / cursor time / undo / zoom) ----------
    auto* editBar = new QHBoxLayout();
    editBar->setContentsMargins(8, 4, 8, 6);

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);
    m_btnSelect = makeToolButton("↖ Select", "Sélectionner / déplacer un événement existant.\n"
                                              "Clic vide = désélectionner.",   "S", m_toolGroup, 0);
    m_btnPulse  = makeToolButton("▱ Pulse",  "Cliquer-glisser sur une lane pour créer une\n"
                                              "impulsion (durée = longueur du drag).",  "P", m_toolGroup, 1);
    m_btnOn     = makeToolButton("▶ On",     "Cliquer sur une lane pour poser un\n"
                                              "ChannelOn à cet instant.",                  "O", m_toolGroup, 2);
    m_btnOff    = makeToolButton("⏹ Off",    "Cliquer sur une lane pour poser un\n"
                                              "ChannelOff à cet instant.",                 "F", m_toolGroup, 3);
    m_btnPulse->setChecked(true);  // matches widget default
    editBar->addWidget(m_btnSelect);
    editBar->addWidget(m_btnPulse);
    editBar->addWidget(m_btnOn);
    editBar->addWidget(m_btnOff);

    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine); sep1->setFrameShadow(QFrame::Sunken);
    editBar->addWidget(sep1);

    editBar->addWidget(new QLabel("Snap:", this));
    m_snapCombo = new QComboBox(this);
    m_snapCombo->addItem("Off",     0);
    m_snapCombo->addItem("25 ms",   25);
    m_snapCombo->addItem("50 ms",   50);
    m_snapCombo->addItem("100 ms",  100);
    m_snapCombo->addItem("250 ms",  250);
    m_snapCombo->addItem("500 ms",  500);
    m_snapCombo->addItem("1 s",     1000);
    m_snapCombo->setCurrentIndex(3);   // 100 ms default
    m_snapCombo->setToolTip(
        "Pas de quantification pour création / déplacement / nudge.\n"
        "Maintiens Shift pendant un drag pour le contourner ponctuellement.");
    editBar->addWidget(m_snapCombo);

    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine); sep2->setFrameShadow(QFrame::Sunken);
    editBar->addWidget(sep2);

    m_timeLabel = new QLabel("t = —", this);
    m_timeLabel->setMinimumWidth(110);
    m_timeLabel->setStyleSheet("font-family: Consolas, 'Courier New'; color:#9bd1ff;");
    m_timeLabel->setToolTip("Position temporelle sous le curseur (mise à jour au survol).");
    editBar->addWidget(m_timeLabel);

    editBar->addStretch();

    m_undoBtn = new QPushButton("↶ Undo", this);
    m_undoBtn->setToolTip("Annuler le dernier changement (Ctrl+Z)");
    m_undoBtn->setEnabled(false);
    editBar->addWidget(m_undoBtn);
    m_redoBtn = new QPushButton("↷ Redo", this);
    m_redoBtn->setToolTip("Rétablir (Ctrl+Y)");
    m_redoBtn->setEnabled(false);
    editBar->addWidget(m_redoBtn);

    auto* sep3 = new QFrame(this);
    sep3->setFrameShape(QFrame::VLine); sep3->setFrameShadow(QFrame::Sunken);
    editBar->addWidget(sep3);

    m_zoomOut = new QPushButton("−", this);
    m_zoomOut->setFixedWidth(28);
    m_zoomOut->setToolTip("Zoom arrière");
    editBar->addWidget(m_zoomOut);
    m_zoomLabel = new QLabel("100%", this);
    m_zoomLabel->setMinimumWidth(46);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setStyleSheet("font-family: Consolas, 'Courier New';");
    editBar->addWidget(m_zoomLabel);
    m_zoomIn = new QPushButton("+", this);
    m_zoomIn->setFixedWidth(28);
    m_zoomIn->setToolTip("Zoom avant");
    editBar->addWidget(m_zoomIn);
    m_zoomFit = new QPushButton("Fit", this);
    m_zoomFit->setToolTip("Ajuster à un cycle complet");
    editBar->addWidget(m_zoomFit);

    outer->addLayout(editBar);

    // ---- Center: canvas + properties side by side -------------------------
    auto* split = new QSplitter(Qt::Horizontal, this);
    m_canvas = new TimelineEditorWidget(split);
    split->addWidget(m_canvas);

    m_propsBox = new QGroupBox("Selected event", split);
    auto* propsLay = new QVBoxLayout(m_propsBox);
    propsLay->setContentsMargins(8, 8, 8, 8);
    auto* placeholder = new QLabel(
        "<i style='color:#888;'>Click an event to edit it,\n"
        "or click-drag on a lane to create a Pulse.</i>", m_propsBox);
    placeholder->setWordWrap(true);
    propsLay->addWidget(placeholder);
    propsLay->addStretch();
    split->addWidget(m_propsBox);

    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 2);
    outer->addWidget(split, 1);

    // ---- Wiring -----------------------------------------------------------
    connect(m_canvas, &TimelineEditorWidget::projectChanged,
            this,     &TimelineEditorPanel::onProjectChanged);
    connect(m_canvas, &TimelineEditorWidget::selectionChanged,
            this,     &TimelineEditorPanel::onSelectionChanged);
    connect(m_canvas, &TimelineEditorWidget::toolChanged,
            this,     &TimelineEditorPanel::onCanvasToolChanged);
    connect(m_canvas, &TimelineEditorWidget::cursorTimeChanged,
            this,     &TimelineEditorPanel::onCursorTimeChanged);
    connect(m_canvas, &TimelineEditorWidget::zoomChanged,
            this,     &TimelineEditorPanel::onZoomChanged);
    connect(m_canvas, &TimelineEditorWidget::undoStateChanged,
            this,     &TimelineEditorPanel::onUndoStateChanged);

    connect(m_loopCheck, &QCheckBox::toggled, this, &TimelineEditorPanel::onLoopToggled);
    connect(m_cycleSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TimelineEditorPanel::onCycleChanged);
    connect(m_pushBtn,  &QPushButton::clicked, this, &TimelineEditorPanel::pushToLuaRequested);
    connect(m_pullBtn,  &QPushButton::clicked, this, &TimelineEditorPanel::pullFromLuaRequested);
    connect(m_clearBtn, &QPushButton::clicked, this, &TimelineEditorPanel::onClearAll);

    connect(m_toolGroup, &QButtonGroup::idClicked,
            this, &TimelineEditorPanel::onToolButtonClicked);
    connect(m_snapCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TimelineEditorPanel::onSnapChanged);

    connect(m_undoBtn, &QPushButton::clicked, m_canvas, &TimelineEditorWidget::undo);
    connect(m_redoBtn, &QPushButton::clicked, m_canvas, &TimelineEditorWidget::redo);
    connect(m_zoomIn,  &QPushButton::clicked, m_canvas, &TimelineEditorWidget::zoomIn);
    connect(m_zoomOut, &QPushButton::clicked, m_canvas, &TimelineEditorWidget::zoomOut);
    connect(m_zoomFit, &QPushButton::clicked, m_canvas, &TimelineEditorWidget::fitToCycle);

    // Initialise canvas snap from the dropdown default.
    m_canvas->setSnapMs(100.0);
    onZoomChanged();
}

const TimelineProject& TimelineEditorPanel::project() const {
    return m_canvas->project();
}

void TimelineEditorPanel::setProject(const TimelineProject& p) {
    m_canvas->setProject(p);
    m_loopCheck->blockSignals(true);
    m_cycleSpin->blockSignals(true);
    m_loopCheck->setChecked(p.loop);
    m_cycleSpin->setValue((int)p.cycleMs);
    m_loopCheck->blockSignals(false);
    m_cycleSpin->blockSignals(false);
    rebuildPropertyPanel();
    onZoomChanged();
}

void TimelineEditorPanel::setAvailableVariables(const QStringList& names) {
    m_availableVars = names;
    rebuildPropertyPanel();
}

void TimelineEditorPanel::onSelectionChanged(int) {
    rebuildPropertyPanel();
}

void TimelineEditorPanel::onProjectChanged() {
    emit projectChanged();
    rebuildPropertyPanel();
    onZoomChanged();
}

void TimelineEditorPanel::onLoopToggled(bool b) {
    auto p = m_canvas->project();
    if (p.loop == b) return;
    m_canvas->pushUndoSnapshot();
    p.loop = b;
    m_canvas->applyProjectKeepHistory(p);
    emit projectChanged();
}

void TimelineEditorPanel::onCycleChanged(int v) {
    auto p = m_canvas->project();
    if ((int)p.cycleMs == v) return;
    m_canvas->pushUndoSnapshot();
    p.cycleMs = v;
    m_canvas->applyProjectKeepHistory(p);
    emit projectChanged();
}

void TimelineEditorPanel::onClearAll() {
    if (m_canvas->project().events.isEmpty()) return;
    auto ans = QMessageBox::question(this, "Clear timeline",
        "Remove all events from the timeline?\n\n"
        "Press Ctrl+Z afterwards to bring them back.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;
    m_canvas->pushUndoSnapshot();
    auto p = m_canvas->project();
    p.events.clear();
    m_canvas->applyProjectKeepHistory(p);
    emit projectChanged();
}

void TimelineEditorPanel::onToolButtonClicked(int id) {
    static const TimelineEditorWidget::Tool map[] = {
        TimelineEditorWidget::ToolSelect,
        TimelineEditorWidget::ToolPulse,
        TimelineEditorWidget::ToolOn,
        TimelineEditorWidget::ToolOff,
    };
    if (id < 0 || id >= 4) return;
    m_canvas->setTool(map[id]);
    m_canvas->setFocus();   // so keyboard nudge / Ctrl+Z work right after a click
}

void TimelineEditorPanel::onCanvasToolChanged(TimelineEditorWidget::Tool t) {
    QToolButton* btn = nullptr;
    switch (t) {
        case TimelineEditorWidget::ToolSelect: btn = m_btnSelect; break;
        case TimelineEditorWidget::ToolPulse:  btn = m_btnPulse;  break;
        case TimelineEditorWidget::ToolOn:     btn = m_btnOn;     break;
        case TimelineEditorWidget::ToolOff:    btn = m_btnOff;    break;
    }
    if (btn && !btn->isChecked()) {
        QSignalBlocker bl(btn);
        btn->setChecked(true);
    }
}

void TimelineEditorPanel::onSnapChanged(int) {
    int v = m_snapCombo->currentData().toInt();
    m_canvas->setSnapMs((double)v);
    rebuildPropertyPanel();   // step values follow the snap
}

void TimelineEditorPanel::onCursorTimeChanged(double tMs) {
    if (tMs < 0) {
        m_timeLabel->setText("t = —");
    } else if (tMs >= 1000) {
        m_timeLabel->setText(QString("t = %1 s").arg(tMs / 1000.0, 0, 'f', 2));
    } else {
        m_timeLabel->setText(QString("t = %1 ms").arg((int)tMs));
    }
}

void TimelineEditorPanel::onZoomChanged() {
    double z = m_canvas->zoomLevel();
    m_zoomLabel->setText(QString("%1%").arg(qRound(z * 100)));
}

void TimelineEditorPanel::onUndoStateChanged() {
    m_undoBtn->setEnabled(m_canvas->canUndo());
    m_redoBtn->setEnabled(m_canvas->canRedo());
}

QWidget* TimelineEditorPanel::buildParamEditor(const TimelineParam& current,
                                               std::function<void(const TimelineParam&)> apply,
                                               const QString& label) {
    auto* w = new QWidget(m_propsBox);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);

    auto* lbl = new QLabel(label, w);
    lbl->setMinimumWidth(60);
    h->addWidget(lbl);

    auto* combo = new QComboBox(w);
    combo->addItem("Literal", 0);
    combo->addItem("Variable", 1);
    combo->setCurrentIndex(current.kind == TimelineParam::Variable ? 1 : 0);
    combo->setFixedWidth(78);
    h->addWidget(combo);

    auto* stack = new QStackedWidget(w);
    h->addWidget(stack, 1);

    auto* spin = new QSpinBox(stack);
    spin->setRange(0, 1000);
    spin->setValue(current.literalValue);
    stack->addWidget(spin);
    auto* varCombo = new QComboBox(stack);
    varCombo->setEditable(true);
    varCombo->addItems(m_availableVars);
    if (!current.variableName.isEmpty()) {
        int existing = varCombo->findText(current.variableName);
        if (existing < 0) varCombo->addItem(current.variableName);
        varCombo->setCurrentText(current.variableName);
    } else if (varCombo->count() > 0) {
        varCombo->setCurrentIndex(0);
    }
    stack->addWidget(varCombo);

    stack->setCurrentIndex(combo->currentIndex());
    auto sync = [combo, stack, spin, varCombo, apply, this]() {
        m_canvas->pushUndoSnapshot();
        TimelineParam p;
        if (combo->currentIndex() == 0) {
            p.kind = TimelineParam::Literal;
            p.literalValue = spin->value();
        } else {
            p.kind = TimelineParam::Variable;
            p.variableName = varCombo->currentText();
        }
        apply(p);
    };
    QObject::connect(combo, qOverload<int>(&QComboBox::currentIndexChanged),
                     stack, [stack, sync](int idx) { stack->setCurrentIndex(idx); sync(); });
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), w, [sync](int) { sync(); });
    QObject::connect(varCombo, &QComboBox::currentTextChanged, w, [sync](const QString&) { sync(); });
    return w;
}

void TimelineEditorPanel::rebuildPropertyPanel() {
    QLayout* lay = m_propsBox->layout();
    while (QLayoutItem* it = lay->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }

    int idx = m_canvas->selectedIndex();
    if (idx < 0) {
        auto* placeholder = new QLabel(
            "<i style='color:#888;'>Click an event to edit it, "
            "or click-drag on a lane to create a Pulse.</i>", m_propsBox);
        placeholder->setWordWrap(true);
        lay->addWidget(placeholder);
        QSpacerItem* sp = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        lay->addItem(sp);
        return;
    }

    const TimelineEvent& e = m_canvas->project().events[idx];
    int snapStep = qMax(1, (int)m_canvas->snapMs());

    // Type
    auto* typeRow = new QWidget(m_propsBox);
    auto* typeH = new QHBoxLayout(typeRow);
    typeH->setContentsMargins(0, 0, 0, 0);
    typeH->addWidget(new QLabel("Type:", typeRow));
    auto* typeCombo = new QComboBox(typeRow);
    typeCombo->addItem("Pulse", 0);
    typeCombo->addItem("ChannelOn", 1);
    typeCombo->addItem("ChannelOff", 2);
    typeCombo->setCurrentIndex((int)e.type);
    typeH->addWidget(typeCombo, 1);
    QObject::connect(typeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                     this, [this, idx](int v) {
                         auto p = m_canvas->project();
                         if (idx < 0 || idx >= p.events.size()) return;
                         m_canvas->pushUndoSnapshot();
                         p.events[idx].type = (TimelineEvent::Type)v;
                         m_canvas->applyProjectKeepHistory(p);
                         m_canvas->setSelectedIndex(idx);
                         emit projectChanged();
                     });
    lay->addWidget(typeRow);

    // Channel
    auto* chRow = new QWidget(m_propsBox);
    auto* chH = new QHBoxLayout(chRow);
    chH->setContentsMargins(0, 0, 0, 0);
    chH->addWidget(new QLabel("Channel:", chRow));
    auto* chSpin = new QSpinBox(chRow);
    chSpin->setRange(1, 4);
    chSpin->setValue(e.channel);
    chH->addWidget(chSpin, 1);
    QObject::connect(chSpin, qOverload<int>(&QSpinBox::valueChanged),
                     this, [this, idx](int v) {
                         auto p = m_canvas->project();
                         if (idx < 0 || idx >= p.events.size()) return;
                         m_canvas->pushUndoSnapshot();
                         p.events[idx].channel = v;
                         m_canvas->applyProjectKeepHistory(p);
                         m_canvas->setSelectedIndex(idx);
                         emit projectChanged();
                     });
    lay->addWidget(chRow);

    // Start time
    auto* startRow = new QWidget(m_propsBox);
    auto* startH = new QHBoxLayout(startRow);
    startH->setContentsMargins(0, 0, 0, 0);
    startH->addWidget(new QLabel("Start (ms):", startRow));
    auto* startSpin = new QSpinBox(startRow);
    startSpin->setRange(0, 600000);
    startSpin->setSingleStep(snapStep);
    startSpin->setValue((int)e.startMs);
    startH->addWidget(startSpin, 1);
    QObject::connect(startSpin, qOverload<int>(&QSpinBox::valueChanged),
                     this, [this, idx](int v) {
                         auto p = m_canvas->project();
                         if (idx < 0 || idx >= p.events.size()) return;
                         m_canvas->pushUndoSnapshot();
                         p.events[idx].startMs = v;
                         m_canvas->applyProjectKeepHistory(p);
                         m_canvas->setSelectedIndex(idx);
                         emit projectChanged();
                     });
    lay->addWidget(startRow);

    if (e.type == TimelineEvent::Pulse) {
        auto* durRow = new QWidget(m_propsBox);
        auto* durH = new QHBoxLayout(durRow);
        durH->setContentsMargins(0, 0, 0, 0);
        durH->addWidget(new QLabel("Duration:", durRow));
        auto* durSpin = new QSpinBox(durRow);
        durSpin->setRange(5, 60000);
        durSpin->setSingleStep(qMax(10, snapStep));
        durSpin->setSuffix(" ms");
        durSpin->setValue((int)e.durationMs);
        durH->addWidget(durSpin, 1);
        QObject::connect(durSpin, qOverload<int>(&QSpinBox::valueChanged),
                         this, [this, idx](int v) {
                             auto p = m_canvas->project();
                             if (idx < 0 || idx >= p.events.size()) return;
                             m_canvas->pushUndoSnapshot();
                             p.events[idx].durationMs = v;
                             m_canvas->applyProjectKeepHistory(p);
                             m_canvas->setSelectedIndex(idx);
                             emit projectChanged();
                         });
        lay->addWidget(durRow);
    }

    lay->addWidget(buildParamEditor(e.power,
        [this, idx](const TimelineParam& np) {
            auto p = m_canvas->project();
            if (idx < 0 || idx >= p.events.size()) return;
            p.events[idx].power = np;
            m_canvas->applyProjectKeepHistory(p);
            m_canvas->setSelectedIndex(idx);
            emit projectChanged();
        }, "Power:"));

    lay->addWidget(buildParamEditor(e.frequency,
        [this, idx](const TimelineParam& np) {
            auto p = m_canvas->project();
            if (idx < 0 || idx >= p.events.size()) return;
            p.events[idx].frequency = np;
            m_canvas->applyProjectKeepHistory(p);
            m_canvas->setSelectedIndex(idx);
            emit projectChanged();
        }, "Freq:"));

    lay->addWidget(buildParamEditor(e.pulseWidth,
        [this, idx](const TimelineParam& np) {
            auto p = m_canvas->project();
            if (idx < 0 || idx >= p.events.size()) return;
            p.events[idx].pulseWidth = np;
            m_canvas->applyProjectKeepHistory(p);
            m_canvas->setSelectedIndex(idx);
            emit projectChanged();
        }, "Width:"));

    auto* cmtRow = new QWidget(m_propsBox);
    auto* cmtH = new QHBoxLayout(cmtRow);
    cmtH->setContentsMargins(0, 0, 0, 0);
    cmtH->addWidget(new QLabel("Note:", cmtRow));
    auto* cmtEdit = new QLineEdit(cmtRow);
    cmtEdit->setPlaceholderText("optional comment");
    cmtEdit->setText(e.comment);
    cmtH->addWidget(cmtEdit, 1);
    QObject::connect(cmtEdit, &QLineEdit::editingFinished,
                     this, [this, idx, cmtEdit]() {
                         auto p = m_canvas->project();
                         if (idx < 0 || idx >= p.events.size()) return;
                         if (p.events[idx].comment == cmtEdit->text()) return;
                         m_canvas->pushUndoSnapshot();
                         p.events[idx].comment = cmtEdit->text();
                         m_canvas->applyProjectKeepHistory(p);
                         m_canvas->setSelectedIndex(idx);
                         emit projectChanged();
                     });
    lay->addWidget(cmtRow);

    auto* btnRow = new QWidget(m_propsBox);
    auto* btnH = new QHBoxLayout(btnRow);
    btnH->setContentsMargins(0, 0, 0, 0);
    auto* dupBtn = new QPushButton("Duplicate (Ctrl+D)", btnRow);
    QObject::connect(dupBtn, &QPushButton::clicked,
                     this, [this]() { m_canvas->duplicateSelected(); m_canvas->setFocus(); });
    btnH->addWidget(dupBtn);
    auto* delBtn = new QPushButton("Delete", btnRow);
    delBtn->setStyleSheet("QPushButton { color:#cc4040; }");
    QObject::connect(delBtn, &QPushButton::clicked,
                     this, [this]() { m_canvas->deleteSelected(); });
    btnH->addWidget(delBtn);
    lay->addWidget(btnRow);

    QSpacerItem* sp = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    lay->addItem(sp);
}
