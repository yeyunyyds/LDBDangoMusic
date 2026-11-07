#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "hud_menu_policy.hpp"

namespace dango::hud {
struct Row {std::string label;uintptr_t action=0;};
enum class RowTone {Neutral,Enabled,Disabled,Partial,Muted};
inline RowTone rowTone(const std::string& label,uintptr_t action,bool interactive){
 if(opensChild(action)||action==9000)return RowTone::Neutral;
 if(action==110005)return RowTone::Enabled;if(action==110006)return RowTone::Disabled;
 if(label.find(u8"[部分开启]")!=std::string::npos)return RowTone::Partial;
 if(label.find(u8"[开启]")!=std::string::npos||label.find(u8"[开]")!=std::string::npos||label.find(u8"[开 ·")!=std::string::npos||label.find(u8": 开")!=std::string::npos)return RowTone::Enabled;
 if(label.find(u8"[关闭]")!=std::string::npos||label.find(u8"[关]")!=std::string::npos||label.find(u8"[关 ·")!=std::string::npos||label.find(u8": 关")!=std::string::npos)return RowTone::Disabled;
 return interactive?RowTone::Neutral:RowTone::Muted;
}
struct Document {std::string title;std::vector<Row> rows;size_t pinned=0;std::vector<size_t> sections={};std::string note={};};
inline std::vector<std::pair<size_t,size_t>> pageRanges(const Document& doc){
 const size_t pin=std::min({doc.pinned,doc.rows.size(),size_t(8)}),capacity=9-pin;std::vector<size_t> cuts{pin};
 for(auto n:doc.sections)if(n>cuts.back()&&n<doc.rows.size())cuts.push_back(n);cuts.push_back(doc.rows.size());std::vector<std::pair<size_t,size_t>> pages;
 for(size_t i=1;i<cuts.size();++i)for(size_t first=cuts[i-1];first<cuts[i];first+=capacity)pages.push_back({first,std::min(first+capacity,cuts[i])});
 if(pages.empty())pages.push_back({pin,pin});return pages;
}
// Decode only our own generated KeyValues. No game pointers or callback
// addresses from the document are ever executed by the native renderer.
inline Document document(std::string_view text){
 size_t at=0;
 auto token=[&](std::string& out){out.clear();while(at<text.size()&&static_cast<unsigned char>(text[at])<=32)++at;if(at==text.size())return false;
 char c=text[at++];if(c=='{'||c=='}'){out+=c;return true;}if(c!='"')return false;
 while(at<text.size()){c=text[at++];if(c=='"')return true;if(c=='\\'){if(at==text.size())return false;c=text[at++];}out+=c;}return false;};
 Document out;std::string t;if(text.size()>131072||!token(out.title)||!token(t)||t!="{")return {};
 while(token(t)&&t!="}"){
 if(t=="description"){if(!token(out.note)||out.note.size()>4096)return {};continue;}
 if(t=="pinned_rows"){if(!token(t))return {};size_t n=0;auto result=std::from_chars(t.data(),t.data()+t.size(),n);if(result.ec!=std::errc{}||result.ptr!=t.data()+t.size()||n>8)return {};out.pinned=n;continue;}
 if(t=="page_starts"){if(!token(t)||t.size()>512)return {};size_t start=0;while(start<t.size()){auto end=t.find(',',start);if(end==std::string::npos)end=t.size();size_t n=0;auto result=std::from_chars(t.data()+start,t.data()+end,n);if(result.ec!=std::errc{}||result.ptr!=t.data()+end||(!out.sections.empty()&&n<=out.sections.back())||out.sections.size()>=64)return {};out.sections.push_back(n);start=end+1;}continue;}
 Row row;row.label=t;if(!token(t)||t!="{")return {};
 while(token(t)&&t!="}"){std::string key=t;if(!token(t))return {};if(key=="user_data"){
 std::string_view value=t;if(value.size()>2&&value.substr(0,2)=="0x")value.remove_prefix(2);
 auto result=std::from_chars(value.data(),value.data()+value.size(),row.action,16);if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())return {};}}
 if(t!="}")return {};out.rows.push_back(std::move(row));
 }
 if(out.pinned>out.rows.size())return {};for(auto n:out.sections)if(n<out.pinned||n>=out.rows.size())return {};return t=="}"?out:Document{};
}
struct MenuFrame {uintptr_t page=0;size_t sheet=0;};
struct MenuStack {
 static constexpr size_t PageSize=9;
 std::vector<MenuFrame> frames{{}};
 void reset(){frames.assign(1,{});}
 MenuFrame& current(){return frames.back();}
 const MenuFrame& current()const{return frames.back();}
 void enter(uintptr_t page){if(page==27)page=5060;if(page!=current().page&&frames.size()<32)frames.push_back({page,0});}
 bool back(){if(frames.size()==1)return false;frames.pop_back();return true;}
 void clamp(size_t count){current().sheet=std::min(current().sheet,count?((count-1)/PageSize):size_t(0));}
 void next(size_t count){clamp(count);if((current().sheet+1)*PageSize<count)++current().sheet;}
 void previous(){if(current().sheet)--current().sheet;}
 void clamp(const Document& doc){current().sheet=std::min(current().sheet,pageRanges(doc).size()-1);}
 void next(const Document& doc){clamp(doc);if(current().sheet+1<pageRanges(doc).size())++current().sheet;}
 std::vector<size_t> visible(const Document& doc)const{auto ranges=pageRanges(doc);auto range=ranges[std::min(current().sheet,ranges.size()-1)];std::vector<size_t> indices;for(size_t i=0;i<std::min({doc.pinned,doc.rows.size(),size_t(8)});++i)indices.push_back(i);for(size_t i=range.first;i<range.second;++i)indices.push_back(i);return indices;}
 uintptr_t select(const Document& doc,size_t digit){auto indices=visible(doc);return digit>=1&&digit<=indices.size()?doc.rows[indices[digit-1]].action:0;}
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
