#pragma once

#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"

// clang-format off
DECLARE_CLASS_CODEGEN(StreamMod, MicCapture, UnityEngine::MonoBehaviour,
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Init);
    DECLARE_INSTANCE_METHOD(void, Stop);
    DECLARE_INSTANCE_METHOD(void, OnAudioFilterRead, ArrayW<float> data, int audioChannels);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

    DECLARE_INSTANCE_FIELD_DEFAULT(int, sampleRate, -1);
    DECLARE_INSTANCE_FIELD_DEFAULT(int, channels, -1);
    DECLARE_INSTANCE_FIELD_DEFAULT(int, deviceId, -1);
    DECLARE_INSTANCE_FIELD(int, bufferPos);

    DECLARE_INSTANCE_FIELD_DEFAULT(int, currentLoudness, 0);

    DECLARE_INSTANCE_FIELD(UnityEngine::AudioClip*, audioClip);
    DECLARE_INSTANCE_FIELD(UnityEngine::AudioSource*, audioSource);

   public:
    std::function<void(ArrayW<float>)> callback;

    static MicCapture* Create();
)
// clang-format on