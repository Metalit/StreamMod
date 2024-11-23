#pragma once

#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"
#include "stream.pb.h"

namespace Manager {
    void Init();
    void SetCamera(UnityEngine::Camera* main);
    void SetFollowLocation(UnityEngine::Vector3 pos, UnityEngine::Quaternion rot);
    void HandleMessage(PacketWrapper const& packet, void* source);
    void SendSettings(void* source = nullptr);
    void RestartCapture();
}
