#include "App/Game/Enemy/Types/ArmoredEnemy.h"

void ArmoredEnemy::Initialize(
    Model* model,
    Model* bulletModel,
    Player* player)
{
    NormalEnemy::Initialize(
        model,
        bulletModel,
        player);

    hp_ = 5.0f;
    transform_.scale = { 2.6f, 2.6f, 2.6f };

    if (object_ != nullptr) {
        object_->SetScale(transform_.scale);
        object_->SetColor({ 0.38f, 0.42f, 0.50f, 1.0f });
        object_->SetEnableLighting(true);
        object_->Update();
    }
}
