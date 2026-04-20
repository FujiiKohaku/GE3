// BlendUtil.h
#pragma once
#include <d3d12.h>

enum BlendMode {
    kBlendModeNone,
    kBlendModeNormal,
    kBlendModeAdd,
    kBlendModeSubtract,
    kBlendModeMultiply,
    kBlendModeScreen,
    kCountOfBlendMode
};

D3D12_BLEND_DESC CreateBlendDesc(BlendMode mode);
