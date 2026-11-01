#ifndef KOHAKU_RT_LOCAL_SHADOW_SELECTION
#define KOHAKU_RT_LOCAL_SHADOW_SELECTION
cbuffer RtLocalShadowSettings : register(b9) {
    uint4 rtLocalLightMasks;
    float4 rtLocalShadowControls;
};
cbuffer RtLocalShadowReceiver : register(b10) { uint shouldReceiveRtLocalShadow; };
static float3 capturedRtLocalLight = 0;
#endif
