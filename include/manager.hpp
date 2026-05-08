#pragma once

#include "stream.pb.h"

#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"

namespace Manager {
    void Init();
    void Update();
    void Invalidate();
    void SetCamera(UnityEngine::Camera* main);
    void HandleMessage(PacketWrapper const& packet, void* source);
    void UpdateSettings(void* source = nullptr);
    bool IsCapturing();
    void RestartCapture();
    void StopCapture();
}
