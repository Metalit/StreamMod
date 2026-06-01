#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"

// all credit to @fern [https://github.com/Fernthedev/]

constexpr inline float ExpoEaseInOut(float t, float b, float c, float d) {
    if (t == 0)
        return b;
    if (t == d)
        return b + c;
    if ((t /= d / 2) < 1)
        return c / 2 * (float) std::pow(2, 10 * (t - 1)) + b;
    return c / 2 * (-(float) std::pow(2, -10 * --t) + 2) + b;
}

constexpr inline float EasedLerp(float from, float to, float time, float deltaTime) {
    return ExpoEaseInOut(time, from, to - from, deltaTime);
}

// This will make it so big movements are actually exponentially bigger while smaller ones are less
constexpr inline UnityEngine::Vector3 EaseLerp(UnityEngine::Vector3 a, UnityEngine::Vector3 b, float time, float deltaTime) {
    return {EasedLerp(a.x, b.x, time, deltaTime), EasedLerp(a.y, b.y, time, deltaTime), EasedLerp(a.z, b.z, time, deltaTime)};
}

constexpr inline UnityEngine::Quaternion Slerp(UnityEngine::Quaternion quaternion1, UnityEngine::Quaternion quaternion2, float amount) {
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

constexpr inline UnityEngine::Quaternion operator*(UnityEngine::Quaternion const& lhs, UnityEngine::Quaternion const& rhs) {
    return UnityEngine::Quaternion(
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
    );
}

constexpr inline UnityEngine::Vector3 operator*(UnityEngine::Quaternion const& lhs, UnityEngine::Vector3 const& rhs) {
    float num = lhs.x * 2.0f;
    float num2 = lhs.y * 2.0f;
    float num3 = lhs.z * 2.0f;
    float num4 = lhs.x * num;
    float num5 = lhs.y * num2;
    float num6 = lhs.z * num3;
    float num7 = lhs.x * num2;
    float num8 = lhs.x * num3;
    float num9 = lhs.y * num3;
    float num10 = lhs.w * num;
    float num11 = lhs.w * num2;
    float num12 = lhs.w * num3;
    return UnityEngine::Vector3(
        (1.0f - (num5 + num6)) * rhs.x + (num7 - num12) * rhs.y + (num8 + num11) * rhs.z,
        (num7 + num12) * rhs.x + (1.0f - (num4 + num6)) * rhs.y + (num9 - num10) * rhs.z,
        (num8 - num11) * rhs.x + (num9 + num10) * rhs.y + (1.0f - (num4 + num5)) * rhs.z
    );
}

constexpr inline UnityEngine::Vector3 operator+(UnityEngine::Vector3 const& lhs, UnityEngine::Vector3 const& rhs) {
    return UnityEngine::Vector3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}
