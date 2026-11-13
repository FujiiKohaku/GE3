#pragma once
#include "Engine/Camera/Camera.h"
#include <cmath>
#include <cstring>
#include <memory>

// 未初期化メモリの内容によらず、生成直後から正しい投影行列を持つことを確認する。
inline bool RunCameraInitializationTests()
{
    for (int pattern : { 0x00, 0xCD, 0xFF }) {
        alignas(Camera) unsigned char storage[sizeof(Camera)];
        std::memset(storage, pattern, sizeof(storage));
        Camera* camera = std::construct_at(reinterpret_cast<Camera*>(storage));
        const Matrix4x4 expected = MatrixMath::MakePerspectiveFovMatrix(
            camera->GetFovY(), camera->GetAspectRatio(), camera->GetNearClip(), camera->GetFarClip());
        bool valid = true;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                const float actual = camera->GetProjectionMatrix().m[row][column];
                if (!std::isfinite(actual) || std::abs(actual - expected.m[row][column]) > 0.00001f) {
                    valid = false;
                }
                const float combined = camera->GetViewProjectionMatrix().m[row][column];
                const float unjittered = camera->GetUnjitteredViewProjectionMatrix().m[row][column];
                if (!std::isfinite(combined) || !std::isfinite(unjittered) || combined != unjittered) {
                    valid = false;
                }
            }
        }
        std::destroy_at(camera);
        if (!valid) { return false; }
    }
    return true;
}
