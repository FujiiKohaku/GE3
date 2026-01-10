#pragma once

#include <Windows.h> 
#include <cassert>
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

#include "StringUtility.h"

// --------------------------------------
// WAVファイルデータ保持用
// --------------------------------------
struct SoundData {
    WAVEFORMATEX wfex {};
    std::vector<BYTE> buffer;
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
    void Finalize();

    SoundData SoundLoadFile(const std::string& filename);
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
