#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/graphics/Renderer.h"
#include "engine/input/InputManager.h"
#include "game/enemies/Enemy.h"
#include "game/player/Player.h"

class Game {
public:
    Game() = default;

    void initialize();
    void update(float deltaTime, const InputManager& inputManager);
    void render(Renderer& renderer);
    void shutdown();

    bool shouldTerminate() const noexcept;

private:
    void applyPlayerShot();
    std::size_t countAliveEnemies() const noexcept;

    Player player_;
    std::vector<Enemy> enemies_;

    std::uint32_t enemiesKilled_ = 0;
    bool victory_ = false;
    bool defeat_ = false;
};
