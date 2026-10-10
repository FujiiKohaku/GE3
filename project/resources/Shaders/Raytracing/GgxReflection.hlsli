#ifndef KOHAKU_GGX_REFLECTION
#define KOHAKU_GGX_REFLECTION
// Isotropic visible GGX sampling, Heitz 2018: https://jcgt.org/published/0007/04/01/
struct GgxVisibleFrame { float3 view; float3 tangent; float3 bitangent; float alpha; };
GgxVisibleFrame BuildGgxVisibleFrame(float3 localView, float roughness) {
    GgxVisibleFrame frame;
    frame.alpha = max(roughness * roughness, 0.0001f);
    frame.view = normalize(float3(localView.xy * frame.alpha, max(localView.z, 0.000001f)));
    float lengthSquared = dot(frame.view.xy, frame.view.xy);
    frame.tangent = float3(1, 0, 0);
    if (lengthSquared > 0) { frame.tangent = float3(-frame.view.y, frame.view.x, 0) * rsqrt(lengthSquared); }
    frame.bitangent = cross(frame.view, frame.tangent);
    return frame;
}
float3 SampleGgxVisibleNormal(GgxVisibleFrame frame, float2 sample) {
    float radius = sqrt(sample.x);
    float angle = 6.2831853f * sample.y;
    float diskX = radius * cos(angle);
    float blend = 0.5f * (1 + frame.view.z);
    float diskY = (1 - blend) * sqrt(max(1 - diskX * diskX, 0)) + blend * radius * sin(angle);
    float3 hemisphere = diskX * frame.tangent + diskY * frame.bitangent
        + sqrt(max(1 - diskX * diskX - diskY * diskY, 0)) * frame.view;
    return normalize(float3(frame.alpha * hemisphere.xy, max(hemisphere.z, 0)));
}
float GgxSmithVisibility(float cosine, float alpha) {
    if (cosine <= 0) { return 0; }
    float alphaSquared = alpha * alpha;
    return 2 * cosine / max(cosine + sqrt(alphaSquared + (1 - alphaSquared) * cosine * cosine), 0.000001f);
}
#endif
