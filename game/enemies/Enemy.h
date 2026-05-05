#pragma once

#include "engine/core/Math.h"

class Player;

class Enemy {
public:
    Enemy() = default;

    void initialize(const Vec3& spawnPosition, float speed = 2.5f, int health = 100) noexcept;
    void update(float deltaTime, Player& player) noexcept;
    void render() const noexcept;

    void takeDamage(int amount) noexcept;

    int getHealth() const noexcept;
    bool isAlive() const noexcept;
    const Vec3& getPosition() const noexcept;

private:
    void attack(Player& player) noexcept;

    int health_ = 100;
    int damage_ = 8;
    float speed_ = 2.5f;
    float attackRangeSquared_ = 2.25f;
    float attackCooldownSeconds_ = 0.0f;
    Vec3 position_{};
};
