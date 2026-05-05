#include "engine/core/Engine.h"

#if defined(TPS_HAS_SDL2)
#include <SDL2/SDL.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "game/Game.h"

bool Engine::initialize() noexcept {
#if defined(TPS_HAS_SDL2)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        window_ = SDL_CreateWindow(
            "TPS Engine",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1280, 720,
            SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
        );
        if (window_ == nullptr) {
            std::cerr << "[Engine] Failed to create SDL window: " << SDL_GetError() << "\n";
        }
    } else {
        std::cerr << "[Engine] Failed to initialize SDL: " << SDL_GetError() << "\n";
    }
#endif

    renderer_.initialize(window_);
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

#if defined(TPS_HAS_SDL2)
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running_ = false;
            } else if (e.type == SDL_MOUSEMOTION) {
                game.onMouseMotion(e.motion.xrel, e.motion.yrel);
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
                game.setAimMode(true);
            } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
                game.setAimMode(false);
            }
        }
        
        static bool mouseModeSet = false;
        if (!mouseModeSet && window_ != nullptr) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            mouseModeSet = true;
        }
#endif

        inputManager_.update();
        if (inputManager_.quitRequested()) {
            running_ = false;
            continue;
        }

        if (accumulator < kFixedTimeStep) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        profiler_.startProfile("frame");

        while (accumulator >= kFixedTimeStep) {
            profiler_.startProfile("update");
            game.update(kFixedTimeStep, inputManager_);
            physics_.update(kFixedTimeStep);
            profiler_.stopProfile("update");

            if (game.shouldTerminate()) {
                running_ = false;
                break;
            }

            accumulator -= kFixedTimeStep;
        }

        if (!running_) {
            profiler_.stopProfile("frame");
            break;
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

#if defined(TPS_HAS_SDL2)
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
#endif

    running_ = false;
}
