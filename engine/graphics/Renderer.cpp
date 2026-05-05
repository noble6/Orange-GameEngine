#include "engine/graphics/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {
constexpr std::uint64_t kDepthBytesPerPixel = 4;
constexpr std::uint64_t kColorBytesPerPixel = 4;
constexpr std::uint64_t kCompactGBufferBytesPerPixel = 8;

const char* resourceStateName(RenderResourceState state) noexcept {
    switch (state) {
        case RenderResourceState::Undefined:
            return "undefined";
        case RenderResourceState::RenderTarget:
            return "render_target";
        case RenderResourceState::DepthStencilTarget:
            return "depth_stencil_target";
        case RenderResourceState::DepthStencilRead:
            return "depth_stencil_read";
        case RenderResourceState::ShaderResource:
            return "shader_resource";
        case RenderResourceState::UnorderedAccess:
            return "unordered_access";
        case RenderResourceState::TransferSrc:
            return "transfer_src";
        case RenderResourceState::TransferDst:
            return "transfer_dst";
        case RenderResourceState::Present:
            return "present";
    }
    return "unknown";
}

bool parseBoolEnv(const char* envName, bool fallback) noexcept {
    const char* value = std::getenv(envName);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }

    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(value[0])));
    if (c == '0' || c == 'f' || c == 'n') {
        return false;
    }
    return true;
}

std::uint8_t parseUInt8Env(const char* envName, std::uint8_t fallback) noexcept {
    const char* value = std::getenv(envName);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }

    const int parsed = std::atoi(value);
    if (parsed <= 0) {
        return fallback;
    }

    return static_cast<std::uint8_t>(std::clamp(parsed, 1, 8));
}

std::uint16_t parseUInt16Env(const char* envName, std::uint16_t fallback, std::uint16_t minValue, std::uint16_t maxValue) noexcept {
    const char* value = std::getenv(envName);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }

    const int parsed = std::atoi(value);
    if (parsed <= 0) {
        return fallback;
    }

    return static_cast<std::uint16_t>(std::clamp(parsed, static_cast<int>(minValue), static_cast<int>(maxValue)));
}

float parseFloatEnv(const char* envName, float fallback) noexcept {
    const char* value = std::getenv(envName);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }

    const float parsed = static_cast<float>(std::atof(value));
    if (parsed <= 0.0f) {
        return fallback;
    }

    return parsed;
}
}  // namespace

void Renderer::initialize(void* windowHandle) noexcept {
    const char* backendEnvValue = std::getenv("TPS_RHI_BACKEND");
    if (backendEnvValue == nullptr || backendEnvValue[0] == '\0') {
        std::cerr << "[RHI] TPS_RHI_BACKEND not set, defaulting to vulkan.\n";
    }

    rhiDevice_ = createRhiDeviceFromEnvironment();
    if (!rhiDevice_ || !rhiDevice_->initialize(windowHandle)) {
        std::cerr << "[RHI] Vulkan backend initialization failed, falling back to null backend.\n";
        rhiDevice_ = createRhiDevice(RhiBackend::Null);
        (void)rhiDevice_->initialize(windowHandle);
    } else if (std::string_view(rhiDevice_->backendName()) == "vulkan_stub") {
        std::cerr << "[RHI] Vulkan SDK was not linked at build time; running with vulkan_stub (no real GPU timestamps).\n";
    }

    config_.enableDepthPrepass = parseBoolEnv("TPS_DEPTH_PREPASS", config_.enableDepthPrepass);
    config_.enableShadowPass = parseBoolEnv("TPS_SHADOWS", config_.enableShadowPass);
    config_.forceDeferredLighting = parseBoolEnv("TPS_FORCE_DEFERRED", config_.forceDeferredLighting);
    config_.enableSSAO = parseBoolEnv("TPS_SSAO", config_.enableSSAO);
    config_.enableVolumetricFog = parseBoolEnv("TPS_FOG", config_.enableVolumetricFog);
    config_.enablePost = parseBoolEnv("TPS_POST", config_.enablePost);
    config_.enableOverlay = parseBoolEnv("TPS_OVERLAY", config_.enableOverlay);
    config_.msaaSamples = parseUInt8Env("TPS_MSAA", config_.msaaSamples);
    config_.cullDistance = parseFloatEnv("TPS_CULL_DISTANCE", config_.cullDistance);
    config_.maxVisibleEnemies = parseUInt16Env("TPS_MAX_VISIBLE_ENEMIES", config_.maxVisibleEnemies, 16U, 4096U);
    config_.shadowCasterBudget = parseUInt16Env("TPS_SHADOW_CASTER_BUDGET", config_.shadowCasterBudget, 1U, 1024U);
    config_.overlayEveryNFrames =
        static_cast<std::uint8_t>(parseUInt16Env("TPS_OVERLAY_EVERY_N_FRAMES", config_.overlayEveryNFrames, 1U, 60U));
    config_.targetFrameMs = parseFloatEnv("TPS_TARGET_FRAME_MS", config_.targetFrameMs);

    renderGraphReady_ = false;
    renderGraphErrorLogged_ = false;
    pendingGpuPasses_.clear();
    lastResolvedGpuMs_.clear();

    frameCount_ = 0;
    terminalPrintCounter_ = 0;

    enemies_.clear();
    enemies_.reserve(2048);
    visibleEnemyIndices_.clear();
    visibleEnemyIndices_.reserve(2048);
    shadowCasterIndices_.clear();
    shadowCasterIndices_.reserve(256);

    frameBuffer_.clear();
    resetDiagnostics();
}

void Renderer::beginFrame() noexcept {
    if (rhiDevice_) {
        rhiDevice_->beginFrame();
        
        std::uint32_t w, h;
        rhiDevice_->getSwapchainExtent(w, h);
        if (w > 0 && h > 0) {
            config_.viewportWidth = static_cast<std::uint16_t>(w);
            config_.viewportHeight = static_cast<std::uint16_t>(h);
        }
    }

    enemies_.clear();
    visibleEnemyIndices_.clear();
    shadowCasterIndices_.clear();
    resetDiagnostics();
    diagnostics_.rhiBackend = rhiDevice_ ? rhiDevice_->backendName() : "none";
}

void Renderer::submitPlayer(const Vec3& position,
                            int health,
                            std::uint32_t shotsFired,
                            std::uint32_t kills,
                            std::uint32_t enemiesAlive,
                            bool victory,
                            bool defeat) noexcept {
    player_.position = position;
    player_.health = health;
    player_.shotsFired = shotsFired;
    player_.kills = kills;
    player_.enemiesAlive = enemiesAlive;
    player_.victory = victory;
    player_.defeat = defeat;
}

void Renderer::submitEnemy(const Vec3& position, std::uint16_t health) noexcept {
    enemies_.push_back(EnemyProxy{position, health});
}

void Renderer::setCameraMatrices(const Mat4& view, const Mat4& proj) noexcept {
    camera_.view = view;
    camera_.proj = proj;
}

void Renderer::submitCamera(float yaw, float pitch, float armLength, const Vec3& position, bool aimMode) noexcept {
    camera_.yaw = yaw;
    camera_.pitch = pitch;
    camera_.armLength = armLength;
    camera_.position = position;
    camera_.aimMode = aimMode;
}

void Renderer::render() noexcept {
    if (!renderGraphReady_) {
        renderGraphReady_ = buildRenderGraph();
    }

    if (!renderGraphReady_) {
        runPass("visibility", true, [this](PassCost& pass) { passVisibility(pass); });
        runPass("depth_prepass", config_.enableDepthPrepass, [this](PassCost& pass) { passDepthPrepass(pass); });
        runPass("shadow", config_.enableShadowPass, [this](PassCost& pass) { passShadow(pass); });
        runPass("lighting", true, [this](PassCost& pass) { passLighting(pass); });
        runPass("volumetric_fog", config_.enableVolumetricFog, [this](PassCost& pass) { passFog(pass); });
        runPass("transparent", true, [this](PassCost& pass) { passTransparent(pass); });
        runPass("post", config_.enablePost, [this](PassCost& pass) { passPost(pass); });
        drawAsciiFrame();
        return;
    }

    for (std::size_t nodeIndex : renderGraph_.executionOrder()) {
        executeRenderGraphPass(nodeIndex);
    }

    drawAsciiFrame();
}

void Renderer::present() noexcept {
    if (rhiDevice_) {
        rhiDevice_->endFrame();
    }

    resolveGpuPassTimes();
    ++frameCount_;
    printDiagnosticsOverlay();
}

void Renderer::cleanup() noexcept {
    if (rhiDevice_) {
        rhiDevice_->shutdown();
        rhiDevice_.reset();
    }

    frameCount_ = 0;
    terminalPrintCounter_ = 0;
    enemies_.clear();
    visibleEnemyIndices_.clear();
    shadowCasterIndices_.clear();
    frameBuffer_.clear();
    pendingGpuPasses_.clear();
    lastResolvedGpuMs_.clear();
    renderGraphReady_ = false;
    renderGraphErrorLogged_ = false;
    resetDiagnostics();
}

std::size_t Renderer::frameCount() const noexcept {
    return frameCount_;
}

const FrameDiagnostics& Renderer::lastFrameDiagnostics() const noexcept {
    return diagnostics_;
}

bool Renderer::buildRenderGraph() noexcept {
    std::vector<RenderPassNodeDesc> nodes;
    nodes.reserve(7);

    nodes.push_back(RenderPassNodeDesc{
        "visibility",
        true,
        {},
        {RenderResourceUsage{"enemy_instances", RenderResourceAccess::Read, RenderResourceState::ShaderResource},
         RenderResourceUsage{"visible_list", RenderResourceAccess::Write, RenderResourceState::UnorderedAccess}}});
    nodes.push_back(RenderPassNodeDesc{
        "depth_prepass",
        config_.enableDepthPrepass,
        {"visibility"},
        {RenderResourceUsage{"visible_list", RenderResourceAccess::Read, RenderResourceState::ShaderResource},
         RenderResourceUsage{"depth", RenderResourceAccess::Write, RenderResourceState::DepthStencilTarget}}});
    nodes.push_back(RenderPassNodeDesc{
        "shadow",
        config_.enableShadowPass,
        {"visibility"},
        {RenderResourceUsage{"visible_list", RenderResourceAccess::Read, RenderResourceState::ShaderResource},
         RenderResourceUsage{"shadow_map", RenderResourceAccess::Write, RenderResourceState::DepthStencilTarget}}});
    nodes.push_back(RenderPassNodeDesc{
        "lighting",
        true,
        {"visibility", "depth_prepass", "shadow"},
        {RenderResourceUsage{"visible_list", RenderResourceAccess::Read, RenderResourceState::ShaderResource},
         RenderResourceUsage{"depth", RenderResourceAccess::Read, RenderResourceState::DepthStencilRead},
         RenderResourceUsage{"shadow_map", RenderResourceAccess::Read, RenderResourceState::ShaderResource},
         RenderResourceUsage{"hdr_color", RenderResourceAccess::Write, RenderResourceState::RenderTarget}}});
    nodes.push_back(RenderPassNodeDesc{
        "volumetric_fog",
        config_.enableVolumetricFog,
        {"lighting"},
        {RenderResourceUsage{"depth", RenderResourceAccess::Read, RenderResourceState::DepthStencilRead},
         RenderResourceUsage{"hdr_color", RenderResourceAccess::ReadWrite, RenderResourceState::RenderTarget}}});
    nodes.push_back(RenderPassNodeDesc{
        "transparent",
        true,
        {"lighting"},
        {RenderResourceUsage{"depth", RenderResourceAccess::Read, RenderResourceState::DepthStencilRead},
         RenderResourceUsage{"hdr_color", RenderResourceAccess::ReadWrite, RenderResourceState::RenderTarget}}});
    nodes.push_back(RenderPassNodeDesc{
        "post",
        config_.enablePost,
        {"transparent", "volumetric_fog"},
        {RenderResourceUsage{"hdr_color", RenderResourceAccess::Read, RenderResourceState::ShaderResource, false},
         RenderResourceUsage{"backbuffer", RenderResourceAccess::Write, RenderResourceState::Present, true}}});

    if (!renderGraph_.build(std::move(nodes))) {
        if (!renderGraphErrorLogged_) {
            std::cerr << "[Renderer] Render graph disabled: " << renderGraph_.lastError() << '\n';
            renderGraphErrorLogged_ = true;
        }
        return false;
    }

    {
        std::ofstream dotFile("render_graph.dot");
        if (dotFile.is_open()) {
            dotFile << renderGraph_.emitDebugGraphviz();
        }
    }

    const std::vector<CompiledRenderPass>& compiledPasses = renderGraph_.compiledPasses();
    std::cout << "[Renderer] Render graph compiled with " << compiledPasses.size() << " active passes.\n";
    for (const CompiledRenderPass& pass : compiledPasses) {
        if (pass.prePassBarriers.empty()) {
            continue;
        }

        for (const RenderGraphBarrier& barrier : pass.prePassBarriers) {
            std::cout << "[Renderer][RG] pass=" << pass.name << " barrier " << barrier.resourceName
                      << " " << resourceStateName(barrier.stateBefore) << " -> "
                      << resourceStateName(barrier.stateAfter) << '\n';
        }
    }

    return true;
}

void Renderer::executeRenderGraphPass(std::size_t nodeIndex) noexcept {
    if (nodeIndex >= renderGraph_.nodes().size()) {
        return;
    }

    const RenderPassNodeDesc& node = renderGraph_.nodes()[nodeIndex];
    const std::string_view name = node.name != nullptr ? std::string_view(node.name) : std::string_view{};
    if (name.empty()) {
        return;
    }

    if (name == "visibility") {
        runPass("visibility", true, [this](PassCost& pass) { passVisibility(pass); });
        return;
    }

    if (name == "depth_prepass") {
        runPass("depth_prepass", config_.enableDepthPrepass, [this](PassCost& pass) { passDepthPrepass(pass); });
        return;
    }

    if (name == "shadow") {
        runPass("shadow", config_.enableShadowPass, [this](PassCost& pass) { passShadow(pass); });
        return;
    }

    if (name == "lighting") {
        runPass("lighting", true, [this](PassCost& pass) { passLighting(pass); });
        return;
    }

    if (name == "volumetric_fog") {
        runPass("volumetric_fog", config_.enableVolumetricFog, [this](PassCost& pass) { passFog(pass); });
        return;
    }

    if (name == "transparent") {
        runPass("transparent", true, [this](PassCost& pass) { passTransparent(pass); });
        return;
    }

    if (name == "post") {
        runPass("post", config_.enablePost, [this](PassCost& pass) { passPost(pass); });
    }
}

template <typename Fn>
void Renderer::runPass(const char* name, bool enabled, Fn&& fn) noexcept {
    if (diagnostics_.passCount >= diagnostics_.passes.size()) {
        return;
    }

    PassCost& pass = diagnostics_.passes[diagnostics_.passCount++];
    pass = PassCost{};
    pass.name = name;

    if (!enabled) {
        return;
    }

    GpuTimestampToken gpuScope{};
    if (rhiDevice_ && rhiDevice_->supportsGpuTimestamps()) {
        gpuScope = rhiDevice_->beginTimestampScope(name);
        pass.gpuToken = gpuScope;
    }

    const auto start = Clock::now();
    fn(pass);
    const auto end = Clock::now();

    if (rhiDevice_ && rhiDevice_->supportsGpuTimestamps()) {
        rhiDevice_->endTimestampScope(gpuScope);
    }

    pass.cpuMs = std::chrono::duration<double, std::milli>(end - start).count();
    pass.executed = true;
    diagnostics_.totalCpuMs += pass.cpuMs;
}

void Renderer::passVisibility(PassCost& pass) noexcept {
    const float cullDistanceSq = config_.cullDistance * config_.cullDistance;

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(enemies_.size()); ++i) {
        const float distSq = distanceSquared(enemies_[i].position, player_.position);
        if (distSq <= cullDistanceSq) {
            if (visibleEnemyIndices_.size() >= static_cast<std::size_t>(config_.maxVisibleEnemies)) {
                diagnostics_.visibilityClamped = true;
                break;
            }
            visibleEnemyIndices_.push_back(i);
        }
    }

    diagnostics_.submittedEnemies = static_cast<std::uint32_t>(enemies_.size());
    diagnostics_.visibleEnemies = static_cast<std::uint32_t>(visibleEnemyIndices_.size());
    diagnostics_.culledEnemies = diagnostics_.submittedEnemies - diagnostics_.visibleEnemies;

    pass.workItems = diagnostics_.submittedEnemies;
    pass.estimatedBytes =
        static_cast<std::uint64_t>(diagnostics_.submittedEnemies) * static_cast<std::uint64_t>(sizeof(EnemyProxy)) +
        static_cast<std::uint64_t>(diagnostics_.visibleEnemies) * sizeof(std::uint32_t);
}

void Renderer::passDepthPrepass(PassCost& pass) noexcept {
    const std::uint32_t visible = diagnostics_.visibleEnemies;
    const std::uint32_t pixelsPerObject = 120U;
    const std::uint32_t depthPixels = visible * pixelsPerObject;

    diagnostics_.estimatedShadedPixels += depthPixels;

    pass.workItems = visible;
    pass.estimatedBytes = static_cast<std::uint64_t>(depthPixels) * kDepthBytesPerPixel;
}

void Renderer::passShadow(PassCost& pass) noexcept {
    shadowCasterIndices_.clear();

    const float shadowDistance = config_.cullDistance * 0.65f;
    const float shadowDistanceSq = shadowDistance * shadowDistance;
    const std::size_t shadowCasterBudget = static_cast<std::size_t>(config_.shadowCasterBudget);

    for (std::uint32_t index : visibleEnemyIndices_) {
        if (shadowCasterIndices_.size() >= shadowCasterBudget) {
            break;
        }

        const float distSq = distanceSquared(enemies_[index].position, player_.position);
        if (distSq <= shadowDistanceSq) {
            shadowCasterIndices_.push_back(index);
        }
    }

    diagnostics_.shadowCasters = static_cast<std::uint32_t>(shadowCasterIndices_.size());

    pass.workItems = diagnostics_.shadowCasters;
    pass.estimatedBytes = static_cast<std::uint64_t>(diagnostics_.shadowCasters) * 16ULL * 1024ULL;
}

void Renderer::passLighting(PassCost& pass) noexcept {
    const std::uint32_t visible = diagnostics_.visibleEnemies;
    const std::uint32_t screenPixels =
        static_cast<std::uint32_t>(config_.viewportWidth) * static_cast<std::uint32_t>(config_.viewportHeight);

    const bool manyLightsLikely = visible > 52U;
    diagnostics_.usedDeferredLighting = config_.forceDeferredLighting || (manyLightsLikely && config_.msaaSamples == 1U);

    if (diagnostics_.usedDeferredLighting) {
        diagnostics_.estimatedShadedPixels += screenPixels;
        diagnostics_.estimatedOverdrawPixels += screenPixels / 2U;

        pass.workItems = visible;
        pass.estimatedBytes = static_cast<std::uint64_t>(screenPixels) *
                                  (kCompactGBufferBytesPerPixel + kColorBytesPerPixel) +
                              static_cast<std::uint64_t>(visible) * 64ULL;
        return;
    }

    const std::uint32_t forwardPixels = visible * 160U + (screenPixels / 6U);
    diagnostics_.estimatedShadedPixels += forwardPixels;
    diagnostics_.estimatedOverdrawPixels += visible * 35U;

    const std::uint32_t msaaFactor = (config_.msaaSamples > 1U) ? static_cast<std::uint32_t>(config_.msaaSamples) : 1U;

    pass.workItems = visible * (2U + msaaFactor);
    pass.estimatedBytes = static_cast<std::uint64_t>(forwardPixels) * kColorBytesPerPixel +
                          static_cast<std::uint64_t>(visible) * 48ULL;
}

void Renderer::passFog(PassCost& pass) noexcept {
    const std::uint32_t quarterResPixels =
        (static_cast<std::uint32_t>(config_.viewportWidth) * static_cast<std::uint32_t>(config_.viewportHeight)) / 4U;

    pass.workItems = quarterResPixels;
    pass.estimatedBytes = static_cast<std::uint64_t>(quarterResPixels) * 4ULL;
}

void Renderer::passTransparent(PassCost& pass) noexcept {
    const std::uint32_t transparentSprites = diagnostics_.visibleEnemies / 3U;
    const std::uint32_t pixels = transparentSprites * 90U;

    diagnostics_.estimatedShadedPixels += pixels;
    diagnostics_.estimatedOverdrawPixels += pixels;

    pass.workItems = transparentSprites;
    pass.estimatedBytes = static_cast<std::uint64_t>(pixels) * kColorBytesPerPixel;
}

void Renderer::passPost(PassCost& pass) noexcept {
    std::uint32_t pixels = static_cast<std::uint32_t>(config_.viewportWidth) *
                           static_cast<std::uint32_t>(config_.viewportHeight);

    if (config_.enableSSAO) {
        const std::uint32_t halfRes = pixels / 2U;
        pass.workItems += halfRes;
        pass.estimatedBytes += static_cast<std::uint64_t>(halfRes) * 6ULL;
    }

    if (config_.msaaSamples > 1U) {
        pixels *= static_cast<std::uint32_t>(config_.msaaSamples);
    }

    pass.workItems += pixels;
    pass.estimatedBytes += static_cast<std::uint64_t>(pixels) * 3ULL;
}

void Renderer::drawAsciiFrame() noexcept {
    const std::size_t width = static_cast<std::size_t>(config_.viewportWidth);
    const std::size_t height = static_cast<std::size_t>(config_.viewportHeight);
    const std::size_t stride = width + 1U;

    frameBuffer_.assign(stride * height, ' ');

    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            frameBuffer_[y * stride + x] = '.';
        }
        frameBuffer_[y * stride + width] = '\n';
    }

    const int centerX = static_cast<int>(width / 2U);
    const int centerY = static_cast<int>(height / 2U);

    auto drawGlyph = [this, stride, width, height](int gx, int gy, char glyph) {
        if (gx < 0 || gy < 0) {
            return;
        }

        const std::size_t x = static_cast<std::size_t>(gx);
        const std::size_t y = static_cast<std::size_t>(gy);
        if (x >= width || y >= height) {
            return;
        }

        frameBuffer_[y * stride + x] = glyph;
    };

    for (const EnemyProxy& enemy : enemies_) {
        const Vec3 delta = enemy.position - player_.position;
        const int gx = centerX + static_cast<int>(std::round(delta.x / config_.worldUnitsPerCell));
        const int gy = centerY - static_cast<int>(std::round(delta.y / config_.worldUnitsPerCell));
        drawGlyph(gx, gy, 'e');
    }

    for (std::uint32_t index : visibleEnemyIndices_) {
        const Vec3 delta = enemies_[index].position - player_.position;
        const int gx = centerX + static_cast<int>(std::round(delta.x / config_.worldUnitsPerCell));
        const int gy = centerY - static_cast<int>(std::round(delta.y / config_.worldUnitsPerCell));
        drawGlyph(gx, gy, 'E');
    }

    for (std::uint32_t index : shadowCasterIndices_) {
        const Vec3 delta = enemies_[index].position - player_.position;
        const int gx = centerX + static_cast<int>(std::round(delta.x / config_.worldUnitsPerCell));
        const int gy = centerY - static_cast<int>(std::round(delta.y / config_.worldUnitsPerCell));
        drawGlyph(gx, gy, 'S');
    }

    drawGlyph(centerX, centerY, 'P');
}

void Renderer::resolveGpuPassTimes() noexcept {
    constexpr std::size_t kPendingGpuFramesTtl = 32U;

    if (!rhiDevice_ || !rhiDevice_->supportsGpuTimestamps()) {
        pendingGpuPasses_.clear();
        return;
    }

    std::size_t pendingWrite = 0U;
    for (std::size_t i = 0; i < pendingGpuPasses_.size(); ++i) {
        PendingGpuPass& pending = pendingGpuPasses_[i];
        double resolvedMs = 0.0;
        const bool resolved = rhiDevice_->resolveTimestampScopeMs(pending.token, resolvedMs);
        const bool expired = (frameCount_ - pending.enqueuedFrame) > kPendingGpuFramesTtl;

        if (resolved) {
            lastResolvedGpuMs_[pending.passName] = resolvedMs;
            continue;
        }

        if (expired) {
            continue;
        }

        if (pendingWrite != i) {
            pendingGpuPasses_[pendingWrite] = pendingGpuPasses_[i];
        }
        ++pendingWrite;
    }
    pendingGpuPasses_.resize(pendingWrite);

    for (std::size_t i = 0; i < diagnostics_.passCount; ++i) {
        PassCost& pass = diagnostics_.passes[i];
        if (!pass.executed || pass.gpuToken.index == GpuTimestampToken::kInvalidIndex) {
            continue;
        }

        double resolvedMs = 0.0;
        const bool resolved = rhiDevice_->resolveTimestampScopeMs(pass.gpuToken, resolvedMs);
        if (resolved) {
            pass.hasGpuTimestamp = true;
            pass.gpuTimestampStale = false;
            pass.gpuMs = resolvedMs;
            lastResolvedGpuMs_[pass.name] = resolvedMs;
            continue;
        }

        pendingGpuPasses_.push_back(PendingGpuPass{std::string(pass.name), pass.gpuToken, frameCount_});

        const auto cached = lastResolvedGpuMs_.find(pass.name);
        if (cached != lastResolvedGpuMs_.end()) {
            pass.hasGpuTimestamp = true;
            pass.gpuTimestampStale = true;
            pass.gpuMs = cached->second;
        }
    }
}

void Renderer::printDiagnosticsOverlay() noexcept {
    diagnostics_.overCpuBudget = diagnostics_.totalCpuMs > static_cast<double>(config_.targetFrameMs);

    if (!config_.enableOverlay) {
        return;
    }

    ++terminalPrintCounter_;
    if ((terminalPrintCounter_ % static_cast<std::size_t>(config_.overlayEveryNFrames)) != 0U) {
        return;
    }

    std::cout << "\x1b[2J\x1b[H";
    std::cout << "TPS Engine Prototype | Controls: WASD move, SPACE shoot, Q quit\n";
    std::cout << "Frame " << frameCount_ << " | HP " << player_.health << " | Shots " << player_.shotsFired
              << " | Kills " << player_.kills << " | Alive " << player_.enemiesAlive;

    if (player_.victory) {
        std::cout << " | STATE: VICTORY";
    } else if (player_.defeat) {
        std::cout << " | STATE: DEFEAT";
    }
    std::cout << "\n";

    std::cout << frameBuffer_;

    std::cout << "Camera: yaw=" << camera_.yaw << " pitch=" << camera_.pitch
              << " armLen=" << camera_.armLength
              << " pos=(" << camera_.position.x << ", " << camera_.position.y << ", " << camera_.position.z << ")"
              << " aim=" << (camera_.aimMode ? "ON" : "OFF") << "\n";

    std::cout << "Visibility: submitted=" << diagnostics_.submittedEnemies << " visible=" << diagnostics_.visibleEnemies
              << " culled=" << diagnostics_.culledEnemies << " shadow_casters=" << diagnostics_.shadowCasters
              << "\n";
    std::cout << "Estimated shaded px=" << diagnostics_.estimatedShadedPixels
              << " overdraw px=" << diagnostics_.estimatedOverdrawPixels
              << " lighting_path=" << (diagnostics_.usedDeferredLighting ? "deferred-lite" : "forward+")
              << " msaa=" << static_cast<int>(config_.msaaSamples) << "x"
              << " rhi=" << diagnostics_.rhiBackend << "\n";
    std::cout << "CPU frame ms=" << diagnostics_.totalCpuMs
              << " target=" << config_.targetFrameMs
              << " budget=" << (diagnostics_.overCpuBudget ? "OVER" : "OK");
    if (diagnostics_.visibilityClamped) {
        std::cout << " visibility_cap=HIT";
    }
    std::cout << "\n";

    std::cout << std::fixed << std::setprecision(3);
    for (std::size_t i = 0; i < diagnostics_.passCount; ++i) {
        const PassCost& pass = diagnostics_.passes[i];
        if (!pass.executed) {
            continue;
        }

        std::cout << "  pass " << pass.name << " cpu_ms=" << pass.cpuMs;
        if (pass.hasGpuTimestamp) {
            std::cout << " gpu_ms=" << pass.gpuMs;
            if (pass.gpuTimestampStale) {
                std::cout << "(stale)";
            }
        }
        std::cout << " work=" << pass.workItems << " est_bytes=" << pass.estimatedBytes << "\n";
    }

    std::cout.flush();
}

void Renderer::resetDiagnostics() noexcept {
    diagnostics_ = FrameDiagnostics{};
}
