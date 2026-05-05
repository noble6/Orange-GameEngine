#include "engine/graphics/ThirdPersonCamera.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace {
    bool rayAABBIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const AABB& box, float& tMin) noexcept {
        float t1 = (box.min.x - rayOrigin.x) * (rayDir.x == 0.0f ? 1e10f : 1.0f / rayDir.x);
        float t2 = (box.max.x - rayOrigin.x) * (rayDir.x == 0.0f ? 1e10f : 1.0f / rayDir.x);
        float t3 = (box.min.y - rayOrigin.y) * (rayDir.y == 0.0f ? 1e10f : 1.0f / rayDir.y);
        float t4 = (box.max.y - rayOrigin.y) * (rayDir.y == 0.0f ? 1e10f : 1.0f / rayDir.y);
        float t5 = (box.min.z - rayOrigin.z) * (rayDir.z == 0.0f ? 1e10f : 1.0f / rayDir.z);
        float t6 = (box.max.z - rayOrigin.z) * (rayDir.z == 0.0f ? 1e10f : 1.0f / rayDir.z);

        float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
        float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

        if (tmax < 0.0f || tmin > tmax) {
            return false;
        }

        tMin = tmin > 0.0f ? tmin : tmax;
        return true;
    }
}

ThirdPersonCamera::ThirdPersonCamera() noexcept {
    const char* lagStr = std::getenv("TPS_CAM_LAG");
    if (lagStr) lagSpeed_ = static_cast<float>(std::atof(lagStr));

    const char* rotLagStr = std::getenv("TPS_CAM_ROT_LAG");
    if (rotLagStr) rotLagSpeed_ = static_cast<float>(std::atof(rotLagStr));

    const char* sensStr = std::getenv("TPS_MOUSE_SENS");
    if (sensStr) mouseSensitivity_ = static_cast<float>(std::atof(sensStr));

    const char* fovStr = std::getenv("TPS_FOV");
    if (fovStr) fov_ = static_cast<float>(std::atof(fovStr));

    currentLagSpeed_ = lagSpeed_;
}

void ThirdPersonCamera::onMouseMotion(int dx, int dy) noexcept {
    targetYaw_ -= static_cast<float>(dx) * mouseSensitivity_;
    targetPitch_ -= static_cast<float>(dy) * mouseSensitivity_;
    
    // Normalize yaw to 0-360
    while (targetYaw_ > 360.0f) targetYaw_ -= 360.0f;
    while (targetYaw_ < 0.0f) targetYaw_ += 360.0f;
}

void ThirdPersonCamera::setAimMode(bool aimMode) noexcept {
    aimMode_ = aimMode;
}

void ThirdPersonCamera::update(float deltaTime, const Vec3& playerPos, const AABB* sceneBounds, std::size_t boundsCount) noexcept {
    deltaTime = std::min(deltaTime, 0.05f);

    float targetArmBase = aimMode_ ? 3.5f : 6.0f;
    Vec3 targetAimOffset = aimMode_ ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 0.0f, 0.0f};
    float targetLag = aimMode_ ? 20.0f : lagSpeed_;
    float targetPitchFloor = aimMode_ ? -45.0f : -35.0f;

    float aimLerpFactor = 1.0f - std::exp(-deltaTime * 15.0f);
    targetArmLength_ = mathLerp(targetArmLength_, targetArmBase, aimLerpFactor);
    currentTargetOffset_ = mathLerp(currentTargetOffset_, targetAimOffset, aimLerpFactor);
    currentLagSpeed_ = mathLerp(currentLagSpeed_, targetLag, aimLerpFactor);
    currentPitchFloor_ = mathLerp(currentPitchFloor_, targetPitchFloor, aimLerpFactor);

    targetPitch_ = std::clamp(targetPitch_, currentPitchFloor_, 60.0f);

    float rotLerpFactor = 1.0f - std::exp(-deltaTime * rotLagSpeed_);
    
    // Smooth angle mathLerp
    float yawDiff = targetYaw_ - yaw_;
    if (yawDiff > 180.0f) yawDiff -= 360.0f;
    if (yawDiff < -180.0f) yawDiff += 360.0f;
    yaw_ += yawDiff * rotLerpFactor;
    
    // Normalize again
    while (yaw_ > 360.0f) yaw_ -= 360.0f;
    while (yaw_ < 0.0f) yaw_ += 360.0f;
    
    pitch_ = mathLerp(pitch_, targetPitch_, rotLerpFactor);

    float yawRad = yaw_ * (3.14159265f / 180.0f);
    float pitchRad = pitch_ * (3.14159265f / 180.0f);

    Vec3 forward{
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad),
        -std::cos(pitchRad) * std::cos(yawRad)
    };
    
    Vec3 right{
        std::cos(yawRad),
        0.0f,
        std::sin(yawRad)
    };

    Vec3 socketOffset{0.0f, 1.7f, 0.0f};
    Vec3 armRoot = playerPos + socketOffset;
    
    Vec3 lookAtPos = armRoot + right * currentTargetOffset_.x + Vec3{0.0f, currentTargetOffset_.y, 0.0f};
    
    if (std::sqrt(distanceSquared(lookAtPos, position_)) < 0.001f) {
        return; // skip frame to avoid degenerate lookAt matrix
    }
    
    Vec3 idealCamPos = lookAtPos - forward * targetArmLength_;

    float safeLength = targetArmLength_;
    if (sceneBounds && boundsCount > 0) {
        Vec3 rayDir = idealCamPos - lookAtPos;
        float rayLen = std::sqrt(lengthSquared(rayDir));
        if (rayLen > 0.001f) {
            rayDir = rayDir * (1.0f / rayLen);
            float closestHit = rayLen;
            for (std::size_t i = 0; i < boundsCount; ++i) {
                float t;
                if (rayAABBIntersect(lookAtPos, rayDir, sceneBounds[i], t)) {
                    if (t < closestHit && t > 0.0f) {
                        closestHit = t;
                    }
                }
            }
            safeLength = closestHit * 0.9f;
        }
    }

    if (currentLength_ > safeLength) {
        currentLength_ = safeLength;
    } else {
        currentLength_ = mathLerp(currentLength_, safeLength, 1.0f - std::exp(-deltaTime * 5.0f));
    }

    Vec3 targetPos = lookAtPos - forward * currentLength_;

    float posLerpFactor = 1.0f - std::exp(-deltaTime * currentLagSpeed_);
    position_ = mathLerp(position_, targetPos, posLerpFactor);
}

Mat4 ThirdPersonCamera::getViewMatrix() const noexcept {
    float yawRad = yaw_ * (3.14159265f / 180.0f);
    float pitchRad = pitch_ * (3.14159265f / 180.0f);

    Vec3 forward{
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad),
        -std::cos(pitchRad) * std::cos(yawRad)
    };
    
    Vec3 right{
        std::cos(yawRad),
        0.0f,
        std::sin(yawRad)
    };

    Vec3 up = cross(right, forward);
    
    return lookAt(position_, position_ + forward, up);
}

Mat4 ThirdPersonCamera::getProjectionMatrix(float aspectRatio) const noexcept {
    float fovRad = fov_ * (3.14159265f / 180.0f);
    return perspective(fovRad, aspectRatio, 0.1f, 500.0f);
}
