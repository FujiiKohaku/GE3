#pragma once
#include "../math/MathStruct.h"
#include "Joint.h"
#include <map>
#include <string>
#include <vector>
#include"../Animation/NodeAnimation.h"
#include <Object3DStruct.h>

struct Skeleton {
    int32_t root; // rootJointのインデックス
    std::map<std::string, int32_t> jointMap; // Joint名 → index
    std::vector<Joint> joints; // 所属しているジョイント

    // 生成
    static Skeleton CreateSkeleton(const Node& rootNode);

    // 更新
    void UpdateSkeleton();
};
