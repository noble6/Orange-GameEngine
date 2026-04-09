#include "engine/input/InputManager.h"

void InputManager::initialize() noexcept {
    keys_.fill(false);
}

void InputManager::update() noexcept {
    // Placeholder for platform input polling.
}

bool InputManager::isKeyPressed(Key key) const noexcept {
    return keys_[static_cast<std::size_t>(key)];
}

void InputManager::setKeyState(Key key, bool pressed) noexcept {
    keys_[static_cast<std::size_t>(key)] = pressed;
}

void InputManager::shutdown() noexcept {
    keys_.fill(false);
}
