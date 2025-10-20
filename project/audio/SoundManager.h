#pragma once
#include <cassert>
#include <fstream>
#include <string>
#include <wrl.h>
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")
// --------------------------------------
// XAudio2 を使用したサウンド管理クラス
// ・WAVファイルの読み込み
// ・再生処理
// ・メモリ解放
// ・初期化と終了処理
// --------------------------------------

// WAVファイルの内容を格納する構造体
struct SoundData {
    WAVEFORMATEX wfex; // 音声フォーマット情報
    BYTE* pBuffer; // 音声データバッファ
    unsigned int bufferSize; // バッファサイズ（バイト単位）
};

// サウンド全体を管理するクラス
class SoundManager {
public:
    // コンストラクタ・デストラクタ
    SoundManager();
    ~SoundManager();

    // XAudio2 を初期化する
    void Initialize();
    // XAudio2 を解放する
    void Finalize(SoundData* soundData);
    // WAVファイルを読み込んでメモリに展開
    SoundData SoundLoadWave(const char* filename);
    // 音声データをメモリから解放
    void SoundUnload(SoundData* soundData);
    // 音声を再生
    void SoundPlayWave(const SoundData& soundData);

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;
};
