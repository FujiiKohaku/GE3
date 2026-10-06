#ifndef KOHAKU_ATMOSPHERE_SCATTERING
#define KOHAKU_ATMOSPHERE_SCATTERING
// 単一散乱の近似。Rayleighの波長差とMieの前方散乱を共有する。
float3 AtmosphereRadiance(float3 direction, float3 sunDirection, float3 sunColor, float sunIntensity, float anisotropy) {
    float cosine = clamp(dot(direction, sunDirection), -1.0f, 1.0f);
    float rayleighPhase = 0.75f * (1.0f + cosine * cosine);
    float g = clamp(anisotropy, 0.0f, 0.9f);
    float miePhase = (1.0f - g * g) / max(pow(1.0f + g * g - 2.0f * g * cosine, 1.5f), 0.01f);
    float sunElevation = sunDirection.y;
    float daylight = smoothstep(-0.12f, 0.10f, sunElevation);
    float airMass = 1.0f / max(abs(direction.y) + 0.18f, 0.18f);
    float sunAirMass = 1.0f / max(sunElevation + 0.12f, 0.08f);
    float3 wavelength = float3(0.24f, 0.56f, 1.0f);
    float3 sunlight = exp(-wavelength * sunAirMass * 0.08f) * sunColor * max(sunIntensity, 0.0f);
    float3 rayleigh = (1.0f - exp(-wavelength * airMass * 0.35f)) * rayleighPhase;
    float3 mie = float3(1.0f, 0.85f, 0.66f) * miePhase * 0.025f;
    return float3(0.01f, 0.02f, 0.05f) + daylight * sunlight * (rayleigh + mie);
}
#endif
