#pragma once

#include "../model/ScriptConfig.h"
#include <QWidget>

class QCheckBox;

class FunctionsPanel : public QWidget {
    Q_OBJECT
public:
    explicit FunctionsPanel(QWidget* parent = nullptr);

    void load(const EnabledFunctions& f);
    void save(EnabledFunctions& f) const;

    // Hide advanced callbacks (BT HID, AudioIntensity, ExternalTrigger).
    void setBeginnerMode(bool beginner);

signals:
    void changed();

private:
    QCheckBox* m_setup;
    QCheckBox* m_loop;
    QCheckBox* m_minMax;
    QCheckBox* m_multiChoice;
    QCheckBox* m_softButton;
    QCheckBox* m_externalTrigger;
    QCheckBox* m_btKeypress;
    QCheckBox* m_btHid;
    QCheckBox* m_audioIntensity;
};
