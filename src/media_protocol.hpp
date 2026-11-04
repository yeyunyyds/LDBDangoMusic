#pragma once
#include "core.hpp"
namespace dango::media {
inline const char* sourceName(int source){static const char* labels[]={u8"自动（LX 优先）",u8"LX 本地 API",u8"Windows 音乐会话",u8"网易云音乐",u8"QQ 音乐",u8"酷狗音乐",u8"Spotify",u8"Tidal"};return source>=0&&source<8?labels[source]:labels[0];}
inline State readStatus(const nlohmann::json& j,int64_t tick,uint32_t parent){
 if(j.at("schema")!=1||j.at("parent").get<uint32_t>()!=parent)throw std::runtime_error("Media bridge identity mismatch");
 auto sampled=j.at("tick").get<int64_t>();if(sampled>tick||tick-sampled>3000)throw std::runtime_error("Media bridge snapshot expired");
 auto s=status(j,sampled);if(!s.key.empty())s.key="smtc\x1f"+j.at("sourceId").get<std::string>()+'\x1f'+s.key;
 s.message=j.value("message",std::string("Windows media session"));return s;
}
inline Timeline readLyrics(const nlohmann::json& j){
 Timeline result;auto& lines=j.at("lines");if(!lines.is_array()||lines.size()>10000)throw std::runtime_error("Invalid media lyric count");
 for(auto& row:lines){Line l;l.start=row.at("start").get<int64_t>();l.text=row.at("text").get<std::string>();l.translation=row.value("translation",std::string{});if(l.start<0||l.start>86400000||l.text.size()>16384||l.translation.size()>16384)throw std::runtime_error("Invalid media lyric line");
 if(row.contains("words")&&!row["words"].is_null()){auto& words=row["words"];if(!words.is_array()||words.size()>4096)throw std::runtime_error("Invalid media word count");for(auto& w:words){Word word{w.at("start").get<int64_t>(),w.at("duration").get<int64_t>(),w.at("text").get<std::string>()};if(word.start<0||word.start>86400000||word.duration<0||word.duration>86400000||word.text.size()>16384)throw std::runtime_error("Invalid media word");l.words.push_back(std::move(word));}}
 result.push_back(std::move(l));}
 std::stable_sort(result.begin(),result.end(),[](const Line& a,const Line& b){return a.start<b.start;});return result;
}
}
