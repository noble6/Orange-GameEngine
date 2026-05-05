#include "game/enemies/Enemy.h"

#include <algorithm>

#include "game/player/Player.h"

void Enemy::initialize(const Vec3& spawnPosition, float speed, int health) noexcept {
    health_ = health;
    speed_ = speed;
    position_ = spawnPosition;
    attackCooldownSeconds_ = 0.0f;
}

void Enemy::update(float deltaTime, Player& player) noexcept {
    if (!isAlive()) {
        return;
    }

    attackCooldownSeconds_ = std::max(0.0f, attackCooldownSeconds_ - deltaTime);

    const Vec3& playerPosition = player.getPosition();
    const Vec3 toPlayer = playerPosition - position_;
    const float distanceSq = lengthSquared(toPlayer);

    if (distanceSq <= attackRangeSquared_) {
        if (attackCooldownSeconds_ <= 0.0f) {
            attack(player);
            attackCooldownSeconds_ = 0.65f;
        }
        return;
    }

    const Vec3 direction = normalizedOrZero(toPlayer);
    position_ = position_ + direction * (speed_ * deltaTime);
}

void Enemy::render() const noexcept {
}

void Enemy::takeDamage(int amount) noexcept {
    if (!isAlive()) {
        return;
    }

    health_ -= amount;
    if (health_ < 0) {
        health_ = 0;
    }
}

int Enemy::getHealth() const noexcept {
    return health_;
}

bool Enemy::isAlive() const noexcept {
    return health_ > 0;
}

const Vec3& Enemy::getPosition() const noexcept {
    return position_;
}

void Enemy::attack(Player& player) noexcept {
    player.takeDamage(damage_);
}
