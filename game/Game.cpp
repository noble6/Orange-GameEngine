#include "game/Game.h"

#include <cmath>
#include <limits>

namespace {
constexpr float kEnemySpawnRadius = 18.0f;
constexpr int kPlayerDamagePerShot = 40;
constexpr float kShotRangeSquared = 12.0f * 12.0f;
}  // namespace

void Game::initialize() {
    player_.initialize();

    const char* seedEnv = std::getenv("TPS_RANDOM_SEED");
    if (seedEnv != nullptr && seedEnv[0] != '\0') {
        rng_.seed(static_cast<std::uint32_t>(std::atoi(seedEnv)));
    } else {
        std::random_device rd;
        rng_.seed(rd());
    }

    enemiesKilled_ = 0;
    currentWave_ = 1;
    waveTransitionTimer_ = 0.0f;
    victory_ = false;
    defeat_ = false;

    spawnWave(currentWave_);
}

void Game::onMouseMotion(int dx, int dy) noexcept {
    camera_.onMouseMotion(dx, dy);
}

void Game::setAimMode(bool aimMode) noexcept {
    camera_.setAimMode(aimMode);
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

    camera_.update(deltaTime, player_.getPosition(), sceneBounds_.data(), numSceneBounds_);

    if (player_.getHealth() <= 0) {
        defeat_ = true;
    } else if (countAliveEnemies() == 0U) {
        if (currentWave_ >= maxWaves_) {
            victory_ = true;
        } else {
            waveTransitionTimer_ += deltaTime;
            if (waveTransitionTimer_ >= 2.0f) {
                ++currentWave_;
                spawnWave(currentWave_);
                waveTransitionTimer_ = 0.0f;
            }
        }
    }
}

void Game::spawnWave(std::uint32_t waveIndex) {
    enemies_.clear();

    const std::size_t enemyCount = 16 + static_cast<std::size_t>(waveIndex) * 16;
    const float speed = 1.5f + static_cast<float>(waveIndex) * 0.25f;
    const int health = 80 + static_cast<int>(waveIndex) * 20;

    enemies_.reserve(enemyCount);

    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318531f);
    std::uniform_real_distribution<float> radiusDist(kEnemySpawnRadius * 0.8f, kEnemySpawnRadius * 1.5f);

    for (std::size_t i = 0; i < enemyCount; ++i) {
        const float angle = angleDist(rng_);
        const float radius = radiusDist(rng_);
        
        const Vec3 spawnPosition{
            std::cos(angle) * radius,
            std::sin(angle) * radius,
            0.0f,
        };

        Enemy enemy;
        enemy.initialize(spawnPosition, speed, health);
        enemies_.push_back(enemy);
    }
}

void Game::render(Renderer& renderer) {
    const std::size_t aliveEnemies = countAliveEnemies();

    // 16:9 aspect ratio standard for a 1280x720 window, though the engine might have a different default
    // We don't have direct access to viewport dimensions here easily, so hardcode 16:9 for now.
    renderer.setCameraMatrices(camera_.getViewMatrix(), camera_.getProjectionMatrix(16.0f / 9.0f));
    renderer.submitCamera(camera_.getYaw(), camera_.getPitch(), camera_.getArmLength(), camera_.getPosition(), camera_.isAimMode());

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
