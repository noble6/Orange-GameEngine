#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

enum class RenderResourceAccess : unsigned char {
    Read = 0,
    Write = 1,
    ReadWrite = 2,
};

enum class RenderResourceState : unsigned char {
    Undefined = 0,
    RenderTarget = 1,
    DepthStencilTarget = 2,
    DepthStencilRead = 3,
    ShaderResource = 4,
    UnorderedAccess = 5,
    TransferSrc = 6,
    TransferDst = 7,
    Present = 8
};

struct RenderResourceUsage {
    const char* name = "";
    RenderResourceAccess access = RenderResourceAccess::Read;
    RenderResourceState requiredState = RenderResourceState::Undefined;
    bool persistent = false;
};

struct RenderPassNodeDesc {
    const char* name = "";
    bool enabled = true;
    std::vector<const char*> dependencies;
    std::vector<RenderResourceUsage> resources;
};

struct RenderGraphBarrier {
    std::string resourceName;
    RenderResourceState stateBefore;
    RenderResourceState stateAfter;
};

struct CompiledRenderPass {
    std::string name;
    std::size_t originalIndex;
    std::vector<RenderGraphBarrier> prePassBarriers;
};

class RenderGraph {
public:
    bool build(std::vector<RenderPassNodeDesc> passNodes) noexcept;

    const std::vector<RenderPassNodeDesc>& nodes() const noexcept;
    const std::vector<std::size_t>& executionOrder() const noexcept;
    const std::vector<CompiledRenderPass>& compiledPasses() const noexcept;
    const std::string& lastError() const noexcept;

    std::string emitDebugGraphviz() const noexcept;

private:
    bool validateDependencies() noexcept;
    bool computeExecutionOrder() noexcept;
    bool validateResourceHazards() noexcept;
    void synthesizeBarriers() noexcept;
    bool hasDependencyPath(std::size_t from, std::size_t to) const noexcept;

    std::vector<RenderPassNodeDesc> nodes_;
    std::vector<std::vector<std::size_t>> adjacency_;
    std::vector<std::size_t> executionOrder_;
    std::vector<CompiledRenderPass> compiledPasses_;
    std::unordered_map<std::string, RenderResourceState> persistentResourceStates_;
    std::string lastError_;
};
