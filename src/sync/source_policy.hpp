#pragma once
#include <cstdint>
namespace dango::sync {
enum class Provider {Unavailable,Demo,LX,Netease,Windows};
inline const char* providerName(Provider p){switch(p){case Provider::LX:return u8"LX 本地 API";case Provider::Netease:return u8"网易云音乐";case Provider::Windows:return u8"Windows 音乐会话";case Provider::Demo:return u8"离线示例";default:return u8"未连接";}}
struct SourceContext {Provider provider=Provider::Unavailable;uint64_t revision=0,epoch=0;};
inline bool lxAllowed(const SourceContext& source){return source.provider==Provider::LX&&source.revision&&source.epoch;}
inline bool currentCommand(const SourceContext& source,uint64_t revision,uint64_t epoch){return lxAllowed(source)&&source.revision==revision&&source.epoch==epoch;}
}
