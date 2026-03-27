#pragma once
#include <array>
#include <cstdint>

class SkyBox {
public:
    struct VertexData {
        float position[4];
    };

public:
    void CreateBox();

    const std::array<VertexData, 24>& GetVertexData() const;
    const std::array<uint32_t, 36>& GetIndexData() const;

private:
    std::array<VertexData, 24> vertexData_ {};
    std::array<uint32_t, 36> indexData_ {};
};