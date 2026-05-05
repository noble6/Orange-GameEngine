#include "engine/graphics/RenderGraph.h"

#include <cstdint>
#include <queue>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace {
bool isWriteAccess(RenderResourceAccess access) noexcept {
    return access == RenderResourceAccess::Write || access == RenderResourceAccess::ReadWrite;
}
}  // namespace

bool RenderGraph::build(std::vector<RenderPassNodeDesc> passNodes) noexcept {
    nodes_ = std::move(passNodes);
    adjacency_.clear();
    executionOrder_.clear();
    compiledPasses_.clear();
    lastError_.clear();

    if (nodes_.empty()) {
        return true;
    }

    if (!validateDependencies()) {
        return false;
    }

    if (!computeExecutionOrder()) {
        return false;
    }

    if (!validateResourceHazards()) {
        return false;
    }

    synthesizeBarriers();

    return true;
}

const std::vector<CompiledRenderPass>& RenderGraph::compiledPasses() const noexcept {
    return compiledPasses_;
}

const std::vector<RenderPassNodeDesc>& RenderGraph::nodes() const noexcept {
    return nodes_;
}

const std::vector<std::size_t>& RenderGraph::executionOrder() const noexcept {
    return executionOrder_;
}

const std::string& RenderGraph::lastError() const noexcept {
    return lastError_;
}

std::string RenderGraph::emitDebugGraphviz() const noexcept {
    std::ostringstream oss;
    oss << "digraph RenderGraph {\n";
    oss << "  rankdir=LR;\n";
    oss << "  node [shape=box, fontname=\"Courier\"];\n";

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        const char* color = node.enabled ? "black" : "gray";
        oss << "  pass" << i << " [label=\"" << (node.name ? node.name : "unnamed") 
            << "\", color=" << color << ", fontcolor=" << color << "];\n";
    }

    for (std::size_t i = 0; i < adjacency_.size(); ++i) {
        for (std::size_t target : adjacency_[i]) {
            oss << "  pass" << i << " -> pass" << target << ";\n";
        }
    }

    // Add resource dependencies as well
    std::unordered_map<std::string, std::vector<std::size_t>> resourceReaders;
    std::unordered_map<std::string, std::vector<std::size_t>> resourceWriters;

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        for (const auto& usage : nodes_[i].resources) {
            if (usage.name && usage.name[0] != '\0') {
                if (isWriteAccess(usage.access)) {
                    resourceWriters[usage.name].push_back(i);
                } else {
                    resourceReaders[usage.name].push_back(i);
                }
            }
        }
    }

    for (const auto& [name, writers] : resourceWriters) {
        oss << "  res_" << name << " [label=\"" << name << "\", shape=ellipse, color=blue, fontcolor=blue];\n";
        for (std::size_t writer : writers) {
            oss << "  pass" << writer << " -> res_" << name << " [color=blue];\n";
        }
        if (resourceReaders.count(name)) {
            for (std::size_t reader : resourceReaders.at(name)) {
                oss << "  res_" << name << " -> pass" << reader << " [color=green];\n";
            }
        }
    }

    oss << "}\n";
    return oss.str();
}

bool RenderGraph::validateDependencies() noexcept {
    std::unordered_map<std::string_view, std::size_t> nameToIndex;
    nameToIndex.reserve(nodes_.size());
    adjacency_.assign(nodes_.size(), {});

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const std::string_view name(nodes_[i].name != nullptr ? nodes_[i].name : "");
        if (name.empty()) {
            lastError_ = "RenderGraph node has empty name.";
            return false;
        }

        const auto inserted = nameToIndex.emplace(name, i);
        if (!inserted.second) {
            lastError_ = "RenderGraph duplicate pass name: " + std::string(name);
            return false;
        }
    }

    for (std::size_t passIndex = 0; passIndex < nodes_.size(); ++passIndex) {
        const RenderPassNodeDesc& node = nodes_[passIndex];
        for (const char* dependencyNameCStr : node.dependencies) {
            const std::string_view dependencyName(dependencyNameCStr != nullptr ? dependencyNameCStr : "");
            if (dependencyName.empty()) {
                lastError_ = "RenderGraph dependency name cannot be empty.";
                return false;
            }

            const auto it = nameToIndex.find(dependencyName);
            if (it == nameToIndex.end()) {
                lastError_ = "RenderGraph missing dependency '" + std::string(dependencyName) + "' for pass '" +
                             std::string(node.name != nullptr ? node.name : "") + "'.";
                return false;
            }

            adjacency_[it->second].push_back(passIndex);
        }
    }

    return true;
}

bool RenderGraph::computeExecutionOrder() noexcept {
    executionOrder_.clear();
    executionOrder_.reserve(nodes_.size());

    std::vector<std::uint32_t> indegree(nodes_.size(), 0U);
    for (const std::vector<std::size_t>& edges : adjacency_) {
        for (std::size_t target : edges) {
            if (target < indegree.size()) {
                ++indegree[target];
            }
        }
    }

    std::queue<std::size_t> ready;
    for (std::size_t i = 0; i < indegree.size(); ++i) {
        if (indegree[i] == 0U) {
            ready.push(i);
        }
    }

    while (!ready.empty()) {
        const std::size_t node = ready.front();
        ready.pop();
        executionOrder_.push_back(node);

        for (std::size_t next : adjacency_[node]) {
            if (next >= indegree.size()) {
                continue;
            }

            if (indegree[next] > 0U) {
                --indegree[next];
            }
            if (indegree[next] == 0U) {
                ready.push(next);
            }
        }
    }

    if (executionOrder_.size() != nodes_.size()) {
        lastError_ = "RenderGraph dependency cycle detected.";
        return false;
    }

    return true;
}

bool RenderGraph::validateResourceHazards() noexcept {
    for (std::size_t orderI = 0; orderI < executionOrder_.size(); ++orderI) {
        const std::size_t passI = executionOrder_[orderI];
        if (passI >= nodes_.size()) {
            continue;
        }

        const RenderPassNodeDesc& nodeI = nodes_[passI];
        if (!nodeI.enabled) {
            continue;
        }

        for (std::size_t orderJ = orderI + 1U; orderJ < executionOrder_.size(); ++orderJ) {
            const std::size_t passJ = executionOrder_[orderJ];
            if (passJ >= nodes_.size()) {
                continue;
            }

            const RenderPassNodeDesc& nodeJ = nodes_[passJ];
            if (!nodeJ.enabled) {
                continue;
            }

            for (const RenderResourceUsage& resourceI : nodeI.resources) {
                const std::string_view resourceIName(resourceI.name != nullptr ? resourceI.name : "");
                if (resourceIName.empty()) {
                    continue;
                }

                for (const RenderResourceUsage& resourceJ : nodeJ.resources) {
                    const std::string_view resourceJName(resourceJ.name != nullptr ? resourceJ.name : "");
                    if (resourceJName.empty() || resourceIName != resourceJName) {
                        continue;
                    }

                    if (!isWriteAccess(resourceI.access) && !isWriteAccess(resourceJ.access)) {
                        continue;
                    }

                    if (hasDependencyPath(passI, passJ)) {
                        continue;
                    }

                    std::ostringstream oss;
                    oss << "RenderGraph resource hazard: '" << (nodeI.name != nullptr ? nodeI.name : "")
                        << "' and '" << (nodeJ.name != nullptr ? nodeJ.name : "") << "' both access '"
                        << resourceIName << "' without an explicit dependency path.";
                    lastError_ = oss.str();
                    return false;
                }
            }
        }
    }

    return true;
}

void RenderGraph::synthesizeBarriers() noexcept {
    compiledPasses_.clear();
    compiledPasses_.reserve(executionOrder_.size());

    // Track the running state of each physical resource during execution order
    std::unordered_map<std::string, RenderResourceState> currentStates = persistentResourceStates_;

    for (std::size_t passIndex : executionOrder_) {
        const RenderPassNodeDesc& node = nodes_[passIndex];
        if (!node.enabled) {
            continue;
        }

        CompiledRenderPass compiled;
        compiled.name = node.name ? node.name : "UnnamedPass";
        compiled.originalIndex = passIndex;

        // Determine required barriers before pass execution
        for (const RenderResourceUsage& usage : node.resources) {
            const std::string name(usage.name ? usage.name : "");
            if (name.empty() || usage.requiredState == RenderResourceState::Undefined) {
                continue;
            }

            auto it = currentStates.find(name);
            const RenderResourceState currentState = (it != currentStates.end()) ? it->second : RenderResourceState::Undefined;

            // Synthesis condition: state mismatch requires explicit transition barrier
            if (currentState != usage.requiredState) {
                RenderGraphBarrier barrier;
                barrier.resourceName = name;
                barrier.stateBefore = currentState;
                barrier.stateAfter = usage.requiredState;
                compiled.prePassBarriers.push_back(std::move(barrier));

                // Update the current state of the resource
                currentStates[name] = usage.requiredState;
                
                if (usage.persistent) {
                    persistentResourceStates_[name] = usage.requiredState;
                }
            }
        }
        
        compiledPasses_.push_back(std::move(compiled));
    }
}

bool RenderGraph::hasDependencyPath(std::size_t from, std::size_t to) const noexcept {
    if (from == to || from >= adjacency_.size() || to >= adjacency_.size()) {
        return from == to;
    }

    std::vector<bool> visited(adjacency_.size(), false);
    std::queue<std::size_t> queue;
    queue.push(from);
    visited[from] = true;

    while (!queue.empty()) {
        const std::size_t node = queue.front();
        queue.pop();

        for (std::size_t next : adjacency_[node]) {
            if (next >= visited.size() || visited[next]) {
                continue;
            }
            if (next == to) {
                return true;
            }
            visited[next] = true;
            queue.push(next);
        }
    }

    return false;
}
