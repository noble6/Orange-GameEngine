#pragma once

#include "engine/core/Math.h"
#include <cstddef>

struct AABB {
    Vec3 min;
    Vec3 max;
};

class ThirdPersonCamera {
public:
    ThirdPersonCamera() noexcept;

    void update(float deltaTime, const Vec3& playerPos, const AABB* sceneBounds, std::size_t boundsCount) noexcept;
    
    void onMouseMotion(int dx, int dy) noexcept;
    void setAimMode(bool aimMode) noexcept;

    Mat4 getViewMatrix() const noexcept;
    Mat4 getProjectionMatrix(float aspectRatio) const noexcept;

    float getYaw() const noexcept { return yaw_; }
    float getPitch() const noexcept { return pitch_; }
    float getArmLength() const noexcept { return currentLength_; }
    Vec3 getPosition() const noexcept { return position_; }
    bool isAimMode() const noexcept { return aimMode_; }

private:
    float yaw_ = 0.0f;
    float pitch_ = -15.0f;

    float currentLength_ = 6.0f;
    Vec3 position_{};

    bool aimMode_ = false;

    float lagSpeed_ = 8.0f;
    float rotLagSpeed_ = 12.0f;
    float mouseSensitivity_ = 0.15f;
    float fov_ = 75.0f;

    float targetYaw_ = 0.0f;
    float targetPitch_ = -15.0f;

    float targetArmLength_ = 6.0f;
    Vec3 currentTargetOffset_{0.0f, 0.0f, 0.0f};

    float currentLagSpeed_ = 8.0f;
    float currentPitchFloor_ = -35.0f;
};
