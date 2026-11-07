#pragma once
#include "factory_defaults.hpp"
#include "visual_presets.hpp"
#include "entity_follow.hpp"
namespace dango::reset {
inline Config appearance(const Config& current){auto defaults=installedDefaults();auto result=visual::apply(current,visual::capture(defaults,""),visual::Scope::All);result.presetId=defaults.presetId;result.progressiveReveal=defaults.progressiveReveal;result.fallbackReveal=defaults.fallbackReveal;result.lyricMove=defaults.lyricMove;result.entitySmooth=defaults.entitySmooth;result.moveCurve=defaults.moveCurve;result.moveMs=defaults.moveMs;return result;}
inline Config all(const Config& current){auto defaults=installedDefaults();defaults.visualPresets=current.visualPresets;return defaults;}
inline follow::Settings followAppearance(follow::Settings current){follow::Settings defaults;current.bobEnabled=defaults.bobEnabled;current.bobPeriod=defaults.bobPeriod;current.bobAmplitude=defaults.bobAmplitude;return current;}
}
