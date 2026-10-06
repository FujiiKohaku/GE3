#pragma once
#include "Model.h"
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

class ModelManager {
public:
    // 追加：他Managerと揃える用
    void Initialize(DirectXCommon* dxCommon);

    // インスタンス取得
    static ModelManager* GetInstance();

    // 終了処理
    static void Finalize();

    Model* Load(const std::string& filepath);

    Model* CreatePlane(const std::string& texturePath = "", float tilingX = 1.0f, float tilingY = 1.0f);

    Model* CreateCube(const std::string& texturePath = "");
    // Thin book geometry with independent textures for the front and back.
    Model* CreateBookLeaf(
        const std::string& frontTexturePath,
        const std::string& backTexturePath,
        uint32_t stripIndex = 0,
        uint32_t stripCount = 1);

    Model* CreateCylinder(const std::string& texturePath = "", uint32_t divisions = 32);

    Model* CreateBeamCross(const std::string& texturePath = "");

    Model* FindModel(const std::string& filePath);

    // Passkey
    class ConstructorKey {
        ConstructorKey() = default;
        friend class ModelManager;
    };
    explicit ModelManager(ConstructorKey);
    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(const ModelManager&) = delete;

private:
    static std::unique_ptr<ModelManager> instance_;

    ModelManager& operator=(const ModelManager&) = delete;
    std::unordered_map<std::string, std::unique_ptr<Model>> models_;
    std::unique_ptr<ModelCommon> modelCommon_;
};
