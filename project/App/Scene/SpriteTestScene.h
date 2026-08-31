#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include <array>
#include <cstddef>
#include <memory>

class SpriteTestScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    void ChangeDemo(int direction);
    void ApplySelectedDemo();

private:
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> previewFrameSprite_;
    std::unique_ptr<Sprite> previewSprite_;
    std::size_t selectedDemoIndex_ = 0;

    const std::array<const char*, 70> materialFolders_ = {
        "Standard",
        "Wave",
        "Shake",
        "Pulse",
        "Twist",
        "Shear",
        "Bounce",
        "Ripple",
        "Jelly",
        "Grayscale",
        "Invert",
        "UVWave",
        "Pixelate",
        "Vignette",
        "Scanline",
        "Chromatic",
        "Dissolve",
        "Posterize",
        "RadialPulse",
        "ColorCycle",
        "Swing",
        "Squash",
        "Curtain",
        "Spiral",
        "Bulge",
        "Pinch",
        "PageCurl",
        "Wind",
        "Orbit",
        "Flip3D",
        "Elastic",
        "SineScale",
        "CornerPin",
        "Slant",
        "Breathing",
        "Sepia",
        "Threshold",
        "Neon",
        "Mosaic",
        "Mirror",
        "Kaleidoscope",
        "CRT",
        "Glitch",
        "Noise",
        "Fade",
        "Border",
        "CircleMask",
        "CheckerFade",
        "HeatHaze",
        "RainbowWave",
        "Shockwave",
        "DiamondWarp",
        "FanFold",
        "Pendulum",
        "Pop",
        "Ribbon",
        "VerticalStretch",
        "FishEyeMesh",
        "CornerWave",
        "WaveZoom",
        "Hologram",
        "Halftone",
        "Emboss",
        "EdgeDetect",
        "Toon",
        "HSVShift",
        "DropShadow",
        "Glow",
        "Blur",
        "OldFilm"
    };
};
