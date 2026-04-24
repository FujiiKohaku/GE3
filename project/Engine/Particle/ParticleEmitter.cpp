#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include <cmath>

void ParticleEmitter::Initialize()
{
    randomEngine_ = std::mt19937(seedGenerator_());
}

Particle ParticleEmitter::MakeParticleDefault(const Vector3& position)
{
    Particle particle {};

    std::uniform_real_distribution<float> randomOffset(-0.05f, 0.05f);
    std::uniform_real_distribution<float> randomDirection(-1.0f, 1.0f);
    std::uniform_real_distribution<float> randomSpeed(1.0f, 1.5f);
    std::uniform_real_distribution<float> randomLife(0.8f, 1.0f);

    particle.transform.translate = {
        position.x + randomOffset(randomEngine_),
        position.y + randomOffset(randomEngine_),
        position.z + randomOffset(randomEngine_)
    };

    particle.transform.scale = { 10.3f, 10.3f, 10.3f };
    particle.transform.rotate = { 0.0f, 0.0f, 0.0f };

    Vector3 velocityDirection = {
        randomDirection(randomEngine_),
        randomDirection(randomEngine_),
        randomDirection(randomEngine_)
    };

    float velocityLength = std::sqrt(
        velocityDirection.x * velocityDirection.x + velocityDirection.y * velocityDirection.y + velocityDirection.z * velocityDirection.z);

    if (velocityLength > 0.0f) {
        velocityDirection.x /= velocityLength;
        velocityDirection.y /= velocityLength;
        velocityDirection.z /= velocityLength;
    }

    float speed = randomSpeed(randomEngine_);

    particle.velocity = {
        velocityDirection.x * speed,
        velocityDirection.y * speed,
        velocityDirection.z * speed
    };

    particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    particle.lifeTime = randomLife(randomEngine_);
    particle.currentTime = 0.0f;

    return particle;
}
Particle ParticleEmitter::MakeNewParticleAttack(const Vector3& position)
{
    Particle particle {};

    std::uniform_real_distribution<float> randomOffset(-0.05f, 0.05f);
    std::uniform_real_distribution<float> randomRotate(-3.14159f, 3.14159f);
    std::uniform_real_distribution<float> randomScaleY(4.0f, 15.0f);
    std::uniform_real_distribution<float> randomLife(0.15f, 0.3f);

    particle.transform.translate = {
        position.x + randomOffset(randomEngine_),
        position.y + randomOffset(randomEngine_),
        position.z + randomOffset(randomEngine_)
    };

    particle.transform.scale = {0.05f,randomScaleY(randomEngine_),1.0f
    };

    particle.transform.rotate = {0.0f,0.0f,randomRotate(randomEngine_)
    };

    particle.velocity = {0.0f,0.0f,0.0f
    };

    particle.color = {1.0f,
        1.0f,
        1.0f,
        1.0f
    };

    particle.lifeTime = randomLife(randomEngine_);
    particle.currentTime = 0.0f;

    return particle;
}
Particle ParticleEmitter::MakeFireParticle(const Vector3& position)
{
    Particle particle {};

    std::uniform_real_distribution<float> randomXZ(-0.1f, 0.1f);
    std::uniform_real_distribution<float> randomUp(0.3f, 0.6f);
    std::uniform_real_distribution<float> randomLife(0.5f, 1.0f);

    particle.transform.translate = {
        position.x + randomXZ(randomEngine_),
        position.y,
        position.z + randomXZ(randomEngine_)
    };

    float scale = 10.1f;
    particle.transform.scale = { scale * 0.5f, scale * 2.0f, scale * 0.5f };
    particle.transform.rotate = { 0.0f, 0.0f, 0.0f };

    particle.velocity = {
        randomXZ(randomEngine_) * 0.1f,
        randomUp(randomEngine_),
        randomXZ(randomEngine_) * 0.1f
    };

    particle.color = { 1.0f, 0.4f, 0.0f, 1.0f };
    particle.lifeTime = randomLife(randomEngine_);
    particle.currentTime = 0.0f;

    return particle;
}