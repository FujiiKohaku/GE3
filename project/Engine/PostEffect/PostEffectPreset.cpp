#include "PostEffectManager.h"
#include "Bloom/BloomRenderer.h"
#include "App/Scene/Common/SceneManager.h"
#include "Engine/Serialization/JsonFile.h"
#include <set>
#include <cmath>

namespace {
using Parameters = CopyImageRenderer::PostEffectParameter;
struct FloatBinding { const char* key; float Parameters::* member; };
constexpr FloatBinding kFloatBindings[] = {
    { "grayScaleStrength", &Parameters::grayScaleStrength }, { "vignetteStrength", &Parameters::vignetteStrength },
    { "outlineScale", &Parameters::outlineScale }, { "outlineThreshold", &Parameters::outlineThreshold },
    { "outlineSoftness", &Parameters::outlineSoftness }, { "outlineNormalStrength", &Parameters::outlineNormalStrength },
    { "radialBlurWidth", &Parameters::radialBlurWidth }, { "dissolveThreshold", &Parameters::dissolveThreshold },
    { "dissolveEdgeWidth", &Parameters::dissolveEdgeWidth }, { "dissolveEdgeStrength", &Parameters::dissolveEdgeStrength },
    { "pixelSize", &Parameters::pixelSize }, { "colorBrightness", &Parameters::colorBrightness },
    { "colorContrast", &Parameters::colorContrast }, { "colorSaturation", &Parameters::colorSaturation },
    { "focusDepth", &Parameters::focusDepth }, { "focusRange", &Parameters::focusRange },
    { "depthOfFieldRadius", &Parameters::depthOfFieldRadius }, { "motionBlurStrength", &Parameters::motionBlurStrength },
    { "chromaticAberrationStrength", &Parameters::chromaticAberrationStrength },
    { "lensDistortionStrength", &Parameters::lensDistortionStrength }, { "filmGrainStrength", &Parameters::filmGrainStrength },
    { "lensDirtStrength", &Parameters::lensDirtStrength }, { "cameraShakeStrength", &Parameters::cameraShakeStrength },
    { "bokehRadius", &Parameters::bokehRadius }, { "fisheyeStrength", &Parameters::fisheyeStrength },
    { "lightThreshold", &Parameters::lightThreshold }, { "lightStrength", &Parameters::lightStrength },
    { "lightRadius", &Parameters::lightRadius }, { "lightAngle", &Parameters::lightAngle }
};
void ValidateFinite(const nlohmann::json& value)
{
    if (value.is_number() && (!std::isfinite(value.get<double>()) || std::abs(value.get<double>()) > 1.0e8)) {
        throw std::runtime_error("設定値が範囲外です");
    }
    if (value.is_structured()) { for (const auto& child : value) { ValidateFinite(child); } }
}
PostEffectType ParseType(const std::string& name)
{
    for (int index = 0; index <= static_cast<int>(PostEffectType::ScreenLighting); ++index) {
        const auto type = static_cast<PostEffectType>(index);
        if (name == GetPostEffectTypeName(type)) { return type; }
    }
    throw std::runtime_error("未対応のポストエフェクト: " + name);
}
}

nlohmann::json PostEffectManager::GetPresetConfig() const
{
    nlohmann::json config = presetConfig_;
    if (!config.is_object()) { config = nlohmann::json::object(); }
    config["formatVersion"] = 1;
    if (!config.contains("name")) { config["name"] = "標準"; }
    config["effects"] = nlohmann::json::array();
    auto* scene = SceneManager::GetInstance();
    for (const auto& effect : scene->GetPostEffects()) {
        std::string stage = "AfterParticle";
        if (effect.stage == PostEffectStage::BeforeParticle) { stage = "BeforeParticle"; }
        config["effects"].push_back({ { "type", GetPostEffectTypeName(effect.type) },
            { "stage", stage }, { "enabled", effect.enabled } });
        Vector3 parameters {};
        const auto defaults = defaultEffectParameters_.find(effect.type);
        if (defaults != defaultEffectParameters_.end()) { parameters = defaults->second; }
        const auto* overrides = scene->FindPostEffectParameters(effect.type);
        if (overrides != nullptr) { parameters = *overrides; }
        config["effectParameters"][GetPostEffectTypeName(effect.type)] = { parameters.x, parameters.y, parameters.z };
    }
    const auto& parameters = copyImageRenderer_->GetPostEffectParameter();
    for (const auto& binding : kFloatBindings) { config["parameters"][binding.key] = parameters.*(binding.member); }
    config["parameters"]["radialBlurSampleCount"] = parameters.radialBlurSampleCount;
    const auto& bloom = *bloomRenderer_->GetBloomParameter();
    config["bloom"] = { { "threshold", bloom.threshold }, { "intensity", bloom.intensity },
        { "blurRadius", bloom.blurRadius }, { "blurSigma", bloom.blurSigma },
        { "softKnee", bloom.softKnee }, { "maxRadiance", bloom.maxRadiance } };
    const auto& fog = scene->GetSceneDistanceFog();
    const auto& color = scene->GetSceneFogColor();
    config["atmosphere"] = { { "exposure", scene->GetSceneExposure() },
        { "fogColor", { color.x, color.y, color.z, color.w } },
        { "distanceFog", { fog.start, fog.end, fog.density, fog.curve } } };
    config["finalPass"] = { { "fxaa", fxaaEnabled_ }, { "toneMapping", static_cast<int>(parameters.colorFinishSettings.x) },
        { "compensationEv", parameters.colorFinishSettings.z } };
    return config;
}

bool PostEffectManager::ApplyPresetConfig(const nlohmann::json& config, std::string& error)
{
    error.clear();
    try {
        if (!config.is_object()) { throw std::runtime_error("プリセット形式が不正です"); }
        if (config.contains("formatVersion") && (!config.at("formatVersion").is_number_integer() || config.at("formatVersion") != 1)) { throw std::runtime_error("プリセット形式が不正です"); }
        for (const char* section : { "parameters", "bloom", "atmosphere", "finalPass" }) {
            if (!config.at(section).is_object()) { throw std::runtime_error(std::string("設定はオブジェクトで指定してください: ") + section); }
        }
        if (config.contains("effectParameters") && !config.at("effectParameters").is_object()) { throw std::runtime_error("効果固有の設定が不正です"); }
        if (config.contains("name") && !config.at("name").is_string()) { throw std::runtime_error("プリセット名は文字列で指定してください"); }
        ValidateFinite(config);
        auto parameters = copyImageRenderer_->GetPostEffectParameter();
        const auto& values = config.at("parameters");
        for (const auto& binding : kFloatBindings) {
            if (values.contains(binding.key)) { parameters.*(binding.member) = values.at(binding.key).get<float>(); }
        }
        if (values.contains("radialBlurSampleCount")) {
            const auto& count = values.at("radialBlurSampleCount");
            if (!count.is_number_integer()) { throw std::runtime_error("サンプル数は整数にしてください"); }
            parameters.radialBlurSampleCount = count.get<int>();
        }
        const auto& finalPass = config.at("finalPass");
        if (!finalPass.at("toneMapping").is_number_integer()) { throw std::runtime_error("色の変換方式は整数で指定してください"); }
        const int toneMapping = finalPass.at("toneMapping").get<int>();
        const float compensationEv = finalPass.at("compensationEv").get<float>();
        if (toneMapping < 0 || toneMapping > 1 || compensationEv < -10 || compensationEv > 10) { throw std::runtime_error("露出設定が範囲外です"); }
        const bool isFxaaEnabled = finalPass.at("fxaa").get<bool>();
        parameters.colorFinishSettings.x = static_cast<float>(toneMapping);
        parameters.colorFinishSettings.z = compensationEv;
        if (!CopyImageRenderer::IsValidParameter(parameters)) { throw std::runtime_error("ポストエフェクト設定が範囲外です"); }
        if (parameters.radialBlurSampleCount < 8 || parameters.radialBlurWidth > 1.0f) { throw std::runtime_error("放射状ブラーは8〜128サンプル、幅0〜1で指定してください"); }
        auto bloom = *bloomRenderer_->GetBloomParameter();
        const auto& bloomConfig = config.at("bloom");
        bloom.threshold = bloomConfig.at("threshold").get<float>();
        bloom.intensity = bloomConfig.at("intensity").get<float>();
        if (!bloomConfig.at("blurRadius").is_number_integer()) { throw std::runtime_error("ぼかし半径は整数にしてください"); }
        bloom.blurRadius = bloomConfig.at("blurRadius").get<int>();
        bloom.blurSigma = bloomConfig.at("blurSigma").get<float>();
        bloom.softKnee = bloomConfig.at("softKnee").get<float>();
        bloom.maxRadiance = bloomConfig.at("maxRadiance").get<float>();
        bloom.isEnabled = 0;
        std::vector<PostEffectInfo> chain;
        std::vector<std::pair<PostEffectType, Vector3>> customParameters;
        std::set<PostEffectType> types;
        const auto& effects = config.at("effects");
        if (!effects.is_array() || effects.size() > 48) { throw std::runtime_error("効果の一覧が不正です"); }
        for (const auto& entry : effects) {
            PostEffectInfo effect;
            effect.type = ParseType(entry.at("type").get<std::string>());
            if (effect.type == PostEffectType::Copy || effect.type == PostEffectType::ToneMap || effect.type == PostEffectType::FXAA) {
                throw std::runtime_error("この効果は最終処理の設定で変更してください");
            }
            if (!types.insert(effect.type).second) { throw std::runtime_error("同じ効果は一度だけ追加できます"); }
            effect.enabled = entry.at("enabled").get<bool>();
            std::string expectedStage = "AfterParticle";
            if (IsTemporalResolveInputEffect(effect.type)) { expectedStage = "BeforeParticle"; effect.stage = PostEffectStage::BeforeParticle; }
            else { effect.stage = PostEffectStage::AfterParticle; }
            if (entry.at("stage") != expectedStage) { throw std::runtime_error("効果の適用位置がエンジンの描画順と一致しません"); }
            if (effect.type == PostEffectType::Bloom && effect.enabled) { bloom.isEnabled = 1; }
            Vector3 custom {};
            const auto defaults = defaultEffectParameters_.find(effect.type);
            if (defaults != defaultEffectParameters_.end()) { custom = defaults->second; }
            if (config.contains("effectParameters") && config.at("effectParameters").contains(entry.at("type").get<std::string>())) {
                const auto& customArray = config.at("effectParameters").at(entry.at("type").get<std::string>());
                if (!customArray.is_array() || customArray.size() != 3) { throw std::runtime_error("効果固有の調整値は3つ指定してください"); }
                custom = { customArray.at(0).get<float>(), customArray.at(1).get<float>(), customArray.at(2).get<float>() };
            }
            customParameters.push_back({ effect.type, custom });
            chain.push_back(effect);
        }
        if (!BloomRenderer::IsValidParameter(bloom)) { throw std::runtime_error("ブルーム設定が範囲外です"); }
        const auto& atmosphere = config.at("atmosphere");
        const auto& fogArray = atmosphere.at("distanceFog");
        const auto& colorArray = atmosphere.at("fogColor");
        if (!fogArray.is_array() || fogArray.size() != 4 || !colorArray.is_array() || colorArray.size() != 4) { throw std::runtime_error("霧の設定が不正です"); }
        DistanceFogData fog { fogArray.at(0).get<float>(), fogArray.at(1).get<float>(), fogArray.at(2).get<float>(), fogArray.at(3).get<float>() };
        Vector4 color { colorArray.at(0).get<float>(), colorArray.at(1).get<float>(), colorArray.at(2).get<float>(), colorArray.at(3).get<float>() };
        const float exposure = atmosphere.at("exposure").get<float>();
        if (fog.start < 0 || fog.end <= fog.start || fog.density < 0 || fog.curve <= 0 || exposure <= 0 || exposure > 64) { throw std::runtime_error("霧・露出が範囲外です"); }
        nlohmann::json savedConfig = config;
        std::vector<PostEffectType> pipelineTypes { PostEffectType::Copy, PostEffectType::ToneMap, PostEffectType::FXAA };
        for (const auto& effect : chain) { if (effect.enabled) { pipelineTypes.push_back(effect.type); } }
        EnsurePresetPipelines(pipelineTypes);
        auto* scene = SceneManager::GetInstance();
        scene->ApplyPostEffectChain(chain);
        for (const auto& parameters : customParameters) { scene->SetPostEffectParameters(parameters.first, parameters.second); }
        scene->SetSceneDistanceFog(fog);
        scene->SetSceneFogColor(color);
        scene->SetSceneExposure(exposure);
        scene->SetVignetteStrength(parameters.vignetteStrength);
        scene->SetCameraShakeStrength(parameters.cameraShakeStrength);
        RadialBlurSettings blur;
        blur.sampleCount = parameters.radialBlurSampleCount;
        blur.width = parameters.radialBlurWidth;
        scene->SetRadialBlurSettings(blur);
        copyImageRenderer_->SetPostEffectParameter(parameters);
        bloomRenderer_->SetBloomParameter(bloom);
        fxaaEnabled_ = isFxaaEnabled;
        presetConfig_.swap(savedConfig);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool PostEffectManager::LoadPreset(const std::string& path, std::string& error)
{
    try { return ApplyPresetConfig(JsonFile::Load(std::filesystem::path(std::u8string(path.begin(), path.end()))), error); }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool PostEffectManager::SavePreset(const std::string& path, std::string& error) const
{
    error.clear();
    try { JsonFile::Save(std::filesystem::path(std::u8string(path.begin(), path.end())), GetPresetConfig()); return true; }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}
