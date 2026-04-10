#include "engine/rhi/IRhiDevice.h"

#include <cstdlib>
#include <string_view>
#include <vector>

#if defined(TPS_HAS_VULKAN)
#include <vulkan/vulkan.h>
#endif

namespace {
class NullRhiDevice final : public IRhiDevice {
public:
    bool initialize() noexcept override {
        frameActive_ = false;
        return true;
    }

    void shutdown() noexcept override {
        frameActive_ = false;
    }

    void beginFrame() noexcept override {
        frameActive_ = true;
    }

    void endFrame() noexcept override {
        frameActive_ = false;
    }

    const char* backendName() const noexcept override {
        return "null";
    }

    bool supportsGpuTimestamps() const noexcept override {
        return false;
    }

    GpuTimestampToken beginTimestampScope(const char* label) noexcept override {
        (void)label;
        return GpuTimestampToken{};
    }

    void endTimestampScope(GpuTimestampToken token) noexcept override {
        (void)token;
    }

    bool resolveTimestampScopeMs(GpuTimestampToken token, double& outMs) noexcept override {
        (void)token;
        outMs = 0.0;
        return false;
    }

private:
    bool frameActive_ = false;
};

#if defined(TPS_HAS_VULKAN)
class VulkanRhiDevice final : public IRhiDevice {
public:
    bool initialize() noexcept override {
        shutdown();

        if (!createInstance()) {
            shutdown();
            return false;
        }

        if (!pickPhysicalDevice()) {
            shutdown();
            return false;
        }

        if (!createLogicalDevice()) {
            shutdown();
            return false;
        }

        if (!createCommandObjects()) {
            shutdown();
            return false;
        }

        if (!createQueryPool()) {
            shutdown();
            return false;
        }

        initialized_ = true;
        return true;
    }

    void shutdown() noexcept override {
        initialized_ = false;
        frameActive_ = false;
        scopes_.clear();

        if (device_ != VK_NULL_HANDLE) {
            (void)vkDeviceWaitIdle(device_);
        }

        if (queryPool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device_, queryPool_, nullptr);
            queryPool_ = VK_NULL_HANDLE;
        }

        if (frameFence_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            vkDestroyFence(device_, frameFence_, nullptr);
            frameFence_ = VK_NULL_HANDLE;
        }

        if (commandPool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
            commandPool_ = VK_NULL_HANDLE;
            commandBuffer_ = VK_NULL_HANDLE;
        }

        if (device_ != VK_NULL_HANDLE) {
            vkDestroyDevice(device_, nullptr);
            device_ = VK_NULL_HANDLE;
        }

        if (instance_ != VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }

        physicalDevice_ = VK_NULL_HANDLE;
        queue_ = VK_NULL_HANDLE;
        queueFamilyIndex_ = 0;
        timestampPeriodNs_ = 1.0;
        nextQuery_ = 0;
        frameSubmitted_ = false;
    }

    void beginFrame() noexcept override {
        if (!initialized_) {
            return;
        }

        if (!waitForPreviousFrame()) {
            return;
        }

        if (vkResetFences(device_, 1U, &frameFence_) != VK_SUCCESS) {
            return;
        }

        if (vkResetCommandPool(device_, commandPool_, 0U) != VK_SUCCESS) {
            return;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer_, &beginInfo) != VK_SUCCESS) {
            return;
        }

        vkCmdResetQueryPool(commandBuffer_, queryPool_, 0U, kMaxTimestampQueries);

        scopes_.clear();
        nextQuery_ = 0;
        frameSubmitted_ = false;
        frameActive_ = true;
    }

    void endFrame() noexcept override {
        if (!initialized_ || !frameActive_) {
            return;
        }

        frameActive_ = false;

        if (vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) {
            return;
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1U;
        submitInfo.pCommandBuffers = &commandBuffer_;

        if (vkQueueSubmit(queue_, 1U, &submitInfo, frameFence_) != VK_SUCCESS) {
            return;
        }

        frameSubmitted_ = true;
        (void)waitForPreviousFrame();
    }

    const char* backendName() const noexcept override {
        return "vulkan";
    }

    bool supportsGpuTimestamps() const noexcept override {
        return initialized_;
    }

    GpuTimestampToken beginTimestampScope(const char* label) noexcept override {
        (void)label;

        if (!initialized_ || !frameActive_) {
            return GpuTimestampToken{};
        }

        if ((nextQuery_ + 1U) >= kMaxTimestampQueries) {
            return GpuTimestampToken{};
        }

        Scope scope{};
        scope.beginQuery = nextQuery_;
        scope.endQuery = nextQuery_ + 1U;

        vkCmdWriteTimestamp(
            commandBuffer_,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            queryPool_,
            scope.beginQuery);

        nextQuery_ += 2U;
        scopes_.push_back(scope);

        GpuTimestampToken token{};
        token.index = static_cast<std::uint32_t>(scopes_.size() - 1U);
        return token;
    }

    void endTimestampScope(GpuTimestampToken token) noexcept override {
        if (!initialized_ || !frameActive_) {
            return;
        }

        if (token.index >= scopes_.size()) {
            return;
        }

        Scope& scope = scopes_[token.index];
        if (scope.ended) {
            return;
        }

        vkCmdWriteTimestamp(
            commandBuffer_,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            queryPool_,
            scope.endQuery);

        scope.ended = true;
    }

    bool resolveTimestampScopeMs(GpuTimestampToken token, double& outMs) noexcept override {
        outMs = 0.0;

        if (!initialized_ || token.index >= scopes_.size()) {
            return false;
        }

        Scope& scope = scopes_[token.index];
        if (scope.resolved) {
            outMs = scope.ms;
            return true;
        }

        if (!frameSubmitted_) {
            return false;
        }

        if (!resolveScopes()) {
            return false;
        }

        if (!scope.resolved) {
            return false;
        }

        outMs = scope.ms;
        return true;
    }

private:
    struct Scope {
        std::uint32_t beginQuery = 0;
        std::uint32_t endQuery = 0;
        bool ended = false;
        bool resolved = false;
        double ms = 0.0;
    };

    static constexpr std::uint32_t kMaxTimestampQueries = 8192U;

    bool createInstance() noexcept {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "TPS_Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(0U, 1U, 0U);
        appInfo.pEngineName = "TPS_Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(0U, 1U, 0U);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;

        return vkCreateInstance(&instanceInfo, nullptr, &instance_) == VK_SUCCESS;
    }

    bool pickPhysicalDevice() noexcept {
        std::uint32_t deviceCount = 0;
        if (vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0U) {
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        if (vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data()) != VK_SUCCESS) {
            return false;
        }

        for (VkPhysicalDevice candidate : devices) {
            std::uint32_t familyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
            if (familyCount == 0U) {
                continue;
            }

            std::vector<VkQueueFamilyProperties> families(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

            for (std::uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex) {
                const VkQueueFamilyProperties& props = families[familyIndex];
                const bool supportsTimestamp = props.timestampValidBits > 0U;
                const bool supportsQueue = (props.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0U;
                if (!supportsTimestamp || !supportsQueue) {
                    continue;
                }

                VkPhysicalDeviceProperties physicalProps{};
                vkGetPhysicalDeviceProperties(candidate, &physicalProps);

                physicalDevice_ = candidate;
                queueFamilyIndex_ = familyIndex;
                timestampPeriodNs_ = static_cast<double>(physicalProps.limits.timestampPeriod);
                return true;
            }
        }

        return false;
    }

    bool createLogicalDevice() noexcept {
        const float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIndex_;
        queueInfo.queueCount = 1U;
        queueInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1U;
        deviceInfo.pQueueCreateInfos = &queueInfo;

        if (vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_) != VK_SUCCESS) {
            return false;
        }

        vkGetDeviceQueue(device_, queueFamilyIndex_, 0U, &queue_);
        return queue_ != VK_NULL_HANDLE;
    }

    bool createCommandObjects() noexcept {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndex_;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1U;

        if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer_) != VK_SUCCESS) {
            return false;
        }

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        return vkCreateFence(device_, &fenceInfo, nullptr, &frameFence_) == VK_SUCCESS;
    }

    bool createQueryPool() noexcept {
        VkQueryPoolCreateInfo queryInfo{};
        queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryInfo.queryCount = kMaxTimestampQueries;

        return vkCreateQueryPool(device_, &queryInfo, nullptr, &queryPool_) == VK_SUCCESS;
    }

    bool waitForPreviousFrame() noexcept {
        if (frameFence_ == VK_NULL_HANDLE) {
            return false;
        }

        constexpr std::uint64_t kFenceTimeoutNs = 1'000'000'000ULL;
        const VkResult waitResult = vkWaitForFences(device_, 1U, &frameFence_, VK_TRUE, kFenceTimeoutNs);
        return waitResult == VK_SUCCESS;
    }

    bool resolveScopes() noexcept {
        if (nextQuery_ == 0U) {
            return true;
        }

        const std::size_t resultCount = static_cast<std::size_t>(nextQuery_);
        std::vector<std::uint64_t> queryData(resultCount, 0U);

        const VkResult result = vkGetQueryPoolResults(
            device_,
            queryPool_,
            0U,
            nextQuery_,
            queryData.size() * sizeof(std::uint64_t),
            queryData.data(),
            sizeof(std::uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        if (result != VK_SUCCESS) {
            return false;
        }

        for (Scope& scope : scopes_) {
            if (!scope.ended) {
                continue;
            }

            const std::size_t beginIndex = static_cast<std::size_t>(scope.beginQuery);
            const std::size_t endIndex = static_cast<std::size_t>(scope.endQuery);
            if (endIndex >= queryData.size() || beginIndex >= queryData.size()) {
                continue;
            }

            const std::uint64_t beginTicks = queryData[beginIndex];
            const std::uint64_t endTicks = queryData[endIndex];
            if (endTicks < beginTicks) {
                continue;
            }

            const double deltaTicks = static_cast<double>(endTicks - beginTicks);
            const double deltaNs = deltaTicks * timestampPeriodNs_;
            scope.ms = deltaNs * 1e-6;
            scope.resolved = true;
        }

        return true;
    }

    bool initialized_ = false;
    bool frameActive_ = false;
    bool frameSubmitted_ = false;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    std::uint32_t queueFamilyIndex_ = 0;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
    VkFence frameFence_ = VK_NULL_HANDLE;
    VkQueryPool queryPool_ = VK_NULL_HANDLE;

    double timestampPeriodNs_ = 1.0;

    std::vector<Scope> scopes_;
    std::uint32_t nextQuery_ = 0;
};
#else
class VulkanRhiDevice final : public IRhiDevice {
public:
    bool initialize() noexcept override {
        return true;
    }

    void shutdown() noexcept override {
    }

    void beginFrame() noexcept override {
    }

    void endFrame() noexcept override {
    }

    const char* backendName() const noexcept override {
        return "vulkan_stub";
    }

    bool supportsGpuTimestamps() const noexcept override {
        return false;
    }

    GpuTimestampToken beginTimestampScope(const char* label) noexcept override {
        (void)label;
        return GpuTimestampToken{};
    }

    void endTimestampScope(GpuTimestampToken token) noexcept override {
        (void)token;
    }

    bool resolveTimestampScopeMs(GpuTimestampToken token, double& outMs) noexcept override {
        (void)token;
        outMs = 0.0;
        return false;
    }
};
#endif

RhiBackend parseRhiBackend(const char* value) noexcept {
    if (value == nullptr || value[0] == '\0') {
        return RhiBackend::Null;
    }

    const std::string_view backend(value);
    if (backend == "v" || backend == "vk" || backend == "vulkan" || backend == "vulkan_stub") {
        return RhiBackend::VulkanStub;
    }

    return RhiBackend::Null;
}
}  // namespace

std::unique_ptr<IRhiDevice> createRhiDevice(RhiBackend backend) noexcept {
    switch (backend) {
        case RhiBackend::VulkanStub:
            return std::make_unique<VulkanRhiDevice>();
        case RhiBackend::Null:
        default:
            return std::make_unique<NullRhiDevice>();
    }
}

std::unique_ptr<IRhiDevice> createRhiDeviceFromEnvironment() noexcept {
    const char* value = std::getenv("TPS_RHI_BACKEND");
    return createRhiDevice(parseRhiBackend(value));
}
