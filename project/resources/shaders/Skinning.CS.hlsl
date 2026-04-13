struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct Vertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
};

struct VertexInfluence
{
    float4 weight;
    int4 index;
};

struct SkinnedVertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
};

struct SkinningInformation
{
    uint vertexCount;
    uint jointCount;
};

StructuredBuffer<Well> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
RWStructuredBuffer<SkinnedVertex> gOutputVertices : register(u0);
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;

    if (vertexIndex >= gSkinningInformation.vertexCount)
    {
        return;
    }

    Vertex inputVertex = gInputVertices[vertexIndex];
    VertexInfluence inputInfluence = gInfluences[vertexIndex];

    // =========================
    // position
    // =========================
    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);

    skinnedPosition += mul(inputVertex.position, gMatrixPalette[inputInfluence.index.x].skeletonSpaceMatrix) * inputInfluence.weight.x;
    skinnedPosition += mul(inputVertex.position, gMatrixPalette[inputInfluence.index.y].skeletonSpaceMatrix) * inputInfluence.weight.y;
    skinnedPosition += mul(inputVertex.position, gMatrixPalette[inputInfluence.index.z].skeletonSpaceMatrix) * inputInfluence.weight.z;
    skinnedPosition += mul(inputVertex.position, gMatrixPalette[inputInfluence.index.w].skeletonSpaceMatrix) * inputInfluence.weight.w;

    // =========================
    // normal
    // =========================
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    skinnedNormal += mul(inputVertex.normal, (float3x3) gMatrixPalette[inputInfluence.index.x].skeletonSpaceInverseTransposeMatrix) * inputInfluence.weight.x;
    skinnedNormal += mul(inputVertex.normal, (float3x3) gMatrixPalette[inputInfluence.index.y].skeletonSpaceInverseTransposeMatrix) * inputInfluence.weight.y;
    skinnedNormal += mul(inputVertex.normal, (float3x3) gMatrixPalette[inputInfluence.index.z].skeletonSpaceInverseTransposeMatrix) * inputInfluence.weight.z;
    skinnedNormal += mul(inputVertex.normal, (float3x3) gMatrixPalette[inputInfluence.index.w].skeletonSpaceInverseTransposeMatrix) * inputInfluence.weight.w;

    skinnedNormal = normalize(skinnedNormal);

    // =========================
    // output
    // =========================
    SkinnedVertex outputVertex;
    outputVertex.position = skinnedPosition;
    outputVertex.texcoord = inputVertex.texcoord;
    outputVertex.normal = skinnedNormal;

    gOutputVertices[vertexIndex] = outputVertex;
}