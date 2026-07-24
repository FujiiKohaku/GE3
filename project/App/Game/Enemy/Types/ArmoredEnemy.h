#pragma once

#include "App/Game/Enemy/Types/NormalEnemy.h"

class Model;
class Player;

class ArmoredEnemy : public NormalEnemy {
public:
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player);
};
