#include "AnimationPreview.h"

bool AnimationPreview::Apply(const UIEditorAnimationClip& generatedClip, UIEditorAnimationClip* targetClip, std::string* errorMessage) const
{
    if (targetClip == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "AnimationPreview target clip is null.";
        }
        return false;
    }

    *targetClip = generatedClip;
    targetClip->EnsureTracks();

    return true;
}
