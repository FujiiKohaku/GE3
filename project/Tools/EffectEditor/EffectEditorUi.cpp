#include "EffectEditorApp.h"
#include "EditorLabels.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Serialization/JsonFile.h"
#include <algorithm>
#include <cstring>

namespace {
constexpr ImGuiWindowFlags kPanelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
void BeginPanel(const char* title, float x, float y, float width, float height, ImGuiWindowFlags extra = 0)
{
    ImGui::SetNextWindowPos({ x, y }); ImGui::SetNextWindowSize({ width, height });
    ImGui::Begin(title, nullptr, kPanelFlags | extra);
}
bool WorkspaceButton(const char* title, bool isSelected)
{
    if (isSelected) { ImGui::PushStyleColor(ImGuiCol_Button, { 0.06f, 0.34f, 0.68f, 1 }); }
    const bool isClicked = ImGui::Button(title);
    if (isSelected) { ImGui::PopStyleColor(); }
    return isClicked;
}
}

void EffectEditorApp::DrawUi()
{
    const auto particleBefore = particleDraft_;
    const auto postBefore = postDraft_;
    didUseHistory_ = false;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("ファイル")) {
            if (ImGui::MenuItem("保存", "Ctrl+S")) { RequestSave(); }
            if (ImGui::MenuItem("プレビュー画像を保存")) { shouldCapture_ = true; }
            if (ImGui::MenuItem("終了")) { RequestClose(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("表示")) {
            if (ImGui::MenuItem("全画面", "F11", WinApp::GetInstance()->IsBorderlessFullscreen())) { shouldToggleFullscreen_ = true; }
            ImGui::MenuItem("床", nullptr, &isFloorVisible_); ImGui::MenuItem("グリッド", nullptr, &isGridVisible_);
            ImGui::MenuItem("モデル", nullptr, &isModelVisible_); ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("ヘルプ")) {
            ImGui::TextUnformatted("右ドラッグ：回転 / 中ドラッグ：移動 / ホイール：ズーム");
            ImGui::TextUnformatted("F：カメラを戻す / Ctrl+S：保存 / Ctrl+Z：元に戻す");
            ImGui::TextUnformatted("F11：全画面／ウィンドウ表示を切り替え");
            ImGui::TextUnformatted("粒子は適用ボタン、画面効果は有効な値を自動反映します。");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    const auto size = ImGui::GetIO().DisplaySize;
    const float leftWidth = 222;
    const float rightWidth = 312;
    const float top = 102;
    const float bodyHeight = size.y - top - 26;
    const float centerWidth = size.x - leftWidth - rightWidth;
    BeginPanel("操作##toolbar", 0, 24, size.x, 78, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    DrawToolbar(); ImGui::End();
    BeginPanel("エフェクト一覧", 0, top, leftWidth, bodyHeight);
    DrawLibrary(); ImGui::End();
    BeginPanel("プレビュー", leftWidth, top, centerWidth, bodyHeight - 202, ImGuiWindowFlags_NoScrollbar);
    DrawViewport(); ImGui::End();
    BeginPanel("詳細", size.x - rightWidth, top, rightWidth, bodyHeight);
    DrawInspector(); ImGui::End();
    BeginPanel("ポストエフェクト構成", leftWidth, top + bodyHeight - 202, centerWidth, 202);
    DrawPostChain(); ImGui::End();
    BeginPanel("状態##status", 0, size.y - 26, size.x, 26, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImVec4 color { 0.40f, 0.85f, 0.58f, 1 };
    if (isStatusError_) { color = { 1, 0.45f, 0.35f, 1 }; }
    ImGui::TextColored(color, "%s", status_.c_str()); ImGui::End();
    DrawPopups();
    if (particleDraft_ != particleBefore && !didUseHistory_) { particleHistory_.Push(particleBefore); }
    if (postDraft_ != postBefore) {
        if (!didUseHistory_) { postHistory_.Push(postBefore); }
        shouldApplyPost_ = true;
    }
}

void EffectEditorApp::DrawToolbar()
{
    ImGui::TextUnformatted("Kohaku Effect Editor"); ImGui::SameLine(205);
    if (WorkspaceButton("パーティクル", workspace_ == 0)) { workspace_ = 0; }
    ImGui::SameLine();
    if (WorkspaceButton("ポストエフェクト", workspace_ == 1)) { workspace_ = 1; }
    ImGui::SameLine();
    if (IsParticleDirty() || IsPostDirty()) { ImGui::TextColored({ 1, 0.72f, 0.25f, 1 }, "未保存"); }
    else { ImGui::TextDisabled("保存済み"); }
    ImGui::SameLine(); ImGui::Checkbox("外部変更を自動反映", &isLiveReloadEnabled_);
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("選択中の設定・シェーダー・共通includeを監視します。未保存の編集は保持します。"); }
    if (hasParticleConflict_ || hasPostConflict_) {
        ImGui::SameLine();
        if (ImGui::Button("外部変更あり")) {
            externalConflictTarget_ = 0;
            if (hasPostConflict_ && (workspace_ == 1 || !hasParticleConflict_)) { externalConflictTarget_ = 1; }
            shouldOpenExternalConflict_ = true;
        }
    }
    if (!reloadError_.empty()) { ImGui::SameLine(); if (ImGui::Button("再読み込みエラー")) { ImGui::OpenPopup("再読み込みの詳細"); } }
    auto& io = ImGui::GetIO();
    bool shouldUndo = false; bool shouldRedo = false;
    if (!io.WantTextInput && io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_S)) { RequestSave(); }
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) { shouldUndo = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Y)) { shouldRedo = true; }
    }
    if (ImGui::Button("保存")) { RequestSave(); }
    ImGui::SameLine(); if (ImGui::Button("元に戻す")) { shouldUndo = true; }
    ImGui::SameLine(); if (ImGui::Button("やり直し")) { shouldRedo = true; }
    auto* history = &particleHistory_; auto* draft = &particleDraft_;
    if (workspace_ == 1) { history = &postHistory_; draft = &postDraft_; }
    if (shouldUndo) { didUseHistory_ = history->Undo(*draft); }
    if (shouldRedo) { didUseHistory_ = history->Redo(*draft); }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.05f, 0.34f, 0.7f, 1 });
    if (ImGui::Button("適用して再開始")) {
        if (workspace_ == 0) { shouldApply_ = true; }
        else { shouldApplyPost_ = true; shouldRestart_ = true; }
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(); if (ImGui::Button("シェーダー再読み込み")) {
        if (workspace_ == 0) { shouldReload_ = true; }
        else { shouldReloadPostShaders_ = true; }
    }
    ImGui::SameLine(); if (ImGui::Button("画像を保存")) { shouldCapture_ = true; }
    ImGui::SetNextWindowSize({ 800, 450 }, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("再読み込みの詳細", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::BeginChild("errorText", { 0, -40 }, ImGuiChildFlags_Borders);
        ImGui::TextWrapped("%s", reloadError_.c_str());
        ImGui::EndChild();
        if (ImGui::Button("エラーをコピー")) { ImGui::SetClipboardText(reloadError_.c_str()); }
        ImGui::SameLine(); if (ImGui::Button("閉じる")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

void EffectEditorApp::DrawLibrary()
{
    ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##search", "名前で検索", search_, sizeof(search_));
    if (workspace_ == 0) {
        if (ImGui::Button("新規作成")) { isDuplicate_ = false; shouldOpenCreate_ = true; }
        ImGui::SameLine(); if (ImGui::Button("複製")) { isDuplicate_ = true; shouldOpenCreate_ = true; }
        ImGui::BeginChild("library", { 0, ImGui::GetContentRegionAvail().y - 150 });
        for (int category = 0; category < 2; ++category) {
            const char* title = "通常粒子 · Mesh";
            if (category == 1) { title = "トレイル · Trail"; }
            if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen)) { continue; }
            for (const auto& entry : entries_) {
                if (entry.isTrail != (category == 1)) { continue; }
                if (search_[0] != 0 && entry.name.find(search_) == std::string::npos && entry.id.find(search_) == std::string::npos) { continue; }
                const std::string label = entry.name + "##" + entry.id;
                if (ImGui::Selectable(label.c_str(), selectedId_ == entry.id)) { RequestSelect(entry.id); }
                if (shouldScrollToSelection_ && selectedId_ == entry.id) { ImGui::SetScrollHereY(0.5f); shouldScrollToSelection_ = false; }
                if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", entry.id.c_str()); }
            }
        }
        ImGui::EndChild();
        ImGui::SeparatorText("テクスチャ");
        const size_t count = (std::min)(texturePaths_.size(), size_t(4));
        for (size_t index = 0; index < count; ++index) {
            ImGui::PushID(static_cast<int>(index));
            const auto* texture = TextureManager::GetInstance()->GetTextureData(texturePaths_[index]);
            if (texture != nullptr) {
                if (ImGui::ImageButton("##texture", static_cast<ImTextureID>(texture->srvHandleGPU.ptr), { 80, 48 })) {
                    particleDraft_["Render"]["Texture"] = texturePaths_[index];
                }
            } else if (ImGui::Button("画像を選択", { 80, 48 })) { particleDraft_["Render"]["Texture"] = texturePaths_[index]; }
            if (index % 2 == 0) { ImGui::SameLine(); }
            ImGui::PopID();
        }
    } else {
        if (ImGui::Button("名前を付けて保存")) { shouldOpenPostSave_ = true; }
        ImGui::SeparatorText("プリセット");
        for (const auto& path : postFiles_) {
            std::string label = path.stem().string();
            if (label == "Standard") { label = "標準"; }
            if (label == "Ice") { label = "氷の空気感"; }
            if (ImGui::Selectable(label.c_str(), path == postPath_)) {
                if (IsPostDirty()) { deferredPostPath_ = path; shouldConfirmPost_ = true; }
                else { pendingPostPath_ = path; }
            }
        }
        ImGui::Spacing(); ImGui::TextWrapped("保存したプリセットはゲームでも利用できます。");
        ImGui::Separator();
        if (ImGui::Checkbox("プレビューに適用", &isPostEnabled_)) { shouldApplyPost_ = true; }
        ImGui::TextDisabled("種類・順番・数値を下部と右側で編集");
    }
}

void EffectEditorApp::DrawViewport()
{
    ImGui::Checkbox("床", &isFloorVisible_); ImGui::SameLine(); ImGui::Checkbox("グリッド", &isGridVisible_);
    ImGui::SameLine(); ImGui::Checkbox("モデル", &isModelVisible_);
    if (ImGui::BeginPopupContextWindow("viewportOptions")) {
        ImGui::ColorEdit3("背景色", &background_.x);
        if (ImGui::BeginMenu("モデルを読み込む")) {
            for (const auto& path : modelPaths_) { if (ImGui::MenuItem(path.filename().string().c_str())) { pendingModel_ = path; } }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImVec2 available = ImGui::GetContentRegionAvail();
    available.y = (std::max)(available.y - 80, 30.0f);
    const float ratio = static_cast<float>(preview_->GetWidth()) / preview_->GetHeight();
    ImVec2 imageSize { available.x, available.x / ratio };
    if (imageSize.y > available.y) { imageSize = { available.y * ratio, available.y }; }
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos({ start.x + (available.x - imageSize.x) * 0.5f, start.y });
    ImGui::Image(static_cast<ImTextureID>(preview_->GetSrv().ptr), imageSize);
    isViewportHovered_ = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos({ start.x, start.y + available.y + 5 });
    if (ImGui::Button("再生")) { isPlaying_ = true; if (!EffectManager::GetInstance()->IsEffectAlive(effectHandle_)) { shouldRestart_ = true; } }
    ImGui::SameLine(); if (ImGui::Button("一時停止")) { isPlaying_ = false; }
    ImGui::SameLine(); if (ImGui::Button("再開始")) { shouldRestart_ = true; }
    ImGui::SameLine(); ImGui::SetNextItemWidth(105); ImGui::SliderFloat("速度", &speed_, 0.1f, 2.0f, "%.1f倍");
    ImGui::SameLine(); if (ImGui::Checkbox("ループ", &isLoop_)) { shouldRestart_ = true; }
    ImGui::SetNextItemWidth(120); ImGui::Combo("発生源の移動", &motion_, "静止\0円を描く\0左右に移動\0");
    ImGui::SameLine(); ImGui::TextDisabled("右ドラッグ：回転 / ホイール：ズーム");
}

void EffectEditorApp::DrawPostChain()
{
    auto& effects = postDraft_["effects"];
    float listWidth = ImGui::GetContentRegionAvail().x * 0.59f;
    ImGui::BeginChild("chainList", { listWidth, 0 });
    int removal = -1;
    int moveUp = -1;
    int moveDown = -1;
    for (size_t index = 0; index < effects.size(); ++index) {
        auto& entry = effects[index];
        const std::string name = entry.at("type").get<std::string>();
        PostEffectType type = PostEffectType::Copy;
        for (int item = 0; item <= static_cast<int>(PostEffectType::ScreenLighting); ++item) {
            if (name == GetPostEffectTypeName(static_cast<PostEffectType>(item))) { type = static_cast<PostEffectType>(item); break; }
        }
        ImGui::PushID(static_cast<int>(index));
        bool isEnabled = entry["enabled"].get<bool>();
        if (ImGui::Checkbox("##enabled", &isEnabled)) { entry["enabled"] = isEnabled; }
        ImGui::SameLine();
        if (ImGui::Selectable(PostLabel(type), selectedPost_ == static_cast<int>(index), 0, { listWidth - 205, 0 })) { selectedPost_ = static_cast<int>(index); }
        ImGui::SameLine();
        const char* stage = "粒子の後";
        if (IsTemporalResolveInputEffect(type)) { stage = "粒子の前"; }
        if (type == PostEffectType::Bloom) { stage = "最終合成"; }
        ImGui::TextDisabled("%s", stage);
        const bool canMoveUp = index > 0 && name != "Bloom" && effects[index - 1]["type"] != "Bloom" && entry["stage"] == effects[index - 1]["stage"];
        const bool canMoveDown = index + 1 < effects.size() && name != "Bloom" && effects[index + 1]["type"] != "Bloom" && entry["stage"] == effects[index + 1]["stage"];
        ImGui::SameLine(); ImGui::BeginDisabled(!canMoveUp); if (ImGui::SmallButton("↑")) { moveUp = static_cast<int>(index); } ImGui::EndDisabled();
        ImGui::SameLine(); ImGui::BeginDisabled(!canMoveDown); if (ImGui::SmallButton("↓")) { moveDown = static_cast<int>(index); } ImGui::EndDisabled();
        if (ImGui::BeginPopupContextItem("row")) { if (ImGui::MenuItem("削除")) { removal = static_cast<int>(index); } ImGui::EndPopup(); }
        ImGui::PopID();
    }
    if (removal >= 0) { effects.erase(effects.begin() + removal); selectedPost_ = 0; }
    if (moveUp > 0 && effects[moveUp]["stage"] == effects[moveUp - 1]["stage"]) { std::swap(effects[moveUp], effects[moveUp - 1]); selectedPost_ = moveUp - 1; }
    if (moveDown >= 0 && moveDown + 1 < static_cast<int>(effects.size()) && effects[moveDown]["stage"] == effects[moveDown + 1]["stage"]) {
        std::swap(effects[moveDown], effects[moveDown + 1]); selectedPost_ = moveDown + 1;
    }
    if (ImGui::Button("効果を追加")) { ImGui::OpenPopup("addPost"); }
    if (ImGui::BeginPopup("addPost")) {
        for (int index = 1; index <= static_cast<int>(PostEffectType::ScreenLighting); ++index) {
            const auto type = static_cast<PostEffectType>(index);
            if (type == PostEffectType::ToneMap || type == PostEffectType::FXAA) { continue; }
            bool isPresent = false;
            for (const auto& entry : effects) { if (entry["type"] == GetPostEffectTypeName(type)) { isPresent = true; } }
            if (isPresent) { continue; }
            if (ImGui::MenuItem(PostLabel(type))) {
                std::string stage = "AfterParticle";
                if (IsTemporalResolveInputEffect(type)) { stage = "BeforeParticle"; }
                effects.push_back({ { "type", GetPostEffectTypeName(type) }, { "stage", stage }, { "enabled", true } });
                selectedPost_ = static_cast<int>(effects.size()) - 1;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild(); ImGui::SameLine();
    ImGui::BeginChild("chainSettings", { 0, 0 });
    if (selectedPost_ >= 0 && selectedPost_ < static_cast<int>(effects.size())) {
        const std::string type = effects[selectedPost_]["type"].get<std::string>();
        ImGui::TextUnformatted("選択した効果の設定");
        EditPostProperties(type, true);
    }
    ImGui::EndChild();
}

void EffectEditorApp::DrawPopups()
{
    if (shouldOpenExternalConflict_) { shouldOpenExternalConflict_ = false; ImGui::OpenPopup("外部変更と未保存の編集"); }
    if (ImGui::BeginPopupModal("外部変更と未保存の編集", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (externalConflictTarget_ == 0) { ImGui::TextUnformatted("パーティクルの設定が外部で変更されています。"); }
        else { ImGui::TextUnformatted("ポストエフェクトの設定が外部で変更されています。"); }
        ImGui::TextUnformatted("どちらの内容を採用するか選択してください。");
        if (ImGui::Button("外部の内容を読み込む")) {
            if (externalConflictTarget_ == 0) { shouldLoadExternalParticle_ = true; }
            else { shouldLoadExternalPost_ = true; }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("画面の編集で上書き保存")) {
            workspace_ = externalConflictTarget_; shouldOverwriteExternal_ = true; shouldSave_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button("あとで決める")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (shouldOpenCreate_) { shouldOpenCreate_ = false; ImGui::OpenPopup("エフェクトを作成"); }
    if (ImGui::BeginPopupModal("エフェクトを作成", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("識別名（英数字）", createId_, sizeof(createId_));
        ImGui::InputText("表示名", createName_, sizeof(createName_));
        if (!isDuplicate_) {
            if (ImGui::RadioButton("通常粒子 · Mesh", !isNewTrail_)) { isNewTrail_ = false; }
            ImGui::SameLine(); if (ImGui::RadioButton("トレイル · Trail", isNewTrail_)) { isNewTrail_ = true; }
        }
        ImGui::TextDisabled("種類は作成後に固定されます");
        if (ImGui::Button("作成")) {
            if (!isDuplicate_ && IsParticleDirty()) { shouldConfirmCreate_ = true; }
            else { shouldCreate_ = true; }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button("キャンセル")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (shouldConfirmCreate_) { shouldConfirmCreate_ = false; ImGui::OpenPopup("新規作成前の確認"); }
    if (ImGui::BeginPopupModal("新規作成前の確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("現在の未保存の変更を破棄して、新しく作成しますか？");
        if (ImGui::Button("破棄して作成")) { shouldCreate_ = true; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if (ImGui::Button("キャンセル")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (shouldConfirmSelection_) { shouldConfirmSelection_ = false; ImGui::OpenPopup("未保存のパーティクル"); }
    if (ImGui::BeginPopupModal("未保存のパーティクル", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("未保存の変更を破棄して切り替えますか？");
        if (ImGui::Button("破棄して切り替え")) { pendingSelection_ = deferredSelection_; deferredSelection_.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if (ImGui::Button("キャンセル")) { deferredSelection_.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (shouldConfirmPost_) { shouldConfirmPost_ = false; ImGui::OpenPopup("未保存のプリセット"); }
    if (ImGui::BeginPopupModal("未保存のプリセット", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("未保存の変更を破棄して切り替えますか？");
        if (ImGui::Button("破棄して切り替え")) {
            pendingPostPath_ = deferredPostPath_;
            deferredPostPath_.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button("キャンセル")) { deferredPostPath_.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (shouldOpenPostSave_) { shouldOpenPostSave_ = false; ImGui::OpenPopup("プリセットを保存"); }
    if (ImGui::BeginPopupModal("プリセットを保存", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("ファイル名（英数字）", postSaveName_, sizeof(postSaveName_));
        if (ImGui::Button("新しいファイルへ保存")) {
            const std::string name = postSaveName_;
            if (!name.empty() && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos) {
                const auto path = std::filesystem::path("resources/Graphics/PostEffects") / (name + ".json");
                if (!std::filesystem::exists(path)) { pendingPostSavePath_ = path; workspace_ = 1; RequestSave(); ImGui::CloseCurrentPopup(); }
                else { SetStatus("同名のプリセットがあります。別の名前を指定してください", true); }
            } else { SetStatus("ファイル名には英数字・_・-を使ってください", true); }
        }
        ImGui::SameLine(); if (ImGui::Button("キャンセル")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (shouldConfirmQuit_) {
        shouldConfirmQuit_ = false;
        if (IsParticleDirty() || IsPostDirty()) { ImGui::OpenPopup("終了の確認"); }
        else { shouldQuit_ = true; }
    }
    if (ImGui::BeginPopupModal("終了の確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("未保存の変更があります。");
        if (ImGui::Button("破棄して終了")) { shouldQuit_ = true; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if (ImGui::Button("キャンセル")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}
