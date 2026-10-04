#include "Engine/Light/LightManager.h"
#include "Engine/Development/DevelopmentWebPanel.h"
#include "Engine/PostEffect/PostEffectManager.h"

#include "App/Scene/Common/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/PostEffect/Bloom/BloomRenderer.h"
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"
#include "Engine/PostEffect/Fog/FogManager.h"
#include "Engine/PostEffect/Fog/FogRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/Time/TimeManager.h"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <charconv>
#include <fstream>
#include <stdexcept>
#include "externals/json.hpp"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#if defined(ENABLE_DEVELOPMENT_TOOLS)
namespace {
bool ParseFogVolumeKey(const std::string& key, uint32_t& volumeIndex, std::string& field) {
    const std::string prefix = "fogVolume.";
    if (!key.starts_with(prefix)) { return false; }
    const size_t separator = key.find('.', prefix.size());
    if (separator == std::string::npos) { return false; }
    const char* begin = key.data() + prefix.size();
    const char* end = key.data() + separator;
    const auto result = std::from_chars(begin, end, volumeIndex);
    if (result.ec != std::errc() || result.ptr != end || volumeIndex >= VolumetricLightRenderer::kMaxFogVolumes) { return false; }
    field = key.substr(separator + 1);
    return !field.empty();
}
}
#endif

PostEffectManager::PostEffectManager() = default;

PostEffectManager::~PostEffectManager()
{
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().UnregisterOwner(this);
#endif
}

#if defined(ENABLE_DEVELOPMENT_TOOLS)
void PostEffectManager::RegisterDevelopmentPanel()
{
    DevelopmentWebPanel::GetInstance().RegisterSource(this, "effects", "ポストエフェクト", false,
        &PostEffectManager::GetDevelopmentSettings, &PostEffectManager::GetDevelopmentControls,
        &PostEffectManager::SetDevelopmentBool, &PostEffectManager::SetDevelopmentNumber,
        &PostEffectManager::ExecuteDevelopmentCommand, &PostEffectManager::ClearDevelopmentPassOverrides);
}

nlohmann::json PostEffectManager::GetDevelopmentControls() const
{
    static const nlohmann::json kControls = nlohmann::json::array({
        {{"key", "ssaoEnabled"}, {"label", "SSAOを有効"}, {"group", "ライティング"}, {"type", "bool"}},
        {{"key", "atmosphereEnabled"}, {"label", "大気散乱を有効"}, {"group", "ライティング"}, {"type", "bool"}},
        {{"key", "clusteredLightingEnabled"}, {"label", "Clusteredを有効"}, {"group", "ライティング"}, {"type", "bool"}},
        {{"key", "ssaoStrength"}, {"label", "SSAO強度"}, {"group", "ライティング"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.6}, {"step", 0.01}},
        {{"key", "ssaoRadius"}, {"label", "SSAO半径"}, {"group", "ライティング"}, {"type", "number"}, {"minimum", 0.1}, {"maximum", 30}, {"step", 0.1}},
        {{"key", "ssaoBias"}, {"label", "SSAOバイアス"}, {"group", "ライティング"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "environmentDiffuse"}, {"label", "環境光の拡散"}, {"group", "ライティング"}, {"type", "number"}, {"minimum", 0}, {"maximum", 2}, {"step", 0.01}},
        {{"key", "environmentSpecular"}, {"label", "環境光の反射"}, {"group", "ライティング"}, {"type", "number"}, {"minimum", 0}, {"maximum", 2}, {"step", 0.01}},
        {{"key", "atmosphereDensity"}, {"label", "大気の密度"}, {"group", "ライティング"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.01}, {"step", 5e-05}},
        {{"key", "atmosphereStrength"}, {"label", "大気散乱の強度"}, {"group", "ライティング"}, {"type", "number"}, {"minimum", 0}, {"maximum", 2}, {"step", 0.01}},
        {{"key", "atmosphereAnisotropy"}, {"label", "大気の前方散乱"}, {"group", "ライティング"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.9}, {"step", 0.01}},
        {{"key", "localFogEnabled"}, {"label", "立体霧を有効"}, {"group", "立体霧"}, {"type", "bool"}},
        {{"key", "localFogHeight"}, {"label", "基準高さ"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", -1000}, {"maximum", 1000}, {"step", 0.1}},
        {{"key", "localFogHeightDensity"}, {"label", "高さ霧の濃度"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.05}, {"step", 0.0001}},
        {{"key", "localFogHeightFalloff"}, {"label", "高さによる薄まり"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", 0.001}, {"maximum", 10}, {"step", 0.001}},
        {{"key", "localFogNoiseScale"}, {"label", "ノイズの細かさ"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", 0.001}, {"maximum", 10}, {"step", 0.001}},
        {{"key", "localFogNoiseStrength"}, {"label", "ノイズの強さ"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "localFogVelocityX"}, {"label", "霧の移動 X"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", -100}, {"maximum", 100}, {"step", 0.1}},
        {{"key", "localFogVelocityY"}, {"label", "霧の移動 Y"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", -100}, {"maximum", 100}, {"step", 0.1}},
        {{"key", "localFogVelocityZ"}, {"label", "霧の移動 Z"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", -100}, {"maximum", 100}, {"step", 0.1}},
        {{"key", "localFogColorR"}, {"label", "霧の色 R"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", 0}, {"maximum", 4}, {"step", 0.01}},
        {{"key", "localFogColorG"}, {"label", "霧の色 G"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", 0}, {"maximum", 4}, {"step", 0.01}},
        {{"key", "localFogColorB"}, {"label", "霧の色 B"}, {"group", "立体霧"}, {"type", "number"}, {"minimum", 0}, {"maximum", 4}, {"step", 0.01}},
        {{"key", "fogEnabled"}, {"label", "霧"}, {"group", "霧・ブルーム"}, {"type", "bool"}},
        {{"key", "distanceFogEnabled"}, {"label", "距離の霧"}, {"group", "霧・ブルーム"}, {"type", "bool"}},
        {{"key", "fogColorR"}, {"label", "霧の色 R"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "fogColorG"}, {"label", "霧の色 G"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "fogColorB"}, {"label", "霧の色 B"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "fogStart"}, {"label", "霧の開始距離"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0}, {"maximum", 3000}, {"step", 0.1}},
        {{"key", "fogEnd"}, {"label", "霧の終了距離"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 3000}, {"step", 0.1}},
        {{"key", "fogCurve"}, {"label", "霧の曲線"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 8}, {"step", 0.01}},
        {{"key", "fogDensity"}, {"label", "霧の濃さ"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "bloomEnabled"}, {"label", "ブルーム"}, {"group", "霧・ブルーム"}, {"type", "bool"}},
        {{"key", "bloomThreshold"}, {"label", "しきい値"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0}, {"maximum", 5}, {"step", 0.05}},
        {{"key", "bloomBlurRadius"}, {"label", "ぼかし半径"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0}, {"maximum", 32}, {"step", 1}},
        {{"key", "bloomBlurSigma"}, {"label", "ぼかし Sigma"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 20}, {"step", 0.01}},
        {{"key", "bloomIntensity"}, {"label", "ブルーム強度"}, {"group", "霧・ブルーム"}, {"type", "number"}, {"minimum", 0}, {"maximum", 5}, {"step", 0.05}},
        {{"key", "toneMapEnabled"}, {"label", "トーンマッピング"}, {"group", "ポストエフェクト"}, {"type", "bool"}},
        {{"key", "toneExposure"}, {"label", "露出"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0.1}, {"maximum", 4}, {"step", 0.01}},
        {{"key", "toneContrast"}, {"label", "最終コントラスト"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0.5}, {"maximum", 1.5}, {"step", 0.01}},
        {{"key", "toneSaturation"}, {"label", "最終彩度"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 2}, {"step", 0.01}},
        {{"key", "volumeEnabled"}, {"label", "ボリューメトリックライト"}, {"group", "ポストエフェクト"}, {"type", "bool"}},
        {{"key", "volumeIntensity"}, {"label", "空間の光 強度"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 4}, {"step", 0.01}},
        {{"key", "volumeDensity"}, {"label", "散乱密度"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.05}, {"step", 0.0001}},
        {{"key", "volumeDistance"}, {"label", "光の計算距離"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 1}, {"maximum", 1000}, {"step", 1}},
        {{"key", "volumeAnisotropy"}, {"label", "散乱方向性"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", -0.8}, {"maximum", 0.8}, {"step", 0.01}},
        {{"key", "volumeSamples"}, {"label", "光の計算サンプル数"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 8}, {"maximum", 64}, {"step", 1}},
        {{"key", "volumeColorR"}, {"label", "空間の光 色 R"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 4}, {"step", 0.01}},
        {{"key", "volumeColorG"}, {"label", "空間の光 色 G"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 4}, {"step", 0.01}},
        {{"key", "volumeColorB"}, {"label", "空間の光 色 B"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 4}, {"step", 0.01}},
        {{"key", "fxaaEnabled"}, {"label", "FXAA"}, {"group", "ポストエフェクト"}, {"type", "bool"}},
        {{"key", "fxaaStrength"}, {"label", "FXAA 強さ"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "fxaaSubpixel"}, {"label", "FXAA サブピクセル"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "fxaaEdgeThreshold"}, {"label", "FXAA 輪郭しきい値"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 0.5}, {"step", 0.01}},
        {{"key", "fxaaEdgeThresholdMin"}, {"label", "FXAA 最小しきい値"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.2}, {"step", 0.001}},
        {{"key", "animationEnabled"}, {"label", "アニメーション"}, {"group", "ポストエフェクト"}, {"type", "bool"}},
        {{"key", "pixelSize"}, {"label", "ピクセルサイズ"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 1}, {"maximum", 64}, {"step", 1}},
        {{"key", "colorBrightness"}, {"label", "明るさ"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", -1}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "colorContrast"}, {"label", "コントラスト"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 3}, {"step", 0.01}},
        {{"key", "colorSaturation"}, {"label", "彩度"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 3}, {"step", 0.01}},
        {{"key", "focusDepth"}, {"label", "フォーカス深度"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.001}},
        {{"key", "focusRange"}, {"label", "フォーカス範囲"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0.0001}, {"maximum", 0.2}, {"step", 0.0001}},
        {{"key", "depthOfFieldRadius"}, {"label", "被写界深度半径"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 32}, {"step", 0.1}},
        {{"key", "motionBlurDirectionX"}, {"label", "モーション方向 X"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", -1}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "motionBlurDirectionY"}, {"label", "モーション方向 Y"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", -1}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "motionBlurStrength"}, {"label", "モーション強度"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.1}, {"step", 0.001}},
        {{"key", "motionBlurSampleCount"}, {"label", "モーションサンプル"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 1}, {"maximum", 32}, {"step", 1}},
        {{"key", "chromaticAberrationStrength"}, {"label", "色収差"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.05}, {"step", 0.001}},
        {{"key", "lensDistortionStrength"}, {"label", "レンズ歪み"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", -1}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "filmGrainStrength"}, {"label", "フィルム粒子"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.5}, {"step", 0.01}},
        {{"key", "lensDirtStrength"}, {"label", "レンズ汚れ"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 3}, {"step", 0.01}},
        {{"key", "cameraShakeStrength"}, {"label", "カメラシェイク"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.05}, {"step", 0.001}},
        {{"key", "bokehRadius"}, {"label", "ボケ半径"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 32}, {"step", 0.1}},
        {{"key", "bokehSides"}, {"label", "ボケ辺数"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 3}, {"maximum", 12}, {"step", 1}},
        {{"key", "fisheyeStrength"}, {"label", "魚眼強度"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 3}, {"step", 0.01}},
        {{"key", "lightThreshold"}, {"label", "光しきい値"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 2}, {"step", 0.01}},
        {{"key", "lightStrength"}, {"label", "光の強さ"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 5}, {"step", 0.05}},
        {{"key", "lightRadius"}, {"label", "光の半径"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "outlineNormalThreshold"}, {"label", "輪郭法線しきい値"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "outlineNormalSoftness"}, {"label", "輪郭法線の柔らかさ"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0.001}, {"maximum", 1}, {"step", 0.01}},
        {{"key", "outlineNormalStrength"}, {"label", "輪郭法線強度"}, {"group", "ポストエフェクト"}, {"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"step", 0.01}},
    });
    nlohmann::json controls = kControls;
    for (uint32_t volumeIndex = 0; volumeIndex < VolumetricLightRenderer::kMaxFogVolumes; ++volumeIndex) {
        const std::string prefix = "fogVolume." + std::to_string(volumeIndex) + ".";
        const std::string group = "局所霧 " + std::to_string(volumeIndex);
        controls.push_back({ {"key", prefix + "enabled"}, {"label", "有効"}, {"group", group}, {"type", "bool"} });
        controls.push_back({ {"key", prefix + "shape"}, {"label", "形状"}, {"group", group}, {"type", "select"},
            {"options", nlohmann::json::array({ {{"value", 0}, {"label", "球"}}, {{"value", 1}, {"label", "箱"}} })} });
        controls.push_back({ {"key", prefix + "centerX"}, {"label", "中心 X"}, {"group", group}, {"type", "number"}, {"minimum", -10000}, {"maximum", 10000}, {"step", 0.1} });
        controls.push_back({ {"key", prefix + "centerY"}, {"label", "中心 Y"}, {"group", group}, {"type", "number"}, {"minimum", -10000}, {"maximum", 10000}, {"step", 0.1} });
        controls.push_back({ {"key", prefix + "centerZ"}, {"label", "中心 Z"}, {"group", group}, {"type", "number"}, {"minimum", -10000}, {"maximum", 10000}, {"step", 0.1} });
        controls.push_back({ {"key", prefix + "extentX"}, {"label", "半幅 X"}, {"group", group}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 1000}, {"step", 0.1} });
        controls.push_back({ {"key", prefix + "extentY"}, {"label", "半幅 Y"}, {"group", group}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 1000}, {"step", 0.1} });
        controls.push_back({ {"key", prefix + "extentZ"}, {"label", "半幅 Z"}, {"group", group}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 1000}, {"step", 0.1} });
        controls.push_back({ {"key", prefix + "radius"}, {"label", "半径"}, {"group", group}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 1000}, {"step", 0.1} });
        controls.push_back({ {"key", prefix + "density"}, {"label", "濃度"}, {"group", group}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.05}, {"step", 0.0001} });
        controls.push_back({ {"key", prefix + "softness"}, {"label", "境界のぼかし"}, {"group", group}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 1000}, {"step", 0.1} });
    }

    for (const auto& pass : SceneManager::GetInstance()->GetPostEffects()) {
        controls.push_back({{"key","pass."+std::to_string(static_cast<int>(pass.type))},
            {"label",GetPostEffectTypeName(pass.type)},{"group","パス"},{"type","bool"}});
    }
    return controls;
}

bool PostEffectManager::SetDevelopmentBool(const std::string& key, bool isEnabled)
{
    LightManager* lights = LightManager::GetInstance();
    const Vector4 ssao = copyImageRenderer_->GetPostEffectParameter().ssaoSettings;
    if (key == "ssaoEnabled") { return SetSsao(isEnabled, ssao.x, ssao.y, ssao.z); }
    if (key == "clusteredLightingEnabled") { lights->SetClusteredLightingEnabled(isEnabled); return true; }
    if (key == "atmosphereEnabled") {
        const Vector4 atmosphere = lights->GetAtmosphereSettings();
        return lights->SetAtmosphere(isEnabled, atmosphere.y, atmosphere.z, atmosphere.w);
    }
    if (key == "localFogEnabled") { volumetricLightRenderer_->SetLocalFogEnabled(isEnabled); return true; }
    uint32_t volumeIndex = 0;
    std::string field;
    if (ParseFogVolumeKey(key, volumeIndex, field)) {
        if (field != "enabled") { return false; }
        FogVolumeSettings settings = volumetricLightRenderer_->GetFogVolumes()[volumeIndex];
        settings.isEnabled = isEnabled;
        return volumetricLightRenderer_->SetFogVolume(volumeIndex, settings);
    }
    if (isEnabled) { return ApplyDevelopmentSetting(key, "true"); }
    return ApplyDevelopmentSetting(key, "false");
}

bool PostEffectManager::SetDevelopmentNumber(const std::string& key, double value)
{
    if (!std::isfinite(value)) { return false; }
    const float number = static_cast<float>(value);
    if (!std::isfinite(number)) { return false; }
    const Vector4 ssao = copyImageRenderer_->GetPostEffectParameter().ssaoSettings;
    if (key == "ssaoStrength") { return SetSsao(ssao.w > 0.5f, number, ssao.y, ssao.z); }
    if (key == "ssaoRadius") { return SetSsao(ssao.w > 0.5f, ssao.x, number, ssao.z); }
    if (key == "ssaoBias") { return SetSsao(ssao.w > 0.5f, ssao.x, ssao.y, number); }
    LightManager* lights = LightManager::GetInstance();
    const Vector4 environment = lights->GetEnvironmentLighting();
    if (key == "environmentDiffuse") { return lights->SetEnvironmentLighting(number, environment.y); }
    if (key == "environmentSpecular") { return lights->SetEnvironmentLighting(environment.x, number); }
    const Vector4 atmosphere = lights->GetAtmosphereSettings();
    if (key == "atmosphereDensity") { return lights->SetAtmosphere(atmosphere.x > 0.5f, number, atmosphere.z, atmosphere.w); }
    if (key == "atmosphereStrength") { return lights->SetAtmosphere(atmosphere.x > 0.5f, atmosphere.y, number, atmosphere.w); }
    if (key == "atmosphereAnisotropy") { return lights->SetAtmosphere(atmosphere.x > 0.5f, atmosphere.y, atmosphere.z, number); }
    uint32_t volumeIndex = 0;
    std::string field;
    if (ParseFogVolumeKey(key, volumeIndex, field)) {
        FogVolumeSettings settings = volumetricLightRenderer_->GetFogVolumes()[volumeIndex];
        if (field == "shape") {
            if (number == 0.0f) { settings.shape = FogVolumeShape::Sphere; }
            else if (number == 1.0f) { settings.shape = FogVolumeShape::Box; }
            else { return false; }
        }
        else if (field == "centerX") { settings.center.x = number; }
        else if (field == "centerY") { settings.center.y = number; }
        else if (field == "centerZ") { settings.center.z = number; }
        else if (field == "extentX") { settings.halfExtents.x = number; }
        else if (field == "extentY") { settings.halfExtents.y = number; }
        else if (field == "extentZ") { settings.halfExtents.z = number; }
        else if (field == "radius") { settings.radius = number; }
        else if (field == "density") { settings.density = number; }
        else if (field == "softness") { settings.edgeSoftness = number; }
        else { return false; }
        return volumetricLightRenderer_->SetFogVolume(volumeIndex, settings);
    }
    LocalFogParameters fog = volumetricLightRenderer_->GetLocalFogParameters();
    if (key == "localFogHeight") { return volumetricLightRenderer_->SetHeightFog(number, fog.heightDensity, fog.heightFalloff); }
    if (key == "localFogHeightDensity") { return volumetricLightRenderer_->SetHeightFog(fog.baseHeight, number, fog.heightFalloff); }
    if (key == "localFogHeightFalloff") { return volumetricLightRenderer_->SetHeightFog(fog.baseHeight, fog.heightDensity, number); }
    if (key == "localFogNoiseScale") { return volumetricLightRenderer_->SetNoiseParameters(number, fog.noiseStrength, fog.noiseVelocity); }
    if (key == "localFogNoiseStrength") { return volumetricLightRenderer_->SetNoiseParameters(fog.noiseScale, number, fog.noiseVelocity); }
    if (key == "localFogVelocityX" || key == "localFogVelocityY" || key == "localFogVelocityZ") {
        if (key == "localFogVelocityX") { fog.noiseVelocity.x = number; }
        if (key == "localFogVelocityY") { fog.noiseVelocity.y = number; }
        if (key == "localFogVelocityZ") { fog.noiseVelocity.z = number; }
        return volumetricLightRenderer_->SetNoiseParameters(fog.noiseScale, fog.noiseStrength, fog.noiseVelocity);
    }
    if (key == "localFogColorR" || key == "localFogColorG" || key == "localFogColorB") {
        if (key == "localFogColorR") { fog.color.x = number; }
        if (key == "localFogColorG") { fog.color.y = number; }
        if (key == "localFogColorB") { fog.color.z = number; }
        return volumetricLightRenderer_->SetFogColor(fog.color);
    }
    return ApplyDevelopmentSetting(key, nlohmann::json(value).dump());
}

bool PostEffectManager::ExecuteDevelopmentCommand(const std::string& key)
{
    return ApplyDevelopmentSetting(key, "");
}

nlohmann::json PostEffectManager::GetDevelopmentSettings() const
{
    const auto& p = copyImageRenderer_->GetPostEffectParameter();
    const LightManager* lights = LightManager::GetInstance();
    const Vector4 environment = lights->GetEnvironmentLighting();
    const Vector4 atmosphere = lights->GetAtmosphereSettings();
    const FogData& fog = fogManager_->GetFogData();
    const auto* bloom = bloomRenderer_->GetBloomParameter();
    const auto& volume = volumetricLightRenderer_->GetParameters();
    nlohmann::json state = {
        {"volumeEnabled", volume.enabled}, {"volumeIntensity", volume.lightIntensity},
        {"volumeDensity", volume.fogDensity}, {"volumeDistance", volume.maxDistance},
        {"volumeAnisotropy", volume.anisotropy}, {"volumeSamples", volume.sampleCount},
        {"volumeColorR", volume.lightColor.x}, {"volumeColorG", volume.lightColor.y}, {"volumeColorB", volume.lightColor.z},
        {"animationEnabled", isAnimationEnabled_},
        {"toneMapEnabled", p.toneMapEnabled != 0}, {"toneExposure", p.toneExposure},
        {"toneContrast", p.toneContrast}, {"toneSaturation", p.toneSaturation},
        {"ssaoEnabled", p.ssaoSettings.w > 0.5f}, {"ssaoStrength", p.ssaoSettings.x},
        {"ssaoRadius", p.ssaoSettings.y}, {"ssaoBias", p.ssaoSettings.z},
        {"environmentDiffuse", environment.x}, {"environmentSpecular", environment.y},
        {"atmosphereEnabled", atmosphere.x > 0.5f}, {"atmosphereDensity", atmosphere.y},
        {"atmosphereStrength", atmosphere.z}, {"atmosphereAnisotropy", atmosphere.w},
        {"clusteredLightingEnabled", lights->IsClusteredLightingEnabled()},
        {"fxaaEnabled", fxaaEnabled_}, {"fxaaStrength", p.fxaaStrength},
        {"fxaaSubpixel", p.fxaaSubpixel}, {"fxaaEdgeThreshold", p.fxaaEdgeThreshold},
        {"fxaaEdgeThresholdMin", p.fxaaEdgeThresholdMin},
        {"pixelSize", p.pixelSize}, {"colorBrightness", p.colorBrightness},
        {"colorContrast", p.colorContrast}, {"colorSaturation", p.colorSaturation},
        {"focusDepth", p.focusDepth}, {"focusRange", p.focusRange},
        {"depthOfFieldRadius", p.depthOfFieldRadius},
        {"motionBlurDirectionX", p.motionBlurDirection.x},
        {"motionBlurDirectionY", p.motionBlurDirection.y},
        {"motionBlurStrength", p.motionBlurStrength},
        {"motionBlurSampleCount", p.motionBlurSampleCount},
        {"chromaticAberrationStrength", p.chromaticAberrationStrength},
        {"lensDistortionStrength", p.lensDistortionStrength},
        {"filmGrainStrength", p.filmGrainStrength}, {"lensDirtStrength", p.lensDirtStrength},
        {"cameraShakeStrength", cameraShakeOverride_.value_or(SceneManager::GetInstance()->GetCameraShakeStrength())},
        {"bokehRadius", p.bokehRadius}, {"bokehSides", p.bokehSides},
        {"fisheyeStrength", p.fisheyeStrength},
        {"lightThreshold", p.lightThreshold}, {"lightStrength", p.lightStrength},
        {"lightRadius", p.lightRadius},
        {"outlineNormalThreshold", p.outlineNormalThreshold},
        {"outlineNormalSoftness", p.outlineNormalSoftness},
        {"outlineNormalStrength", p.outlineNormalStrength},
        {"fogEnabled", fog.isEnabled != 0}, {"distanceFogEnabled", fog.distanceEnabled != 0},
        {"fogColorR", fog.color.x}, {"fogColorG", fog.color.y}, {"fogColorB", fog.color.z},
        {"fogStart", fog.distance.start}, {"fogEnd", fog.distance.end},
        {"fogCurve", fog.distance.curve}, {"fogDensity", fog.distance.density},
        {"bloomEnabled", bloom && bloom->isEnabled != 0},
        {"bloomThreshold", bloom ? bloom->threshold : 0.0f},
        {"bloomBlurRadius", bloom ? bloom->blurRadius : 0},
        {"bloomBlurSigma", bloom ? bloom->blurSigma : 0.0f},
        {"bloomIntensity", bloom ? bloom->intensity : 0.0f}
    };
    state["passes"] = nlohmann::json::array();
    for (const auto& pass : SceneManager::GetInstance()->GetPostEffects()) {
        state["passes"].push_back({{"type", static_cast<int>(pass.type)},
                                    {"name", GetPostEffectTypeName(pass.type)},
                                    {"enabled", pass.enabled}});
    }
    for (const auto& pass : state["passes"]) {
        state["pass."+std::to_string(pass["type"].get<int>())] = pass["enabled"];
    }
    const LocalFogParameters& localFog = volumetricLightRenderer_->GetLocalFogParameters();
    state["localFogEnabled"] = localFog.isEnabled;
    state["localFogHeight"] = localFog.baseHeight;
    state["localFogHeightDensity"] = localFog.heightDensity;
    state["localFogHeightFalloff"] = localFog.heightFalloff;
    state["localFogNoiseScale"] = localFog.noiseScale;
    state["localFogNoiseStrength"] = localFog.noiseStrength;
    state["localFogVelocityX"] = localFog.noiseVelocity.x;
    state["localFogVelocityY"] = localFog.noiseVelocity.y;
    state["localFogVelocityZ"] = localFog.noiseVelocity.z;
    state["localFogColorR"] = localFog.color.x;
    state["localFogColorG"] = localFog.color.y;
    state["localFogColorB"] = localFog.color.z;
    for (uint32_t volumeIndex = 0; volumeIndex < VolumetricLightRenderer::kMaxFogVolumes; ++volumeIndex) {
        const FogVolumeSettings& settings = volumetricLightRenderer_->GetFogVolumes()[volumeIndex];
        const std::string prefix = "fogVolume." + std::to_string(volumeIndex) + ".";
        state[prefix + "enabled"] = settings.isEnabled;
        state[prefix + "shape"] = static_cast<int>(settings.shape);
        state[prefix + "centerX"] = settings.center.x;
        state[prefix + "centerY"] = settings.center.y;
        state[prefix + "centerZ"] = settings.center.z;
        state[prefix + "extentX"] = settings.halfExtents.x;
        state[prefix + "extentY"] = settings.halfExtents.y;
        state[prefix + "extentZ"] = settings.halfExtents.z;
        state[prefix + "radius"] = settings.radius;
        state[prefix + "density"] = settings.density;
        state[prefix + "softness"] = settings.edgeSoftness;
    }
    return state;
}

std::string PostEffectManager::GetDevelopmentSettingsJson() const
{
    return GetDevelopmentSettings().dump();
}

bool PostEffectManager::ApplyDevelopmentSetting(const std::string& key, const std::string& value)
{
    if ((key == "toneMapEnabled" || key == "volumeEnabled" || key == "fxaaEnabled" || key == "animationEnabled" || key == "fogEnabled" || key == "distanceFogEnabled" || key == "bloomEnabled" || key.starts_with("pass.")) && value != "true" && value != "false") { return false; }
    const bool isEnabled = value == "true";
    auto& parameters = copyImageRenderer_->GetPostEffectParameter();
    FogData* fog = fogManager_->GetEditableFogData();
    auto* bloom = bloomRenderer_->GetEditableBloomParameter();
    if (key == "toneMapEnabled") {
        parameters.toneMapEnabled = 0;
        if (isEnabled) { parameters.toneMapEnabled = 1; }
        return true;
    }
    if (key == "volumeEnabled") { volumetricLightRenderer_->SetEnabled(isEnabled); return true; }
    if (key == "fxaaEnabled") { fxaaEnabled_ = isEnabled; return true; }
    if (key == "animationEnabled") {
        isAnimationEnabled_ = isEnabled;
        parameters.animationEnabled = 0;
        if (isEnabled) { parameters.animationEnabled = 1; }
        return true;
    }
    if (key == "fogEnabled" && fog) { fog->isEnabled = isEnabled; return true; }
    if (key == "distanceFogEnabled" && fog) { fog->distanceEnabled = isEnabled; return true; }
    if (key == "bloomEnabled" && bloom) { bloom->isEnabled = isEnabled; return true; }
    if (key.starts_with("pass.")) {
        char* typeEnd = nullptr;
        const long type = std::strtol(key.c_str() + 5, &typeEnd, 10);
        if (typeEnd == key.c_str() + 5 || *typeEnd != '\0') { return false; }
        for (const auto& pass : SceneManager::GetInstance()->GetPostEffects()) {
            if (static_cast<int>(pass.type) == type) {
                passOverrides_[type] = isEnabled;
                SceneManager::GetInstance()->SetPostEffectEnabled(pass.type, isEnabled);
                return true;
            }
        }
        return false;
    }
    char* numberEnd = nullptr;
    const float number = std::strtof(value.c_str(), &numberEnd);
    if (numberEnd == value.c_str() || *numberEnd != '\0' || !std::isfinite(number)) { return false; }
    if (key == "volumeIntensity") { volumetricLightRenderer_->SetLightIntensity(number); return true; }
    if (key == "volumeDensity") { volumetricLightRenderer_->SetFogDensity(number); return true; }
    if (key == "volumeDistance") { volumetricLightRenderer_->SetMaxDistance(number); return true; }
    if (key == "volumeAnisotropy") { volumetricLightRenderer_->SetAnisotropy(number); return true; }
    if (key == "volumeSamples") { volumetricLightRenderer_->SetSampleCount(static_cast<int32_t>(std::clamp(number, 8.0f, 64.0f))); return true; }
    if (key == "volumeColorR" || key == "volumeColorG" || key == "volumeColorB") {
        Vector3 color = volumetricLightRenderer_->GetParameters().lightColor;
        if (key == "volumeColorR") { color.x = number; }
        if (key == "volumeColorG") { color.y = number; }
        if (key == "volumeColorB") { color.z = number; }
        volumetricLightRenderer_->SetLightColor(color);
        return true;
    }
#define SET_FLOAT(name, field, low, high) if (key == name) { field = std::clamp(number, low, high); return true; }
#define SET_INT(name, field, low, high) if (key == name) { field = static_cast<int>(std::clamp(number, float(low), float(high))); return true; }
    SET_FLOAT("toneExposure", parameters.toneExposure, 0.1f, 4.0f)
    SET_FLOAT("toneContrast", parameters.toneContrast, 0.5f, 1.5f)
    SET_FLOAT("toneSaturation", parameters.toneSaturation, 0.0f, 2.0f)
    SET_FLOAT("fxaaStrength", parameters.fxaaStrength, 0.0f, 1.0f)
    SET_FLOAT("fxaaSubpixel", parameters.fxaaSubpixel, 0.0f, 1.0f)
    SET_FLOAT("fxaaEdgeThreshold", parameters.fxaaEdgeThreshold, 0.01f, 0.5f)
    SET_FLOAT("fxaaEdgeThresholdMin", parameters.fxaaEdgeThresholdMin, 0.0f, 0.2f)
    SET_FLOAT("pixelSize", parameters.pixelSize, 1.0f, 64.0f)
    SET_FLOAT("colorBrightness", parameters.colorBrightness, -1.0f, 1.0f)
    SET_FLOAT("colorContrast", parameters.colorContrast, 0.0f, 3.0f)
    SET_FLOAT("colorSaturation", parameters.colorSaturation, 0.0f, 3.0f)
    SET_FLOAT("focusDepth", parameters.focusDepth, 0.0f, 1.0f)
    SET_FLOAT("focusRange", parameters.focusRange, 0.0001f, 0.2f)
    SET_FLOAT("depthOfFieldRadius", parameters.depthOfFieldRadius, 0.0f, 32.0f)
    SET_FLOAT("motionBlurDirectionX", parameters.motionBlurDirection.x, -1.0f, 1.0f)
    SET_FLOAT("motionBlurDirectionY", parameters.motionBlurDirection.y, -1.0f, 1.0f)
    SET_FLOAT("motionBlurStrength", parameters.motionBlurStrength, 0.0f, 0.1f)
    SET_INT("motionBlurSampleCount", parameters.motionBlurSampleCount, 1, 32)
    SET_FLOAT("chromaticAberrationStrength", parameters.chromaticAberrationStrength, 0.0f, 0.05f)
    SET_FLOAT("lensDistortionStrength", parameters.lensDistortionStrength, -1.0f, 1.0f)
    SET_FLOAT("filmGrainStrength", parameters.filmGrainStrength, 0.0f, 0.5f)
    SET_FLOAT("lensDirtStrength", parameters.lensDirtStrength, 0.0f, 3.0f)
    SET_FLOAT("bokehRadius", parameters.bokehRadius, 0.0f, 32.0f)
    SET_INT("bokehSides", parameters.bokehSides, 3, 12)
    SET_FLOAT("fisheyeStrength", parameters.fisheyeStrength, 0.01f, 3.0f)
    SET_FLOAT("lightThreshold", parameters.lightThreshold, 0.0f, 2.0f)
    SET_FLOAT("lightStrength", parameters.lightStrength, 0.0f, 5.0f)
    SET_FLOAT("lightRadius", parameters.lightRadius, 0.01f, 1.0f)
    SET_FLOAT("outlineNormalThreshold", parameters.outlineNormalThreshold, 0.0f, 1.0f)
    SET_FLOAT("outlineNormalSoftness", parameters.outlineNormalSoftness, 0.001f, 1.0f)
    SET_FLOAT("outlineNormalStrength", parameters.outlineNormalStrength, 0.0f, 1.0f)
    if (key == "cameraShakeStrength") {
        cameraShakeOverride_ = std::clamp(number, 0.0f, 0.05f);
        SceneManager::GetInstance()->SetCameraShakeStrength(*cameraShakeOverride_);
        return true;
    }
    if (fog) {
        SET_FLOAT("fogColorR", fog->color.x, 0.0f, 1.0f)
        SET_FLOAT("fogColorG", fog->color.y, 0.0f, 1.0f)
        SET_FLOAT("fogColorB", fog->color.z, 0.0f, 1.0f)
        SET_FLOAT("fogStart", fog->distance.start, 0.0f, 3000.0f)
        SET_FLOAT("fogEnd", fog->distance.end, 0.01f, 3000.0f)
        SET_FLOAT("fogCurve", fog->distance.curve, 0.01f, 8.0f)
        SET_FLOAT("fogDensity", fog->distance.density, 0.0f, 1.0f)
    }
    if (bloom) {
        SET_FLOAT("bloomThreshold", bloom->threshold, 0.0f, 5.0f)
        SET_INT("bloomBlurRadius", bloom->blurRadius, 0, 32)
        SET_FLOAT("bloomBlurSigma", bloom->blurSigma, 0.01f, 20.0f)
        SET_FLOAT("bloomIntensity", bloom->intensity, 0.0f, 5.0f)
    }
#undef SET_FLOAT
#undef SET_INT
    return false;
}
#endif

void PostEffectManager::Initialize(DirectXCommon* dxCommon)
{
    finalPassTimer_.Initialize();
    assert(dxCommon != nullptr);
    dxCommon_ = dxCommon;

    copyImageRenderer_ = std::make_unique<CopyImageRenderer>();
    copyImageRenderer_->Initialize(dxCommon_);
    std::ifstream presetFile("resources/Graphics/visual-presets.json");
    if (!presetFile) { throw std::runtime_error("Cannot open post effect preset data"); }
    const auto presetData = nlohmann::json::parse(presetFile);
    for (const auto& entry : presetData.at("effectParameters").items()) {
        bool isFound = false;
        for (int typeIndex = 0; typeIndex <= static_cast<int>(PostEffectType::ScreenLighting); ++typeIndex) {
            const auto type = static_cast<PostEffectType>(typeIndex);
            if (entry.key() == GetPostEffectTypeName(type)) {
                defaultEffectParameters_[type] = { entry.value().at(0).get<float>(),
                    entry.value().at(1).get<float>(), entry.value().at(2).get<float>() };
                isFound = true;
                break;
            }
        }
        if (!isFound) { throw std::runtime_error("Unknown post effect parameter preset: " + entry.key()); }
    }

    bloomRenderer_ = std::make_unique<BloomRenderer>();
    bloomRenderer_->Initialize(dxCommon_);
    volumetricLightRenderer_ = std::make_unique<VolumetricLightRenderer>();
    volumetricLightRenderer_->Initialize(dxCommon_);

    fogManager_ = std::make_unique<FogManager>();
    fogManager_->Initialize(dxCommon_);

    fogRenderer_ = std::make_unique<FogRenderer>();
    fogRenderer_->Initialize(dxCommon_);

    for (uint32_t index = 0; index < kPingPongRenderTargetCount; ++index) {
        pingPongRenderTargets_[index].Initialize(dxCommon_, kFirstPingPongRTVIndex + index);
    }
}

bool PostEffectManager::SetSsao(bool isEnabled, float strength, float radius, float bias)
{
    if (!std::isfinite(strength) || !std::isfinite(radius) || !std::isfinite(bias) ||
        strength < 0.0f || strength > 0.6f || radius < 0.1f || radius > 30.0f || bias < 0.0f || bias > radius) { return false; }
    float enabled = 0.0f;
    if (isEnabled) { enabled = 1.0f; }
    copyImageRenderer_->GetPostEffectParameter().ssaoSettings = { strength, radius, bias, enabled };
    return true;
}

void PostEffectManager::UpdateCameraInputs(Camera* camera)
{
    auto& screenParameters = copyImageRenderer_->GetPostEffectParameter();
    screenParameters.screenCameraSettings.z = 0.0f;
    LightManager* lights = LightManager::GetInstance();
    screenParameters.atmosphereSettings = lights->GetAtmosphereSettings();
    const DirectionalLight sun = lights->GetDirectionalLight();
    screenParameters.screenSunDirection = { -sun.direction.x, -sun.direction.y, -sun.direction.z, sun.intensity };
    screenParameters.screenSunColor = sun.color;
    if (camera != nullptr && camera->GetNearClip() > 0.0f && camera->GetFarClip() > camera->GetNearClip()) {
        screenParameters.screenInverseProjection = MatrixMath::Inverse(camera->GetProjectionMatrix());
        screenParameters.screenCameraRotation = camera->GetWorldMatrix();
        screenParameters.screenCameraRotation.m[3][0] = 0.0f;
        screenParameters.screenCameraRotation.m[3][1] = 0.0f;
        screenParameters.screenCameraRotation.m[3][2] = 0.0f;
        bool isFiniteCamera = true;
        for (uint32_t row = 0; row < 4; ++row) {
            for (uint32_t column = 0; column < 4; ++column) {
                if (!std::isfinite(screenParameters.screenInverseProjection.m[row][column]) ||
                    !std::isfinite(screenParameters.screenCameraRotation.m[row][column])) { isFiniteCamera = false; }
            }
        }
        if (isFiniteCamera) {
            float hasNormals = 0.0f;
            if (normalTextureHandle_.ptr != 0) { hasNormals = 1.0f; }
            screenParameters.screenCameraSettings = { camera->GetNearClip(), camera->GetFarClip(), 1.0f, hasNormals };
        }
    }
}

void PostEffectManager::Update(Camera* camera)
{
    auto& animationParameters = copyImageRenderer_->GetPostEffectParameter();
    if (isAnimationEnabled_) { animationParameters.time += TimeManager::GetInstance()->GetDeltaTime(); }
    if (animationParameters.time > 1000.0f) { animationParameters.time = 0.0f; }
    UpdateCameraInputs(camera);
    SceneManager* sceneManager = SceneManager::GetInstance();
    if (sceneExposureRevision_ != sceneManager->GetSceneExposureRevision()) {
        copyImageRenderer_->GetPostEffectParameter().toneExposure = sceneManager->GetSceneExposure();
        sceneExposureRevision_ = sceneManager->GetSceneExposureRevision();
    }
    if (FogData* fogData = fogManager_->GetEditableFogData()) {
        SceneManager* sceneManager = SceneManager::GetInstance();
        fogData->color = sceneManager->GetSceneFogColor();
        // Apply a preset once, so development sliders remain usable afterwards.
        if (sceneFogRevision_ != sceneManager->GetSceneFogRevision()) {
            fogData->distance = sceneManager->GetSceneDistanceFog();
            sceneFogRevision_ = sceneManager->GetSceneFogRevision();
        }
    }
    if (camera != nullptr) {
        auto& parameter = copyImageRenderer_->GetPostEffectParameter();
        parameter.outlineNearClip = camera->GetNearClip();
        parameter.outlineFarClip = camera->GetFarClip();
        fogManager_->SetCameraInfo(
            camera->GetNearClip(),
            camera->GetFarClip(),
            camera->GetFovY(),
            camera->GetAspectRatio());
    }

    fogManager_->Update();
    bloomRenderer_->Update();
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (cameraShakeOverride_) {
        SceneManager::GetInstance()->SetCameraShakeStrength(*cameraShakeOverride_);
        if (*cameraShakeOverride_ > 0.0f) {
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::CameraShake, PostEffectStage::BeforeParticle);
        } else {
            SceneManager::GetInstance()->RemovePostEffect(PostEffectType::CameraShake);
        }
    }
    for (const auto& [type, enabled] : passOverrides_) {
        for (const auto& pass : SceneManager::GetInstance()->GetPostEffects()) {
            if (static_cast<int>(pass.type) == type) {
                SceneManager::GetInstance()->SetPostEffectEnabled(pass.type, enabled);
                break;
            }
        }
    }
#endif
}

void PostEffectManager::DrawImGui()
{
    fogManager_->DrawImGui();
    bloomRenderer_->DrawImGui();
    volumetricLightRenderer_->DrawImGui();

#ifdef USE_IMGUI
    ImGui::Begin("Post Effects");

    CopyImageRenderer::PostEffectParameter& parameter = copyImageRenderer_->GetPostEffectParameter();
    bool toneEnabled = parameter.toneMapEnabled != 0;
    if (ImGui::Checkbox("Tone Mapping", &toneEnabled)) {
        parameter.toneMapEnabled = 0;
        if (toneEnabled) { parameter.toneMapEnabled = 1; }
    }
    ImGui::SliderFloat("Exposure", &parameter.toneExposure, 0.1f, 4.0f);
    ImGui::SliderFloat("Final Contrast", &parameter.toneContrast, 0.5f, 1.5f);
    ImGui::SliderFloat("Final Saturation", &parameter.toneSaturation, 0.0f, 2.0f);
    ImGui::Checkbox("FXAA", &fxaaEnabled_);
    ImGui::SliderFloat("FXAA Strength", &parameter.fxaaStrength, 0.0f, 1.0f);
    ImGui::SliderFloat("FXAA Subpixel", &parameter.fxaaSubpixel, 0.0f, 1.0f);
    ImGui::SliderFloat("FXAA Edge Threshold", &parameter.fxaaEdgeThreshold, 0.01f, 0.5f);
    ImGui::SliderFloat("FXAA Minimum Threshold", &parameter.fxaaEdgeThresholdMin, 0.0f, 0.2f);
    const char* animationButtonLabel = "Animated Effects: OFF";
    if (isAnimationEnabled_) {
        animationButtonLabel = "Animated Effects: ON";
    }
    if (ImGui::Button(animationButtonLabel)) {
        isAnimationEnabled_ = !isAnimationEnabled_;
        if (isAnimationEnabled_) { parameter.animationEnabled = 1; } else { parameter.animationEnabled = 0; }
    }
    ImGui::SliderFloat("Pixel Size", &parameter.pixelSize, 1.0f, 64.0f, "%.0f");
    ImGui::SliderFloat("Brightness", &parameter.colorBrightness, -1.0f, 1.0f);
    ImGui::SliderFloat("Contrast", &parameter.colorContrast, 0.0f, 3.0f);
    ImGui::SliderFloat("Saturation", &parameter.colorSaturation, 0.0f, 3.0f);
    ImGui::SliderFloat("Focus Depth", &parameter.focusDepth, 0.0f, 1.0f, "%.4f");
    ImGui::SliderFloat("Focus Range", &parameter.focusRange, 0.0001f, 0.2f, "%.4f");
    ImGui::SliderFloat("DoF Radius", &parameter.depthOfFieldRadius, 0.0f, 32.0f);
    ImGui::SliderFloat2("Motion Direction", &parameter.motionBlurDirection.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Motion Strength", &parameter.motionBlurStrength, 0.0f, 0.1f);
    ImGui::SliderInt("Motion Samples", &parameter.motionBlurSampleCount, 1, 32);
    ImGui::SliderFloat("Chromatic Aberration", &parameter.chromaticAberrationStrength, 0.0f, 0.05f);
    ImGui::SliderFloat("Lens Distortion", &parameter.lensDistortionStrength, -1.0f, 1.0f);
    ImGui::SliderFloat("Film Grain", &parameter.filmGrainStrength, 0.0f, 0.5f);
    ImGui::SliderFloat("Lens Dirt", &parameter.lensDirtStrength, 0.0f, 3.0f);
    float cameraShakeStrength = SceneManager::GetInstance()->GetCameraShakeStrength();
    if (ImGui::SliderFloat("Camera Shake", &cameraShakeStrength, 0.0f, 0.05f)) {
        SceneManager::GetInstance()->SetCameraShakeStrength(cameraShakeStrength);
    }
    ImGui::SliderFloat("Bokeh Radius", &parameter.bokehRadius, 0.0f, 32.0f);
    ImGui::SliderInt("Bokeh Sides", &parameter.bokehSides, 3, 12);
    ImGui::SliderFloat("Fisheye", &parameter.fisheyeStrength, 0.01f, 3.0f);
    ImGui::SliderFloat("Light Threshold", &parameter.lightThreshold, 0.0f, 2.0f);
    ImGui::SliderFloat("Light Strength", &parameter.lightStrength, 0.0f, 5.0f);
    ImGui::SliderFloat("Light Radius", &parameter.lightRadius, 0.01f, 1.0f);
    ImGui::SliderFloat("Outline Normal Threshold", &parameter.outlineNormalThreshold, 0.0f, 1.0f);
    ImGui::SliderFloat("Outline Normal Softness", &parameter.outlineNormalSoftness, 0.001f, 1.0f);
    ImGui::SliderFloat("Outline Normal Strength", &parameter.outlineNormalStrength, 0.0f, 1.0f);
    ImGui::Separator();

    const std::vector<PostEffectInfo>& postEffects = SceneManager::GetInstance()->GetPostEffects();
    if (postEffects.empty()) {
        ImGui::Text("1: Copy (implicit)");
    }

    for (std::size_t index = 0; index < postEffects.size(); ++index) {
        const PostEffectInfo& postEffect = postEffects[index];

        ImGui::Text("%u: %s", static_cast<unsigned int>(index + 1), GetPostEffectTypeName(postEffect.type));
        ImGui::SameLine();

        bool enabled = postEffect.enabled;
        char label[64] {};
        sprintf_s(label, sizeof(label), "Enabled##PostEffect%u", static_cast<unsigned int>(index));
        if (ImGui::Checkbox(label, &enabled)) {
            SceneManager::GetInstance()->SetPostEffectEnabled(postEffect.type, enabled);
        }
    }

    ImGui::End();
#endif
}

void PostEffectManager::PreDrawDepth()
{
    sceneDepthReady_ = false;
    fogRenderer_->PreDrawDepth();
}

void PostEffectManager::PostDrawDepth()
{
    fogRenderer_->PostDrawDepth();
    sceneDepthReady_ = true;
}

void PostEffectManager::PrepareDepthForParticleDraw()
{
    sceneDepthReady_ = false;
    fogRenderer_->PrepareDepthForParticleDraw();
}

void PostEffectManager::UpdatePostEffectParameters(
    SceneManager* sceneManager)
{
    if (sceneManager == nullptr) {
        return;
    }

    CopyImageRenderer::PostEffectParameter& postEffectParameter =
        copyImageRenderer_->GetPostEffectParameter();
    const RadialBlurSettings& blur = sceneManager->GetRadialBlurSettings();
    postEffectParameter.radialBlurSampleCount = blur.sampleCount;
    postEffectParameter.radialBlurWidth = blur.width;
    postEffectParameter.radialBlurImpulseStrength = blur.impulseStrength;

    postEffectParameter.radialBlurCenter =
        sceneManager->GetPostEffectCenter();
    postEffectParameter.cameraShakeStrength =
        sceneManager->GetCameraShakeStrength();
    postEffectParameter.vignetteStrength =
        sceneManager->GetVignetteStrength();
    postEffectParameter.sonicBoomProgress =
        sceneManager->GetSonicBoomProgress();
    postEffectParameter.sonicBoomCenter =
        sceneManager->GetSonicBoomCenter();
    postEffectParameter.blackHoleCenter =
        sceneManager->GetBlackHoleCenter();
    postEffectParameter.blackHoleRadius =
        sceneManager->GetBlackHoleRadius();
    postEffectParameter.blackHoleStrength =
        sceneManager->GetBlackHoleStrength();
    postEffectParameter.waterEffectIntensity =
        sceneManager->GetWaterEffectIntensity();
    postEffectParameter.paintProgress =
        sceneManager->GetPaintProgress();
    postEffectParameter.paintIntensity =
        sceneManager->GetPaintIntensity();
    postEffectParameter.paintSeed =
        sceneManager->GetPaintSeed();
    postEffectParameter.paintPatternType =
        sceneManager->GetPaintPatternType();
    postEffectParameter.paintColor =
        sceneManager->GetPaintColor();
}

void PostEffectManager::Apply(SceneManager* sceneManager, D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    PrepareSceneForParticleDraw(sceneManager, sceneColorHandle);
    ApplyAfterParticleDraw(sceneManager);
}

void PostEffectManager::PrepareSceneForParticleDraw(SceneManager* sceneManager, D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    PrepareSceneForTemporalResolve(sceneManager, sceneColorHandle);
}

ID3D12Resource* PostEffectManager::GetSceneColorTexture() const
{
    return pingPongRenderTargets_[particleCompositionTargetIndex_].GetTexture();
}

D3D12_GPU_DESCRIPTOR_HANDLE PostEffectManager::GetSceneColorSrv() const
{
    return pingPongRenderTargets_[particleCompositionTargetIndex_].GetSrvHandleGPU();
}

void PostEffectManager::ReplaceSceneColor(D3D12_GPU_DESCRIPTOR_HANDLE colorSrv)
{
    if (colorSrv.ptr == GetSceneColorSrv().ptr) { return; }
    uint32_t targetIndex = GetNextPingPongIndex(particleCompositionTargetIndex_);
    RenderTarget& target = pingPongRenderTargets_[targetIndex];
    target.BeginRender();
    ApplyPostEffectToCurrentTarget(PostEffectType::Copy, colorSrv);
    target.EndRender();
    particleCompositionTargetIndex_ = targetIndex;
}

void PostEffectManager::PrepareSceneForTemporalResolve(
    SceneManager* sceneManager,
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    UpdatePostEffectParameters(sceneManager);

    particleCompositionTargetIndex_ = 0;
    bool volumeApplied = false;
    auto& screenSettings = copyImageRenderer_->GetPostEffectParameter();
    screenSettings.screenCameraSettings.w = 0.0f;
    if (normalTextureHandle_.ptr != 0) { screenSettings.screenCameraSettings.w = 1.0f; }
    if (sceneDepthReady_ && screenSettings.screenCameraSettings.z > 0.5f &&
        ((screenSettings.ssaoSettings.w > 0.5f && normalTextureHandle_.ptr != 0) || screenSettings.atmosphereSettings.x > 0.5f)) {
        RenderTarget& target = pingPongRenderTargets_[0];
        target.BeginRender();
        ApplyPostEffectToCurrentTarget(PostEffectType::ScreenLighting, sceneColorHandle);
        target.EndRender();
        sceneColorHandle = target.GetSrvHandleGPU();
        volumeApplied = true;
    }
    if (volumetricLightRenderer_->Generate(fogRenderer_->GetDepthSRVHandle(), sceneDepthReady_)) {
        uint32_t targetIndex = 0;
        if (volumeApplied) { targetIndex = 1; }
        particleCompositionTargetIndex_ = targetIndex;
        RenderTarget& target = pingPongRenderTargets_[targetIndex];
        target.BeginRender();
        volumetricLightRenderer_->Composite(sceneColorHandle);
        target.EndRender();
        sceneColorHandle = target.GetSrvHandleGPU();
        volumeApplied = true;
    }

    if (sceneManager == nullptr) {
        if (volumeApplied) { return; }
        RenderTarget& renderTarget =
            pingPongRenderTargets_[particleCompositionTargetIndex_];
        renderTarget.BeginRender();
        ApplyPostEffectToCurrentTarget(
            PostEffectType::Copy,
            sceneColorHandle);
        renderTarget.EndRender();
        return;
    }

    const std::vector<PostEffectInfo>& postEffects =
        sceneManager->GetPostEffects();

    D3D12_GPU_DESCRIPTOR_HANDLE inputHandle =
        sceneColorHandle;
    uint32_t targetIndex = 0;
    if (volumeApplied) { targetIndex = GetNextPingPongIndex(particleCompositionTargetIndex_); }
    bool appliedSceneEffect = volumeApplied;

    for (const PostEffectInfo& postEffect : postEffects) {
        if (!postEffect.enabled || postEffect.type == PostEffectType::FXAA ||
            postEffect.type == PostEffectType::ToneMap || postEffect.type == PostEffectType::Bloom) {
            continue;
        }

        if (postEffect.stage !=
            PostEffectStage::BeforeParticle || !IsTemporalResolveInputEffect(postEffect.type)) {
            continue;
        }

        RenderTarget& renderTarget =
            pingPongRenderTargets_[targetIndex];

        renderTarget.BeginRender();
        ApplyPostEffectToCurrentTarget(postEffect.type, inputHandle);
        renderTarget.EndRender();

        inputHandle = renderTarget.GetSrvHandleGPU();
        particleCompositionTargetIndex_ = targetIndex;
        targetIndex = GetNextPingPongIndex(targetIndex);
        appliedSceneEffect = true;
    }

    if (!appliedSceneEffect) {
        particleCompositionTargetIndex_ = 0;
        RenderTarget& renderTarget =
            pingPongRenderTargets_[particleCompositionTargetIndex_];
        renderTarget.BeginRender();
        ApplyPostEffectToCurrentTarget(
            PostEffectType::Copy,
            sceneColorHandle);
        renderTarget.EndRender();
    }
}

void PostEffectManager::BeginParticleDraw()
{
    pingPongRenderTargets_[particleCompositionTargetIndex_]
        .BeginRenderWithDepth(GetDepthDSVHandle());
}

void PostEffectManager::EndParticleDraw()
{
    pingPongRenderTargets_[particleCompositionTargetIndex_]
        .EndRender();
}

void PostEffectManager::ApplyAfterParticleDraw(SceneManager* sceneManager)
{
    D3D12_GPU_DESCRIPTOR_HANDLE inputHandle =
        pingPongRenderTargets_[particleCompositionTargetIndex_].GetSrvHandleGPU();
    uint32_t targetIndex = GetNextPingPongIndex(particleCompositionTargetIndex_);
    if (sceneManager != nullptr) {
        for (const PostEffectInfo& effect : sceneManager->GetPostEffects()) {
            if (!effect.enabled || (effect.stage != PostEffectStage::AfterParticle &&
                (effect.stage != PostEffectStage::BeforeParticle || IsTemporalResolveInputEffect(effect.type))) ||
                effect.type == PostEffectType::FXAA || effect.type == PostEffectType::ToneMap ||
                effect.type == PostEffectType::Bloom) { continue; }
            RenderTarget& target = pingPongRenderTargets_[targetIndex];
            target.BeginRender();
            ApplyPostEffectToCurrentTarget(effect.type, inputHandle);
            target.EndRender();
            inputHandle = target.GetSrvHandleGPU();
            particleCompositionTargetIndex_ = targetIndex;
            targetIndex = GetNextPingPongIndex(targetIndex);
        }
    }
    FinishSceneColor(inputHandle);
}

void PostEffectManager::FinishSceneColor(D3D12_GPU_DESCRIPTOR_HANDLE inputHandle)
{
    if (bloomRenderer_->IsEnabled()) {
        bloomRenderer_->Generate(inputHandle);
        RenderTarget& target = pingPongRenderTargets_[GetNextPingPongIndex(particleCompositionTargetIndex_)];
        target.BeginRender();
        bloomRenderer_->Composite(inputHandle);
        target.EndRender();
        inputHandle = target.GetSrvHandleGPU();
    }
    SetBackBufferRenderTarget();
    PostEffectType finalType = PostEffectType::ToneMap;
    if (fxaaEnabled_) { finalType = PostEffectType::FXAA; }
    finalPassTimer_.Begin();
    ApplyPostEffectToCurrentTarget(finalType, inputHandle);
    finalPassTimer_.End();
}

D3D12_CPU_DESCRIPTOR_HANDLE PostEffectManager::GetDepthDSVHandle() const
{
    return fogRenderer_->GetDepthDSVHandle();
}
ID3D12Resource* PostEffectManager::GetDepthTexture() const
{
    return fogRenderer_->GetDepthTexture();
}

D3D12_GPU_VIRTUAL_ADDRESS PostEffectManager::GetFogConstantBufferView() const
{
    return fogManager_->GetConstantBufferView();
}

void PostEffectManager::ApplyPostEffectToCurrentTarget(PostEffectType type, D3D12_GPU_DESCRIPTOR_HANDLE inputHandle)
{
    Vector3 customParameters {};
    const auto defaults = defaultEffectParameters_.find(type);
    if (defaults != defaultEffectParameters_.end()) { customParameters = defaults->second; }
    const Vector3* overrides = SceneManager::GetInstance()->FindPostEffectParameters(type);
    if (overrides != nullptr) { customParameters = *overrides; }
    auto& parameters = copyImageRenderer_->GetPostEffectParameter();
    parameters.customParameter0 = customParameters.x;
    parameters.customParameter1 = customParameters.y;
    parameters.customParameter2 = customParameters.z;
    DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (type == PostEffectType::FXAA || type == PostEffectType::ToneMap) {
        format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }
    copyImageRenderer_->SetOutputFormat(format);
    if (type == PostEffectType::Fog) {
        D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView = fogManager_->GetConstantBufferView();
        if (fogConstantBufferView == 0) {
            copyImageRenderer_->SetPostEffectType(PostEffectType::Copy);
            copyImageRenderer_->Draw(
                inputHandle, fogRenderer_->GetDepthSRVHandle(), normalTextureHandle_);
            return;
        }

        fogRenderer_->Apply(inputHandle, fogConstantBufferView);
        return;
    }

    if (type == PostEffectType::Bloom) {
        copyImageRenderer_->SetPostEffectType(PostEffectType::Copy);
        copyImageRenderer_->Draw(
            inputHandle, fogRenderer_->GetDepthSRVHandle(), normalTextureHandle_);
        return;
    }

    copyImageRenderer_->SetPostEffectType(type);
    copyImageRenderer_->Draw(
        inputHandle, fogRenderer_->GetDepthSRVHandle(), normalTextureHandle_);
}

void PostEffectManager::SetBackBufferRenderTarget()
{
    dxCommon_->SetBackBufferRenderTarget(dxCommon_->GetDSVHandle());
}

uint32_t PostEffectManager::GetNextPingPongIndex(uint32_t currentIndex) const
{
    uint32_t nextIndex = currentIndex + 1;
    if (nextIndex >= kPingPongRenderTargetCount) {
        nextIndex = 0;
    }

    return nextIndex;
}

void PostEffectManager::RenderTarget::Initialize(DirectXCommon* dxCommon, uint32_t rtvIndex)
{
    assert(dxCommon != nullptr);
    dxCommon_ = dxCommon;

    CreateResource();
    CreateViews(rtvIndex);

    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void PostEffectManager::RenderTarget::BeginRender()
{
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, nullptr);
    commandList->ClearRenderTargetView(rtvHandle_, clearColor_, 0, nullptr);
}

void PostEffectManager::RenderTarget::BeginRenderWithDepth(
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12GraphicsCommandList* commandList =
        dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->OMSetRenderTargets(
        1,
        &rtvHandle_,
        false,
        &dsvHandle);
}

void PostEffectManager::RenderTarget::EndRender()
{
    Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

D3D12_GPU_DESCRIPTOR_HANDLE PostEffectManager::RenderTarget::GetSrvHandleGPU() const
{
    return srvHandleGPU_;
}

void PostEffectManager::RenderTarget::CreateResource()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = WinApp::kClientWidth;
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format_;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue {};
    clearValue.Format = format_;
    clearValue.Color[0] = clearColor_[0];
    clearValue.Color[1] = clearColor_[1];
    clearValue.Color[2] = clearColor_[2];
    clearValue.Color[3] = clearColor_[3];

    HRESULT result = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource_));
    assert(SUCCEEDED(result));

    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void PostEffectManager::RenderTarget::CreateViews(uint32_t rtvIndex)
{
    ID3D12Device* device = dxCommon_->GetDevice();

    rtvHandle_ = dxCommon_->GetRTVHandle(rtvIndex);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    srvIndex_ = SrvManager::GetInstance()->Allocate();
    srvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex_);
    srvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
    device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);
}

void PostEffectManager::RenderTarget::Transition(D3D12_RESOURCE_STATES nextState)
{
    if (currentState_ == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = currentState_;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    currentState_ = nextState;
}
