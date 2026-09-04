#pragma once
#include "Engine/Audio/SoundManager.h"
#include <unordered_map>

class ArchiveAudio {
public:
    ~ArchiveAudio() { Stop(); }
    void Initialize()
    {
        HRESULT result = XAudio2Create(engine_.GetAddressOf());
        if (FAILED(result)) return;
        result = engine_->CreateMasteringVoice(&master_);
        if (FAILED(result)) engine_.Reset();
    }
    void Load(const std::string& name, const std::string& path)
    {
        sounds_.emplace(name, SoundManager::GetInstance()->SoundLoadFile(path));
    }
    void Play(const std::string& name, float volume)
    {
        Update();
        const auto found = sounds_.find(name);
        if (!engine_ || found == sounds_.end() || found->second.buffer.empty()) return;
        const auto& sound = found->second;
        IXAudio2SourceVoice* voice = nullptr;
        if (FAILED(engine_->CreateSourceVoice(&voice, &sound.wfex))) return;
        XAUDIO2_BUFFER buffer {};
        buffer.pAudioData = sound.buffer.data();
        buffer.AudioBytes = static_cast<UINT32>(sound.buffer.size());
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        voice->SetVolume(volume);
        if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start())) {
            voice->DestroyVoice();
            return;
        }
        voices_.push_back(voice);
    }
    void Update()
    {
        for (auto it = voices_.begin(); it != voices_.end();) {
            XAUDIO2_VOICE_STATE state {};
            (*it)->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (state.BuffersQueued == 0) {
                (*it)->DestroyVoice();
                it = voices_.erase(it);
            } else ++it;
        }
    }
    void Stop()
    {
        for (auto* voice : voices_) voice->DestroyVoice();
        voices_.clear();
        if (master_) { master_->DestroyVoice(); master_ = nullptr; }
        engine_.Reset();
        sounds_.clear();
    }
private:
    Microsoft::WRL::ComPtr<IXAudio2> engine_;
    IXAudio2MasteringVoice* master_ = nullptr;
    std::unordered_map<std::string, SoundData> sounds_;
    std::vector<IXAudio2SourceVoice*> voices_;
};
