#include "PostEffectType.h"

const char* GetPostEffectTypeName(PostEffectType type)
{
    switch (type) {
    case PostEffectType::Copy:
        return "Copy";
    case PostEffectType::GrayScale:
        return "GrayScale";
    case PostEffectType::Vignette:
        return "Vignette";
    case PostEffectType::smoothing:
        return "Smoothing";
    case PostEffectType::GaussianFilter:
        return "GaussianFilter";
    case PostEffectType::LuminanceBasedOutline:
        return "LuminanceBasedOutline";
    case PostEffectType::DepthOutline:
        return "DepthOutline";
    case PostEffectType::RadialBlur:
        return "RadialBlur";
    case PostEffectType::Dissolve:
        return "Dissolve";
    case PostEffectType::Random:
        return "Random";
    case PostEffectType::Bloom:
        return "Bloom";
    case PostEffectType::Outline:
        return "Outline";
    case PostEffectType::Fog:
        return "Fog";
    case PostEffectType::FocusLine:
        return "FocusLine";
    }

    return "Unknown";
}
