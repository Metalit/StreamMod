#include "mic.hpp"

#include "UnityEngine/Android/Permission.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioRolloffMode.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/GameObject.hpp"
#include "config.hpp"
#include "main.hpp"

DEFINE_TYPE(StreamMod, MicCapture);

using namespace StreamMod;
using namespace UnityEngine;

static constexpr float SamplesPerFragment = 512;
static constexpr float LoudnessDecreasePerSecond = 5;

static bool HasPermission() {
    return UnityEngine::Android::Permission::HasUserAuthorizedPermission("android.permission.RECORD_AUDIO");
}

static ArrayW<StringW> GetDevices() {
    static auto icall = il2cpp_utils::resolve_icall<ArrayW<StringW>>("UnityEngine.Microphone::get_devices");
    return icall();
}

static int GetMicrophoneDeviceIDFromName(StringW name) {
    static auto icall = il2cpp_utils::resolve_icall<int, StringW>("UnityEngine.Microphone::GetMicrophoneDeviceIDFromName");
    return icall(name);
}

static std::pair<int, int> GetDeviceCaps(int deviceId) {
    static auto icall = il2cpp_utils::resolve_icall<void, int, ByRef<int>, ByRef<int>>("UnityEngine.Microphone::GetDeviceCaps");
    int min, max;
    icall(deviceId, byref(min), byref(max));
    return {min, max};
}

static AudioClip* StartRecord(int deviceId, bool loop, float lengthSec, int frequency) {
    static auto icall = il2cpp_utils::resolve_icall<AudioClip*, int, bool, float, int>("UnityEngine.Microphone::StartRecord");
    return icall(deviceId, loop, lengthSec, frequency);
}

static void EndRecord(int deviceId) {
    static auto icall = il2cpp_utils::resolve_icall<void, int>("UnityEngine.Microphone::EndRecord");
    icall(deviceId);
}

static int GetRecordPosition(int deviceId) {
    static auto icall = il2cpp_utils::resolve_icall<int, int>("UnityEngine.Microphone::GetRecordPosition");
    return icall(deviceId);
}

static int GetLoopDataLength(int bufferLength, int head, int tail) {
    if (head < tail)
        return tail - head;
    else
        return bufferLength - head + tail;
}

static int Resample(ArrayW<float> source, int position, ArrayW<float> target, int sourceFrequency, int targetFrequency) {
    // equal frequencies - fast copy
    if (sourceFrequency == targetFrequency) {
        int remain = source.size() - position;
        if (remain < target.size()) {
            std::copy_n(source.begin() + position, remain, target.begin());
            std::copy_n(source.begin(), target.size() - remain, target.begin() + remain);
        } else
            std::copy_n(source.begin() + position, target.size(), target.begin());
        return target.size();
    }

    float sampleRatio = sourceFrequency / (float) targetFrequency;

    // simpler behavior for integer ratios (implies that the output needs less samples)
    if (std::abs(sampleRatio - std::round(sampleRatio)) < 0.0001) {
        int intSampleRatio = std::round(sampleRatio);
        for (int i = 0; i < target.size(); i++) {
            target[i] = source[position];
            position += intSampleRatio;
            if (position >= source.size())
                position -= source.size();
        }
        return position;
    }

    // interpolate between samples
    float fpos = position;
    for (int i = 0; i < target.size(); i++) {
        fpos += sampleRatio;
        if (fpos >= source.size())
            fpos -= source.size();
        int lower = fpos;
        int upper = std::ceil(fpos);
        if (upper >= source.size())
            upper -= source.size();
        target[i] = std::lerp(source[lower], source[upper], fpos - (int) fpos);
    }
    position = std::ceil(fpos);
    if (position >= source.size())
        position -= source.size();
    return position;
}

static float GetLoudness(ArrayW<float> data) {
    // root mean square - maybe use some sort of weighting filter in the future?
    // https://community.vcvrack.com/t/complete-list-of-native-fft-libraries-for-audio/9153
    float sum = 0;
    for (int i = 0; i < data.size(); i++)
        sum += data[i] * data[i];
    // apply a boost to make 0-2 a good range (why so much? idk)
    return sqrt(sum / data.size()) * getConfig().MicVolume.GetValue() * 100;
}

void MicCapture::Init() {
    if (deviceId != -1)
        return;

    auto devices = GetDevices();
    if (devices.size() == 0) {
        logger.error("no microphone devices found (permission: {})", HasPermission());
        return;
    }
    logger.debug("found microphones {}", fmt::join(devices, ", "));

    deviceId = GetMicrophoneDeviceIDFromName(devices[0]);
    logger.info("recording with microphone {} ({})", deviceId, devices[0]);

    auto [min, max] = GetDeviceCaps(deviceId);
    outputSampleRate = AudioSettings::get_outputSampleRate();
    micSampleRate = std::clamp(outputSampleRate, min, max);
    logger.info("using sample rate {} ({}: {} - {})", micSampleRate, outputSampleRate, min, max);

    bufferPos = 0;
    audioClip = StartRecord(deviceId, true, 1, micSampleRate);
    if (!audioClip) {
        logger.error("failed to start recording");
        micSampleRate = -1;
        deviceId = -1;
        return;
    }

    if (!micBuffer || micBuffer.size() != micSampleRate)
        micBuffer = ArrayW<float>(micSampleRate);
    if (!fragmentBuffer)
        fragmentBuffer = ArrayW<float>(SamplesPerFragment);
}

void MicCapture::Stop() {
    if (deviceId == -1)
        return;

    logger.info("ending recording of microphone {}", deviceId);

    micSampleRate = -1;
    currentLoudness = 0;

    EndRecord(deviceId);
    deviceId = -1;

    Object::Destroy(audioClip);
    audioClip = nullptr;
}

void MicCapture::Update() {
    if (deviceId == -1)
        return;

    int position = GetRecordPosition(deviceId);
    if (position < 0 || position == bufferPos)
        return;

    if (!audioClip->GetData(micBuffer, 0))
        return;

    while (GetLoopDataLength(micBuffer.size(), bufferPos, position) > SamplesPerFragment) {
        bufferPos = Resample(micBuffer, bufferPos, fragmentBuffer, micSampleRate, outputSampleRate);
        float newLoudness = GetLoudness(fragmentBuffer);
        if (newLoudness < currentLoudness)
            currentLoudness -= (LoudnessDecreasePerSecond * fragmentBuffer.size()) / (outputSampleRate);
        else
            currentLoudness = newLoudness;
        if (currentLoudness < 0)
            currentLoudness = 0;
        callback(fragmentBuffer);
    }
}

void MicCapture::OnDestroy() {
    Stop();
}

MicCapture* MicCapture::Create() {
    static ConstString name("StreamingMicCapture");
    if (auto go = GameObject::Find(name))
        return go->GetComponent<MicCapture*>();
    auto go = GameObject::New_ctor(name);
    Object::DontDestroyOnLoad(go);
    auto ret = go->AddComponent<MicCapture*>();
    return ret;
}
