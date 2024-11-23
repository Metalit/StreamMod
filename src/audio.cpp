#include "audio.hpp"

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/GameObject.hpp"
#include "config.hpp"
#include "main.hpp"
#include "mic.hpp"

DEFINE_TYPE(StreamMod, AudioCapture);

using namespace StreamMod;

static void AddMix(std::vector<float>& base, std::vector<float>& add) {
    for (int i = 0; i < add.size(); i++)
        base[i] += add[i];
}

static void AvgMix(std::vector<float>& base, std::vector<float>& add) {
    for (int i = 0; i < base.size(); i++) {
        auto const& val = i < add.size() ? add[i] : 0;
        base[i] = (base[i] + val) / 2;
    }
}

static void DuckMix(std::vector<float>& base, std::vector<float>& add, bool duck, bool duckFirst) {
    if (!duck) {
        if (!duckFirst)
            base = add;
        return;
    }
    for (int i = 0; i < base.size(); i++) {
        auto const& val = i < add.size() ? add[i] : 0;
        if (duckFirst)
            base[i] = base[i] / 2 + val;
        else
            base[i] = base[i] + val / 2;
    }
}

void AudioCapture::SetMicCapture(bool enabled) {
    if (enabled) {
        if (!mic) {
            mic = MicCapture::Create();
            mic->callback = [this](ArrayW<float> data) {
                if (channels == -1 || sampleRate == -1)
                    return;
                std::shared_lock lock(mutex);
                micBuffer.insert(micBuffer.end(), data->begin(), data->end());
                if (mic->currentLoudness >= getConfig().MicThreshold.GetValue())
                    mixLastMic = true;
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
    std::shared_lock lock(mutex);
    gameBuffer.insert(gameBuffer.end(), data->begin(), data->end());
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

    if (mic->channels != channels || mic->sampleRate != sampleRate)
        logger.warn("mismatch in reported config! mic: {}/{}, game: {}/{}", mic->channels, mic->sampleRate, channels, sampleRate);

    if (micBuffer.empty() && gameBuffer.empty())
        return;

    bool flag = micBuffer.size() > gameBuffer.size();
    auto& base = flag ? micBuffer : gameBuffer;
    auto& add = flag ? gameBuffer : micBuffer;

    switch (getConfig().MixMode.GetValue()) {
        case 1:
            AvgMix(base, add);
            break;
        case 2:
            DuckMix(base, add, mixLastMic, flag);
            break;
        default:
            AddMix(base, add);
            break;
    }
    callback(base, sampleRate, channels);

    gameBuffer.clear();
    micBuffer.clear();
    mixLastMic = false;
}

void AudioCapture::OnDisable() {
    if (onDisable)
        onDisable(this);
}

void AudioCapture::OnDestroy() {
    if (mic)
        mic->callback = nullptr;
}
