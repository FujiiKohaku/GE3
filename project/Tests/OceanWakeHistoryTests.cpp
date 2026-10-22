#include "Engine/3D/OceanSurface.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

// Standalone CPU test: rendering dependencies fail immediately if accidentally invoked.
#ifdef OCEAN_HISTORY_STANDALONE
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Math/MatrixMath.h"
#include "Engine/Logger/Logger.h"

void Logger::Log(const std::string&) { throw std::runtime_error("Unexpected GPU initialization"); }
DirectXCommon* DirectXCommon::GetInstance() { throw std::runtime_error("Unexpected GPU initialization"); }
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::LoadCompiledShader(const std::wstring&)
{
    throw std::runtime_error("Unexpected shader loading");
}
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t)
{
    throw std::runtime_error("Unexpected GPU allocation");
}
Matrix4x4 MatrixMath::MakeAffineMatrix(const Vector3&, const Vector3&, const Vector3&)
{
    throw std::runtime_error("Unexpected rendering update");
}
Object3dManager* Object3dManager::GetInstance() { throw std::runtime_error("Unexpected rendering update"); }
void Object3dManager::BindShadowResources(bool, uint32_t, uint32_t, uint32_t)
{
    throw std::runtime_error("Unexpected rendering update");
}
#endif

int main()
{
    try {
        // History and analytic height queries work without allocating a GPU resource.
        OceanSurface oceanSurface;
        oceanSurface.SetLength(100.0f);
        const Vector3 forward{0.0f, 0.0f, 1.0f};
        const Vector3 bossSource{0.0f, 0.0f, 80.0f};
        const Vector3 playerSource{0.0f, 0.0f, 10.0f};
        for (int wakeIndex = 0; wakeIndex < 16; ++wakeIndex) {
            if (!oceanSurface.AddWake(bossSource, forward, 0.05f, 1.0f,
                OceanSurface::WakeType::Directional, OceanSurface::WakeSource::Boss, 0.5f)) {
                throw std::runtime_error("Boss history must accept sixteen live records");
            }
        }
        const float initialBossHeight = oceanSurface.GetHeightAt(4.66f, 77.0f);
        const float naturalHeight = oceanSurface.GetNaturalHeightAt(4.66f, 77.0f);
        if (std::abs(initialBossHeight - naturalHeight) < 0.01f) {
            throw std::runtime_error("The boss history probe must lie on a visible crest");
        }
        if (oceanSurface.AddWake(bossSource, forward, 1.0f, 1.0f,
            OceanSurface::WakeType::Directional, OceanSurface::WakeSource::Boss, 0.5f)
            || oceanSurface.GetHeightAt(4.66f, 77.0f) != initialBossHeight) {
            throw std::runtime_error("A full boss history must retain its live crests");
        }
        for (int wakeIndex = 0; wakeIndex < 128; ++wakeIndex) {
            if (!oceanSurface.AddWake(playerSource, forward, 0.05f, 1.0f,
                OceanSurface::WakeType::Directional, OceanSurface::WakeSource::Player, 0.5f)) {
                throw std::runtime_error("Player emission must continue when the shared history is full");
            }
        }
        if (oceanSurface.GetHeightAt(4.66f, 77.0f) != initialBossHeight) {
            throw std::runtime_error("Player emission must not abruptly remove boss crests");
        }
        if (oceanSurface.AddWake(bossSource, forward, 1.0f, 1.0f,
            OceanSurface::WakeType::Radial, OceanSurface::WakeSource::Boss, 2.5f)) {
            throw std::runtime_error("Expired pending impacts must not enter history");
        }
        std::cout << "PASS: boss history lifetime, player/boss retention and expired impact rejection\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
