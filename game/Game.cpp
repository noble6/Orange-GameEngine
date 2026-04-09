#include "game/Game.h"

namespace {
constexpr std::size_t kInitialEnemyCount = 128;
constexpr float kSpawnSpacing = 2.5f;
}  // namespace

void Game::initialize() {
    player_.initialize();

    enemies_.clear();
    enemies_.reserve(kInitialEnemyCount);

    for (std::size_t i = 0; i < kInitialEnemyCount; ++i) {
        Enemy enemy;
        enemy.initialize(Vec3{static_cast<float>(i) * kSpawnSpacing, 0.0f, 0.0f});
        enemies_.push_back(enemy);
    }
}

void Game::update(float deltaTime, const InputManager& inputManager) {
    player_.update(deltaTime, inputManager);

    for (auto& enemy : enemies_) {
        enemy.update(deltaTime, player_);
    }
}

void Game::render(Renderer& renderer) {
    player_.render();
    for (const auto& enemy : enemies_) {
        enemy.render();
    }

    renderer.render();
}

void Game::shutdown() {
    enemies_.clear();
    player_.shutdown();
}
