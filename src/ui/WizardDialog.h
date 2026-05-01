#pragma once

#include "../model/ScriptConfig.h"
#include <QDialog>
#include <QString>

class QStackedWidget;
class QPushButton;
class QButtonGroup;
class QSpinBox;
class QCheckBox;
class QLabel;

// "Create my first pattern" wizard. Walks a beginner through 5 questions,
// then generates a Lua script with French comments explaining every line.
//
// On Accept: caller can read `config()` and `generatedSource()` and feed
// them into the form / editor.
class WizardDialog : public QDialog {
    Q_OBJECT
public:
    explicit WizardDialog(QWidget* parent = nullptr);

    ScriptConfig config() const { return m_result; }
    QString generatedSource() const { return m_source; }

private slots:
    void next();
    void prev();

private:
    enum Page { PType = 0, PIntensity, PCycle, PChannels, PKillSwitch, PSummary, PageCount };

    void buildTypePage();
    void buildIntensityPage();
    void buildCyclePage();
    void buildChannelsPage();
    void buildKillSwitchPage();
    void buildSummaryPage();
    void updateNav();
    void produceResult();

    // Per-page info-panel refresh slots — re-renders the right-hand
    // help column when the user changes a control on the left.
    void updateTypeInfo();
    void updateIntensityInfo();
    void updateCycleInfo();
    void updateChannelsInfo();
    void updateKillSwitchInfo();

    // Helper that wraps a QWidget (left controls) and a QLabel (right
    // info panel) into a single horizontal page widget.
    class QWidget* buildSplitPage(QWidget* leftControls, QLabel*& outInfo);

    QStackedWidget* m_stack = nullptr;
    QPushButton*    m_prevBtn = nullptr;
    QPushButton*    m_nextBtn = nullptr;
    QPushButton*    m_finishBtn = nullptr;
    QLabel*         m_pageHeader = nullptr;

    // Page 1 — pattern type
    QButtonGroup* m_typeGroup = nullptr;
    enum Type { TConstant, TPulse, TFade, TBurst, TTens } m_type = TPulse;

    // Page 2 — intensity ceiling
    QButtonGroup* m_intGroup = nullptr;
    enum Intensity { IGentle = 400, IMedium = 700, IStrong = 1000 } m_intensity = IGentle;

    // Page 3 — cycle duration
    QSpinBox* m_cycleSec = nullptr;

    // Page 4 — channels
    QCheckBox* m_chBoxes[4] = {nullptr, nullptr, nullptr, nullptr};

    // Page 5 — kill-switch
    QCheckBox* m_killSwitch = nullptr;

    // Page 6 — summary (read-only preview)
    QLabel* m_summaryLabel = nullptr;

    // Per-page info panels (right-hand column).
    QLabel* m_typeInfo       = nullptr;
    QLabel* m_intensityInfo  = nullptr;
    QLabel* m_cycleInfo      = nullptr;
    QLabel* m_channelsInfo   = nullptr;
    QLabel* m_killSwitchInfo = nullptr;

    // Output
    ScriptConfig m_result;
    QString m_source;
};
