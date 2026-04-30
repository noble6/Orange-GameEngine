#include "game/Game.h"

#include <cmath>
#include <limits>

namespace {
constexpr std::size_t kInitialEnemyCount = 96;
constexpr float kEnemySpawnRadius = 18.0f;
constexpr int kPlayerDamagePerShot = 40;
constexpr float kShotRangeSquared = 12.0f * 12.0f;
}  // namespace

void Game::initialize() {
    player_.initialize();

    enemies_.clear();
    enemies_.reserve(kInitialEnemyCount);

    for (std::size_t i = 0; i < kInitialEnemyCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kInitialEnemyCount);
        const float angle = t * 6.28318531f;
        const Vec3 spawnPosition{
            std::cos(angle) * kEnemySpawnRadius,
            std::sin(angle) * kEnemySpawnRadius,
            0.0f,
        };

        Enemy enemy;
        enemy.initialize(spawnPosition);
        enemies_.push_back(enemy);
    }

    enemiesKilled_ = 0;
    victory_ = false;
    defeat_ = false;
}

void Game::update(float deltaTime, const InputManager& inputManager) {
    if (victory_ || defeat_) {
        return;
    }

    player_.update(deltaTime, inputManager);
    if (player_.consumeShootRequest()) {
        applyPlayerShot();
    }

    for (auto& enemy : enemies_) {
        enemy.update(deltaTime, player_);
    }

    if (player_.getHealth() <= 0) {
        defeat_ = true;
    } else if (countAliveEnemies() == 0U) {
        victory_ = true;
    }
}

void Game::render(Renderer& renderer) {
    const std::size_t aliveEnemies = countAliveEnemies();

    renderer.submitPlayer(
        player_.getPosition(),
        player_.getHealth(),
        player_.shotsFired(),
        enemiesKilled_,
        static_cast<std::uint32_t>(aliveEnemies),
        victory_,
        defeat_);

    for (const auto& enemy : enemies_) {
        if (!enemy.isAlive()) {
            continue;
        }

        renderer.submitEnemy(enemy.getPosition(), static_cast<std::uint16_t>(enemy.getHealth()));
    }

    renderer.render();
}

void Game::shutdown() {
    enemies_.clear();
    player_.shutdown();
}

bool Game::shouldTerminate() const noexcept {
    return victory_ || defeat_;
}

void Game::applyPlayerShot() {
    const Vec3& playerPosition = player_.getPosition();

    float bestDistanceSq = std::numeric_limits<float>::max();
    Enemy* bestTarget = nullptr;

    for (auto& enemy : enemies_) {
        if (!enemy.isAlive()) {
            continue;
        }

        const float distSq = distanceSquared(enemy.getPosition(), playerPosition);
        if (distSq > kShotRangeSquared || distSq >= bestDistanceSq) {
            continue;
        }

        bestDistanceSq = distSq;
        bestTarget = &enemy;
    }

    if (bestTarget == nullptr) {
        return;
    }

    const bool wasAlive = bestTarget->isAlive();
    bestTarget->takeDamage(kPlayerDamagePerShot);
    if (wasAlive && !bestTarget->isAlive()) {
        ++enemiesKilled_;
    }
}

std::size_t Game::countAliveEnemies() const noexcept {
    std::size_t count = 0;
    for (const auto& enemy : enemies_) {
        count += enemy.isAlive() ? 1U : 0U;
    }
    return count;
}
