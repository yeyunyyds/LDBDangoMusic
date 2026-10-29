#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dango::hud {
struct Row {std::string label;uintptr_t action=0;};
struct Document {std::string title;std::vector<Row> rows;};
// Decode only our own generated KeyValues. No game pointers or callback
// addresses from the document are ever executed by the native renderer.
inline Document document(std::string_view text){
 size_t at=0;
 auto token=[&](std::string& out){out.clear();while(at<text.size()&&static_cast<unsigned char>(text[at])<=32)++at;if(at==text.size())return false;
 char c=text[at++];if(c=='{'||c=='}'){out+=c;return true;}if(c!='"')return false;
 while(at<text.size()){c=text[at++];if(c=='"')return true;if(c=='\\'){if(at==text.size())return false;c=text[at++];}out+=c;}return false;};
 Document out;std::string t;if(text.size()>131072||!token(out.title)||!token(t)||t!="{")return {};
 while(token(t)&&t!="}"){
 Row row;row.label=t;if(!token(t)||t!="{")return {};
 while(token(t)&&t!="}"){std::string key=t;if(!token(t))return {};if(key=="user_data"){
 std::string_view value=t;if(value.size()>2&&value.substr(0,2)=="0x")value.remove_prefix(2);
 auto result=std::from_chars(value.data(),value.data()+value.size(),row.action,16);if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())return {};}}
 if(t!="}")return {};out.rows.push_back(std::move(row));
 }
 return t=="}"?out:Document{};
}
struct MenuFrame {uintptr_t page=0;size_t sheet=0;};
struct MenuStack {
 static constexpr size_t PageSize=9;
 std::vector<MenuFrame> frames{{}};
 void reset(){frames.assign(1,{});}
 MenuFrame& current(){return frames.back();}
 const MenuFrame& current()const{return frames.back();}
 void enter(uintptr_t page){if(page==27)page=23;if(page!=current().page&&frames.size()<32)frames.push_back({page,0});}
 bool back(){if(frames.size()==1)return false;frames.pop_back();return true;}
 void clamp(size_t count){current().sheet=std::min(current().sheet,count?((count-1)/PageSize):size_t(0));}
 void next(size_t count){clamp(count);if((current().sheet+1)*PageSize<count)++current().sheet;}
 void previous(){if(current().sheet)--current().sheet;}
 uintptr_t select(const Document& doc,size_t digit){auto index=current().sheet*PageSize+digit-1;return digit>=1&&digit<=PageSize&&index<doc.rows.size()?doc.rows[index].action:0;}
};
struct KeyGate {
 static constexpr size_t Count=14;
 using Keys=std::array<bool,Count>;
 bool armed=false;Keys held{};
 void reset(){armed=false;held.fill(false);}
 Keys update(const Keys& down,bool focused){
 Keys edges{};if(!focused){reset();return edges;}
 if(!armed){bool any=false;for(bool d:down)any|=d;if(!any)armed=true;held=down;return edges;}
 for(size_t i=0;i<Count;++i){edges[i]=down[i]&&!held[i];held[i]=down[i];}return edges;
 }
};
}
