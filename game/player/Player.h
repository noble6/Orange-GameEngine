#pragma once

#include "engine/input/InputManager.h"

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

class Player {
public:
    void initialize() noexcept;
    void update(float deltaTime, const InputManager& inputManager) noexcept;
    void render() const noexcept;
    void shutdown() noexcept;

    void takeDamage(int damage) noexcept;

    int getHealth() const noexcept;
    const Vec3& getPosition() const noexcept;

private:
    void shoot() noexcept;

    int health_ = 100;
    Vec3 position_{};
    float moveSpeed_ = 6.0f;
    float shootCooldownSeconds_ = 0.0f;
};
