#include <cassert>
#define DANGO_SYNC_STEAM_HEADER "fake_steam_transport.hpp"
#include "sync/runtime.hpp"
#include "lyrics_settings.hpp"
#include "menu_pages.hpp"
#include <iostream>
int main(){using namespace dango;sync::Requests q;sync::Runtime runtime(q,nullptr);sync::Settings cfg;
 assert(!cfg.enabled&&!cfg.discover&&!cfg.autoSearch&&!cfg.apply);
 auto migrated=sync::readSettings({{"role",1},{"enabled",true},{"discover",true},{"autoSearch",true},{"apply",true}});assert(!migrated.enabled&&!migrated.discover&&!migrated.autoSearch&&!migrated.apply);
 cfg.enabled=cfg.discover=cfg.autoSearch=cfg.apply=true;auto opted=sync::readSettings(sync::settingsJson(cfg));assert(opted.enabled&&opted.discover&&opted.autoSearch&&opted.apply);
 sync::Roster roster;roster.active=true;roster.generation=1;roster.local=76561197960265729ULL;roster.players={roster.local,roster.local+1};roster.tick=1000;State state;state.connected=true;state.key="song";state.title="Song";state.artist="Artist";state.duration=120000;state.sampled=1000;
 int writes=0,reads=0,schemes=0;auto read=[&](){++reads;return state;};auto http=[&](const std::string&){++writes;};auto scheme=[&](const std::string&){++schemes;};sync::SourceContext lx{sync::Provider::LX,1,1};
 // Valid Source roster and all request IDs must not bypass an off master.
 cfg.enabled=false;cfg.discover=true;for(int round=0;round<10;++round){for(int id=0;id<10;++id)q.push({id,{},{}});runtime.tick(cfg,state,1000,read,http,lx,roster,scheme);}
 assert(sync::steam::connects==0&&sync::steam::pumps==0&&sync::steam::sends==0&&sync::steam::closes==0&&reads==0&&writes==0&&schemes==0);
 cfg.enabled=true;runtime.tick(cfg,state,1000,read,http,lx,roster,scheme);assert(sync::steam::connects==1&&sync::steam::pumps>0&&sync::steam::sends>0);
 q.push({0,{},{}});q.push({7,{},{}});runtime.tick(cfg,state,1050,read,http,lx,roster,scheme);assert(reads==0&&writes==0&&schemes==0);
 // Revocation tears down once; subsequent off ticks make no transport calls.
 cfg.enabled=false;runtime.tick(cfg,state,1100,read,http,lx,roster,scheme);auto closes=sync::steam::closes,pumps=sync::steam::pumps,sends=sync::steam::sends;assert(closes==1);
 for(int t=1200;t<2200;t+=100)runtime.tick(cfg,state,t,read,http,lx,roster,scheme);assert(sync::steam::closes==closes&&sync::steam::pumps==pumps&&sync::steam::sends==sends);
 cfg.enabled=true;runtime.tick(cfg,state,2200,read,http,lx,roster,scheme,[]{return false;});assert(sync::steam::connects==1);cfg.discover=false;runtime.tick(cfg,state,2200,read,http,lx,roster,scheme);assert(sync::steam::connects==1);
 cfg.discover=true;runtime.tick(cfg,state,2200,read,http,{sync::Provider::Netease,1,1},roster,scheme);assert(sync::steam::connects==1);
 runtime.tick(cfg,state,2300,read,http,lx,roster,scheme);auto bound=sync::steam::connects;auto beforeReset=sync::steam::closes;assert(bound==2);runtime.reset();assert(sync::steam::closes==beforeReset+1);runtime.reset();assert(sync::steam::closes==beforeReset+1);cfg.enabled=false;runtime.tick(cfg,state,2400,read,http,lx,roster,scheme);assert(sync::steam::connects==bound);cfg.enabled=true;runtime.tick(cfg,state,2450,read,http,lx,roster,scheme);assert(sync::steam::connects==bound+1);runtime.reset();assert(reads==0&&writes==0&&schemes==0);
 auto c=decodeConfig({{"cameraDiagnostics",true},{"debugOverlay",true},{"worldTestAnchor",true},{"wallDebug",true}});assert(!c.cameraDiagnostics&&!c.debugOverlay&&!c.worldTestAnchor&&!c.wallDebug);
 for(auto& row:menus::more().rows)assert(!build::diagnosticAction(row.action));for(auto& row:menus::wallAppearance(c).rows)assert(!build::diagnosticAction(row.action));
 std::cout<<"Passed: old consent revoked; all defaults off; zero transport/control calls while off despite roster and queued callbacks; opt-in enables; revoke cleans once then stays inert; source/permission gates; diagnostic config/menu disabled\n";
}
