#pragma once
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
namespace dango {
struct AutoFollowConfig {bool enabled=false;float range=1200;std::vector<std::string> rules;};
inline bool autoRule(const AutoFollowConfig& c,const std::string& key){return std::find(c.rules.begin(),c.rules.end(),key)!=c.rules.end();}
inline void setAutoRule(AutoFollowConfig& c,const std::string& key,bool enabled){c.rules.erase(std::remove(c.rules.begin(),c.rules.end(),key),c.rules.end());if(enabled&&c.rules.size()<512)c.rules.push_back(key);}
inline AutoFollowConfig readAutoFollow(const nlohmann::json& j){AutoFollowConfig c;if(!j.is_object())return c;c.enabled=j.value("enabled",false);float range=j.value("range",1200.f);c.range=std::isfinite(range)?std::clamp(range,100.f,4000.f):1200;
 if(j.contains("rules")&&j["rules"].is_array())for(const auto& v:j["rules"]){if(v.is_string()){auto s=v.get<std::string>();if(!s.empty()&&s.size()<=512)setAutoRule(c,s,true);}}return c;}
inline nlohmann::json autoFollowJson(const AutoFollowConfig& c){return {{"enabled",c.enabled},{"range",c.range},{"rules",c.rules}};}
}
