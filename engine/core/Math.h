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

inline Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float mathLerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

inline Vec3 mathLerp(const Vec3& a, const Vec3& b, float t) noexcept {
    return Vec3{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

struct Mat4 {
    float m[4][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    };
};

inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) noexcept {
    Vec3 f = normalizedOrZero(center - eye);
    Vec3 s = normalizedOrZero(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 Result;
    Result.m[0][0] = s.x;
    Result.m[1][0] = s.y;
    Result.m[2][0] = s.z;
    Result.m[0][1] = u.x;
    Result.m[1][1] = u.y;
    Result.m[2][1] = u.z;
    Result.m[0][2] = -f.x;
    Result.m[1][2] = -f.y;
    Result.m[2][2] = -f.z;
    Result.m[3][0] = -dot(s, eye);
    Result.m[3][1] = -dot(u, eye);
    Result.m[3][2] = dot(f, eye);
    return Result;
}

inline Mat4 perspective(float fovY, float aspect, float zNear, float zFar) noexcept {
    const float tanHalfFovy = std::tan(fovY * 0.5f);
    Mat4 Result;
    Result.m[0][0] = 0.0f;
    Result.m[1][1] = 0.0f;
    Result.m[2][2] = 0.0f;
    Result.m[3][3] = 0.0f;

    Result.m[0][0] = 1.0f / (aspect * tanHalfFovy);
    Result.m[1][1] = 1.0f / (tanHalfFovy);
    Result.m[2][2] = zFar / (zNear - zFar);
    Result.m[2][3] = -1.0f;
    Result.m[3][2] = -(zFar * zNear) / (zFar - zNear);

    // Result.m[1][1] *= -1.0f; // Vulkan Y-flip (Commented out for SDL2 software output)
    return Result;
}
