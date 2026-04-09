#pragma once

#include "game/player/Player.h"

class Enemy {
public:
    Enemy() = default;

    void initialize(const Vec3& spawnPosition) noexcept;
    void update(float deltaTime, Player& player) noexcept;
    void render() const noexcept;

    void takeDamage(int amount) noexcept;
    int getHealth() const noexcept;

private:
    void attack(Player& player) const noexcept;

    int health_ = 100;
    int damage_ = 10;
    float speed_ = 2.5f;
    float attackRangeSquared_ = 2.25f;
    Vec3 position_{};
};
