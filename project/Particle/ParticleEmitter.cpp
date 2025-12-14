#include "ParticleEmitter.h"
#include "ParticleManager.h"

static constexpr float kDeltaTime = 0.1f;

ParticleEmitter::ParticleEmitter()
{
    // 未初期化状態
}

void ParticleEmitter::Init(
    const std::string& groupName,
    const Transform& transform,
    uint32_t count,
    float frequency)
{
    name_ = groupName;
    transform_ = transform;
    count_ = count;
    frequency_ = frequency;
    elapsedTime_ = 0.0f;
}

void ParticleEmitter::Update()
{
    if (frequency_ <= 0.0f) {
        return; // Init 前ガード
    }

    elapsedTime_ += kDeltaTime;

    if (elapsedTime_ >= frequency_) {
        ParticleManager::GetInstance()->Emit(
            name_,
            transform_.translate,
            count_);

        elapsedTime_ -= frequency_;
    }
}

void ParticleEmitter::Emit()
{
    if (count_ == 0) {
        return; // Init 前ガード
    }

    ParticleManager::GetInstance()->Emit(
        name_,
        transform_.translate,
        count_);
}
