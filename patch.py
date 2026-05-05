import re

with open('engine/rhi/IRhiDevice.cpp', 'r') as f:
    content = f.read()

vulkan_class_old = re.search(r'class VulkanRhiDevice final : public IRhiDevice \{.*?\n};\n', content, re.DOTALL).group(0)

vulkan_class_new = """class VulkanRhiDevice final : public IRhiDevice {
public:
    bool initialize() noexcept override {
        shutdown();

        if (!createInstance()) { shutdown(); return false; }
        if (!pickPhysicalDevice()) { shutdown(); return false; }
        if (!createLogicalDevice()) { shutdown(); return false; }

        for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (!createFrameContext(frames_[i])) {
                shutdown();
                return false;
            }
        }

        currentFrameSerial_ = 0;
        currentFrameIndex_ = 0;
        initialized_ = true;
        return true;
    }

    void shutdown() noexcept override {
        initialized_ = false;
        frameActive_ = false;

        completedFrames_.clear();

        if (device_ != VK_NULL_HANDLE) {
            (void)vkDeviceWaitIdle(device_);
        }

        for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            destroyFrameContext(frames_[i]);
        }

        if (device_ != VK_NULL_HANDLE) {
            vkDestroyDevice(device_, nullptr);
            device_ = VK_NULL_HANDLE;
        }

        destroyDebugMessenger();

        if (instance_ != VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }

        physicalDevice_ = VK_NULL_HANDLE;
        queue_ = VK_NULL_HANDLE;
        queueFamilyIndex_ = 0;
        timestampPeriodNs_ = 1.0;
        gpuTimestampsSupported_ = false;
        currentFrameSerial_ = 0;
        currentFrameIndex_ = 0;
        validationEnabled_ = false;
    }

    void beginFrame() noexcept override {
        if (!initialized_) return;

        FrameContext& frame = frames_[currentFrameIndex_];

        if (!waitForFrame(frame)) return;

        if (frame.frameSubmitted) {
            (void)resolveScopes(frame);
            cacheCompletedFrame(frame);
        }

        if (vkResetFences(device_, 1U, &frame.frameFence) != VK_SUCCESS) return;
        if (vkResetCommandPool(device_, frame.commandPool, 0U) != VK_SUCCESS) return;

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS) return;

        if (frame.queryPool != VK_NULL_HANDLE) {
            vkCmdResetQueryPool(frame.commandBuffer, frame.queryPool, 0U, kMaxTimestampQueries);
        }

        frame.scopes.clear();
        frame.nextQuery = 0;
        frame.frameSubmitted = false;
        frame.frameSerial = ++currentFrameSerial_;
        frameActive_ = true;
    }

    void endFrame() noexcept override {
        if (!initialized_ || !frameActive_) return;

        frameActive_ = false;
        FrameContext& frame = frames_[currentFrameIndex_];

        if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS) return;

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1U;
        submitInfo.pCommandBuffers = &frame.commandBuffer;

        if (vkQueueSubmit(queue_, 1U, &submitInfo, frame.frameFence) != VK_SUCCESS) return;

        frame.frameSubmitted = true;
        currentFrameIndex_ = (currentFrameIndex_ + 1U) % kMaxFramesInFlight;
    }

    const char* backendName() const noexcept override { return "vulkan"; }

    bool supportsGpuTimestamps() const noexcept override {
        return initialized_ && gpuTimestampsSupported_;
    }

    GpuTimestampToken beginTimestampScope(const char* label) noexcept override {
        (void)label;
        if (!initialized_ || !frameActive_ || !gpuTimestampsSupported_) return GpuTimestampToken{};

        FrameContext& frame = frames_[currentFrameIndex_];
        if (frame.queryPool == VK_NULL_HANDLE || (frame.nextQuery + 1U) >= kMaxTimestampQueries) {
            return GpuTimestampToken{};
        }

        Scope scope{};
        scope.beginQuery = frame.nextQuery;
        scope.endQuery = frame.nextQuery + 1U;

        vkCmdWriteTimestamp(frame.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPool, scope.beginQuery);

        frame.nextQuery += 2U;
        frame.scopes.push_back(scope);

        GpuTimestampToken token{};
        token.frameSerial = frame.frameSerial;
        token.index = static_cast<std::uint32_t>(frame.scopes.size() - 1U);
        return token;
    }

    void endTimestampScope(GpuTimestampToken token) noexcept override {
        if (!initialized_ || !frameActive_ || !gpuTimestampsSupported_) return;

        FrameContext& frame = frames_[currentFrameIndex_];
        if (frame.queryPool == VK_NULL_HANDLE || token.frameSerial != frame.frameSerial || token.index >= frame.scopes.size()) return;

        Scope& scope = frame.scopes[token.index];
        if (scope.ended) return;

        vkCmdWriteTimestamp(frame.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPool, scope.endQuery);
        scope.ended = true;
    }

    bool resolveTimestampScopeMs(GpuTimestampToken token, double& outMs) noexcept override {
        outMs = 0.0;
        if (!initialized_ || !gpuTimestampsSupported_ || token.index == GpuTimestampToken::kInvalidIndex) return false;

        if (tryResolveFromCompletedFrames(token, outMs)) return true;

        const FrameContext& currentFrame = frames_[currentFrameIndex_];
        if (token.frameSerial == currentFrame.frameSerial) return false;

        for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            FrameContext& frame = frames_[i];
            if (frame.frameSerial == token.frameSerial && frame.frameSubmitted && token.index < frame.scopes.size()) {
                if (!resolveScopes(frame)) return false;
                const Scope& scope = frame.scopes[token.index];
                if (scope.resolved) {
                    outMs = scope.ms;
                    return true;
                }
            }
        }
        return false;
    }

private:
    struct Scope {
        std::uint32_t beginQuery = 0;
        std::uint32_t endQuery = 0;
        bool ended = false;
        bool resolved = false;
        double ms = 0.0;
    };

    struct FrameContext {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFence frameFence = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;

        std::vector<Scope> scopes;
        std::uint32_t nextQuery = 0;
        bool frameSubmitted = false;
        std::uint32_t frameSerial = 0;
    };

    struct CompletedFrame {
        std::uint32_t frameSerial = 0;
        std::vector<double> scopeMs;
        std::vector<bool> scopeResolved;
    };

    static constexpr std::uint32_t kMaxTimestampQueries = 8192U;
    static constexpr std::size_t kCompletedFrameHistory = 8U;
    static constexpr std::uint32_t kMaxFramesInFlight = 2U;

    bool createInstance() noexcept {
        const bool requestValidation = shouldEnableValidationLayers();

        std::vector<const char*> layers;
        std::vector<const char*> extensions;

        if (requestValidation) {
            if (hasInstanceLayer("VK_LAYER_KHRONOS_validation")) {
                layers.push_back("VK_LAYER_KHRONOS_validation");
                validationEnabled_ = true;

                if (hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                } else {
                    std::cerr << "[RHI][Vulkan] Validation requested but VK_EXT_debug_utils is unavailable.\\n";
                }
            } else {
                std::cerr << "[RHI][Vulkan] Validation requested but VK_LAYER_KHRONOS_validation is unavailable.\\n";
            }
        }

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
        instanceInfo.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        instanceInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
        instanceInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        instanceInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

        const VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance_);
        if (result != VK_SUCCESS) {
            std::cerr << "[RHI][Vulkan] vkCreateInstance failed: " << vkResultToString(result) << '\\n';
            return false;
        }

        if (validationEnabled_ && !extensions.empty() && !createDebugMessenger()) {
            std::cerr << "[RHI][Vulkan] Validation layer enabled, but debug messenger setup failed.\\n";
        }

        if (validationEnabled_) {
            std::cerr << "[RHI][Vulkan] Validation enabled (TPS_VK_VALIDATION).\\n";
        }

        return true;
    }

    bool hasInstanceLayer(const char* layerName) const noexcept {
        std::uint32_t layerCount = 0U;
        if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS || layerCount == 0U) return false;
        std::vector<VkLayerProperties> properties(layerCount);
        if (vkEnumerateInstanceLayerProperties(&layerCount, properties.data()) != VK_SUCCESS) return false;
        for (const VkLayerProperties& property : properties) {
            if (std::string_view(property.layerName) == layerName) return true;
        }
        return false;
    }

    bool hasInstanceExtension(const char* extensionName) const noexcept {
        std::uint32_t extensionCount = 0U;
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) != VK_SUCCESS || extensionCount == 0U) return false;
        std::vector<VkExtensionProperties> properties(extensionCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, properties.data()) != VK_SUCCESS) return false;
        for (const VkExtensionProperties& property : properties) {
            if (std::string_view(property.extensionName) == extensionName) return true;
        }
        return false;
    }

    bool createDebugMessenger() noexcept {
        if (instance_ == VK_NULL_HANDLE) return false;
        auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (createFn == nullptr) return false;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = vkDebugMessageCallback;

        const VkResult result = createFn(instance_, &createInfo, nullptr, &debugMessenger_);
        if (result != VK_SUCCESS) { debugMessenger_ = VK_NULL_HANDLE; return false; }
        return true;
    }

    void destroyDebugMessenger() noexcept {
        if (instance_ == VK_NULL_HANDLE || debugMessenger_ == VK_NULL_HANDLE) return;
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn != nullptr) destroyFn(instance_, debugMessenger_, nullptr);
        debugMessenger_ = VK_NULL_HANDLE;
    }

    bool pickPhysicalDevice() noexcept {
        std::uint32_t deviceCount = 0;
        const VkResult countResult = vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
        if (countResult != VK_SUCCESS || deviceCount == 0U) return false;

        std::vector<VkPhysicalDevice> devices(deviceCount);
        const VkResult listResult = vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
        if (listResult != VK_SUCCESS) return false;

        VkPhysicalDevice fallbackDevice = VK_NULL_HANDLE;
        std::uint32_t fallbackQueueFamilyIndex = 0U;
        double fallbackTimestampPeriodNs = 1.0;

        for (VkPhysicalDevice candidate : devices) {
            std::uint32_t familyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
            if (familyCount == 0U) continue;

            std::vector<VkQueueFamilyProperties> families(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

            for (std::uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex) {
                const VkQueueFamilyProperties& props = families[familyIndex];
                const bool supportsTimestamp = props.timestampValidBits > 0U;
                const bool supportsQueue = (props.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0U;
                if (!supportsQueue) continue;

                VkPhysicalDeviceProperties physicalProps{};
                vkGetPhysicalDeviceProperties(candidate, &physicalProps);

                if (supportsTimestamp) {
                    physicalDevice_ = candidate;
                    queueFamilyIndex_ = familyIndex;
                    timestampPeriodNs_ = static_cast<double>(physicalProps.limits.timestampPeriod);
                    gpuTimestampsSupported_ = true;
                    return true;
                }

                if (fallbackDevice == VK_NULL_HANDLE) {
                    fallbackDevice = candidate;
                    fallbackQueueFamilyIndex = familyIndex;
                    fallbackTimestampPeriodNs = static_cast<double>(physicalProps.limits.timestampPeriod);
                }
            }
        }

        if (fallbackDevice != VK_NULL_HANDLE) {
            physicalDevice_ = fallbackDevice;
            queueFamilyIndex_ = fallbackQueueFamilyIndex;
            timestampPeriodNs_ = fallbackTimestampPeriodNs;
            gpuTimestampsSupported_ = false;
            std::cerr << "[RHI][Vulkan] Selected device queue without timestamp support; GPU pass timings disabled.\\n";
            return true;
        }

        std::cerr << "[RHI][Vulkan] No suitable graphics/compute queue family found.\\n";
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

        const VkResult createDeviceResult = vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_);
        if (createDeviceResult != VK_SUCCESS) return false;

        vkGetDeviceQueue(device_, queueFamilyIndex_, 0U, &queue_);
        return queue_ != VK_NULL_HANDLE;
    }

    bool createFrameContext(FrameContext& frame) noexcept {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndex_;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &frame.commandPool) != VK_SUCCESS) return false;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = frame.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1U;

        if (vkAllocateCommandBuffers(device_, &allocInfo, &frame.commandBuffer) != VK_SUCCESS) return false;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateFence(device_, &fenceInfo, nullptr, &frame.frameFence) != VK_SUCCESS) return false;

        if (gpuTimestampsSupported_) {
            VkQueryPoolCreateInfo queryInfo{};
            queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryInfo.queryCount = kMaxTimestampQueries;

            if (vkCreateQueryPool(device_, &queryInfo, nullptr, &frame.queryPool) != VK_SUCCESS) return false;
        }

        return true;
    }

    void destroyFrameContext(FrameContext& frame) noexcept {
        if (frame.queryPool != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device_, frame.queryPool, nullptr);
            frame.queryPool = VK_NULL_HANDLE;
        }
        if (frame.frameFence != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            vkDestroyFence(device_, frame.frameFence, nullptr);
            frame.frameFence = VK_NULL_HANDLE;
        }
        if (frame.commandPool != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, frame.commandPool, nullptr);
            frame.commandPool = VK_NULL_HANDLE;
            frame.commandBuffer = VK_NULL_HANDLE;
        }
    }

    bool waitForFrame(FrameContext& frame) noexcept {
        if (frame.frameFence == VK_NULL_HANDLE) return false;
        if (!frame.frameSubmitted) return true;

        constexpr std::uint64_t kFenceTimeoutNs = 1'000'000'000ULL;
        const VkResult waitResult = vkWaitForFences(device_, 1U, &frame.frameFence, VK_TRUE, kFenceTimeoutNs);
        return waitResult == VK_SUCCESS;
    }

    bool resolveScopes(FrameContext& frame) noexcept {
        if (frame.nextQuery == 0U) return true;

        for (Scope& scope : frame.scopes) {
            if (!scope.ended || scope.resolved) continue;

            const std::array<std::uint64_t, 4> kUnavailable = {0U, 0U, 0U, 0U};
            std::array<std::uint64_t, 4> data = kUnavailable;

            const VkResult result = vkGetQueryPoolResults(
                device_, frame.queryPool, scope.beginQuery, 2U,
                sizeof(data), data.data(), sizeof(std::uint64_t) * 2U,
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

            if (result == VK_NOT_READY) continue;
            if (result != VK_SUCCESS) return false;

            if (data[1] == 0U || data[3] == 0U) continue;

            const std::uint64_t beginTicks = data[0];
            const std::uint64_t endTicks = data[2];
            if (endTicks < beginTicks) continue;

            const double deltaTicks = static_cast<double>(endTicks - beginTicks);
            const double deltaNs = deltaTicks * timestampPeriodNs_;
            scope.ms = deltaNs * 1e-6;
            scope.resolved = true;
        }
        return true;
    }

    void cacheCompletedFrame(FrameContext& frame) noexcept {
        if (!frame.frameSubmitted) return;

        CompletedFrame completed{};
        completed.frameSerial = frame.frameSerial;
        completed.scopeMs.assign(frame.scopes.size(), 0.0);
        completed.scopeResolved.assign(frame.scopes.size(), false);

        for (std::size_t i = 0; i < frame.scopes.size(); ++i) {
            const Scope& scope = frame.scopes[i];
            if (!scope.resolved) continue;
            completed.scopeMs[i] = scope.ms;
            completed.scopeResolved[i] = true;
        }

        completedFrames_.push_back(std::move(completed));
        if (completedFrames_.size() > kCompletedFrameHistory) {
            completedFrames_.erase(completedFrames_.begin());
        }
    }

    bool tryResolveFromCompletedFrames(GpuTimestampToken token, double& outMs) const noexcept {
        for (auto it = completedFrames_.rbegin(); it != completedFrames_.rend(); ++it) {
            if (it->frameSerial != token.frameSerial) continue;
            const std::size_t index = static_cast<std::size_t>(token.index);
            if (index >= it->scopeResolved.size() || !it->scopeResolved[index]) return false;
            outMs = it->scopeMs[index];
            return true;
        }
        return false;
    }

    bool initialized_ = false;
    bool frameActive_ = false;
    bool gpuTimestampsSupported_ = false;
    bool validationEnabled_ = false;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    std::uint32_t queueFamilyIndex_ = 0;

    double timestampPeriodNs_ = 1.0;

    std::array<FrameContext, kMaxFramesInFlight> frames_;
    std::vector<CompletedFrame> completedFrames_;
    std::uint32_t currentFrameSerial_ = 0;
    std::uint32_t currentFrameIndex_ = 0;
};
"""

with open('engine/rhi/IRhiDevice.cpp', 'w') as f:
    f.write(content.replace(vulkan_class_old, vulkan_class_new))
