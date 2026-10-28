#pragma once
namespace dango {
// A SwapChain Present must never fall back to the implicit chain: that paints
// another window. Device Present can use the implicit chain if the Host query failed.
template<class Device, class Chain, class Surface, class Kind, class Result>
auto currentBackBuffer(Device* device, Chain* chain, bool swapPresent,
                       Kind mono, Surface** surface, Result missing)
    -> decltype(device->GetBackBuffer(0,0,mono,surface)) {
    if(chain)return chain->GetBackBuffer(0,mono,surface);
    if(swapPresent)return missing;
    return device->GetBackBuffer(0,0,mono,surface);
}
}
