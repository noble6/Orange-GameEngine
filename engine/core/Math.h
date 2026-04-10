#pragma once

#include <cmath>

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 operator-(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(const Vec3& v, float scalar) noexcept {
    return Vec3{v.x * scalar, v.y * scalar, v.z * scalar};
}

inline float dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float lengthSquared(const Vec3& v) noexcept {
    return dot(v, v);
}

inline float distanceSquared(const Vec3& a, const Vec3& b) noexcept {
    return lengthSquared(a - b);
}

inline Vec3 normalizedOrZero(const Vec3& v) noexcept {
    const float lenSq = lengthSquared(v);
    if (lenSq <= 1e-10f) {
        return Vec3{};
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    return v * invLen;
}
