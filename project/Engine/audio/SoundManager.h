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

// --------------------------------------
// XAudio2ベースのサウンド管理クラス
// Singleton 対応版
// --------------------------------------
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
    void Finalize();

    SoundData SoundLoadFile(const std::string& filename);
    void SoundUnload(SoundData* soundData);

    void SoundPlayWave(const SoundData& soundData);

private:
    void EnsureInitialized();
    std::future<void> initFuture_;
    std::atomic<bool> isInitialized_ = false;

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice = nullptr;
};
