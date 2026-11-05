#pragma once
#include "protocol.hpp"
namespace dango::sync {
struct Settings {int role=0,threshold=350;bool enabled=false,discover=true,autoSearch=true,apply=false;std::set<Peer> peers;Peer host=0;};
inline Settings readSettings(const nlohmann::json& j){if(!j.is_object())throw std::runtime_error("Sync settings must be an object");Settings s;s.role=std::clamp(j.value("role",0),0,2);s.enabled=j.value("enabled",s.role!=0);s.discover=j.value("discover",true);s.autoSearch=j.value("autoSearch",true);s.threshold=std::clamp(j.value("thresholdMs",350),100,2000);s.apply=j.value("apply",false);if(j.contains("peers")){if(!j["peers"].is_array()||j["peers"].size()>8)throw std::runtime_error("At most eight sync Steam peers");for(auto& p:j["peers"])s.peers.insert(peer(p.get<std::string>()));}auto h=j.value("host",std::string{});if(!h.empty())s.host=peer(h);if(s.host)s.peers.insert(s.host);if(s.peers.size()>8)throw std::runtime_error("At most eight sync Steam peers");return s;}
inline nlohmann::json settingsJson(const Settings& s){auto peers=nlohmann::json::array();for(auto p:s.peers)peers.push_back(std::to_string(p));return {{"enabled",s.enabled},{"discover",s.discover},{"autoSearch",s.autoSearch},{"role",s.role},{"thresholdMs",s.threshold},{"apply",s.apply},{"host",s.host?std::to_string(s.host):""},{"peers",peers}};}
}
