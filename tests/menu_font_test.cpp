#include "menu_font.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv){
 auto context=ImGui::CreateContext();auto& atlas=*ImGui::GetIO().Fonts;ImVector<ImWchar> ranges;dango::hud::menuFontRanges(atlas,ranges);
 const auto text=std::string(dango::hud::MenuGlyphs);unsigned checked=0;
 ImFont* font=nullptr;if(argc>1){font=atlas.AddFontFromFileTTF(argv[1],24,nullptr,ranges.Data);if(!font||!atlas.Build())throw std::runtime_error("Build menu font atlas failed");}
 for(size_t i=0;i<text.size();){unsigned char first=text[i++];unsigned cp=first,count=0;if(first>=0xf0){cp=first&7;count=3;}else if(first>=0xe0){cp=first&15;count=2;}else if(first>=0xc0){cp=first&31;count=1;}while(count--)cp=(cp<<6)|(static_cast<unsigned char>(text.at(i++))&63);
 bool covered=false;for(int r=0;ranges[r];r+=2)if(cp>=ranges[r]&&cp<=ranges[r+1])covered=true;
 if(!covered||(font&&!font->FindGlyphNoFallback(static_cast<ImWchar>(cp)))){std::cerr<<"Missing menu glyph U+"<<std::hex<<cp<<'\n';throw std::runtime_error("Menu glyph coverage regression");}++checked;}
 ImGui::DestroyContext(context);std::cout<<"Passed: all "<<checked<<" menu/source characters covered"<<(font?" and rasterized without fallback":"")<<'\n';
}
