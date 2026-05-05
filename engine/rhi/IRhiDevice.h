#pragma once

#include <cstdint>
#include <memory>

struct GpuTimestampToken {
    static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFu;
    std::uint32_t frameSerial = 0;
    std::uint32_t index = kInvalidIndex;
};

enum class RhiBackend : std::uint8_t {
    Null = 0,
    VulkanStub,
};

class IRhiDevice {
public:
    virtual ~IRhiDevice() = default;

    virtual bool initialize(void* windowHandle = nullptr) noexcept = 0;
    virtual void shutdown() noexcept = 0;

    virtual void beginFrame() noexcept = 0;
    virtual void endFrame() noexcept = 0;

    virtual const char* backendName() const noexcept = 0;
    virtual bool supportsGpuTimestamps() const noexcept = 0;

    virtual void getSwapchainExtent(std::uint32_t& width, std::uint32_t& height) const noexcept = 0;
    virtual std::uint32_t getSwapchainFormat() const noexcept = 0;

    virtual GpuTimestampToken beginTimestampScope(const char* label) noexcept = 0;
    virtual void endTimestampScope(GpuTimestampToken token) noexcept = 0;
    virtual bool resolveTimestampScopeMs(GpuTimestampToken token, double& outMs) noexcept = 0;
};

std::unique_ptr<IRhiDevice> createRhiDevice(RhiBackend backend) noexcept;
std::unique_ptr<IRhiDevice> createRhiDeviceFromEnvironment() noexcept;
