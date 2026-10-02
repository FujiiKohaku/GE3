// Filmic shoulder preserves differences between HDR highlights before sRGB output.
float3 FinishColor(float3 hdr)
{
    float3 color = max(hdr, 0.0f) * max(toneExposure, 0.0f);
    if (toneMapEnabled != 0) {
        color = (color * (2.51f * color + 0.03f)) /
            (color * (2.43f * color + 0.59f) + 0.14f);
    }
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(luma.xxx, color, toneSaturation);
    return saturate((color - 0.18f) * toneContrast + 0.18f);
}
