#include "engine/core/Engine.h"
#include "game/Game.h"

int main() {
    Engine engine;
    Game game;

    if (!engine.initialize()) {
        return 1;
    }

    engine.run(game, 600);
    return 0;
}
