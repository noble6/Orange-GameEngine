#include "engine/graphics/TextureLoader.h"
#include "engine/graphics/AssetManager.h"

#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
struct OgtHeader {
    char magic[4];
    uint32_t width;
    uint32_t height;
    uint32_t mipCount;
    uint32_t format;
};

bool isPowerOfTwo(uint32_t x) noexcept {
    return (x > 0) && ((x & (x - 1)) == 0);
}
} // namespace

TextureHandle loadTextureFromFile(const char* path, AssetManager& mgr) noexcept {
    if (!path) return InvalidTextureHandle;

    FILE* file = fopen(path, "rb");
    if (!file) return InvalidTextureHandle;

    OgtHeader header;
    if (fread(&header, sizeof(OgtHeader), 1, file) != 1) {
        fclose(file);
        return InvalidTextureHandle;
    }

    if (strncmp(header.magic, "OGT1", 4) != 0 || header.width == 0 || header.height == 0 || 
        !isPowerOfTwo(header.width) || !isPowerOfTwo(header.height) || header.mipCount < 1) {
        fclose(file);
        return InvalidTextureHandle;
    }

    VkFormat vkFormat = VK_FORMAT_UNDEFINED;
    uint32_t bytesPerBlock = 0;
    if (header.format == 4) {
        vkFormat = VK_FORMAT_BC4_UNORM_BLOCK;
        bytesPerBlock = 8;
    } else if (header.format == 5) {
        vkFormat = VK_FORMAT_BC5_UNORM_BLOCK;
        bytesPerBlock = 16;
    } else if (header.format == 7) {
        vkFormat = VK_FORMAT_BC7_UNORM_BLOCK;
        bytesPerBlock = 16;
    } else {
        fclose(file);
        return InvalidTextureHandle;
    }

    std::vector<uint32_t> mipOffsets(header.mipCount);
    uint32_t totalCompressedBytes = 0;

    for (uint32_t i = 0; i < header.mipCount; ++i) {
        uint32_t mipW = std::max(1u, header.width >> i);
        uint32_t mipH = std::max(1u, header.height >> i);
        uint32_t blocksX = (mipW + 3) / 4;
        uint32_t blocksY = (mipH + 3) / 4;
        
        mipOffsets[i] = totalCompressedBytes;
        totalCompressedBytes += blocksX * blocksY * bytesPerBlock;
    }

    std::vector<uint8_t> stagingData(totalCompressedBytes);
    if (fread(stagingData.data(), 1, totalCompressedBytes, file) != totalCompressedBytes) {
        fclose(file);
        return InvalidTextureHandle;
    }
    fclose(file);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkFormat;
    imageInfo.extent.width = header.width;
    imageInfo.extent.height = header.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = header.mipCount;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkDevice device = mgr.getDevice();
    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        return InvalidTextureHandle;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = mgr.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        return InvalidTextureHandle;
    }
    vkBindImageMemory(device, image, imageMemory, 0);

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!mgr.createBuffer(totalCompressedBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          stagingBuffer, stagingMemory)) {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        return InvalidTextureHandle;
    }

    void* dataMap = nullptr;
    vkMapMemory(device, stagingMemory, 0, totalCompressedBytes, 0, &dataMap);
    memcpy(dataMap, stagingData.data(), totalCompressedBytes);
    vkUnmapMemory(device, stagingMemory);

    VkCommandBuffer cmd = mgr.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier1{};
    barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier1.image = image;
    barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier1.subresourceRange.baseMipLevel = 0;
    barrier1.subresourceRange.levelCount = header.mipCount;
    barrier1.subresourceRange.baseArrayLayer = 0;
    barrier1.subresourceRange.layerCount = 1;
    barrier1.srcAccessMask = 0;
    barrier1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &barrier1);

    std::vector<VkBufferImageCopy> regions(header.mipCount);
    for (uint32_t i = 0; i < header.mipCount; ++i) {
        regions[i].bufferOffset = mipOffsets[i];
        regions[i].bufferRowLength = 0;
        regions[i].bufferImageHeight = 0;
        regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel = i;
        regions[i].imageSubresource.baseArrayLayer = 0;
        regions[i].imageSubresource.layerCount = 1;
        regions[i].imageOffset = {0, 0, 0};
        regions[i].imageExtent = {
            std::max(1u, header.width >> i),
            std::max(1u, header.height >> i),
            1
        };
    }

    vkCmdCopyBufferToImage(cmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(regions.size()), regions.data());

    VkImageMemoryBarrier barrier2{};
    barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.image = image;
    barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier2.subresourceRange.baseMipLevel = 0;
    barrier2.subresourceRange.levelCount = header.mipCount;
    barrier2.subresourceRange.baseArrayLayer = 0;
    barrier2.subresourceRange.layerCount = 1;
    barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &barrier2);

    mgr.endSingleTimeCommandsAndWait(cmd);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = header.mipCount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        return InvalidTextureHandle;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(header.mipCount);

    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        vkDestroyImageView(device, imageView, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        return InvalidTextureHandle;
    }

    mgr.stats.textureVramBytes += totalCompressedBytes;
    mgr.stats.stagingPeakBytes = std::max(mgr.stats.stagingPeakBytes, static_cast<uint64_t>(totalCompressedBytes));

    Texture resource;
    resource.image = image;
    resource.memory = imageMemory;
    resource.view = imageView;
    resource.sampler = sampler;

    return mgr.registerTexture(path, std::move(resource));
}
