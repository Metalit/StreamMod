#pragma once

#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(StreamMod, MicCapture, UnityEngine::MonoBehaviour) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Init);
    DECLARE_INSTANCE_METHOD(void, Stop);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

    DECLARE_INSTANCE_FIELD_DEFAULT(int, outputSampleRate, -1);
    DECLARE_INSTANCE_FIELD_DEFAULT(int, micSampleRate, -1);
    DECLARE_INSTANCE_FIELD_DEFAULT(int, deviceId, -1);
    DECLARE_INSTANCE_FIELD_DEFAULT(int, bufferPos, 0);

    DECLARE_INSTANCE_FIELD_DEFAULT(float, currentLoudness, 0);

    DECLARE_INSTANCE_FIELD(ArrayW<float>, fragmentBuffer);
    DECLARE_INSTANCE_FIELD(ArrayW<float>, micBuffer);
    DECLARE_INSTANCE_FIELD(UnityEngine::AudioClip*, audioClip);

   public:
    std::function<void(ArrayW<float>)> callback;

    static MicCapture* Create();
};
