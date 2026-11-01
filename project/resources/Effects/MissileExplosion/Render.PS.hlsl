#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    // Surface patterns use 3D normals, with no camera-facing card or UV cutout.
    float32_t3 normal = normalize(input.normal);
    float phase = (1.0f - input.color.a) * 7.0f;
    float32_t3 warped = normal * 7.0f + float32_t3(
        sin(normal.y * 8.0f + phase), sin(normal.z * 7.0f - phase), sin(normal.x * 9.0f + phase));
    float turbulence = sin(warped.x + phase) * sin(warped.y - phase * 1.3f) * sin(warped.z + phase * 0.7f);
    float heat = smoothstep(-0.7f, 0.7f, turbulence);
    float shading = 0.18f + 0.82f * saturate(dot(normal, normalize(float32_t3(-0.4f, 0.75f, -0.5f))));
    float32_t3 emberColor = float32_t3(0.9f, 0.08f, 0.005f);
    float32_t4 color;
    color.rgb = lerp(emberColor, input.color.rgb * 1.2f, heat) * shading;
    color.a = input.color.a * (0.4f + 0.4f * heat);
    // Keep the inside visible without doubling the outside's brightness.
    if (input.texcoord.x < 0.0f) { color.a *= 0.08f; }
    color *= gMaterial.color;
    color = ApplyParticleFog(color, input);
    return MakeParticlePixelOutput(color);
}
