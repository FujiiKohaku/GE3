#pragma once

#include "App/Game/Stage/StageCatalog.h"
#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <memory>
#include <vector>

class StageSelectScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    struct StageCard {
        std::unique_ptr<Sprite> background;
        std::unique_ptr<Text> numberText;
        std::unique_ptr<Text> nameText;
    };

    bool IsMouseOverCard(size_t visibleIndex) const;
    void StartSelectedStage();
    void RefreshCards();

    std::vector<StageSettings> stages_;
    std::vector<StageCard> cards_;
    std::unique_ptr<Sprite> background_;
    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> descriptionText_;
    std::unique_ptr<Text> helpText_;
    std::unique_ptr<Text> toolsText_;
    size_t selectedIndex_ = 0;
    size_t currentPage_ = 0;
};
