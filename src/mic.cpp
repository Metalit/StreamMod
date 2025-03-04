#include "mic.hpp"

#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioRolloffMode.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/GameObject.hpp"
#include "config.hpp"
#include "main.hpp"

DEFINE_TYPE(StreamMod, MicCapture);

using namespace StreamMod;
using namespace UnityEngine;

static ArrayW<StringW> GetDevices() {
    static auto icall = il2cpp_utils::resolve_icall<ArrayW<StringW>>("UnityEngine.Microphone::get_devices");
    return icall();
}

static int GetMicrophoneDeviceIDFromName(StringW name) {
    static auto icall = il2cpp_utils::resolve_icall<int, StringW>("UnityEngine.Microphone::GetMicrophoneDeviceIDFromName");
    return icall(name);
}

// static std::pair<int, int> GetDeviceCaps(int deviceId) {
//     static auto icall = il2cpp_utils::resolve_icall<void, int, ByRef<int>, ByRef<int>>("UnityEngine.Microphone::GetDeviceCaps");
//     int min, max;
//     icall(deviceId, byref(min), byref(max));
//     return {min, max};
// }

static AudioClip* StartRecord(int deviceId, bool loop, float lengthSec, int frequency) {
    static auto icall = il2cpp_utils::resolve_icall<AudioClip*, int, bool, float, int>("UnityEngine.Microphone::StartRecord");
    return icall(deviceId, loop, lengthSec, frequency);
}

static void EndRecord(int deviceId) {
    static auto icall = il2cpp_utils::resolve_icall<void, int>("UnityEngine.Microphone::EndRecord");
    icall(deviceId);
}

static float GetLoudness(ArrayW<float> data) {
    // root mean square - maybe use some sort of weighting filter in the future?
    // https://community.vcvrack.com/t/complete-list-of-native-fft-libraries-for-audio/9153
    float sum = 0;
    for (int i = 0; i < data.size(); i++)
        sum += data[i] * data[i];
    // call before ScaleData, plus apply a little boost to make 0-2 a good range
    return sqrt(sum / data.size()) * getConfig().MicVolume.GetValue() * 200;
}

static void ScaleData(ArrayW<float> data) {
    // not sure why it's so quiet that I have to multiply it by 10
    // audioSource volume doesn't seem to matter, at least above 1
    float scale = getConfig().MicVolume.GetValue() * 10;
    for (int i = 0; i < data.size(); i++)
        data[i] *= scale;
    // deal with clipping later, if at all
}

void MicCapture::Init() {
    if (deviceId != -1)
        return;

    auto devices = GetDevices();
    if (devices.size() == 0) {
        logger.error("no microphone devices found");
        return;
    }
    logger.debug("found microphones {}", fmt::join(devices, ", "));

    deviceId = GetMicrophoneDeviceIDFromName(devices[0]);
    logger.info("recording with microphone {} ({})", deviceId, devices[0]);

    // auto [min, max] = GetDeviceCaps(deviceId);
    sampleRate = AudioSettings::get_outputSampleRate();

    bufferPos = 0;
    audioClip = StartRecord(deviceId, true, 1, sampleRate);
    if (!audioClip) {
        logger.error("failed to start recording");
        sampleRate = -1;
        deviceId = -1;
        return;
    }
    audioSource->clip = audioClip;
    audioSource->loop = true;
    audioSource->Play();
}

void MicCapture::Stop() {
    if (deviceId == -1)
        return;

    logger.info("ending recording of microphone {}", deviceId);

    sampleRate = -1;
    channels = -1;
    currentLoudness = 0;

    EndRecord(deviceId);
    deviceId = -1;

    audioSource->Stop();
    audioSource->clip = nullptr;
    Object::Destroy(audioClip);
    audioClip = nullptr;
}

void MicCapture::OnAudioFilterRead(ArrayW<float> data, int audioChannels) {
    channels = audioChannels;
    bool zeroed = false;
    if (sampleRate != -1 && callback) {
        currentLoudness = GetLoudness(data);
        if (currentLoudness < getConfig().MicThreshold.GetValue()) {
            zeroed = true;
            std::fill(data.begin(), data.end(), 0);
        } else
            ScaleData(data);
        callback(data);
    }
    // make sure nothing is actually played
    if (!zeroed)
        std::fill(data.begin(), data.end(), 0);
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
    ret->audioSource = go->AddComponent<AudioSource*>();
    return ret;
}
