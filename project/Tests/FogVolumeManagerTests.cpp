#include "Engine/PostEffect/Volumetric/FogVolumeManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

void TestPlacementAndValidation()
{
    FogVolumeManager fogVolumeManager;
    FogVolumeSettings settings;
    settings.isEnabled = true;
    settings.center = { 1.0f, 2.0f, 3.0f };
    assert(fogVolumeManager.SetVolume(0, settings));
    settings.center.x = 9.0f;
    assert(fogVolumeManager.GetVolumes()[0].center.x == 1.0f);
    settings.density = std::numeric_limits<float>::quiet_NaN();
    assert(!fogVolumeManager.SetVolume(0, settings));
    assert(fogVolumeManager.GetVolumes()[0].density == 0.01f);
    settings = {};
    settings.shape = FogVolumeShape::Box;
    settings.edgeSoftness = 11.0f;
    assert(!fogVolumeManager.SetVolume(0, settings));
    settings.edgeSoftness = 1.0f;
    assert(fogVolumeManager.SetVolume(7, settings));
    assert(!fogVolumeManager.SetVolume(8, settings));
    assert(!fogVolumeManager.SetHeightFog(0.0f, -1.0f, 0.1f));
    assert(!fogVolumeManager.SetNoiseParameters(0.0f, 1.0f, {}));
    assert(!fogVolumeManager.SetColor({ std::numeric_limits<float>::infinity(), 0.0f, 0.0f }));
    fogVolumeManager.ClearVolumes();
    assert(!fogVolumeManager.GetVolumes()[0].isEnabled);
}

void TestPresetRollbackAndReset()
{
    FogVolumeManager fogVolumeManager;
    FogPreset preset;
    preset.parameters.isEnabled = true;
    preset.parameters.color = { 0.4f, 0.5f, 0.6f };
    preset.parameters.noiseVelocity = { 1.0f, 0.0f, 0.0f };
    preset.volumes[0].isEnabled = true;
    assert(fogVolumeManager.ApplyPreset(preset));
    fogVolumeManager.UpdateNoise(0.1f);
    const float previousNoiseOffset = fogVolumeManager.GetNoiseOffset().x;
    const uint64_t previousRevision = fogVolumeManager.GetResetRevision();
    preset.parameters.color.x = 0.9f;
    preset.volumes[7].edgeSoftness = -1.0f;
    assert(!fogVolumeManager.ApplyPreset(preset));
    assert(fogVolumeManager.GetParameters().color.x == 0.4f);
    assert(fogVolumeManager.GetVolumes()[0].isEnabled);
    assert(fogVolumeManager.GetNoiseOffset().x == previousNoiseOffset);
    assert(fogVolumeManager.GetResetRevision() == previousRevision);
    fogVolumeManager.SetEnabled(true);
    assert(fogVolumeManager.GetResetRevision() == previousRevision);
    fogVolumeManager.SetEnabled(false);
    assert(fogVolumeManager.GetResetRevision() != previousRevision);
    fogVolumeManager.Reset();
    assert(!fogVolumeManager.GetParameters().isEnabled);
    assert(!fogVolumeManager.GetVolumes()[0].isEnabled);
    assert(fogVolumeManager.GetNoiseOffset().x == 0.0f);
    assert(fogVolumeManager.ApplyPreset({}));
    assert(!fogVolumeManager.GetParameters().isEnabled);
}

void TestNoiseTimeAndIndependentInstances()
{
    FogVolumeManager firstManager;
    FogVolumeManager secondManager;
    assert(firstManager.SetNoiseParameters(1.0f, 0.5f, { 100.0f, -100.0f, 0.0f }));
    firstManager.UpdateNoise(1.0f);
    assert(firstManager.GetNoiseOffset().x == 10.0f);
    firstManager.UpdateNoise(-1.0f);
    firstManager.UpdateNoise(std::numeric_limits<float>::quiet_NaN());
    assert(firstManager.GetNoiseOffset().x == 10.0f);
    assert(secondManager.GetNoiseOffset().x == 0.0f);
    for (int frameIndex = 0; frameIndex < 100; ++frameIndex) { firstManager.UpdateNoise(0.1f); }
    assert(std::abs(firstManager.GetNoiseOffset().x) < 256.0f);
    assert(std::abs(firstManager.GetNoiseOffset().y) < 256.0f);
}

int main()
{
    TestPlacementAndValidation();
    TestPresetRollbackAndReset();
    TestNoiseTimeAndIndependentInstances();
    std::puts("PASS: fog placement, validation, preset rollback, reset revision, noise time and independent instances without GPU");
}
