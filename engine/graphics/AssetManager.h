#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "engine/core/Math.h"

struct Vec2 { float x = 0.0f; float y = 0.0f; };
struct Vec4 { float x = 0.0f; float y = 0.0f; float z = 0.0f; float w = 0.0f; };

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;
    Vec4 tangent;

    bool operator==(const Vertex& other) const {
        return position.x == other.position.x && position.y == other.position.y && position.z == other.position.z &&
               normal.x == other.normal.x && normal.y == other.normal.y && normal.z == other.normal.z &&
               texcoord.x == other.texcoord.x && texcoord.y == other.texcoord.y;
    }
};

struct VertexHasher {
    std::size_t operator()(const Vertex& v) const noexcept {
        return std::hash<float>()(v.position.x) ^ (std::hash<float>()(v.position.y) << 1);
    }
};

struct VertexEqual {
    bool operator()(const Vertex& a, const Vertex& b) const noexcept {
        return a == b;
    }
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

using TextureHandle = std::uint32_t;
using MeshHandle = std::uint32_t;

constexpr TextureHandle InvalidTextureHandle = 0xFFFFFFFF;
constexpr MeshHandle InvalidMeshHandle = 0xFFFFFFFF;

struct Texture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};

struct Mesh {
    VkBuffer vbo = VK_NULL_HANDLE;
    VkDeviceMemory vboMemory = VK_NULL_HANDLE;
    VkBuffer ibo = VK_NULL_HANDLE;
    VkDeviceMemory iboMemory = VK_NULL_HANDLE;
    std::uint32_t indexCount = 0;
};

struct AssetStats {
    uint64_t textureVramBytes = 0;
    uint64_t meshVramBytes = 0;
    uint64_t stagingPeakBytes = 0;
};

class AssetManager {
public:
    AssetManager() = default;
    void initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue) noexcept;
    void shutdown() noexcept;

    MeshHandle loadMesh(const char* path) noexcept;
    TextureHandle loadTexture(const char* path) noexcept;

    TextureHandle registerTexture(const char* path, Texture&& resource) noexcept;
    MeshHandle registerMesh(const char* path, Mesh&& resource) noexcept;

    VkDevice getDevice() const noexcept { return device_; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, VkBuffer& outBuffer, VkDeviceMemory& outMemory) noexcept;
    VkCommandBuffer beginSingleTimeCommands() noexcept;
    void endSingleTimeCommandsAndWait(VkCommandBuffer commandBuffer) noexcept;

    AssetStats stats;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;

    std::vector<Texture> textures_;
    std::unordered_map<std::string, TextureHandle> textureCache_;

    std::vector<Mesh> meshes_;
    std::unordered_map<std::string, MeshHandle> meshCache_;
};
