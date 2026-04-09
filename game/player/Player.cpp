#include "game/player/Player.h"

#include <algorithm>

void Player::initialize() noexcept {
    health_ = 100;
    position_ = Vec3{};
    shootCooldownSeconds_ = 0.0f;
}

void Player::update(float deltaTime, const InputManager& inputManager) noexcept {
    float x = 0.0f;
    float y = 0.0f;

    if (inputManager.isKeyPressed(InputManager::Key::MoveForward)) {
        y += 1.0f;
    }
    if (inputManager.isKeyPressed(InputManager::Key::MoveBackward)) {
        y -= 1.0f;
    }
    if (inputManager.isKeyPressed(InputManager::Key::MoveLeft)) {
        x -= 1.0f;
    }
    if (inputManager.isKeyPressed(InputManager::Key::MoveRight)) {
        x += 1.0f;
    }

    const float normalization = (x != 0.0f && y != 0.0f) ? 0.70710678f : 1.0f;
    const float distance = moveSpeed_ * deltaTime * normalization;

    position_.x += x * distance;
    position_.y += y * distance;

    shootCooldownSeconds_ = std::max(0.0f, shootCooldownSeconds_ - deltaTime);
    if (inputManager.isKeyPressed(InputManager::Key::Shoot) && shootCooldownSeconds_ <= 0.0f) {
        shoot();
    }
}

void Player::render() const noexcept {
}

void Player::shutdown() noexcept {
}

void Player::shoot() noexcept {
    shootCooldownSeconds_ = 0.15f;
}

void Player::takeDamage(int damage) noexcept {
    health_ -= damage;
    if (health_ < 0) {
        health_ = 0;
    }
}

int Player::getHealth() const noexcept {
    return health_;
}

const Vec3& Player::getPosition() const noexcept {
    return position_;
}
