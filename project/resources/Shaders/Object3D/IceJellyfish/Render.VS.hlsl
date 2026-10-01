#include "../Object3d.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

cbuffer VertexShaderParameters : register(b1)
{
    // x: gameplay animation time, y: local amplitude, z: phase, w: part type.
    // Part type 0 is the bell; part type 1 is a tentacle link.
    float4 animation;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

void DeformBell(inout float3 position, inout float3 normal)
{
    const float kRadiusSquared = 22.0f * 22.0f;
    float amplitude = animation.y;
    float time = animation.x;
    float radiusSquared = dot(position.xz, position.xz) / kRadiusSquared;
    float rimWeight = saturate(radiusSquared);
    float2 weightGradient = float2(0.0f, 0.0f);
    if (radiusSquared < 1.0f)
    {
        weightGradient = 2.0f * position.xz / kRadiusSquared;
    }

    float phaseA = time * 1.80f + animation.z + position.x * 0.16f + position.z * 0.12f;
    float phaseB = time * 1.35f + animation.z - position.x * 0.11f + position.z * 0.18f;
    float wave = sin(phaseA) * 0.65f + sin(phaseB) * 0.35f;
    float2 waveGradient = cos(phaseA) * 0.65f * float2(0.16f, 0.12f) +
        cos(phaseB) * 0.35f * float2(-0.11f, 0.18f);
    float2 heightGradient = amplitude * (weightGradient * wave + rimWeight * waveGradient);
    float pulseScale = 1.0f + amplitude * 0.015f * sin(time * 1.10f);

    position.y += amplitude * rimWeight * wave;
    position.xz *= pulseScale;

    // Inverse-transpose of the local deformation Jacobian, before world scaling.
    normal.xz = (normal.xz - heightGradient * normal.y) / pulseScale;
}

void DeformTentacle(inout float3 position, inout float3 normal)
{
    float amplitude = animation.y;
    float along = saturate(-position.y);
    // Both ends remain anchored to the existing CPU-controlled joints.
    float weight = 4.0f * along * (1.0f - along);
    float weightDerivative = -4.0f * (1.0f - 2.0f * along);
    float phaseDerivative = -1.0f;
    if (position.y < -1.0f || position.y > 0.0f)
    {
        weightDerivative = 0.0f;
        phaseDerivative = 0.0f;
    }

    float phaseX = animation.x * 2.0f + animation.z + along * 5.0f;
    float phaseZ = animation.x * 1.65f + animation.z + along * 4.0f;
    float dxdy = amplitude * (weightDerivative * sin(phaseX) +
        weight * cos(phaseX) * 5.0f * phaseDerivative);
    float dzdy = amplitude * 0.65f * (weightDerivative * cos(phaseZ) -
        weight * sin(phaseZ) * 4.0f * phaseDerivative);

    position.x += amplitude * weight * sin(phaseX);
    position.z += amplitude * 0.65f * weight * cos(phaseZ);
    normal.y -= dxdy * normal.x + dzdy * normal.z;
}

VertexShaderOutput main(VertexShaderInput input)
{
    float3 position = input.position.xyz;
    float3 normal = input.normal;
    if (animation.w < 0.5f)
    {
        DeformBell(position, normal);
    }
    else
    {
        DeformTentacle(position, normal);
    }

    float4 localPosition = float4(position, input.position.w);
    VertexShaderOutput output;
    output.position = mul(localPosition, gTransformationMatrix.WVP);
    output.worldPosition = mul(localPosition, gTransformationMatrix.World).xyz;
    output.normal = normalize(mul(normal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    output.texcoord = input.texcoord;
    return output;
}
