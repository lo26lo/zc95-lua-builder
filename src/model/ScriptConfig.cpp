#include "ScriptConfig.h"

QString ScriptConfig::audioModeToString(AudioMode m) {
    switch (m) {
        case AudioMode::Off: return "OFF";
        case AudioMode::AudioIntensity: return "AUDIO_INTENSITY";
    }
    return "OFF";
}
