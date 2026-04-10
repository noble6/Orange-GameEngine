#pragma once

#include <cstdint>

#include "engine/core/Math.h"
#include "engine/input/InputManager.h"

class Player {
public:
    void initialize() noexcept;
    void update(float deltaTime, const InputManager& inputManager) noexcept;
    void render() const noexcept;
    void shutdown() noexcept;

    bool consumeShootRequest() noexcept;
    void takeDamage(int damage) noexcept;

    int getHealth() const noexcept;
    const Vec3& getPosition() const noexcept;
    std::uint32_t shotsFired() const noexcept;

private:
    void shoot() noexcept;

    int health_ = 100;
    Vec3 position_{};
    float moveSpeed_ = 6.0f;
    float shootCooldownSeconds_ = 0.0f;
    bool shootRequested_ = false;
    std::uint32_t shotsFired_ = 0;
};
