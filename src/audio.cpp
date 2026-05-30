#include "audio.hpp"

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Time.hpp"
#include "config.hpp"
#include "main.hpp"
#include "mic.hpp"

DEFINE_TYPE(StreamMod, AudioCapture);

using namespace StreamMod;

static constexpr float MicDuration = 0.5;

template <class T>
static void ScaledInsert(T& mutex, std::vector<float>& buffer, ArrayW<float>& data, float volume, int duplicates) {
    std::shared_lock lock(mutex);
    buffer.reserve(buffer.size() + data.size() * duplicates);
    for (float sample : data) {
        for (int i = 0; i < duplicates; i++)
            buffer.emplace_back(sample * volume);
    }
}

void AudioCapture::SetMicCapture(bool enabled) {
    logger.debug("set mic {}", enabled);
    if (enabled) {
        if (!mic) {
            mic = MicCapture::Create([this](ArrayW<float> data) {
                if (channels == -1 || sampleRate == -1)
                    return;
                hasMicData = mic->currentLoudness >= getConfig().MicThreshold.GetValue();
                float time = UnityEngine::Time::get_time();
                if (hasMicData)
                    lastMicData = time;
                else
                    hasMicData = time - lastMicData < MicDuration;
                // not sure why it's so quiet that I have to multiply it by 10
                float volume = hasMicData ? getConfig().MicVolume.GetValue() * 10 : 0;
                ScaledInsert(mutex, micBuffer, data, volume, channels);
            });
        }
        mic->Init();
    } else if (mic)
        mic->Stop();
}

void AudioCapture::OnAudioFilterRead(ArrayW<float> data, int audioChannels) {
    channels = audioChannels;
    if (sampleRate == -1)
        return;  // can't get it on this thread
    ScaledInsert(mutex, gameBuffer, data, getConfig().GameVolume.GetValue(), 1);
}

void AudioCapture::Update() {
    if (sampleRate == -1)
        sampleRate = UnityEngine::AudioSettings::get_outputSampleRate();
    if (channels == -1 || sampleRate == -1)
        return;
    if (!initedLimiter)
        limiter.init(channels, sampleRate);

    std::unique_lock lock(mutex);

    if (!mic || micBuffer.empty()) {
        if (!gameBuffer.empty()) {
            for (auto& data : gameBuffer)
                data = limiter.process(data);
            callback(gameBuffer, sampleRate, channels);
            gameBuffer.clear();
        }
        return;
    }

    if (mic->outputSampleRate != -1 && mic->outputSampleRate != sampleRate)
        logger.warn("mismatch in reported sample rate! mic: {}, game: {}", mic->outputSampleRate, sampleRate);

    int size = std::min(gameBuffer.size(), micBuffer.size());

    int micMix = getConfig().MixMode.GetValue();
    // Combine, or Duck with mic data
    int divisor = micMix == 0 || (micMix == 1 && hasMicData) ? 2 : 1;
    for (int i = 0; i < size; i++)
        gameBuffer[i] = limiter.process((gameBuffer[i] + micBuffer[i]) / divisor);

    callback(std::span(gameBuffer).subspan(0, size), sampleRate, channels);

    gameBuffer.erase(gameBuffer.begin(), gameBuffer.begin() + size);
    micBuffer.erase(micBuffer.begin(), micBuffer.begin() + size);
}

void AudioCapture::OnDisable() {
    if (onDisable)
        onDisable(this);
}

void AudioCapture::OnDestroy() {
    if (mic)
        mic->ClearCallback();
    mic = nullptr;
}
