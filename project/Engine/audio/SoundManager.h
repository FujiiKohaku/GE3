#pragma once

#include <Windows.h>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

// ===== Media Foundation =====
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#include <initializer_list>
#include <chrono>
#include <unordered_map>
#include <future>
#include <atomic>
#include "Engine/StringUtility/StringUtility.h"

// --------------------------------------
// WAVファイルデータ保持用
// --------------------------------------
struct SoundData {
    WAVEFORMATEX wfex {};
    std::vector<BYTE> buffer;
};

class SoundVoiceCallback final : public IXAudio2VoiceCallback {
public:
    void STDMETHODCALLTYPE OnStreamEnd() override { finished = true; }
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnBufferEnd(void*) override {}
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override { finished = true; }

    std::atomic<bool> finished = false;
};

// --------------------------------------
// XAudio2ベースのサウンド管理クラス
// Singleton 対応版
// --------------------------------------
struct SoundRegistration {
    const char* id;
    const char* filename;
    float volume = 1.0f;
    float minimumIntervalSeconds = 0.0f;
};

enum class SoundRegistrationResult { Success, InvalidId, DuplicateId, InvalidSettings, AudioUnavailable, LoadFailed };

class SoundManager {
public:
    // ================================
    // Singleton
    // ================================
    static SoundManager* GetInstance();
    SoundManager(const SoundManager&) = delete; // コピーコンストラクタを削除
    SoundManager& operator=(const SoundManager&) = delete; // コピー代入演算子を削除

private:
    static std::unique_ptr<SoundManager> instance_;

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class SoundManager;
    };

    explicit SoundManager(ConstructorKey);
    ~SoundManager() = default;
    // ================================
    // 基本操作
    // ================================
    void Initialize();
    void Update();
    void Finalize();

    SoundData SoundLoadFile(const std::string& filename);
    void SoundUnload(SoundData* soundData);

    bool SoundPlayWave(const SoundData& soundData, float volume = 1.0f);

    // IDs are supplied by the application. Duplicate IDs keep the existing sound.
    bool Register(const std::string& id, const std::string& filename,
        float volume = 1.0f, float minimumIntervalSeconds = 0.0f,
        SoundRegistrationResult* result = nullptr);
    // シーンで必要な音を登録します。登録済みIDは再読込しません。
    bool RegisterSounds(std::initializer_list<SoundRegistration> sounds);
    // Returns false for missing IDs or calls suppressed by the playback interval.
    bool Play(const std::string& id);

private:
    void InitializeAudio();
    bool EnsureInitialized();
    bool mediaFoundationStarted_ = false;
    struct RegisteredSound {
        SoundData data;
        float volume = 1.0f;
        float minimumIntervalSeconds = 0.0f;
        std::chrono::steady_clock::time_point lastPlayed {};
        bool hasPlayed = false;
    };
    std::unordered_map<std::string, RegisteredSound> registeredSounds_;
    std::future<void> initFuture_;
    std::atomic<bool> isInitialized_ = false;

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice = nullptr;
    struct ActiveVoice {
        IXAudio2SourceVoice* voice = nullptr;
        std::unique_ptr<SoundVoiceCallback> callback;
    };
    std::vector<ActiveVoice> activeVoices_;
};
