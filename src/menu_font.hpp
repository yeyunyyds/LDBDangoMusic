#pragma once
#include "imgui/imgui.h"
#include "menu_glyphs.hpp"
namespace dango::hud {
inline void menuFontRanges(ImFontAtlas& atlas,ImVector<ImWchar>& ranges){
 ImFontGlyphRangesBuilder builder;
 builder.AddRanges(atlas.GetGlyphRangesChineseSimplifiedCommon());
 builder.AddText(MenuGlyphs);
 builder.BuildRanges(&ranges);
}
}
