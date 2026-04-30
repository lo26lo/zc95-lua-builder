#pragma once

#include "../model/ScriptConfig.h"
#include <QWidget>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QFormLayout;

class ConfigPanel : public QWidget {
    Q_OBJECT
public:
    explicit ConfigPanel(QWidget* parent = nullptr);

    void load(const ScriptConfig& config);
    void save(ScriptConfig& config) const;

    // Hide advanced fields in beginner mode (loop frequency,
    // BT passthrough, allow triphase).
    void setBeginnerMode(bool beginner);

signals:
    void changed();

private:
    QLineEdit* m_name;
    QComboBox* m_audioMode;
    QLineEdit* m_softButton;
    QSpinBox* m_loopFreq;
    QCheckBox* m_allowTriphase;
    QCheckBox* m_btPassthrough;

    QFormLayout* m_form = nullptr;
};
