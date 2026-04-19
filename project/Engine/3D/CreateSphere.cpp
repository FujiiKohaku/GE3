#define _USE_MATH_DEFINES
#include "CreateSphere.h"
#include <cmath>
//使わないかもしれない
void CreateSphere::GenerateSphereVertices(VertexData* vertices, int kSubdivision, float radius)
{
    // 経度(360)
    const float kLonEvery = static_cast<float>(M_PI * 2.0f) / kSubdivision;
    // 緯度(180)
    const float kLatEvery = static_cast<float>(M_PI) / kSubdivision;

    for (int latIndex = 0; latIndex < kSubdivision; ++latIndex) {
        float lat = -static_cast<float>(M_PI) / 2.0f + kLatEvery * latIndex;
        float nextLat = lat + kLatEvery;

        for (int lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
            float lon = kLonEvery * lonIndex;
            float nextLon = lon + kLonEvery;

            // verA
            VertexData vertA;
            vertA.position = { cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon),
                1.0f };
            vertA.texcoord = { static_cast<float>(lonIndex) / kSubdivision,
                1.0f - static_cast<float>(latIndex) / kSubdivision };
            vertA.normal = { vertA.position.x, vertA.position.y, vertA.position.z };

            // verB
            VertexData vertB;
            vertB.position = { cosf(nextLat) * cosf(lon), sinf(nextLat),
                cosf(nextLat) * sinf(lon), 1.0f };
            vertB.texcoord = { static_cast<float>(lonIndex) / kSubdivision,
                1.0f - static_cast<float>(latIndex + 1) / kSubdivision };
            vertB.normal = { vertB.position.x, vertB.position.y, vertB.position.z };

            // vertC
            VertexData vertC;
            vertC.position = { cosf(lat) * cosf(nextLon), sinf(lat),
                cosf(lat) * sinf(nextLon), 1.0f };
            vertC.texcoord = { static_cast<float>(lonIndex + 1) / kSubdivision,
                1.0f - static_cast<float>(latIndex) / kSubdivision };
            vertC.normal = { vertC.position.x, vertC.position.y, vertC.position.z };

            // vertD
            VertexData vertD;
            vertD.position = { cosf(nextLat) * cosf(nextLon), sinf(nextLat),
                cosf(nextLat) * sinf(nextLon), 1.0f };
            vertD.texcoord = { static_cast<float>(lonIndex + 1) / kSubdivision,
                1.0f - static_cast<float>(latIndex + 1) / kSubdivision };
            vertD.normal = { vertD.position.x, vertD.position.y, vertD.position.z };

            // 初期位置//
            uint32_t startIndex = (latIndex * kSubdivision + lonIndex) * 6;

            vertices[startIndex + 0] = vertA;
            vertices[startIndex + 1] = vertB;
            vertices[startIndex + 2] = vertC;
            vertices[startIndex + 3] = vertC;
            vertices[startIndex + 4] = vertD;
            vertices[startIndex + 5] = vertB;
        }
    }
}
