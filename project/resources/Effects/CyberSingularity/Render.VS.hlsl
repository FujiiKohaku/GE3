#include "../Common/ParticleRenderCommon.hlsli"

VertexShaderOutput main(
    VertexShaderInput input,
    uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    ParticleCS particle = gParticles[instanceId];

    float32_t4x4 worldMatrix = gPerView.billboardMatrix;
    float32_t4 billboardX = worldMatrix[0];
    float32_t4 billboardY = worldMatrix[1];

    float velocityX =
        dot(particle.velocity, billboardX.xyz);
    float velocityY =
        dot(particle.velocity, billboardY.xyz);
    float velocityAngle =
        atan2(velocityY, velocityX) -
        1.57079632679f;
    float rotationSin = sin(velocityAngle);
    float rotationCos = cos(velocityAngle);

    worldMatrix[0] =
        (billboardX * rotationCos + billboardY * rotationSin) *
        particle.scale.x;
    worldMatrix[1] =
        (-billboardX * rotationSin + billboardY * rotationCos) *
        particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;

    float32_t4x4 wvpMatrix =
        mul(worldMatrix, gPerView.viewProjection);
    float32_t4 worldPosition =
        mul(input.position, worldMatrix);

    output.position = mul(input.position, wvpMatrix);
    output.texcoord = input.texcoord;
    output.normal = input.normal;
    output.color = particle.color;
    output.worldPosition = worldPosition.xyz;
    output.viewDistance =
        length(worldPosition.xyz - gPerView.cameraPosition);

    return output;
}
