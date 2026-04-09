#include "engine/core/Engine.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "game/Game.h"

bool Engine::initialize() noexcept {
    renderer_.initialize();
    inputManager_.initialize();
    physics_.initialize();
    running_ = true;
    return true;
}

void Engine::run(Game& game, std::size_t maxFrames) {
    if (!running_ && !initialize()) {
        return;
    }

    game.initialize();

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    auto lastProfileFlush = previous;
    float accumulator = 0.0f;
    std::size_t frameCount = 0;

    while (running_) {
        const auto now = Clock::now();
        float frameDelta = std::chrono::duration<float>(now - previous).count();
        previous = now;

        frameDelta = std::clamp(frameDelta, 0.0f, kMaxFrameDelta);
        accumulator += frameDelta;

        if (accumulator < kFixedTimeStep) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        inputManager_.update();

        profiler_.startProfile("frame");
        while (accumulator >= kFixedTimeStep) {
            profiler_.startProfile("update");
            game.update(kFixedTimeStep, inputManager_);
            physics_.update(kFixedTimeStep);
            profiler_.stopProfile("update");
            accumulator -= kFixedTimeStep;
        }

        profiler_.startProfile("render");
        renderer_.beginFrame();
        game.render(renderer_);
        renderer_.present();
        profiler_.stopProfile("render");
        profiler_.stopProfile("frame");

        ++frameCount;
        if (maxFrames > 0 && frameCount >= maxFrames) {
            running_ = false;
        }

        if (now - lastProfileFlush >= std::chrono::seconds(1)) {
            profiler_.printAndReset(std::cout);
            lastProfileFlush = now;
        }
    }

    game.shutdown();
    shutdown();
}

void Engine::shutdown() noexcept {
    renderer_.cleanup();
    inputManager_.shutdown();
    physics_.cleanup();
    running_ = false;
}
