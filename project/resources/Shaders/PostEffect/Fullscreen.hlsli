struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};
cbuffer PostEffectParameter : register(b0)
{
    float grayScaleStrength;
    float vignetteStrength;
    float outlineScale;
    float time;
    float2 radialBlurCenter;
    int radialBlurSampleCount;
    float radialBlurWidth;
    float dissolveThreshold;
    float dissolveEdgeWidth;
    float dissolveEdgeStrength;
    float dissolvePadding;
    float boostKickStrength;
    float pixelSize;
    float colorBrightness;
    float colorContrast;
    float colorSaturation;
    float padding0;
    float padding1;
    float padding2;
    float focusDepth;
    float focusRange;
    float depthOfFieldRadius;
    float motionBlurStrength;
    float2 motionBlurDirection;
    int motionBlurSampleCount;
    float chromaticAberrationStrength;
    float lensDistortionStrength;
    float filmGrainStrength;
    float lensDirtStrength;
    float cameraShakeStrength;
    float bokehRadius;
    int bokehSides;
    float fisheyeStrength;
    int animationEnabled;
    float lightThreshold;
    float lightStrength;
    float lightRadius;
    float lightAngle;
    float paintProgress;
    float paintIntensity;
    float paintSeed;
    int paintPatternType;
    float3 paintColor;
    float sonicBoomProgress;
    float2 sonicBoomCenter; // 自機の画面上位置(0.0〜1.0)を中心発生源として指定
    float2 paddingSonicBoom;
};
