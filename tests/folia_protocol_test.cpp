#include "folia_protocol.hpp"
#include <cassert>
using namespace dango;
int main(){
 auto c=folia::connection(nlohmann::json{{"version",1},{"pid",10},{"port",32107},{"baseUrl","http://127.0.0.1:32107"},{"token","abc123"}});
 assert(c.port==32107);
 bool refused=false;try{folia::connection(nlohmann::json{{"version",1},{"pid",10},{"port",32107},{"baseUrl","http://example.com:32107"},{"token","abc"}});}catch(...){refused=true;}assert(refused);
 auto s=folia::playback(nlohmann::json::parse(R"({"current":{"id":"abc","source":"local","title":"Song","artist":"Artist"},"playerState":"PLAYING","durationMs":90000,"positionMs":1200})"),100);
 assert(s.playing&&s.position==1200&&s.at(200)==1300);
 auto j=nlohmann::json::parse(R"({"title":"Song","artist":"Artist","offset":250,"clipStartMs":1000,"wordByWord":true,"lines":[{"text":"a","translation":"b","startTime":2,"endTime":3,"words":[{"text":"a","startTime":2,"endTime":2.5}]}]})");
 auto l=folia::lyrics(j,s);assert(l.size()==1&&l[0].start==1250&&l[0].words[0].start==1250&&l[0].words[0].duration==500);
 j["wordByWord"]=false;assert(folia::lyrics(j,s)[0].words.empty());j["title"]="Other";assert(folia::lyrics(j,s).empty());
 assert(folia::lyrics(nullptr,s).empty());
}
