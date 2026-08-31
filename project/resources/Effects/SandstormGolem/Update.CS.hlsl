#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);
cbuffer SkeletonPose : register(b3)
{
    float32_t4 gJointPositions[18];
    uint32_t gJointCount;
    float32_t3 gSkeletonPadding;
}

float Hash(uint32_t id, float salt)
{
    return frac(
        sin((float32_t(id) + 1.0f) * (12.9898f + salt)) *
        43758.5453f);
}

float32_t3 GetBoneSegmentTarget(
    float32_t3 segmentStart,
    float32_t3 segmentEnd,
    float segmentRate,
    float radius,
    float radialAngle,
    float radialRate)
{
    float32_t3 boneAxis = segmentEnd - segmentStart;
    float boneLength = length(boneAxis);
    if (boneLength <= 0.0001f)
    {
        return segmentStart;
    }
    boneAxis /= boneLength;

    float32_t3 referenceAxis = float32_t3(0.0f, 1.0f, 0.0f);
    if (abs(dot(boneAxis, referenceAxis)) >= 0.92f)
    {
        referenceAxis = float32_t3(1.0f, 0.0f, 0.0f);
    }
    float32_t3 radialAxisX =
        normalize(cross(boneAxis, referenceAxis));
    float32_t3 radialAxisY =
        normalize(cross(boneAxis, radialAxisX));
    float32_t radialDistance = radius * sqrt(radialRate);
    float32_t3 radialOffset =
        radialAxisX * cos(radialAngle) * radialDistance +
        radialAxisY * sin(radialAngle) * radialDistance;

    return
        lerp(segmentStart, segmentEnd, segmentRate) +
        radialOffset;
}

float32_t3 GetGolemTarget(uint32_t particleIndex)
{
    uint32_t group = particleIndex % 128;
    float randomA = Hash(particleIndex, 1.17f);
    float randomB = Hash(particleIndex, 4.83f);
    float randomC = Hash(particleIndex, 9.41f);
    float32_t3 target = float32_t3(0.0f, 3.5f, 0.0f);

    if (gJointCount >= 18)
    {
        float headLayerScale =
            lerp(0.28f, 1.0f, pow(randomC, 0.333333f));

        float radialAngle = randomB * 6.28318530718f;
        float32_t3 faceRight =
            gJointPositions[9].xyz -
            gJointPositions[6].xyz;
        faceRight =
            normalize(faceRight + float32_t3(0.001f, 0.0f, 0.0f));
        float32_t3 faceUp =
            gJointPositions[5].xyz -
            gJointPositions[4].xyz;
        faceUp =
            normalize(faceUp + float32_t3(0.0f, 0.001f, 0.0f));
        float32_t3 faceForward =
            normalize(cross(faceRight, faceUp));
        if (group < 36)
        {
            target =
                GetBoneSegmentTarget(
                    gJointPositions[1].xyz,
                    gJointPositions[3].xyz,
                    randomA,
                    lerp(0.34f, 0.62f, randomA),
                    radialAngle,
                    randomC);
        }
        else if (group < 42)
        {
            target =
                GetBoneSegmentTarget(
                    gJointPositions[4].xyz,
                    gJointPositions[5].xyz,
                    randomA,
                    lerp(0.22f, 0.30f, randomA),
                    radialAngle,
                    randomC);
        }
        else if (group < 50)
        {
            float longitude = randomA * 6.28318530718f;
            float latitude =
                (randomB - 0.5f) * 3.14159265359f;
            float32_t3 headDirection =
                faceRight * cos(latitude) * cos(longitude) +
                faceUp * sin(latitude) +
                faceForward * cos(latitude) * sin(longitude);
            target =
                gJointPositions[5].xyz +
                faceRight *
                    dot(headDirection, faceRight) *
                    0.38f *
                    headLayerScale +
                faceUp *
                    dot(headDirection, faceUp) *
                    0.46f *
                    headLayerScale +
                faceForward *
                    dot(headDirection, faceForward) *
                    0.34f *
                    headLayerScale;
        }
        else if (group < 64)
        {
            if (randomA < 0.5f)
            {
                target =
                    GetBoneSegmentTarget(
                        gJointPositions[6].xyz,
                        gJointPositions[7].xyz,
                        randomA * 2.0f,
                        0.24f,
                        radialAngle,
                        randomC);
            }
            else
            {
                target =
                    GetBoneSegmentTarget(
                        gJointPositions[7].xyz,
                        gJointPositions[8].xyz,
                        (randomA - 0.5f) * 2.0f,
                        0.22f,
                        radialAngle,
                        randomC);
            }
        }
        else if (group < 82)
        {
            if (randomA < 0.5f)
            {
                target =
                    GetBoneSegmentTarget(
                        gJointPositions[9].xyz,
                        gJointPositions[10].xyz,
                        randomA * 2.0f,
                        0.24f,
                        radialAngle,
                        randomC);
            }
            else
            {
                target =
                    GetBoneSegmentTarget(
                        gJointPositions[10].xyz,
                        gJointPositions[11].xyz,
                        (randomA - 0.5f) * 2.0f,
                        0.22f,
                        radialAngle,
                        randomC);
            }
        }
        else if (group < 91)
        {
            if (randomA < 0.5f)
            {
                target =
                    GetBoneSegmentTarget(
                        gJointPositions[12].xyz,
                        gJointPositions[13].xyz,
                        randomA * 2.0f,
                        0.30f,
                        radialAngle,
                        randomC);
            }
            else
            {
                target =
                    GetBoneSegmentTarget(
                        gJointPositions[13].xyz,
                        gJointPositions[14].xyz,
                        (randomA - 0.5f) * 2.0f,
                        0.27f,
                        radialAngle,
                        randomC);
            }
        }
        else if (group < 100)
        {
            if (randomA < 0.5f)
            {
                target =
                    GetBoneSegmentTarget(
                        gJointPositions[15].xyz,
                        gJointPositions[16].xyz,
                        randomA * 2.0f,
                        0.30f,
                        radialAngle,
                        randomC);
            }
            else
            {
                target =
                    GetBoneSegmentTarget(
                        gJointPositions[16].xyz,
                        gJointPositions[17].xyz,
                        (randomA - 0.5f) * 2.0f,
                        0.27f,
                        radialAngle,
                        randomC);
            }
        }
        else if (group < 112)
        {
            target =
                GetBoneSegmentTarget(
                    gJointPositions[1].xyz,
                    gJointPositions[3].xyz,
                    randomA,
                    lerp(0.34f, 0.48f, randomA),
                    radialAngle,
                    randomC);
        }
        else if (group < 116)
        {
            float eyeSide = -1.0f;
            if ((group % 2) == 0)
            {
                eyeSide = 1.0f;
            }
            target =
                gJointPositions[5].xyz +
                faceRight * eyeSide * 0.18f +
                faceUp * 0.06f -
                faceForward * 0.30f;
        }
        else if (group < 120)
        {
            target =
                gJointPositions[3].xyz +
                faceRight * (randomA - 0.5f) * 0.24f +
                faceUp * (randomB - 0.5f) * 0.24f -
                faceForward * 0.25f;
        }
        else
        {
            target =
                GetBoneSegmentTarget(
                    gJointPositions[1].xyz,
                    gJointPositions[3].xyz,
                    randomA,
                    0.46f,
                    radialAngle,
                    randomC);
        }

        return target;
    }

    if (group < 36)
    {
        float bodyHeight = randomA * 2.9f + 2.15f;
        float shoulderRate =
            smoothstep(2.3f, 4.9f, bodyHeight);
        float halfWidth = lerp(0.62f, 1.18f, shoulderRate);
        target =
            float32_t3(
                (randomB * 2.0f - 1.0f) * halfWidth,
                bodyHeight,
                (randomC * 2.0f - 1.0f) * 0.38f);
    }
    else if (group < 50)
    {
        float32_t3 headDirection =
            float32_t3(
                randomA * 2.0f - 1.0f,
                randomB * 2.0f - 1.0f,
                randomC * 2.0f - 1.0f);
        headDirection =
            normalize(headDirection + float32_t3(0.001f, 0.0f, 0.0f));
        target =
            float32_t3(0.0f, 5.85f, 0.0f) +
            headDirection * lerp(0.52f, 0.88f, randomC);
    }
    else if (group < 64)
    {
        float armRate = randomA;
        target =
            lerp(
                float32_t3(-0.86f, 4.72f, 0.0f),
                float32_t3(-1.82f, 2.72f, 0.02f),
                armRate);
        target +=
            float32_t3(
                randomB * 0.34f - 0.17f,
                randomC * 0.28f - 0.14f,
                randomC * 0.32f - 0.16f);
    }
    else if (group < 82)
    {
        float armRate = randomA;
        target =
            lerp(
                float32_t3(0.86f, 4.72f, 0.0f),
                float32_t3(2.05f, 3.05f, -0.04f),
                armRate);
        target +=
            float32_t3(
                randomB * 0.38f - 0.19f,
                randomC * 0.30f - 0.15f,
                randomC * 0.36f - 0.18f);
    }
    else if (group < 100)
    {
        float legRate = randomA;
        float side = -1.0f;
        if ((group % 2) == 0)
        {
            side = 1.0f;
        }
        target =
            lerp(
                float32_t3(side * 0.48f, 2.28f, 0.0f),
                float32_t3(side * 0.80f, 0.12f, -0.08f),
                legRate);
        target +=
            float32_t3(
                randomB * 0.34f - 0.17f,
                randomC * 0.18f,
                randomC * 0.34f - 0.17f);
    }
    else if (group < 112)
    {
        float auraAngle = randomA * 6.28318530718f;
        float auraRadius = lerp(1.35f, 2.45f, randomB);
        target =
            float32_t3(
                cos(auraAngle) * auraRadius,
                lerp(0.20f, 5.50f, randomC),
                sin(auraAngle) * auraRadius);
    }
    else if (group < 116)
    {
        float eyeSide = -1.0f;
        if ((group % 2) == 0)
        {
            eyeSide = 1.0f;
        }
        target =
            float32_t3(
                eyeSide * 0.31f + (randomA - 0.5f) * 0.10f,
                5.94f + (randomB - 0.5f) * 0.10f,
                -0.70f);
    }
    else if (group < 120)
    {
        target =
            float32_t3(
                (randomA - 0.5f) * 0.35f,
                4.05f + (randomB - 0.5f) * 0.38f,
                -0.43f);
    }
    else
    {
        float ringAngle = randomA * 6.28318530718f;
        target =
            float32_t3(
                cos(ringAngle) * 0.72f,
                0.10f,
                sin(ringAngle) * 0.72f);
    }

    return target;
}

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t particleIndex = dispatchThreadId.x;
    if (particleIndex >= gEmitter.maxParticles)
    {
        return;
    }

    ParticleCS particle = gParticles[particleIndex];
    if (particle.lifeTime <= 0.0f)
    {
        return;
    }

    particle.currentTime += gPerFrame.deltaTime;
    float animationTime = particle.currentTime;
    uint32_t group = particleIndex % 128;
    float32_t3 localTarget = GetGolemTarget(particleIndex);

    float32_t3 worldTarget = gEmitter.translate + localTarget;
    float gatherRate = smoothstep(0.05f, 1.45f, animationTime);

    if (animationTime < 1.55f)
    {
        float springStrength = lerp(4.5f, 32.0f, gatherRate);
        particle.velocity +=
            (worldTarget - particle.translate) *
            springStrength *
            gPerFrame.deltaTime;
        particle.velocity *=
            pow(0.86f, gPerFrame.deltaTime * 60.0f);
        particle.velocity +=
            MakeNoise(particleIndex, gPerFrame.time) *
            gEffectSettings.noiseStrength *
            (1.0f - gatherRate * 0.72f) *
            gPerFrame.deltaTime;
    }
    else
    {
        particle.translate = worldTarget;
        particle.velocity = float32_t3(0.0f, 0.0f, 0.0f);
    }

    if (animationTime < 1.55f)
    {
        particle.translate +=
            particle.velocity * gPerFrame.deltaTime;
    }
    particle.rotation +=
        particle.rotationSpeed * gPerFrame.deltaTime;

    float baseScale =
        lerp(
            0.088f,
            0.220f,
            Hash(particleIndex, 2.61f));
    float bodyPartScale = 1.0f;
    if (group < 36 || group >= 100)
    {
        bodyPartScale = 1.16f;
    }
    else if (group >= 50 && group < 100)
    {
        bodyPartScale = 0.92f;
    }
    float silhouettePulse =
        0.90f +
        sin(gPerFrame.time * 7.0f + float32_t(particleIndex)) * 0.10f;
    particle.scale =
        float32_t3(
            baseScale * silhouettePulse * bodyPartScale,
            baseScale * silhouettePulse * bodyPartScale,
            baseScale * silhouettePulse * bodyPartScale);

    float appearAlpha = smoothstep(0.05f, 0.85f, animationTime);
    float collapseAlpha = 1.0f;
    float32_t4 sandColor =
        lerp(
            float32_t4(0.28f, 0.085f, 0.015f, 0.88f),
            float32_t4(0.92f, 0.49f, 0.095f, 0.96f),
            Hash(particleIndex, 6.71f));
    particle.color = sandColor;
    particle.color.a *= appearAlpha * collapseAlpha;

    if (group >= 112 && group < 120)
    {
        float glowPulse =
            0.72f + sin(gPerFrame.time * 14.0f) * 0.28f;
        particle.color =
            float32_t4(1.0f, 0.78f, 0.18f, glowPulse * collapseAlpha);
        particle.scale *= 1.75f;
    }

    gParticles[particleIndex] = particle;
}
