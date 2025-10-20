#pragma once
#include "DirectXCommon.h"
class ModelCommon {

public:
    DirectXCommon* GetDxCommon() { return dxCommon_; }
    void Initialize(DirectXCommon* dxCommon);

private:
    DirectXCommon* dxCommon_ = nullptr;
    // 初期化
};
