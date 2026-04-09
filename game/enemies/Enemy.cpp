#include "game/enemies/Enemy.h"

#include <cmath>

void Enemy::initialize(const Vec3& spawnPosition) noexcept {
    health_ = 100;
    position_ = spawnPosition;
}

void Enemy::update(float deltaTime, Player& player) noexcept {
    const Vec3& playerPosition = player.getPosition();

    const float dx = playerPosition.x - position_.x;
    const float dy = playerPosition.y - position_.y;
    const float dz = playerPosition.z - position_.z;

    const float distanceSquared = dx * dx + dy * dy + dz * dz;
    if (distanceSquared <= attackRangeSquared_) {
        attack(player);
        return;
    }

    const float distance = std::sqrt(distanceSquared + 1e-6f);
    const float invDistance = 1.0f / distance;
    const float distanceStep = speed_ * deltaTime;

    position_.x += dx * invDistance * distanceStep;
    position_.y += dy * invDistance * distanceStep;
    position_.z += dz * invDistance * distanceStep;
}

void Enemy::render() const noexcept {
}

void Enemy::takeDamage(int amount) noexcept {
    health_ -= amount;
    if (health_ < 0) {
        health_ = 0;
    }
}

int Enemy::getHealth() const noexcept {
    return health_;
}

void Enemy::attack(Player& player) const noexcept {
    player.takeDamage(damage_);
}
