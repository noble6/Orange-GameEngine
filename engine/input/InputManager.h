#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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
    bool quitRequested() const noexcept;
    void shutdown() noexcept;

private:
    void mapCharacter(char input) noexcept;

    std::array<bool, static_cast<std::size_t>(Key::Count)> keys_{};
    std::array<std::uint8_t, static_cast<std::size_t>(Key::Count)> holdFrames_{};

    bool quitRequested_ = false;
    bool terminalInputEnabled_ = false;
    std::size_t scriptedFrame_ = 0;
};
