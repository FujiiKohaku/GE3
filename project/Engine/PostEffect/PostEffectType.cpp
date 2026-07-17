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
    case PostEffectType::DepthOfField:
        return "DepthOfField";
    case PostEffectType::MotionBlur:
        return "MotionBlur";
    case PostEffectType::ChromaticAberration:
        return "ChromaticAberration";
    case PostEffectType::LensDistortion:
        return "LensDistortion";
    case PostEffectType::FilmGrain:
        return "FilmGrain";
    case PostEffectType::LensDirt:
        return "LensDirt";
    case PostEffectType::CameraShake:
        return "CameraShake";
    case PostEffectType::BokehShape:
        return "BokehShape";
    case PostEffectType::Fisheye:
        return "Fisheye";
    case PostEffectType::Pixelate:
        return "Pixelate";
    case PostEffectType::ColorAdjust:
        return "ColorAdjust";
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
    case PostEffectType::LensFlare: return "LensFlare";
    case PostEffectType::Glare: return "Glare";
    case PostEffectType::LightShafts: return "LightShafts";
    case PostEffectType::VolumetricLight: return "VolumetricLight";
    case PostEffectType::AnamorphicFlare: return "AnamorphicFlare";
    case PostEffectType::Halo: return "Halo";
    case PostEffectType::LightStreak: return "LightStreak";
    case PostEffectType::NeonGlow: return "NeonGlow";
    case PostEffectType::GhostImage: return "GhostImage";
    case PostEffectType::Outline:
        return "Outline";
    case PostEffectType::Fog:
        return "Fog";
    case PostEffectType::FocusLine:
        return "FocusLine";
    }

    return "Unknown";
}
