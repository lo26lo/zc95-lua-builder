#pragma once

#include <QString>
#include <QVector>

enum class MenuItemType {
    MinMax,
    MultiChoice,
    AudioViewIntensityStereo,
    AudioViewIntensityMono
};

struct MultiChoiceOption {
    int choiceId = 1;
    QString description;
};

struct MenuItem {
    MenuItemType type = MenuItemType::MinMax;
    QString title;
    int id = 1;
    int group = 0;

    // MIN_MAX fields
    int min = 0;
    int max = 100;
    int incrementStep = 1;
    QString uom;
    int defaultValue = 0;

    // MULTI_CHOICE fields
    QVector<MultiChoiceOption> choices;

    static QString typeToString(MenuItemType t);
    QString summary() const;
};
