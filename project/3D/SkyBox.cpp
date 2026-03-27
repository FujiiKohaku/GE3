#include "SkyBox.h"

void SkyBox::CreateBox()
{
    // 右面
    vertexData_[0].position[0] = 1.0f;
    vertexData_[0].position[1] = 1.0f;
    vertexData_[0].position[2] = -1.0f;
    vertexData_[0].position[3] = 1.0f;

    vertexData_[1].position[0] = 1.0f;
    vertexData_[1].position[1] = 1.0f;
    vertexData_[1].position[2] = 1.0f;
    vertexData_[1].position[3] = 1.0f;

    vertexData_[2].position[0] = 1.0f;
    vertexData_[2].position[1] = -1.0f;
    vertexData_[2].position[2] = -1.0f;
    vertexData_[2].position[3] = 1.0f;

    vertexData_[3].position[0] = 1.0f;
    vertexData_[3].position[1] = -1.0f;
    vertexData_[3].position[2] = 1.0f;
    vertexData_[3].position[3] = 1.0f;

    // 左面
    vertexData_[4].position[0] = -1.0f;
    vertexData_[4].position[1] = 1.0f;
    vertexData_[4].position[2] = 1.0f;
    vertexData_[4].position[3] = 1.0f;

    vertexData_[5].position[0] = -1.0f;
    vertexData_[5].position[1] = 1.0f;
    vertexData_[5].position[2] = -1.0f;
    vertexData_[5].position[3] = 1.0f;

    vertexData_[6].position[0] = -1.0f;
    vertexData_[6].position[1] = -1.0f;
    vertexData_[6].position[2] = 1.0f;
    vertexData_[6].position[3] = 1.0f;

    vertexData_[7].position[0] = -1.0f;
    vertexData_[7].position[1] = -1.0f;
    vertexData_[7].position[2] = -1.0f;
    vertexData_[7].position[3] = 1.0f;

    // 前面
    vertexData_[8].position[0] = -1.0f;
    vertexData_[8].position[1] = 1.0f;
    vertexData_[8].position[2] = 1.0f;
    vertexData_[8].position[3] = 1.0f;

    vertexData_[9].position[0] = 1.0f;
    vertexData_[9].position[1] = 1.0f;
    vertexData_[9].position[2] = 1.0f;
    vertexData_[9].position[3] = 1.0f;

    vertexData_[10].position[0] = -1.0f;
    vertexData_[10].position[1] = -1.0f;
    vertexData_[10].position[2] = 1.0f;
    vertexData_[10].position[3] = 1.0f;

    vertexData_[11].position[0] = 1.0f;
    vertexData_[11].position[1] = -1.0f;
    vertexData_[11].position[2] = 1.0f;
    vertexData_[11].position[3] = 1.0f;

    // 背面
    vertexData_[12].position[0] = 1.0f;
    vertexData_[12].position[1] = 1.0f;
    vertexData_[12].position[2] = -1.0f;
    vertexData_[12].position[3] = 1.0f;

    vertexData_[13].position[0] = -1.0f;
    vertexData_[13].position[1] = 1.0f;
    vertexData_[13].position[2] = -1.0f;
    vertexData_[13].position[3] = 1.0f;

    vertexData_[14].position[0] = 1.0f;
    vertexData_[14].position[1] = -1.0f;
    vertexData_[14].position[2] = -1.0f;
    vertexData_[14].position[3] = 1.0f;

    vertexData_[15].position[0] = -1.0f;
    vertexData_[15].position[1] = -1.0f;
    vertexData_[15].position[2] = -1.0f;
    vertexData_[15].position[3] = 1.0f;

    // 上面
    vertexData_[16].position[0] = -1.0f;
    vertexData_[16].position[1] = 1.0f;
    vertexData_[16].position[2] = -1.0f;
    vertexData_[16].position[3] = 1.0f;

    vertexData_[17].position[0] = 1.0f;
    vertexData_[17].position[1] = 1.0f;
    vertexData_[17].position[2] = -1.0f;
    vertexData_[17].position[3] = 1.0f;

    vertexData_[18].position[0] = -1.0f;
    vertexData_[18].position[1] = 1.0f;
    vertexData_[18].position[2] = 1.0f;
    vertexData_[18].position[3] = 1.0f;

    vertexData_[19].position[0] = 1.0f;
    vertexData_[19].position[1] = 1.0f;
    vertexData_[19].position[2] = 1.0f;
    vertexData_[19].position[3] = 1.0f;

    // 下面
    vertexData_[20].position[0] = -1.0f;
    vertexData_[20].position[1] = -1.0f;
    vertexData_[20].position[2] = 1.0f;
    vertexData_[20].position[3] = 1.0f;

    vertexData_[21].position[0] = 1.0f;
    vertexData_[21].position[1] = -1.0f;
    vertexData_[21].position[2] = 1.0f;
    vertexData_[21].position[3] = 1.0f;

    vertexData_[22].position[0] = -1.0f;
    vertexData_[22].position[1] = -1.0f;
    vertexData_[22].position[2] = -1.0f;
    vertexData_[22].position[3] = 1.0f;

    vertexData_[23].position[0] = 1.0f;
    vertexData_[23].position[1] = -1.0f;
    vertexData_[23].position[2] = -1.0f;
    vertexData_[23].position[3] = 1.0f;

    // 内側を向くインデックス
    indexData_[0] = 0;
    indexData_[1] = 1;
    indexData_[2] = 2;
    indexData_[3] = 2;
    indexData_[4] = 1;
    indexData_[5] = 3;

    indexData_[6] = 4;
    indexData_[7] = 5;
    indexData_[8] = 6;
    indexData_[9] = 6;
    indexData_[10] = 5;
    indexData_[11] = 7;

    indexData_[12] = 8;
    indexData_[13] = 9;
    indexData_[14] = 10;
    indexData_[15] = 10;
    indexData_[16] = 9;
    indexData_[17] = 11;

    indexData_[18] = 12;
    indexData_[19] = 13;
    indexData_[20] = 14;
    indexData_[21] = 14;
    indexData_[22] = 13;
    indexData_[23] = 15;

    indexData_[24] = 16;
    indexData_[25] = 17;
    indexData_[26] = 18;
    indexData_[27] = 18;
    indexData_[28] = 17;
    indexData_[29] = 19;

    indexData_[30] = 20;
    indexData_[31] = 21;
    indexData_[32] = 22;
    indexData_[33] = 22;
    indexData_[34] = 21;
    indexData_[35] = 23;
}

const std::array<SkyBox::VertexData, 24>& SkyBox::GetVertexData() const
{
    return vertexData_;
}

const std::array<uint32_t, 36>& SkyBox::GetIndexData() const
{
    return indexData_;
}