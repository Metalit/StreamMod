#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"

// all credit to @fern [https://github.com/Fernthedev/]

static float ExpoEaseInOut(float t, float b, float c, float d) {
    if (t == 0)
        return b;
    if (t == d)
        return b + c;
    if ((t /= d / 2) < 1)
        return c / 2 * (float) std::pow(2, 10 * (t - 1)) + b;
    return c / 2 * (-(float) std::pow(2, -10 * --t) + 2) + b;
}

static float EasedLerp(float from, float to, float time, float deltaTime) {
    return ExpoEaseInOut(time, from, to - from, deltaTime);
}

// This will make it so big movements are actually exponentially bigger while smaller ones are less
static UnityEngine::Vector3 EaseLerp(UnityEngine::Vector3 a, UnityEngine::Vector3 b, float time, float deltaTime) {
    return {EasedLerp(a.x, b.x, time, deltaTime), EasedLerp(a.y, b.y, time, deltaTime), EasedLerp(a.z, b.z, time, deltaTime)};
}

static UnityEngine::Quaternion Slerp(UnityEngine::Quaternion quaternion1, UnityEngine::Quaternion quaternion2, float amount) {
    float num = quaternion1.x * quaternion2.x + quaternion1.y * quaternion2.y + quaternion1.z * quaternion2.z + quaternion1.w * quaternion2.w;
    bool flag = false;
    if (num < 0) {
        flag = true;
        num = -num;
    }
    float num2;
    float num3;
    if (num > 0.999999) {
        num2 = 1 - amount;
        num3 = (flag ? (-amount) : amount);
    } else {
        float num4 = std::acos(num);
        float num5 = 1.0f / std::sin(num4);
        num2 = std::sin((1.0f - amount) * num4) * num5;
        num3 = (flag ? (-std::sin(amount * num4) * num5) : (std::sin(amount * num4) * num5));
    }
    UnityEngine::Quaternion result;
    result.x = num2 * quaternion1.x + num3 * quaternion2.x;
    result.y = num2 * quaternion1.y + num3 * quaternion2.y;
    result.z = num2 * quaternion1.z + num3 * quaternion2.z;
    result.w = num2 * quaternion1.w + num3 * quaternion2.w;
    return result;
}
