#pragma once
#include "Engine/math/EngineStruct.h"

class BoxCollider {
public:
    void SetCenter(const Vector3& center) { center_ = center; }
    void SetSize(const Vector3& size) { size_ = size; }

    const Vector3& GetCenter() const { return center_; }
    const Vector3& GetSize() const { return size_; }

private:
    Vector3 center_ = { 0.0f, 0.0f, 0.0f };
    Vector3 size_ = { 1.0f, 1.0f, 1.0f };
};
