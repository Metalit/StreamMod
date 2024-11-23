#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"
#include "mic.hpp"

// clang-format off
DECLARE_CLASS_CODEGEN(StreamMod, AudioCapture, UnityEngine::MonoBehaviour,
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, SetMicCapture, bool enabled);
    DECLARE_INSTANCE_METHOD(void, OnAudioFilterRead, ArrayW<float> data, int audioChannels);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

    DECLARE_INSTANCE_FIELD_DEFAULT(int, sampleRate, -1);
    DECLARE_INSTANCE_FIELD_DEFAULT(int, channels, -1);
    DECLARE_INSTANCE_FIELD_DEFAULT(MicCapture*, mic, nullptr);
    DECLARE_INSTANCE_FIELD_DEFAULT(bool, mixLastMic, false);

   public:
    std::function<void(std::vector<float> const&, int, int)> callback;
    std::function<void(AudioCapture*)> onDisable;

    std::vector<float> gameBuffer;
    std::vector<float> micBuffer;
    std::shared_mutex mutex;
)
// clang-format on