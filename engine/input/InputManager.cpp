#include "engine/input/InputManager.h"

#include <cstdlib>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {
constexpr std::uint8_t kHoldDurationFrames = 3;

#if defined(__unix__) || defined(__APPLE__)
struct TerminalState {
    bool active = false;
    int originalFlags = 0;
    termios originalTermios{};
};

TerminalState gTerminalState;
void restoreTerminalInput() noexcept;

bool enableRawTerminalInput() noexcept {
    if (gTerminalState.active) {
        return true;
    }

    if (isatty(STDIN_FILENO) == 0) {
        return false;
    }

    termios rawMode{};
    if (tcgetattr(STDIN_FILENO, &gTerminalState.originalTermios) != 0) {
        return false;
    }

    rawMode = gTerminalState.originalTermios;
    rawMode.c_lflag = static_cast<tcflag_t>(rawMode.c_lflag & static_cast<tcflag_t>(~(ICANON | ECHO)));
    rawMode.c_cc[VMIN] = 0;
    rawMode.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &rawMode) != 0) {
        return false;
    }

    const int currentFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (currentFlags < 0) {
        tcsetattr(STDIN_FILENO, TCSANOW, &gTerminalState.originalTermios);
        return false;
    }

    if (fcntl(STDIN_FILENO, F_SETFL, currentFlags | O_NONBLOCK) != 0) {
        tcsetattr(STDIN_FILENO, TCSANOW, &gTerminalState.originalTermios);
        return false;
    }

    gTerminalState.originalFlags = currentFlags;
    gTerminalState.active = true;
    std::atexit(restoreTerminalInput);
    return true;
}

void restoreTerminalInput() noexcept {
    if (!gTerminalState.active) {
        return;
    }

    (void)tcsetattr(STDIN_FILENO, TCSANOW, &gTerminalState.originalTermios);
    (void)fcntl(STDIN_FILENO, F_SETFL, gTerminalState.originalFlags);
    gTerminalState.active = false;
}
#endif
}  // namespace

void InputManager::initialize() noexcept {
    keys_.fill(false);
    holdFrames_.fill(0);
    quitRequested_ = false;
    scriptedFrame_ = 0;

#if defined(__unix__) || defined(__APPLE__)
    terminalInputEnabled_ = enableRawTerminalInput();
#else
    terminalInputEnabled_ = false;
#endif
}

void InputManager::update() noexcept {
    keys_.fill(false);

    for (std::size_t i = 0; i < holdFrames_.size(); ++i) {
        if (holdFrames_[i] == 0U) {
            continue;
        }

        keys_[i] = true;
        --holdFrames_[i];
    }

    if (!terminalInputEnabled_) {
        // Deterministic fallback path for non-interactive runs.
        const std::size_t loop = scriptedFrame_ % 480U;
        setKeyState(Key::Shoot, (scriptedFrame_ % 10U) == 0U);

        if (loop < 120U) {
            setKeyState(Key::MoveRight, true);
        } else if (loop < 240U) {
            setKeyState(Key::MoveForward, true);
        } else if (loop < 360U) {
            setKeyState(Key::MoveLeft, true);
        } else {
            setKeyState(Key::MoveBackward, true);
        }

        ++scriptedFrame_;
        return;
    }

#if defined(__unix__) || defined(__APPLE__)
    char inputChar = 0;
    std::size_t readCount = 0;
    while (readCount < 32U) {
        const ssize_t bytesRead = read(STDIN_FILENO, &inputChar, 1);
        if (bytesRead <= 0) {
            break;
        }

        mapCharacter(inputChar);
        ++readCount;
    }
#endif
}

bool InputManager::isKeyPressed(Key key) const noexcept {
    return keys_[static_cast<std::size_t>(key)];
}

void InputManager::setKeyState(Key key, bool pressed) noexcept {
    const std::size_t index = static_cast<std::size_t>(key);
    keys_[index] = pressed;
    holdFrames_[index] = pressed ? kHoldDurationFrames : 0U;
}

bool InputManager::quitRequested() const noexcept {
    return quitRequested_;
}

void InputManager::shutdown() noexcept {
    keys_.fill(false);
    holdFrames_.fill(0);
    quitRequested_ = false;

#if defined(__unix__) || defined(__APPLE__)
    restoreTerminalInput();
#endif

    terminalInputEnabled_ = false;
}

void InputManager::mapCharacter(char input) noexcept {
    switch (input) {
        case 'w':
        case 'W':
            setKeyState(Key::MoveForward, true);
            break;
        case 's':
        case 'S':
            setKeyState(Key::MoveBackward, true);
            break;
        case 'a':
        case 'A':
            setKeyState(Key::MoveLeft, true);
            break;
        case 'd':
        case 'D':
            setKeyState(Key::MoveRight, true);
            break;
        case ' ': 
            setKeyState(Key::Shoot, true);
            break;
        case 'q':
        case 'Q':
            quitRequested_ = true;
            break;
        default:
            break;
    }
}
