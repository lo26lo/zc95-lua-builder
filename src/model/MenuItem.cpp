#include "MenuItem.h"

QString MenuItem::typeToString(MenuItemType t) {
    switch (t) {
        case MenuItemType::MinMax: return "MIN_MAX";
        case MenuItemType::MultiChoice: return "MULTI_CHOICE";
        case MenuItemType::AudioViewIntensityStereo: return "AUDIO_VIEW_INTENSITY_STEREO";
        case MenuItemType::AudioViewIntensityMono: return "AUDIO_VIEW_INTENSITY_MONO";
    }
    return "MIN_MAX";
}

QString MenuItem::summary() const {
    QString base = QString("[%1] id=%2  %3").arg(typeToString(type)).arg(id).arg(title);
    if (type == MenuItemType::MinMax) {
        base += QString("  (%1..%2 %3)").arg(min).arg(max).arg(uom);
    } else if (type == MenuItemType::MultiChoice) {
        base += QString("  (%1 choices)").arg(choices.size());
    }
    return base;
}
