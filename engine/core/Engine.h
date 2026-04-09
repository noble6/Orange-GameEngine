#pragma once

#include <cstddef>

#include "engine/core/Profiler.h"
#include "engine/graphics/Renderer.h"
#include "engine/input/InputManager.h"
#include "engine/physics/Physics.h"

class Game;

class Engine {
public:
    bool initialize() noexcept;
    void run(Game& game, std::size_t maxFrames = 0);
    void shutdown() noexcept;

private:
    static constexpr float kFixedTimeStep = 1.0f / 60.0f;
    static constexpr float kMaxFrameDelta = 0.25f;

    bool running_ = false;
    Renderer renderer_;
    InputManager inputManager_;
    Physics physics_;
    Profiler profiler_;
};
