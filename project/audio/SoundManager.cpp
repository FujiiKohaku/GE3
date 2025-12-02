#include "SoundManager.h"



void SoundManager::Initialize()
{

    //==XAudioエンジンのインスタンスを生成==//
    HRESULT result_ = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result_));
    //==マスターボイスを生成==//
    result_ = xAudio2->CreateMasteringVoice(&masterVoice);
    assert(SUCCEEDED(result_));
}

void SoundManager::Finalize(SoundData* soundData)
{
    xAudio2.Reset();
    SoundUnload(soundData);
}

// チャンクヘッダ
struct ChunkHeader {
    char id[4]; // チャンクID
    uint32_t size; // チャンクサイズ
};
// RIFFヘッダチャンク
struct RiffHeader {
    ChunkHeader chunk; // チャンクヘッダ(RIFF)
    char type[4]; // フォーマット（"WAVE"）
};
// FMTチャンク
struct FormatChunk {
    ChunkHeader chunk; // チャンクヘッダ(FMT)
    WAVEFORMATEX fmt; // WAVEフォーマット
};

SoundData SoundManager::SoundLoadWave(const char* filename)
{
    HRESULT result;

    std::ifstream file(filename, std::ios::binary);
    assert(file.is_open());

    // RIFFヘッダーの読み込み
    RiffHeader riff;
    file.read((char*)&riff, sizeof(riff));
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0) {
        assert(0);
    }

    FormatChunk format = {};
    ChunkHeader chunk = {};

    // チャンクを順に読み取って fmt と data を探す
    char* pBuffer = nullptr;
    unsigned int dataSize = 0;

    while (file.read((char*)&chunk, sizeof(chunk))) {
        if (strncmp(chunk.id, "fmt ", 4) == 0) {
            assert(chunk.size <= sizeof(WAVEFORMATEX));
            file.read((char*)&format.fmt, chunk.size);
        } else if (strncmp(chunk.id, "data", 4) == 0) {
            pBuffer = new char[chunk.size];
            file.read(pBuffer, chunk.size);
            dataSize = chunk.size;
        } else {
            // 他のチャンク（JUNKなど）はスキップ
            file.seekg(chunk.size, std::ios::cur);
        }

        // fmt も data も読み込めたら終わり
        if (format.fmt.nChannels != 0 && pBuffer != nullptr) {
            break;
        }
    }

    file.close();

    assert(format.fmt.nChannels != 0); // fmt チャンクが見つからなかった
    assert(pBuffer != nullptr); // data チャンクが見つからなかった

    SoundData soundData = {};
    soundData.wfex = format.fmt;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
    soundData.bufferSize = dataSize;
    return soundData;
}

//==音声データ解放==//
void SoundManager::SoundUnload(SoundData* soundData)
{

    // バッファのメモリを解放
    delete[] soundData->pBuffer;

    soundData->pBuffer = 0;
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

void SoundManager::SoundPlayWave(const SoundData& soundData)
{
    HRESULT result;

    // 波形フォーマットをもとにsourceVoiceの生成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    // 再生する波形データの設定
    XAUDIO2_BUFFER buf {};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // 波形データの再生
    result = pSourceVoice->SubmitSourceBuffer(&buf);
    result = pSourceVoice->Start();
}