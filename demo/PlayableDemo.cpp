#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kPlayerSpeed = 280.0f;
constexpr float kPlayerRadius = 18.0f;
constexpr float kBulletSpeed = 560.0f;
constexpr float kBulletLifeSeconds = 1.4f;
constexpr float kShotCooldownSeconds = 0.15f;
constexpr float kEnemyBaseSpeed = 62.0f;
constexpr float kEnemySpeedJitter = 38.0f;
constexpr float kEnemyRadius = 14.0f;
constexpr float kSpawnIntervalSeconds = 0.72f;
constexpr int kMaxEnemies = 52;
constexpr int kInitialHealth = 100;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

Vec2 operator+(const Vec2& a, const Vec2& b) noexcept {
    return Vec2{a.x + b.x, a.y + b.y};
}

Vec2 operator-(const Vec2& a, const Vec2& b) noexcept {
    return Vec2{a.x - b.x, a.y - b.y};
}

Vec2 operator*(const Vec2& a, float scale) noexcept {
    return Vec2{a.x * scale, a.y * scale};
}

float lengthSquared(const Vec2& v) noexcept {
    return (v.x * v.x) + (v.y * v.y);
}

Vec2 normalizedOrZero(const Vec2& v) noexcept {
    const float lenSq = lengthSquared(v);
    if (lenSq <= 0.000001f) {
        return Vec2{};
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    return Vec2{v.x * invLen, v.y * invLen};
}

float clampFloat(float v, float minValue, float maxValue) noexcept {
    return std::max(minValue, std::min(v, maxValue));
}

SDL_Texture* createCheckerTexture(SDL_Renderer* renderer,
                                  int width,
                                  int height,
                                  SDL_Color a,
                                  SDL_Color b,
                                  int checkerSize) noexcept {
    if (renderer == nullptr || width <= 0 || height <= 0 || checkerSize <= 0) {
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return nullptr;
    }

    const Uint32 ca = SDL_MapRGBA(surface->format, a.r, a.g, a.b, a.a);
    const Uint32 cb = SDL_MapRGBA(surface->format, b.r, b.g, b.b, b.a);

    Uint32* pixels = static_cast<Uint32*>(surface->pixels);
    const int pitch = surface->pitch / static_cast<int>(sizeof(Uint32));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool odd = ((x / checkerSize) + (y / checkerSize)) % 2 != 0;
            pixels[y * pitch + x] = odd ? ca : cb;
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

struct Voice {
    bool active = false;
    float frequency = 0.0f;
    float phase = 0.0f;
    float amplitude = 0.0f;
    float decay = 0.0f;
    int remainingSamples = 0;
};

class SoundSynth {
public:
    bool initialize() noexcept {
        SDL_AudioSpec desired{};
        desired.freq = 48000;
        desired.format = AUDIO_F32SYS;
        desired.channels = 1;
        desired.samples = 1024;
        desired.callback = &SoundSynth::audioCallbackBridge;
        desired.userdata = this;

        SDL_AudioSpec obtained{};
        device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
        if (device_ == 0U) {
            return false;
        }

        sampleRate_ = obtained.freq > 0 ? obtained.freq : 48000;
        SDL_PauseAudioDevice(device_, 0);
        return true;
    }

    void shutdown() noexcept {
        if (device_ != 0U) {
            SDL_CloseAudioDevice(device_);
            device_ = 0U;
        }
        voices_.fill(Voice{});
    }

    void shot() noexcept {
        addVoice(720.0f, 0.18f, 0.32f, 0.9976f);
    }

    void hit() noexcept {
        addVoice(160.0f, 0.22f, 0.42f, 0.9972f);
        addVoice(230.0f, 0.16f, 0.30f, 0.9972f);
    }

    void hurt() noexcept {
        addVoice(105.0f, 0.26f, 0.46f, 0.9970f);
    }

private:
    static void audioCallbackBridge(void* userdata, Uint8* stream, int len) noexcept {
        SoundSynth* self = static_cast<SoundSynth*>(userdata);
        if (self != nullptr) {
            self->audioCallback(stream, len);
        }
    }

    void addVoice(float frequency, float durationSeconds, float amplitude, float decay) noexcept {
        if (device_ == 0U) {
            return;
        }

        SDL_LockAudioDevice(device_);
        for (Voice& voice : voices_) {
            if (voice.active) {
                continue;
            }
            voice.active = true;
            voice.frequency = frequency;
            voice.phase = 0.0f;
            voice.amplitude = amplitude;
            voice.decay = decay;
            voice.remainingSamples = static_cast<int>(durationSeconds * static_cast<float>(sampleRate_));
            break;
        }
        SDL_UnlockAudioDevice(device_);
    }

    void audioCallback(Uint8* stream, int len) noexcept {
        float* out = reinterpret_cast<float*>(stream);
        const int sampleCount = len / static_cast<int>(sizeof(float));
        constexpr float kTwoPi = 6.28318530718f;

        for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
            float mixed = 0.0f;

            for (Voice& voice : voices_) {
                if (!voice.active || voice.remainingSamples <= 0 || voice.amplitude < 0.0002f) {
                    voice.active = false;
                    continue;
                }

                mixed += std::sin(voice.phase) * voice.amplitude;
                voice.phase += (kTwoPi * voice.frequency) / static_cast<float>(sampleRate_);
                if (voice.phase > kTwoPi) {
                    voice.phase -= kTwoPi;
                }

                voice.amplitude *= voice.decay;
                --voice.remainingSamples;
            }

            out[sampleIndex] = clampFloat(mixed, -0.95f, 0.95f);
        }
    }

    SDL_AudioDeviceID device_ = 0U;
    int sampleRate_ = 48000;
    std::array<Voice, 24> voices_{};
};

struct Bullet {
    Vec2 position{};
    Vec2 velocity{};
    float remainingLife = 0.0f;
    bool alive = true;
};

struct Enemy {
    Vec2 position{};
    float speed = kEnemyBaseSpeed;
    float touchCooldown = 0.0f;
    bool alive = true;
};

void drawGrid(SDL_Renderer* renderer) noexcept {
    SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
    for (int x = 0; x < kWindowWidth; x += 40) {
        SDL_RenderDrawLine(renderer, x, 0, x, kWindowHeight);
    }
    for (int y = 0; y < kWindowHeight; y += 40) {
        SDL_RenderDrawLine(renderer, 0, y, kWindowWidth, y);
    }
}

void drawHealthBar(SDL_Renderer* renderer, int health) noexcept {
    const int clamped = std::max(0, std::min(health, kInitialHealth));
    SDL_Rect back{20, 20, 300, 20};
    SDL_SetRenderDrawColor(renderer, 40, 25, 25, 255);
    SDL_RenderFillRect(renderer, &back);

    SDL_Rect value{20, 20, (300 * clamped) / kInitialHealth, 20};
    SDL_SetRenderDrawColor(renderer, 220, 70, 70, 255);
    SDL_RenderFillRect(renderer, &value);
}

}  // namespace

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("TPS Playable Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* playerTexture = createCheckerTexture(
        renderer, 48, 48, SDL_Color{64, 180, 255, 255}, SDL_Color{10, 72, 140, 255}, 6);
    SDL_Texture* enemyTexture = createCheckerTexture(
        renderer, 36, 36, SDL_Color{255, 114, 90, 255}, SDL_Color{140, 34, 24, 255}, 6);
    SDL_Texture* bulletTexture = createCheckerTexture(
        renderer, 14, 14, SDL_Color{255, 240, 126, 255}, SDL_Color{180, 120, 22, 255}, 2);
    if (playerTexture == nullptr || enemyTexture == nullptr || bulletTexture == nullptr) {
        std::cerr << "Texture generation failed.\n";
        SDL_DestroyTexture(playerTexture);
        SDL_DestroyTexture(enemyTexture);
        SDL_DestroyTexture(bulletTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SoundSynth synth;
    const bool audioReady = synth.initialize();

    std::vector<Bullet> bullets;
    bullets.reserve(256);
    std::vector<Enemy> enemies;
    enemies.reserve(128);

    std::uint32_t seed = 1337U;
    const char* seedEnv = std::getenv("TPS_RANDOM_SEED");
    if (seedEnv != nullptr && seedEnv[0] != '\0') {
        seed = static_cast<std::uint32_t>(std::atoi(seedEnv));
    }
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> edgePick(0.0f, 1.0f);
    std::uniform_real_distribution<float> spawnX(0.0f, static_cast<float>(kWindowWidth));
    std::uniform_real_distribution<float> spawnY(0.0f, static_cast<float>(kWindowHeight));
    std::uniform_real_distribution<float> speedJitter(0.0f, kEnemySpeedJitter);

    Vec2 playerPos{static_cast<float>(kWindowWidth) * 0.5f, static_cast<float>(kWindowHeight) * 0.5f};
    float shotCooldown = 0.0f;
    float enemySpawnTimer = 0.0f;
    int health = kInitialHealth;
    int score = 0;
    bool gameOver = false;
    bool running = true;

    std::uint64_t lastCounter = SDL_GetPerformanceCounter();
    const double invFrequency = 1.0 / static_cast<double>(SDL_GetPerformanceFrequency());

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r && gameOver) {
                playerPos = Vec2{static_cast<float>(kWindowWidth) * 0.5f, static_cast<float>(kWindowHeight) * 0.5f};
                bullets.clear();
                enemies.clear();
                shotCooldown = 0.0f;
                enemySpawnTimer = 0.0f;
                health = kInitialHealth;
                score = 0;
                gameOver = false;
            }
        }

        const std::uint64_t nowCounter = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>(static_cast<double>(nowCounter - lastCounter) * invFrequency);
        lastCounter = nowCounter;
        deltaTime = clampFloat(deltaTime, 0.001f, 0.05f);

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (!gameOver) {
            Vec2 move{};
            if (keys[SDL_SCANCODE_W] != 0U) {
                move.y -= 1.0f;
            }
            if (keys[SDL_SCANCODE_S] != 0U) {
                move.y += 1.0f;
            }
            if (keys[SDL_SCANCODE_A] != 0U) {
                move.x -= 1.0f;
            }
            if (keys[SDL_SCANCODE_D] != 0U) {
                move.x += 1.0f;
            }

            move = normalizedOrZero(move);
            playerPos = playerPos + (move * (kPlayerSpeed * deltaTime));
            playerPos.x = clampFloat(playerPos.x, kPlayerRadius, static_cast<float>(kWindowWidth) - kPlayerRadius);
            playerPos.y = clampFloat(playerPos.y, kPlayerRadius, static_cast<float>(kWindowHeight) - kPlayerRadius);

            shotCooldown = std::max(0.0f, shotCooldown - deltaTime);
            enemySpawnTimer += deltaTime;

            if (enemySpawnTimer >= kSpawnIntervalSeconds && static_cast<int>(enemies.size()) < kMaxEnemies) {
                enemySpawnTimer = 0.0f;
                Enemy e{};
                e.speed = kEnemyBaseSpeed + speedJitter(rng);
                const float side = edgePick(rng);
                if (side < 0.25f) {
                    e.position = Vec2{spawnX(rng), -40.0f};
                } else if (side < 0.5f) {
                    e.position = Vec2{spawnX(rng), static_cast<float>(kWindowHeight) + 40.0f};
                } else if (side < 0.75f) {
                    e.position = Vec2{-40.0f, spawnY(rng)};
                } else {
                    e.position = Vec2{static_cast<float>(kWindowWidth) + 40.0f, spawnY(rng)};
                }
                enemies.push_back(e);
            }

            int mouseX = 0;
            int mouseY = 0;
            const Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
            const bool wantsShoot = (keys[SDL_SCANCODE_SPACE] != 0U) || ((mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0U);
            if (wantsShoot && shotCooldown <= 0.0f) {
                const Vec2 aim = normalizedOrZero(Vec2{static_cast<float>(mouseX) - playerPos.x, static_cast<float>(mouseY) - playerPos.y});
                if (lengthSquared(aim) > 0.0f) {
                    Bullet bullet{};
                    bullet.position = playerPos;
                    bullet.velocity = aim * kBulletSpeed;
                    bullet.remainingLife = kBulletLifeSeconds;
                    bullet.alive = true;
                    bullets.push_back(bullet);
                    shotCooldown = kShotCooldownSeconds;
                    if (audioReady) {
                        synth.shot();
                    }
                }
            }

            for (Bullet& bullet : bullets) {
                if (!bullet.alive) {
                    continue;
                }

                bullet.position = bullet.position + (bullet.velocity * deltaTime);
                bullet.remainingLife -= deltaTime;
                if (bullet.remainingLife <= 0.0f || bullet.position.x < -80.0f || bullet.position.x > static_cast<float>(kWindowWidth) + 80.0f ||
                    bullet.position.y < -80.0f || bullet.position.y > static_cast<float>(kWindowHeight) + 80.0f) {
                    bullet.alive = false;
                }
            }

            for (Enemy& enemy : enemies) {
                if (!enemy.alive) {
                    continue;
                }
                enemy.touchCooldown = std::max(0.0f, enemy.touchCooldown - deltaTime);
                const Vec2 direction = normalizedOrZero(playerPos - enemy.position);
                enemy.position = enemy.position + (direction * (enemy.speed * deltaTime));
            }

            const float bulletHitDistance = (kEnemyRadius + 7.0f) * (kEnemyRadius + 7.0f);
            for (Bullet& bullet : bullets) {
                if (!bullet.alive) {
                    continue;
                }

                for (Enemy& enemy : enemies) {
                    if (!enemy.alive) {
                        continue;
                    }

                    const float distSq = lengthSquared(enemy.position - bullet.position);
                    if (distSq > bulletHitDistance) {
                        continue;
                    }

                    bullet.alive = false;
                    enemy.alive = false;
                    ++score;
                    if (audioReady) {
                        synth.hit();
                    }
                    break;
                }
            }

            const float touchDistanceSq = (kEnemyRadius + kPlayerRadius - 2.0f) * (kEnemyRadius + kPlayerRadius - 2.0f);
            for (Enemy& enemy : enemies) {
                if (!enemy.alive) {
                    continue;
                }
                if (enemy.touchCooldown > 0.0f) {
                    continue;
                }

                const float distSq = lengthSquared(enemy.position - playerPos);
                if (distSq > touchDistanceSq) {
                    continue;
                }

                health = std::max(0, health - 8);
                enemy.touchCooldown = 0.50f;
                if (audioReady) {
                    synth.hurt();
                }
                if (health == 0) {
                    gameOver = true;
                    break;
                }
            }

            bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) { return !b.alive; }), bullets.end());
            enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.alive; }), enemies.end());
        }

        SDL_SetRenderDrawColor(renderer, 9, 11, 14, 255);
        SDL_RenderClear(renderer);
        drawGrid(renderer);

        for (const Bullet& bullet : bullets) {
            SDL_Rect dst{static_cast<int>(bullet.position.x - 7.0f), static_cast<int>(bullet.position.y - 7.0f), 14, 14};
            SDL_RenderCopy(renderer, bulletTexture, nullptr, &dst);
        }

        for (const Enemy& enemy : enemies) {
            SDL_Rect dst{static_cast<int>(enemy.position.x - 18.0f), static_cast<int>(enemy.position.y - 18.0f), 36, 36};
            SDL_RenderCopy(renderer, enemyTexture, nullptr, &dst);
        }

        SDL_Rect playerDst{static_cast<int>(playerPos.x - 24.0f), static_cast<int>(playerPos.y - 24.0f), 48, 48};
        SDL_RenderCopy(renderer, playerTexture, nullptr, &playerDst);
        drawHealthBar(renderer, health);

        SDL_RenderPresent(renderer);

        std::string title = "TPS Playable Demo | WASD Move | Mouse/SPACE Shoot | ESC Quit | HP " + std::to_string(health) +
                            " | Score " + std::to_string(score) + " | Enemies " + std::to_string(enemies.size());
        if (gameOver) {
            title += " | GAME OVER - Press R to Restart";
        }
        SDL_SetWindowTitle(window, title.c_str());
    }

    synth.shutdown();
    SDL_DestroyTexture(playerTexture);
    SDL_DestroyTexture(enemyTexture);
    SDL_DestroyTexture(bulletTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
