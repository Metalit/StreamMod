#pragma once

#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"
#include "stream.pb.h"

namespace Manager {
    void Init();
    void Update();
    void Invalidate();
    void SetCamera(UnityEngine::Camera* main);
    void HandleMessage(PacketWrapper const& packet, void* source);
    void UpdateVideoSettings(void* source = nullptr);
    void UpdateAudioSettings(void* source = nullptr);
    void UpdateCameraSettings(void* source = nullptr);
    void UpdateAudioConfig();
    bool IsCapturing();
    void RestartCapture();
    void StopCapture();
}
