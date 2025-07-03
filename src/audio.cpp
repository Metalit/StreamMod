#include "audio.hpp"

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/GameObject.hpp"
#include "config.hpp"
#include "main.hpp"
#include "mic.hpp"

DEFINE_TYPE(StreamMod, AudioCapture);

using namespace StreamMod;

static void AddMix(std::vector<float>& base, std::vector<float>& add, int size) {
    for (int i = 0; i < size; i++)
        base[i] += add[i];
}

static void AvgMix(std::vector<float>& base, std::vector<float>& add, int size) {
    for (int i = 0; i < size; i++)
        base[i] = (base[i] + add[i]) / 2;
}

template <class T>
static void ScaledInsert(T& mutex, std::vector<float>& buffer, ArrayW<float>& data, float volume) {
    std::shared_lock lock(mutex);
    buffer.reserve(buffer.size() + data.size());
    for (float sample : data)
        buffer.emplace_back(sample * volume);
}

void AudioCapture::SetMicCapture(bool enabled) {
    logger.debug("set mic {}", enabled);
    if (enabled) {
        if (!mic) {
            mic = MicCapture::Create();
            mic->callback = [this](ArrayW<float> data) {
                if (channels == -1 || sampleRate == -1)
                    return;
                bool overThreshold = mic->currentLoudness >= getConfig().MicThreshold.GetValue();
                if (overThreshold)
                    hasMicData = true;
                // not sure why it's so quiet that I have to multiply it by 10
                // audioSource volume doesn't seem to matter, at least above 1
                float volume = overThreshold ? getConfig().MicVolume.GetValue() * 10 : 0;
                ScaledInsert(mutex, micBuffer, data, volume);
            };
        }
        mic->Init();
    } else if (mic)
        mic->Stop();
}

void AudioCapture::OnAudioFilterRead(ArrayW<float> data, int audioChannels) {
    channels = audioChannels;
    if (sampleRate == -1)
        return;  // can't get it on this thread
    ScaledInsert(mutex, gameBuffer, data, getConfig().GameVolume.GetValue());
}

void AudioCapture::Update() {
    if (sampleRate == -1)
        sampleRate = UnityEngine::AudioSettings::get_outputSampleRate();
    if (channels == -1 || sampleRate == -1)
        return;

    std::unique_lock lock(mutex);

    if (!mic || micBuffer.empty()) {
        if (!gameBuffer.empty()) {
            callback(gameBuffer, sampleRate, channels);
            gameBuffer.clear();
        }
        return;
    }

    if (mic->channels != -1 && mic->sampleRate != -1 && (mic->channels != channels || mic->sampleRate != sampleRate))
        logger.warn("mismatch in reported config! mic: {}/{}, game: {}/{}", mic->channels, mic->sampleRate, channels, sampleRate);

    int size = std::min(gameBuffer.size(), micBuffer.size());

    switch (getConfig().MixMode.GetValue()) {
        case 1:
            if (!hasMicData)
                break;
        case 0:
            AvgMix(gameBuffer, micBuffer, size);
            break;
        default:
            AddMix(gameBuffer, micBuffer, size);
            break;
    }
    callback(std::span(gameBuffer).subspan(0, size), sampleRate, channels);

    gameBuffer.erase(gameBuffer.begin(), gameBuffer.begin() + size);
    micBuffer.erase(micBuffer.begin(), micBuffer.begin() + size);
    hasMicData = false;
}

void AudioCapture::OnDisable() {
    if (onDisable)
        onDisable(this);
}

void AudioCapture::OnDestroy() {
    if (mic)
        mic->callback = nullptr;
    mic = nullptr;
}
