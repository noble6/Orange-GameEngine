#pragma once

#include <vector>

#include "engine/input/InputManager.h"
#include "engine/graphics/Renderer.h"
#include "game/enemies/Enemy.h"
#include "game/player/Player.h"

class Game {
public:
    Game() = default;

    void initialize();
    void update(float deltaTime, const InputManager& inputManager);
    void render(Renderer& renderer);
    void shutdown();

private:
    Player player_;
    std::vector<Enemy> enemies_;
};
