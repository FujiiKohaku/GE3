#include "SoundManager.h"
#include "Engine/Logger/Logger.h"
#include <algorithm>
#include <cmath>
namespace {
const char* RegistrationError(SoundRegistrationResult result)
{
    switch (result) {
    case SoundRegistrationResult::Success: return "成功";
    case SoundRegistrationResult::InvalidId: return "IDが空です";
    case SoundRegistrationResult::DuplicateId: return "IDが重複しています";
    case SoundRegistrationResult::InvalidSettings: return "音量または再生間隔が不正です";
    case SoundRegistrationResult::AudioUnavailable: return "音声システムを初期化できませんでした";
    case SoundRegistrationResult::LoadFailed: return "音声ファイルを読み込めませんでした";
    }
    return "不明なエラー";
}
}

std::unique_ptr<SoundManager> SoundManager::instance_ = nullptr;

SoundManager::SoundManager(ConstructorKey)
{
}

SoundManager* SoundManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<SoundManager>(ConstructorKey());
    }
    return instance_.get();
}

void SoundManager::Initialize()
{
    if (isInitialized_ || initFuture_.valid()) { return; }
    isInitialized_ = false;
    initFuture_ = std::async(std::launch::async, &SoundManager::InitializeAudio, this);
}

void SoundManager::InitializeAudio()
{
    HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(result)) { return; }
    mediaFoundationStarted_ = true;
    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(result)) { return; }
    result = xAudio2->CreateMasteringVoice(&masterVoice);
    if (FAILED(result)) { return; }
    isInitialized_ = true;
}

bool SoundManager::EnsureInitialized()
{
    if (isInitialized_) { return true; }
    if (initFuture_.valid()) { initFuture_.wait(); }
    return isInitialized_;
}

void SoundManager::Update()
{
    for (auto iterator = activeVoices_.begin(); iterator != activeVoices_.end();) {
        if (!iterator->callback->finished.load()) {
            ++iterator;
            continue;
        }
        iterator->voice->DestroyVoice();
        iterator = activeVoices_.erase(iterator);
    }
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
    if (!EnsureInitialized()) { return SoundData {}; }
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
    // PCM指定
    Microsoft::WRL::ComPtr<IMFMediaType> pPCType;
    result = MFCreateMediaType(&pPCType);
    if (FAILED(result)) { return SoundData {}; }
    result = pPCType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(result)) { return SoundData {}; }
    result = pPCType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(result)) { return SoundData {}; }
    result = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPCType.Get());

    if (FAILED(result)) {
        OutputDebugStringA(("PCM set failed: " + filename + "\n").c_str());
        return SoundData {};
    }

    // 実際のWaveFormat取得
    Microsoft::WRL::ComPtr<IMFMediaType> pOutType;
    result = reader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        &pOutType);
    if (FAILED(result)) { return SoundData {}; }

    WAVEFORMATEX* waveFormat = nullptr;
    result = MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);
    if (FAILED(result) || waveFormat == nullptr) { return SoundData {}; }

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

        if (FAILED(result) || (flags & MF_SOURCE_READERF_ERROR)) { return SoundData {}; }

        // ストリームの末尾に達したら終了
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;

        if (pSample) {

            Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
            // サンプルからメディアバッファを取得
            result = pSample->ConvertToContiguousBuffer(&pBuffer);
            if (FAILED(result)) { return SoundData {}; }

            BYTE* pData = nullptr;
            DWORD maxLength = 0, currrentLength = 0;
            // バッファ読み込み用にロック
            result = pBuffer->Lock(&pData, &maxLength, &currrentLength);
            if (FAILED(result)) { return SoundData {}; }
            // バッファの末尾にデータ追加
            soundData.buffer.insert(soundData.buffer.end(), pData, pData + currrentLength);
            // バッファのロック解除
            result = pBuffer->Unlock();
            if (FAILED(result)) { return SoundData {}; }
        }
    }
    return soundData;
}

//==音声データ解放==//
void SoundManager::SoundUnload(SoundData* soundData)
{
    EnsureInitialized();

    soundData->buffer.clear();
    soundData->wfex = {};
}
bool SoundManager::SoundPlayWave(const SoundData& soundData, float volume)
{
    if (!EnsureInitialized() || soundData.buffer.empty() || !std::isfinite(volume) ||
        soundData.buffer.size() > XAUDIO2_MAX_BUFFER_BYTES) {
        return false;
    }
    auto callback = std::make_unique<SoundVoiceCallback>();
    IXAudio2SourceVoice* voice = nullptr;
    HRESULT result = xAudio2->CreateSourceVoice(&voice, &soundData.wfex, 0,
        XAUDIO2_DEFAULT_FREQ_RATIO, callback.get());
    if (FAILED(result)) { return false; }

    XAUDIO2_BUFFER buffer {};
    buffer.pAudioData = soundData.buffer.data();
    buffer.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    result = voice->SubmitSourceBuffer(&buffer);
    if (FAILED(result)) { voice->DestroyVoice(); return false; }
    result = voice->SetVolume(std::clamp(volume, 0.0f, 1.0f));
    if (FAILED(result)) { voice->DestroyVoice(); return false; }
    result = voice->Start();
    if (FAILED(result)) { voice->DestroyVoice(); return false; }
    activeVoices_.push_back({ voice, std::move(callback) });
    return true;
}
bool SoundManager::Register(const std::string& id, const std::string& filename,
    float volume, float minimumIntervalSeconds, SoundRegistrationResult* result)
{
    SoundRegistrationResult status = SoundRegistrationResult::Success;
    if (id.empty()) { status = SoundRegistrationResult::InvalidId; }
    else if (registeredSounds_.find(id) != registeredSounds_.end()) {
        status = SoundRegistrationResult::DuplicateId;
    } else if (!std::isfinite(volume) || !std::isfinite(minimumIntervalSeconds)) {
        status = SoundRegistrationResult::InvalidSettings;
    } else if (!EnsureInitialized()) {
        status = SoundRegistrationResult::AudioUnavailable;
    }
    if (result != nullptr) { *result = status; }
    if (status != SoundRegistrationResult::Success) { return false; }
    RegisteredSound sound;
    sound.data = SoundLoadFile(filename);
    if (sound.data.buffer.empty()) {
        if (result != nullptr) { *result = SoundRegistrationResult::LoadFailed; }
        return false;
    }
    sound.volume = std::clamp(volume, 0.0f, 1.0f);
    sound.minimumIntervalSeconds = (std::max)(minimumIntervalSeconds, 0.0f);
    registeredSounds_.emplace(id, std::move(sound));
    return true;
}

bool SoundManager::RegisterSounds(std::initializer_list<SoundRegistration> sounds)
{
    bool succeeded = true;
    for (const SoundRegistration& sound : sounds) {
        if (sound.id == nullptr || sound.filename == nullptr) {
            Logger::Warning("音声登録のIDまたはファイルが未指定です");
            succeeded = false;
            continue;
        }
        SoundRegistrationResult result {};
        if (!Register(sound.id, sound.filename, sound.volume, sound.minimumIntervalSeconds, &result)) {
            if (result == SoundRegistrationResult::DuplicateId) { continue; }
            Logger::Warning(std::string("音声の登録失敗: ") + sound.id + " / " +
                sound.filename + " / " + RegistrationError(result));
            succeeded = false;
        }
    }
    return succeeded;
}

bool SoundManager::Play(const std::string& id)
{
    const auto entry = registeredSounds_.find(id);
    if (entry == registeredSounds_.end()) {
        return false;
    }
    RegisteredSound& sound = entry->second;
    const auto now = std::chrono::steady_clock::now();
    if (sound.hasPlayed) {
        const float elapsedSeconds =
            std::chrono::duration<float>(now - sound.lastPlayed).count();
        if (elapsedSeconds < sound.minimumIntervalSeconds) {
            return false;
        }
    }
    if (!SoundPlayWave(sound.data, sound.volume)) { return false; }
    sound.lastPlayed = now;
    sound.hasPlayed = true;
    return true;
}

void SoundManager::Finalize()
{
    EnsureInitialized();
    if (!instance_) {
        return;
    }

    for (ActiveVoice& activeVoice : activeVoices_) {
        if (activeVoice.voice != nullptr) {
            activeVoice.voice->DestroyVoice();
        }
    }
    activeVoices_.clear();
    registeredSounds_.clear();

    if (instance_->masterVoice) {
        instance_->masterVoice->DestroyVoice();
        instance_->masterVoice = nullptr;
    }

    instance_->xAudio2.Reset();
    if (mediaFoundationStarted_) { MFShutdown(); }

    instance_.reset();
}
