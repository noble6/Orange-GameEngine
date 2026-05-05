#include "engine/graphics/AssetManager.h"
#include "engine/graphics/TextureLoader.h"
#include "engine/graphics/MeshLoader.h"

#include <iostream>
#include <cstring>

void AssetManager::initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue) noexcept {
    device_ = device;
    physicalDevice_ = physicalDevice;
    commandPool_ = commandPool;
    graphicsQueue_ = graphicsQueue;
}

void AssetManager::shutdown() noexcept {
    for (auto& mesh : meshes_) {
        if (mesh.vbo != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, mesh.vbo, nullptr);
            vkFreeMemory(device_, mesh.vboMemory, nullptr);
        }
        if (mesh.ibo != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, mesh.ibo, nullptr);
            vkFreeMemory(device_, mesh.iboMemory, nullptr);
        }
    }
    meshes_.clear();
    meshCache_.clear();

    for (auto& tex : textures_) {
        if (tex.sampler != VK_NULL_HANDLE) vkDestroySampler(device_, tex.sampler, nullptr);
        if (tex.view != VK_NULL_HANDLE) vkDestroyImageView(device_, tex.view, nullptr);
        if (tex.image != VK_NULL_HANDLE) {
            vkDestroyImage(device_, tex.image, nullptr);
            vkFreeMemory(device_, tex.memory, nullptr);
        }
    }
    textures_.clear();
    textureCache_.clear();

    device_ = VK_NULL_HANDLE;
    physicalDevice_ = VK_NULL_HANDLE;
    commandPool_ = VK_NULL_HANDLE;
    graphicsQueue_ = VK_NULL_HANDLE;
}

uint32_t AssetManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

bool AssetManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, VkBuffer& outBuffer, VkDeviceMemory& outMemory) noexcept {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, outBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, memProps);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &outMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, outBuffer, nullptr);
        outBuffer = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindBufferMemory(device_, outBuffer, outMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(device_, outMemory, nullptr);
        vkDestroyBuffer(device_, outBuffer, nullptr);
        outBuffer = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

VkCommandBuffer AssetManager::beginSingleTimeCommands() noexcept {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void AssetManager::endSingleTimeCommandsAndWait(VkCommandBuffer commandBuffer) noexcept {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    VkFence fence;
    vkCreateFence(device_, &fenceInfo, nullptr, &fence);

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, fence);
    vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(device_, fence, nullptr);
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

TextureHandle AssetManager::loadTexture(const char* path) noexcept {
    auto it = textureCache_.find(path);
    if (it != textureCache_.end()) {
        return it->second;
    }
    return loadTextureFromFile(path, *this);
}

MeshHandle AssetManager::loadMesh(const char* path) noexcept {
    auto it = meshCache_.find(path);
    if (it != meshCache_.end()) {
        return it->second;
    }

    std::string pathStr = path;
    MeshData meshData;
    if (pathStr.length() >= 5 && pathStr.substr(pathStr.length() - 5) == ".gltf") {
        meshData = loadMeshGLTF(path);
    } else {
        meshData = loadMeshOBJ(path);
    }
    
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        return InvalidMeshHandle;
    }

    VkDeviceSize vertexSize = sizeof(meshData.vertices[0]) * meshData.vertices.size();
    VkDeviceSize indexSize = sizeof(meshData.indices[0]) * meshData.indices.size();
    VkDeviceSize totalSize = vertexSize + indexSize;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    if (!createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingMemory)) {
        return InvalidMeshHandle;
    }

    void* data = nullptr;
    vkMapMemory(device_, stagingMemory, 0, totalSize, 0, &data);
    memcpy(data, meshData.vertices.data(), vertexSize);
    memcpy(static_cast<uint8_t*>(data) + vertexSize, meshData.indices.data(), indexSize);
    vkUnmapMemory(device_, stagingMemory);

    VkBuffer vbo = VK_NULL_HANDLE;
    VkDeviceMemory vboMemory = VK_NULL_HANDLE;
    if (!createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vbo, vboMemory)) {
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);
        return InvalidMeshHandle;
    }

    VkBuffer ibo = VK_NULL_HANDLE;
    VkDeviceMemory iboMemory = VK_NULL_HANDLE;
    if (!createBuffer(indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ibo, iboMemory)) {
        vkDestroyBuffer(device_, vbo, nullptr);
        vkFreeMemory(device_, vboMemory, nullptr);
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);
        return InvalidMeshHandle;
    }

    VkCommandBuffer cmd = beginSingleTimeCommands();
    
    VkBufferCopy copyRegionVbo{};
    copyRegionVbo.srcOffset = 0;
    copyRegionVbo.dstOffset = 0;
    copyRegionVbo.size = vertexSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, vbo, 1, &copyRegionVbo);

    VkBufferCopy copyRegionIbo{};
    copyRegionIbo.srcOffset = vertexSize;
    copyRegionIbo.dstOffset = 0;
    copyRegionIbo.size = indexSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, ibo, 1, &copyRegionIbo);

    endSingleTimeCommandsAndWait(cmd);

    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);

    Mesh resource;
    resource.vbo = vbo;
    resource.vboMemory = vboMemory;
    resource.ibo = ibo;
    resource.iboMemory = iboMemory;
    resource.indexCount = static_cast<std::uint32_t>(meshData.indices.size());

    stats.meshVramBytes += totalSize;
    stats.stagingPeakBytes = std::max(stats.stagingPeakBytes, totalSize);

    return registerMesh(path, std::move(resource));
}

TextureHandle AssetManager::registerTexture(const char* path, Texture&& resource) noexcept {
    TextureHandle handle = static_cast<TextureHandle>(textures_.size());
    textures_.push_back(std::move(resource));
    textureCache_[path] = handle;
    return handle;
}

MeshHandle AssetManager::registerMesh(const char* path, Mesh&& resource) noexcept {
    MeshHandle handle = static_cast<MeshHandle>(meshes_.size());
    meshes_.push_back(std::move(resource));
    meshCache_[path] = handle;
    return handle;
}
