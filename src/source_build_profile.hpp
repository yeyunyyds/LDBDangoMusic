#pragma once
#include <cstdint>
namespace source_camera {
struct MethodSlot {unsigned slot;uint32_t rva;};
// Statically verified against the user's DLL pair, not inferred from version strings.
constexpr uint32_t ClientTimestamp=1782845135,ClientImageSize=9842688;
constexpr uint32_t EngineTimestamp=1782845142,EngineImageSize=7598080;
constexpr MethodSlot ClientMethods[]={{3,0xb2360},{6,0xb14e0},{25,0xa8bf0}};
constexpr MethodSlot EngineMethods[]={{5,0x60680},{26,0x5f1a0},{28,0x5d350},{37,0x5d620},{38,0x5d630}};
constexpr unsigned ShutdownSlot=3,LevelShutdownSlot=6,RenderSlot=25;
constexpr unsigned ScreenSizeSlot=5,InGameSlot=26,LoadingSlot=28,WorldClipSlot=37,WorldViewSlot=38;
}
