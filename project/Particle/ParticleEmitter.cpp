#include "ParticleEmitter.h"
#include "ParticleManager.h"

ParticleEmitter::ParticleEmitter() = default;

ParticleEmitter::ParticleEmitter(const std::string& groupName, const Vector3& position)
    : groupName_(groupName)
    , position_(position)
{
}

void ParticleEmitter::Initialize()
{
    // 初期化処理が必要ならここに書く
}

void ParticleEmitter::Update()
{
    // パーティクルの位置を動かしたい場合など
}

void ParticleEmitter::SetGroupName(const std::string& name)
{
    groupName_ = name;
}

void ParticleEmitter::SetPosition(const Vector3& pos)
{
    position_ = pos;
}

void ParticleEmitter::Emit()
{
    // パーティクルを発生させる
    ParticleManager::GetInstance()->Emit(groupName_, position_, 10);
}
