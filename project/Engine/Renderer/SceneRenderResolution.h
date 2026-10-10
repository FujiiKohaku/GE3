#pragma once
#include "Engine/Winapp/WinApp.h"
// One active renderer, like DirectXCommon. Change only before recording a frame,
// after the previous frame's fence; display and shadow-map sizes are independent.
class SceneRenderResolution {
public:
    static int GetWidth() { return width_; }
    static int GetHeight() { return height_; }
    static void SetSize(int width, int height) { width_ = width; height_ = height; }
private:
    inline static int width_ = WinApp::kClientWidth;
    inline static int height_ = WinApp::kClientHeight;
};
