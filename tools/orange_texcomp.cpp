#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <string>
#include <vector>

// Stub structure matching what the loader expects
struct OgtHeader {
    char magic[4];
    uint32_t width;
    uint32_t height;
    uint32_t mipCount;
    uint32_t format; // 7 = BC7
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: orange_texcomp <input_image>\n";
        return 1;
    }

    std::string inputPath = argv[1];
    
    int width, height, channels;
    stbi_uc* pixels = stbi_load(inputPath.c_str(), &width, &height, &channels, 4);
    
    if (!pixels) {
        std::cerr << "Failed to load image: " << inputPath << "\n";
        return 1;
    }

    uint32_t mipW = width;
    uint32_t mipH = height;
    uint32_t mipCount = 0;

    // FIX: Using && instead of || for non-square textures
    while (mipW > 0 && mipH > 0) {
        mipCount++;
        mipW >>= 1;
        mipH >>= 1;
    }

    std::cout << "Compressing " << width << "x" << height << " with " << mipCount << " mips...\n";
    
    // Stub: actual bc7enc compression goes here
    
    stbi_image_free(pixels);
    return 0;
}
