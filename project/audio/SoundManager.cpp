#include "SoundManager.h"

void SoundManager::Initialize()
{
    HRESULT result;

    result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    assert(SUCCEEDED(result));

    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));

    result = xAudio2->CreateMasteringVoice(&masterVoice);
    assert(SUCCEEDED(result));
}

void SoundManager::Finalize()
{
    xAudio2.Reset();
    MFShutdown();
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

SoundData SoundManager::SoundLoadFile(const std::string& filename)
{
    HRESULT result;
    SoundData soundData {};

    // パスをワイド文字列へ
    std::wstring path = StringUtility::ConvertString(filename);

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    result = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
    if (FAILED(result)) {
        std::string msg = "SoundLoadFile failed: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return SoundData {};
    }
    if (FAILED(result)) {
        char buf[512];
        sprintf_s(buf,
            "SoundLoadFile failed\nFile: %s\nHRESULT: 0x%08X\n",
            filename.c_str(),
            result);
        OutputDebugStringA(buf);
        return SoundData {};
    }

    // PCM指定
    Microsoft::WRL::ComPtr<IMFMediaType> pPCType;
    MFCreateMediaType(&pPCType);
    pPCType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pPCType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    result = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,pPCType.Get());

    if (FAILED(result)) {
        OutputDebugStringA(("PCM set failed: " + filename + "\n").c_str());
        return SoundData {};
    }


    // 実際のWaveFormat取得
    Microsoft::WRL::ComPtr<IMFMediaType> pOutType;
    reader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        &pOutType);

    WAVEFORMATEX* waveFormat = nullptr;
    MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);

    // コンテナに格納
   
    soundData.wfex = *waveFormat;
    // 音声データの読み込み
    CoTaskMemFree(waveFormat);
    // バッファサイズの取得
    while (true) {

        Microsoft::WRL::ComPtr<IMFSample> pSample;
        DWORD streamIndex = 0, flags = 0;
        LONGLONG llTimeStamp = 0;
        // サンプルの読み込み
        result = reader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            &streamIndex,
            &flags,
            &llTimeStamp,
            &pSample);

        // ストリームの末尾に達したら終了
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;

        if (pSample) {

            Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
            // サンプルからメディアバッファを取得
            result = pSample->ConvertToContiguousBuffer(&pBuffer);

            BYTE* pData = nullptr;
            DWORD maxLength = 0,currrentLength=0;
            //バッファ読み込み用にロック
            pBuffer->Lock(&pData, &maxLength, &currrentLength);
            //バッファの末尾にデータ追加
            soundData.buffer.insert(soundData.buffer.end(), pData, pData + currrentLength);
            // バッファのロック解除
            pBuffer->Unlock();

        }
    }
    return soundData;
}

//==音声データ解放==//
void SoundManager::SoundUnload(SoundData* soundData)
{

    soundData->buffer.clear();
    soundData->wfex = {};
}
void SoundManager::SoundPlayWave(const SoundData& soundData)
{
    HRESULT result;

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    XAUDIO2_BUFFER buf {};
    buf.pAudioData = soundData.buffer.data(); 
    buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size()); 
    buf.Flags = XAUDIO2_END_OF_STREAM;

    result = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));

    result = pSourceVoice->Start();
    assert(SUCCEEDED(result));
}
