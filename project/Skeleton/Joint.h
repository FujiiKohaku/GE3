#pragma once
#include "../math/MathStruct.h"
#include <string>
#include <vector>
#include <Object3DStruct.h>
#include <optional>
#include"../math/MatrixMath.h"
struct Joint {
    QuaternionTransform transform; // Transform情報
    Matrix4x4 localMatrix; // ローカル行列
    Matrix4x4 skeletonSpaceMatrix; // スケルトンスペース行列
    std::string name;//名前
    std::vector<int32_t> children; // 子Joint配列
    int32_t index; // 自身のインデックス
    std::optional<int32_t> parent; // 親Jointのインデックス（親がいない場合はstd::nullopt）

};
int32_t CreateJoint(const Node& node,const std::optional<int32_t>& parent,std::vector<Joint>& joints);