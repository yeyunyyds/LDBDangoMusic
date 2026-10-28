#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include "json.hpp"
namespace dango {
struct CameraSample {
 uint64_t tick=0,chain=0;uint32_t generation=0,path=0;
 int32_t viewResult=0,projectionResult=0,viewportResult=0;
 std::array<float,16> view{},projection{};std::array<float,6> viewport{};
};
// Host Present callbacks are produced on its render thread; only the background
// music worker consumes. No allocations, file IO or mutex on the producer path.
template<size_t Capacity=64> class CameraQueue {
 std::array<CameraSample,Capacity> samples{};std::atomic<size_t> read{0},write{0};
public:
 std::atomic<uint64_t> dropped{0};
 bool push(const CameraSample& sample){auto w=write.load(std::memory_order_relaxed),next=(w+1)%Capacity;if(next==read.load(std::memory_order_acquire)){++dropped;return false;}samples[w]=sample;write.store(next,std::memory_order_release);return true;}
 bool pop(CameraSample& sample){auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return false;sample=samples[r];read.store((r+1)%Capacity,std::memory_order_release);return true;}
};
inline bool finiteMatrix(const std::array<float,16>& matrix){for(float f:matrix)if(!std::isfinite(f))return false;return true;}
inline bool identityMatrix(const std::array<float,16>& matrix){for(size_t i=0;i<16;++i)if(!std::isfinite(matrix[i])||std::abs(matrix[i]-(i%5==0?1.f:0.f))>1e-4f)return false;return true;}
inline nlohmann::json cameraJson(const CameraSample& s){return {
 {"schema",1},{"capture","before_plugin_render"},{"tick_ms",s.tick},{"chain_id",s.chain},{"device_generation",s.generation},{"present_path",s.path?"SwapChain":"Device"},
 {"view_hresult",s.viewResult},{"projection_hresult",s.projectionResult},{"viewport_hresult",s.viewportResult},
 {"view",s.view},{"projection",s.projection},{"viewport",s.viewport},
 {"view_finite",finiteMatrix(s.view)},{"projection_finite",finiteMatrix(s.projection)},
 {"view_identity",identityMatrix(s.view)},{"projection_identity",identityMatrix(s.projection)},
 {"camera_confirmed",false}};}
}
