#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "engine/graphics/Renderer.h"
#include "engine/input/InputManager.h"
#include "engine/graphics/ThirdPersonCamera.h"
#include "game/enemies/Enemy.h"
#include "game/player/Player.h"
#include <array>

class Game {
public:
    Game() = default;

    void initialize();
    void update(float deltaTime, const InputManager& inputManager);
    void render(Renderer& renderer);
    void shutdown();

    bool shouldTerminate() const noexcept;
    void onMouseMotion(int dx, int dy) noexcept;
    void setAimMode(bool aimMode) noexcept;

private:
    void applyPlayerShot();
    std::size_t countAliveEnemies() const noexcept;
    void spawnWave(std::uint32_t waveIndex);

    Player player_;
    ThirdPersonCamera camera_;
    std::array<AABB, 64> sceneBounds_{};
    std::size_t numSceneBounds_ = 0;

    std::vector<Enemy> enemies_;
    std::mt19937 rng_;

    std::uint32_t enemiesKilled_ = 0;
    std::uint32_t currentWave_ = 0;
    std::uint32_t maxWaves_ = 5;
    float waveTransitionTimer_ = 0.0f;
    bool victory_ = false;
    bool defeat_ = false;
};
