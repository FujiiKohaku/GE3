#include "AIAnimationWindow.h"
#include "AnimationBuilder.h"
#include "AnimationCommandParser.h"
#include "AnimationSerializer.h"
#include "GeminiClient.h"
#include "externals/imgui/imgui.h"
#include <cstring>
#include <exception>
#include <mutex>
#include <vector>

AIAnimationWindow::AIAnimationWindow()
{
    aiClient_ = std::make_unique<GeminiClient>();
    CopyPrompt("左からゆっくりスライドして表示");
}

AIAnimationWindow::~AIAnimationWindow()
{
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

bool AIAnimationWindow::Draw(UIEditorAnimationClip* clip, std::string* statusMessage)
{
    bool applied = ConsumeFinishedResult(clip, statusMessage);

    bool isGenerating = false;
    std::string errorMessage;
    std::string infoMessage;
    std::string lastCommandText;
    std::string retryMessage;

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        isGenerating = isGenerating_;
        errorMessage = errorMessage_;
        infoMessage = infoMessage_;
        lastCommandText = lastCommandText_;
        retryMessage = retryMessage_;
    }

    if (ImGui::CollapsingHeader("AI Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawPresetButtons();

        ImGui::InputTextMultiline(
            "Prompt",
            promptBuffer_,
            sizeof(promptBuffer_),
            ImVec2(-1.0f, 96.0f));

        if (isGenerating) {
            ImGui::Button("Generating...", ImVec2(-1.0f, 0.0f));
        } else {
            if (ImGui::Button("Generate", ImVec2(-1.0f, 0.0f))) {
                StartGeneration(clip, statusMessage);
            }
        }

        if (!retryMessage.empty()) {
            ImGui::TextWrapped("%s", retryMessage.c_str());
        }

        if (!errorMessage.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.34f, 0.28f, 1.0f));
            ImGui::TextWrapped("%s", errorMessage.c_str());
            ImGui::PopStyleColor();
        }

        if (!infoMessage.empty()) {
            ImGui::TextWrapped("%s", infoMessage.c_str());
        }

        if (!lastCommandText.empty()) {
            if (ImGui::TreeNode("Generated Commands")) {
                ImGui::BeginChild("GeneratedCommandsText", ImVec2(0.0f, 160.0f), true);
                ImGui::TextUnformatted(lastCommandText.c_str());
                ImGui::EndChild();
                ImGui::TreePop();
            }
        }
    }

    return applied;
}

void AIAnimationWindow::OnAIRetryWait(const AIRetryStatus& status)
{
    std::lock_guard<std::mutex> lock(workerMutex_);
    retryMessage_ = status.message;
    infoMessage_ = status.message;
}

void AIAnimationWindow::StartGeneration(UIEditorAnimationClip* clip, std::string* statusMessage)
{
    bool isGenerating = false;

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        isGenerating = isGenerating_;
    }

    if (isGenerating) {
        return;
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        errorMessage_.clear();
        infoMessage_.clear();
        retryMessage_.clear();
        workerErrorMessage_.clear();
        workerCommandText_.clear();
        workerFinished_ = false;
        workerSucceeded_ = false;
    }

    if (clip == nullptr) {
        std::lock_guard<std::mutex> lock(workerMutex_);
        errorMessage_ = "AI generation failed: target animation is null.";
        return;
    }

    std::string prompt = promptBuffer_;

    if (prompt.empty()) {
        std::lock_guard<std::mutex> lock(workerMutex_);
        errorMessage_ = "AI generation failed: prompt is empty.";
        return;
    }

    if (!aiClient_) {
        std::lock_guard<std::mutex> lock(workerMutex_);
        errorMessage_ = "AI generation failed: AI client is not initialized.";
        return;
    }

    AIAnimationRequest request;
    request.prompt = prompt;
    request.animationName = clip->GetName();
    request.canvasWidth = clip->GetCanvasWidth();
    request.canvasHeight = clip->GetCanvasHeight();
    request.currentLength = clip->GetLength();

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        isGenerating_ = true;
        infoMessage_ = "AI animation is generating...";
    }

    if (statusMessage != nullptr) {
        *statusMessage = "AI animation is generating...";
    }

    try {
        workerThread_ = std::thread(&AIAnimationWindow::RunGenerationWorker, this, request, *clip);
    } catch (const std::exception& exception) {
        std::lock_guard<std::mutex> lock(workerMutex_);
        isGenerating_ = false;
        workerFinished_ = false;
        errorMessage_ = std::string("AI generation failed: ") + exception.what();
        infoMessage_.clear();
    }
}

void AIAnimationWindow::RunGenerationWorker(AIAnimationRequest request, UIEditorAnimationClip sourceClip)
{
    bool succeeded = false;
    UIEditorAnimationClip generatedClip;
    std::string commandText;
    std::string resultErrorMessage;

    AIAnimationResponse response = aiClient_->GenerateAnimationCommands(request, this);

    if (!response.success) {
        resultErrorMessage = response.errorMessage;
    } else {
        commandText = response.commandText;

        AnimationCommandParser commandParser;
        AnimationBuilder animationBuilder;
        AnimationSerializer animationSerializer;
        std::vector<AnimationCommand> commands;

        if (!commandParser.Parse(response.commandText, &commands, &resultErrorMessage)) {
            resultErrorMessage = "Command parse failed: " + resultErrorMessage;
        } else if (!animationBuilder.Build(commands, sourceClip, &generatedClip, &resultErrorMessage)) {
            resultErrorMessage = "Animation build failed: " + resultErrorMessage;
        } else if (!animationSerializer.ValidateProjectJson(generatedClip, &resultErrorMessage)) {
            resultErrorMessage = "JSON generation failed: " + resultErrorMessage;
        } else {
            succeeded = true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        workerSucceeded_ = succeeded;
        workerGeneratedClip_ = generatedClip;
        workerCommandText_ = commandText;
        workerErrorMessage_ = resultErrorMessage;
        workerFinished_ = true;
        isGenerating_ = false;
        retryMessage_.clear();

        if (succeeded) {
            infoMessage_ = "AI animation generated. Applying...";
        } else {
            errorMessage_ = resultErrorMessage;
            infoMessage_.clear();
        }
    }
}

bool AIAnimationWindow::ConsumeFinishedResult(UIEditorAnimationClip* clip, std::string* statusMessage)
{
    bool workerFinished = false;

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        workerFinished = workerFinished_;
    }

    if (!workerFinished) {
        return false;
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    bool workerSucceeded = false;
    UIEditorAnimationClip generatedClip;
    std::string commandText;
    std::string workerErrorMessage;

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        workerSucceeded = workerSucceeded_;
        generatedClip = workerGeneratedClip_;
        commandText = workerCommandText_;
        workerErrorMessage = workerErrorMessage_;
        workerFinished_ = false;
        workerSucceeded_ = false;
        workerErrorMessage_.clear();
        workerCommandText_.clear();
        retryMessage_.clear();
    }

    if (!workerSucceeded) {
        if (statusMessage != nullptr) {
            *statusMessage = workerErrorMessage;
        }
        return false;
    }

    if (clip == nullptr) {
        std::lock_guard<std::mutex> lock(workerMutex_);
        errorMessage_ = "Preview apply failed: target animation is null.";
        infoMessage_.clear();
        return false;
    }

    std::string applyErrorMessage;

    if (!animationPreview_.Apply(generatedClip, clip, &applyErrorMessage)) {
        std::lock_guard<std::mutex> lock(workerMutex_);
        errorMessage_ = "Preview apply failed: " + applyErrorMessage;
        infoMessage_.clear();

        if (statusMessage != nullptr) {
            *statusMessage = errorMessage_;
        }

        return false;
    }

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        lastCommandText_ = commandText;
        errorMessage_.clear();
        infoMessage_ = "AI animation generated and applied.";
    }

    if (statusMessage != nullptr) {
        *statusMessage = "AI animation generated.";
    }

    return true;
}

void AIAnimationWindow::CopyPrompt(const std::string& text)
{
    errno_t result = strncpy_s(promptBuffer_, sizeof(promptBuffer_), text.c_str(), _TRUNCATE);

    if (result != 0) {
        promptBuffer_[0] = '\0';
    }
}

void AIAnimationWindow::DrawPresetButtons()
{
    if (ImGui::Button("Slide In")) {
        CopyPrompt("左からゆっくりスライドして表示");
    }

    ImGui::SameLine();

    if (ImGui::Button("Fade Scale")) {
        CopyPrompt("フェードインしながら少し拡大する");
    }

    if (ImGui::Button("Bounce")) {
        CopyPrompt("3回跳ねて止まる");
    }

    ImGui::SameLine();

    if (ImGui::Button("Shooting Title")) {
        CopyPrompt("シューティングゲーム風のタイトル演出");
    }
}
