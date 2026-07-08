#pragma once
enum class PostEffectType {
    Copy,
    GrayScale,
    Vignette,
    smoothing,
    GaussianFilter,
    LuminanceBasedOutline,
    DepthOutline,
    RadialBlur,
    Dissolve,
    Random,
    Bloom,
    Outline,
    Fog,
    FocusLine,
};

const char* GetPostEffectTypeName(PostEffectType type);
