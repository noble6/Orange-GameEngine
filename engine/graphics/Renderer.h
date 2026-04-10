#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/Math.h"
#include "engine/rhi/IRhiDevice.h"

struct RendererConfig {
    std::uint16_t viewportWidth = 96;
    std::uint16_t viewportHeight = 32;
    float worldUnitsPerCell = 0.85f;
    float cullDistance = 20.0f;
    std::uint16_t maxVisibleEnemies = 256;
    std::uint16_t shadowCasterBudget = 48;
    std::uint8_t overlayEveryNFrames = 8;
    float targetFrameMs = 16.67f;

    bool enableDepthPrepass = true;
    bool enableShadowPass = true;
    bool forceDeferredLighting = false;
    bool enableSSAO = false;
    bool enableVolumetricFog = false;
    bool enablePost = true;
    bool enableOverlay = true;

    std::uint8_t msaaSamples = 1;
};

struct PassCost {
    const char* name = "";
    double cpuMs = 0.0;
    double gpuMs = 0.0;
    GpuTimestampToken gpuToken{};
    std::uint32_t workItems = 0;
    std::uint64_t estimatedBytes = 0;
    bool executed = false;
    bool hasGpuTimestamp = false;
};

struct FrameDiagnostics {
    std::uint32_t submittedEnemies = 0;
    std::uint32_t visibleEnemies = 0;
    std::uint32_t shadowCasters = 0;
    std::uint32_t culledEnemies = 0;
    std::uint32_t estimatedShadedPixels = 0;
    std::uint32_t estimatedOverdrawPixels = 0;
    bool usedDeferredLighting = false;
    const char* rhiBackend = "null";
    bool visibilityClamped = false;
    double totalCpuMs = 0.0;
    bool overCpuBudget = false;

    std::array<PassCost, 8> passes{};
    std::size_t passCount = 0;
};

class Renderer {
public:
    void initialize() noexcept;
    void beginFrame() noexcept;

    void submitPlayer(const Vec3& position,
                      int health,
                      std::uint32_t shotsFired,
                      std::uint32_t kills,
                      std::uint32_t enemiesAlive,
                      bool victory,
                      bool defeat) noexcept;
    void submitEnemy(const Vec3& position, std::uint16_t health) noexcept;

    void render() noexcept;
    void present() noexcept;
    void cleanup() noexcept;

    std::size_t frameCount() const noexcept;
    const FrameDiagnostics& lastFrameDiagnostics() const noexcept;

private:
    struct EnemyProxy {
        Vec3 position{};
        std::uint16_t health = 0;
    };

    struct PlayerProxy {
        Vec3 position{};
        int health = 0;
        std::uint32_t shotsFired = 0;
        std::uint32_t kills = 0;
        std::uint32_t enemiesAlive = 0;
        bool victory = false;
        bool defeat = false;
    };

    using Clock = std::chrono::steady_clock;

    template <typename Fn>
    void runPass(const char* name, bool enabled, Fn&& fn) noexcept;

    void passVisibility(PassCost& pass) noexcept;
    void passDepthPrepass(PassCost& pass) noexcept;
    void passShadow(PassCost& pass) noexcept;
    void passLighting(PassCost& pass) noexcept;
    void passFog(PassCost& pass) noexcept;
    void passTransparent(PassCost& pass) noexcept;
    void passPost(PassCost& pass) noexcept;

    void drawAsciiFrame() noexcept;
    void resolveGpuPassTimes() noexcept;
    void printDiagnosticsOverlay() noexcept;
    void resetDiagnostics() noexcept;

    RendererConfig config_{};

    PlayerProxy player_{};
    std::vector<EnemyProxy> enemies_;
    std::vector<std::uint32_t> visibleEnemyIndices_;
    std::vector<std::uint32_t> shadowCasterIndices_;

    FrameDiagnostics diagnostics_{};
    std::unique_ptr<IRhiDevice> rhiDevice_;

    std::size_t frameCount_ = 0;
    std::size_t terminalPrintCounter_ = 0;
    std::string frameBuffer_;
};
