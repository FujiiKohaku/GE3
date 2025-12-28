#pragma once
#include <cassert>
#include <fstream>
#include <string>
#include <wrl.h>
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

// --------------------------------------
// WAVファイルデータ保持用
// --------------------------------------
struct SoundData {
    WAVEFORMATEX wfex;
    BYTE* pBuffer = nullptr;
    unsigned int bufferSize = 0;
};

// --------------------------------------
// XAudio2ベースのサウンド管理クラス
// Singleton 対応版
// --------------------------------------
class SoundManager {
public:
    // ================================
    // Singleton
    // ================================
    static SoundManager* GetInstance()
    {
        static SoundManager instance;
        return &instance;
    }

    // ================================
    // 基本操作
    // ================================
    void Initialize();
    void Finalize(SoundData* soundData);

    SoundData SoundLoadWave(const char* filename);
    void SoundUnload(SoundData* soundData);

    void SoundPlayWave(const SoundData& soundData);

private:
    // シングルトン用
    SoundManager() = default;
    ~SoundManager() = default;

    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice = nullptr;
};
