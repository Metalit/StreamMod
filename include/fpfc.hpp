#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"

namespace FPFC {
    void GetControllers();
    void ReleaseControllers();
    void OnSceneChange();
    void KeyDown(std::string const& key);
    void KeyUp(std::string const& key);
    void MouseMove(UnityEngine::Vector2 move);
    void MouseDown();
    void MouseUp();
    void AddScroll(float amount);
    UnityEngine::Vector3 GetMovement();
    UnityEngine::Quaternion GetRotation();
    void MoveControllers(UnityEngine::Transform* target);
}
