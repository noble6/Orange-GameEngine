#include "engine/graphics/Renderer.h"

void Renderer::initialize() noexcept {
    frameCount_ = 0;
}

void Renderer::beginFrame() noexcept {
    // Clear backbuffer/state here when a real renderer backend is attached.
}

void Renderer::render() noexcept {
    // Submit render commands here.
}

void Renderer::present() noexcept {
    ++frameCount_;
}

void Renderer::cleanup() noexcept {
    frameCount_ = 0;
}

std::size_t Renderer::frameCount() const noexcept {
    return frameCount_;
}
