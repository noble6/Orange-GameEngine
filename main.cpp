#include <cstdlib>
#include <string>

#include "engine/core/Engine.h"
#include "game/Game.h"

int main(int argc, char* argv[]) {
    Engine engine;
    Game game;

    std::size_t maxFrames = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--frames=", 0) == 0) {
            maxFrames = static_cast<std::size_t>(std::stoull(arg.substr(9)));
        }
    }

    if (!engine.initialize()) {
        return 1;
    }

    engine.run(game, maxFrames);
    return 0;
}
