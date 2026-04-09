#pragma once

#include <array>
#include <cstddef>

class InputManager {
public:
    enum class Key : std::size_t {
        MoveForward = 0,
        MoveBackward,
        MoveLeft,
        MoveRight,
        Shoot,
        Count
    };

    InputManager() = default;

    void initialize() noexcept;
    void update() noexcept;
    bool isKeyPressed(Key key) const noexcept;
    void setKeyState(Key key, bool pressed) noexcept;
    void shutdown() noexcept;

private:
    std::array<bool, static_cast<std::size_t>(Key::Count)> keys_{};
};
