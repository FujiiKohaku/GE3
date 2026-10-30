#include "EffectEditorApp.h"
#include "EditorLabels.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
bool EnumControl(const std::string& key, std::string& value)
{
    const char* choices = nullptr;
    if (key == "Shape") { choices = "Sphere\0Box\0Cone\0Cylinder\0Circle\0"; }
    if (key == "MeshType") { choices = "Board\0Ring\0Cylinder\0Box\0Sphere\0Cone\0"; }
    if (key == "BlendMode") { choices = "None\0Normal\0Add\0Subtract\0Multiply\0Screen\0"; }
    if (key == "CullMode") { choices = "None\0Back\0Front\0"; }
    if (key == "Space") { choices = "Local\0World\0"; }
    if (key == "Type" && value != "Mesh" && value != "Trail" && value != "Point") { choices = "Wind\0Attractor\0Repulsor\0Vortex\0"; }
    if (choices == nullptr) { return false; }
    int current = 0;
    int index = 0;
    for (const char* choice = choices; *choice != 0; choice += std::strlen(choice) + 1) {
        if (value == choice) { current = index; }
        ++index;
    }
    const char* translated = choices;
    if (key == "Shape") { translated = "球\0箱\0円すい\0円柱\0円\0"; }
    if (key == "MeshType") { translated = "板\0リング\0円柱\0箱\0球\0円すい\0"; }
    if (key == "BlendMode") { translated = "合成なし\0通常\0加算\0減算\0乗算\0スクリーン\0"; }
    if (key == "CullMode") { translated = "両面を表示\0裏面を隠す\0表面を隠す\0"; }
    if (key == "Space") { translated = "発生源を基準\0ワールドを基準\0"; }
    if (key == "Type") { translated = "風\0吸引\0反発\0渦\0"; }
    if (ImGui::Combo("##value", &current, translated)) {
        index = 0;
        for (const char* choice = choices; *choice != 0; choice += std::strlen(choice) + 1) {
            if (index == current) { value = choice; break; }
            ++index;
        }
    }
    return true;
}
}

void EffectEditorApp::EditProperties(nlohmann::json& value, const std::string& parent)
{
    if (!value.is_object()) { return; }
    static constexpr const char* kPrimaryKeys[] = { "DisplayName", "Type", "Enabled", "Shape", "Count", "Frequency", "Radius",
        "LifeTime", "MaxParticles", "StartScale", "EndScale", "StartColor", "EndColor", "Velocity" };
    std::vector<std::string> keys;
    for (const char* key : kPrimaryKeys) { if (value.contains(key)) { keys.emplace_back(key); } }
    for (const auto& item : value.items()) {
        if (std::find(keys.begin(), keys.end(), item.key()) == keys.end()) { keys.push_back(item.key()); }
    }
    for (const auto& key : keys) {
        auto& property = value[key];
        ImGui::PushID(key.c_str());
        const char* label = EditorLabel(key);
        if (label == nullptr) { label = key.c_str(); }
        if (property.is_object()) {
            if (ImGui::CollapsingHeader(label)) { EditProperties(property, key); }
        } else if (property.is_array() && !property.empty() && property.front().is_object()) {
            int removal = -1;
            for (size_t index = 0; index < property.size(); ++index) {
                ImGui::PushID(static_cast<int>(index));
                ImGui::SeparatorText((std::string("フィールド ") + std::to_string(index + 1)).c_str());
                if (ImGui::SmallButton("削除")) { removal = static_cast<int>(index); }
                EditProperties(property[index], key);
                ImGui::PopID();
            }
            if (removal >= 0) { property.erase(property.begin() + removal); }
        } else {
            if (!ImGui::BeginTable("##property", 2, ImGuiTableFlags_SizingStretchProp)) { ImGui::PopID(); continue; }
            ImGui::TableSetupColumn("項目", ImGuiTableColumnFlags_WidthStretch, 0.46f);
            ImGui::TableSetupColumn("値", ImGuiTableColumnFlags_WidthStretch, 0.54f);
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s", label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1);
            if (property.is_boolean()) {
                bool isEnabled = property.get<bool>();
                if (ImGui::Checkbox("##value", &isEnabled)) { property = isEnabled; }
            } else if (property.is_number_integer()) {
                int number = property.get<int>();
                if (key == "toneMapping") {
                    if (ImGui::Combo("##value", &number, "標準\0フィルム調\0")) { property = number; }
                } else if (ImGui::InputInt("##value", &number)) { property = number; }
            } else if (property.is_number()) {
                float number = property.get<float>();
                if (ImGui::DragFloat("##value", &number, 0.01f, 0, 0, "%.3f")) { property = number; }
            } else if (property.is_string()) {
                std::string text = property.get<std::string>();
                if (key == "Type" && (parent == "Render" || parent == "Light")) {
                    if (text == "Trail") { ImGui::TextDisabled("Trail（固定）"); }
                    else if (text == "Mesh") { ImGui::TextDisabled("Mesh（固定）"); }
                    else { ImGui::TextDisabled("ポイント（固定）"); }
                } else if (EnumControl(key, text)) { property = text; }
                else {
                    char buffer[1024] {};
                    std::snprintf(buffer, sizeof(buffer), "%s", text.c_str());
                    if (ImGui::InputText("##value", buffer, sizeof(buffer))) { property = std::string(buffer); }
                }
            } else if (property.is_array() && property.size() >= 2 && property.size() <= 4 && property.front().is_number()) {
                float components[4] {};
                for (size_t index = 0; index < property.size(); ++index) { components[index] = property[index].get<float>(); }
                bool isChanged = false;
                if ((key.find("Color") != std::string::npos || key == "fogColor") && property.size() == 4) {
                    if (ImGui::ColorButton("##value", { components[0], components[1], components[2], components[3] }, ImGuiColorEditFlags_AlphaPreviewHalf, { ImGui::GetContentRegionAvail().x, 23 })) {
                        ImGui::OpenPopup("色を選択");
                    }
                    if (ImGui::BeginPopup("色を選択")) {
                        ImGui::SetNextItemWidth(220);
                        isChanged = ImGui::ColorPicker4("##picker", components, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_Float);
                        ImGui::EndPopup();
                    }
                } else if (property.size() == 2) { isChanged = ImGui::DragFloat2("##value", components, 0.01f); }
                else if (property.size() == 3) { isChanged = ImGui::DragFloat3("##value", components, 0.02f, 0, 0, "%.2f"); }
                else if (key == "distanceFog") {
                    const char* names[] = { "開始", "終了", "濃度", "曲線" };
                    for (int index = 0; index < 4; ++index) {
                        ImGui::PushID(index); ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat(names[index], &components[index], 0.01f)) { isChanged = true; }
                        ImGui::PopID();
                    }
                } else { isChanged = ImGui::DragFloat4("##value", components, 0.02f, 0, 0, "%.2f"); }
                if (isChanged) { for (size_t index = 0; index < property.size(); ++index) { property[index] = components[index]; } }
            } else { ImGui::TextDisabled("この値は詳細JSONで管理されています"); }
            ImGui::EndTable();
        }
        ImGui::PopID();
    }
}

void EffectEditorApp::EditPostProperties(const std::string& type, bool isCompact)
{
    if (type == "Bloom") {
        auto primary = nlohmann::json { { "intensity", postDraft_["bloom"]["intensity"] }, { "threshold", postDraft_["bloom"]["threshold"] } };
        EditProperties(primary);
        for (const auto& item : primary.items()) { postDraft_["bloom"][item.key()] = item.value(); }
        if (!isCompact || ImGui::CollapsingHeader("詳細")) {
            auto advanced = postDraft_["bloom"]; advanced.erase("intensity"); advanced.erase("threshold");
            EditProperties(advanced);
            for (const auto& item : advanced.items()) { postDraft_["bloom"][item.key()] = item.value(); }
        }
        return;
    }
    if (type == "Fog") { EditProperties(postDraft_["atmosphere"]); return; }
    struct Binding { const char* type; const char* key; };
    static constexpr Binding kBindings[] = {
        { "ColorAdjust", "colorBrightness" }, { "ColorAdjust", "colorContrast" }, { "ColorAdjust", "colorSaturation" },
        { "GrayScale", "grayScaleStrength" }, { "Vignette", "vignetteStrength" },
        { "Outline", "outlineScale" }, { "Outline", "outlineThreshold" }, { "Outline", "outlineSoftness" }, { "Outline", "outlineNormalStrength" },
        { "RadialBlur", "radialBlurWidth" }, { "RadialBlur", "radialBlurSampleCount" },
        { "Dissolve", "dissolveThreshold" }, { "Dissolve", "dissolveEdgeWidth" }, { "Dissolve", "dissolveEdgeStrength" },
        { "Pixelate", "pixelSize" }, { "DepthOfField", "focusDepth" }, { "DepthOfField", "focusRange" }, { "DepthOfField", "depthOfFieldRadius" },
        { "MotionBlur", "motionBlurStrength" }, { "ChromaticAberration", "chromaticAberrationStrength" },
        { "LensDistortion", "lensDistortionStrength" }, { "FilmGrain", "filmGrainStrength" }, { "LensDirt", "lensDirtStrength" },
        { "CameraShake", "cameraShakeStrength" }, { "BokehShape", "bokehRadius" }, { "Fisheye", "fisheyeStrength" }
    };
    auto parameters = nlohmann::json::object();
    for (const auto& binding : kBindings) {
        if (type == binding.type) { parameters[binding.key] = postDraft_["parameters"][binding.key]; }
    }
    if (!parameters.empty()) {
        EditProperties(parameters);
        for (const auto& item : parameters.items()) { postDraft_["parameters"][item.key()] = item.value(); }
        return;
    }
    ImGui::TextWrapped("効果固有の調整値");
    float values[3] {};
    if (postDraft_["effectParameters"].contains(type)) {
        for (int index = 0; index < 3; ++index) { values[index] = postDraft_["effectParameters"][type][index].get<float>(); }
    }
    const char* labels[] = { "調整値 1", "調整値 2", "調整値 3" };
    for (int index = 0; index < 3; ++index) {
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragFloat(labels[index], &values[index], 0.01f)) { postDraft_["effectParameters"][type] = { values[0], values[1], values[2] }; }
    }
}

void EffectEditorApp::DrawInspector()
{
    if (workspace_ == 1) {
        ImGui::TextUnformatted("ポストエフェクト");
        ImGui::Separator();
        if (selectedPost_ >= 0 && selectedPost_ < static_cast<int>(postDraft_["effects"].size())) {
            const std::string type = postDraft_["effects"][selectedPost_]["type"].get<std::string>();
            for (int index = 0; index <= static_cast<int>(PostEffectType::ScreenLighting); ++index) {
                if (type == GetPostEffectTypeName(static_cast<PostEffectType>(index))) { ImGui::SeparatorText(PostLabel(static_cast<PostEffectType>(index))); break; }
            }
            EditPostProperties(type);
        }
        if (ImGui::CollapsingHeader("その他の色・画面設定")) { EditProperties(postDraft_["parameters"]); }
        if (ImGui::CollapsingHeader("霧・露出")) { EditProperties(postDraft_["atmosphere"]); }
        if (ImGui::CollapsingHeader("最終処理")) { EditProperties(postDraft_["finalPass"]); }
        return;
    }
    ImGui::TextWrapped("%s", selectedName_.c_str());
    const bool isTrail = particleDraft_.at("Render").at("Type") == "Trail";
    if (isTrail) { ImGui::TextColored({ 0.45f, 0.8f, 1, 1 }, "トレイル · Trail"); }
    else { ImGui::TextColored({ 0.45f, 0.8f, 1, 1 }, "通常粒子 · Mesh"); }
    ImGui::TextDisabled("種類は作成後に固定");
    ImGui::Separator();
    const char* tabs[] = { "基本", "描画", "フィールド", "ライト" };
    for (int index = 0; index < 4; ++index) {
        if (index != 0) { ImGui::SameLine(); }
        if (ImGui::SmallButton(tabs[index])) { inspectorTab_ = index; }
    }
    ImGui::Separator();
    if (inspectorTab_ == 0) {
        if (particleDraft_.contains("DisplayName")) {
            nlohmann::json name = { { "DisplayName", particleDraft_["DisplayName"] } };
            EditProperties(name); particleDraft_["DisplayName"] = name["DisplayName"];
        }
        for (const char* section : { "Emitter", "Particle", "Simulation", "Trail" }) {
            if (!particleDraft_.contains(section)) { continue; }
            if (ImGui::CollapsingHeader(EditorLabel(section), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (std::strcmp(section, "Particle") == 0 && !particleDraft_[section].contains("MaxParticles")) {
                    // Legacy Effect.json files omit this engine default. Materialize it only after an edit.
                    constexpr int kDefaultParticleCapacity = 1024;
                    auto properties = particleDraft_[section]; properties["MaxParticles"] = kDefaultParticleCapacity;
                    EditProperties(properties, section);
                    if (properties["MaxParticles"] == kDefaultParticleCapacity) { properties.erase("MaxParticles"); }
                    particleDraft_[section] = properties;
                } else { EditProperties(particleDraft_[section], section); }
            }
        }
    }
    if (inspectorTab_ == 1) {
        EditProperties(particleDraft_["Render"], "Render");
        if (ImGui::CollapsingHeader("シェーダー（詳細）")) { EditProperties(particleDraft_["Shaders"], "Shaders"); }
    }
    if (inspectorTab_ == 2) {
        if (ImGui::Button("フィールドを追加")) {
            if (!particleDraft_.contains("Fields")) { particleDraft_["Fields"] = nlohmann::json::array(); }
            if (particleDraft_["Fields"].size() < 16) {
                particleDraft_["Fields"].push_back({ { "Type", "Wind" }, { "Space", "Local" }, { "Position", { 0, 0, 0 } },
                    { "Direction", { 1, 0, 0 } }, { "Radius", 5.0f }, { "Strength", 1.0f }, { "Falloff", 1.0f } });
            }
        }
        if (particleDraft_.contains("Fields") && !particleDraft_["Fields"].empty()) {
            nlohmann::json fields = { { "Fields", particleDraft_["Fields"] } };
            EditProperties(fields); particleDraft_["Fields"] = fields["Fields"];
        } else { ImGui::TextWrapped("風・吸引・反発・渦を追加できます。"); }
    }
    if (inspectorTab_ == 3) {
        if (!particleDraft_.contains("Light")) {
            if (ImGui::Button("ライトを追加")) {
                particleDraft_["Light"] = { { "Type", "Point" }, { "Enabled", true }, { "Color", { 1, 0.65, 0.2, 1 } },
                    { "Offset", { 0, 0.5, 0 } }, { "Intensity", 2.0f }, { "Radius", 5.0f }, { "Decay", 2.0f },
                    { "FollowEmitter", true }, { "FadeOut", true }, { "FadeDuration", 0.25f } };
            }
        } else { EditProperties(particleDraft_["Light"], "Light"); }
    }
}
