#pragma once

#include "AnimationPreview.h"
#include "IAIClient.h"
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class AIAnimationWindow : public IAIClientProgressListener {
public:
    AIAnimationWindow();
    ~AIAnimationWindow();

    bool Draw(UIEditorAnimationClip* clip, std::string* statusMessage);
    void OnAIRetryWait(const AIRetryStatus& status) override;

private:
    void StartGeneration(UIEditorAnimationClip* clip, std::string* statusMessage);
    void RunGenerationWorker(AIAnimationRequest request, UIEditorAnimationClip sourceClip);
    bool ConsumeFinishedResult(UIEditorAnimationClip* clip, std::string* statusMessage);
    void CopyPrompt(const std::string& text);
    void DrawPresetButtons();

private:
    std::unique_ptr<IAIClient> aiClient_;
    AnimationPreview animationPreview_;

    char promptBuffer_[2048] = {};
    std::string lastCommandText_;
    std::string errorMessage_;
    std::string infoMessage_;

    std::thread workerThread_;
    std::mutex workerMutex_;
    bool isGenerating_ = false;
    bool workerFinished_ = false;
    bool workerSucceeded_ = false;
    UIEditorAnimationClip workerGeneratedClip_;
    std::string workerCommandText_;
    std::string workerErrorMessage_;
    std::string retryMessage_;
};
