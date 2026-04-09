#pragma once

#include <cstddef>

class Renderer {
public:
    void initialize() noexcept;
    void beginFrame() noexcept;
    void render() noexcept;
    void present() noexcept;
    void cleanup() noexcept;

    std::size_t frameCount() const noexcept;

private:
    std::size_t frameCount_ = 0;
};
