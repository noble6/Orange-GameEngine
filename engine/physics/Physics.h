#pragma once

class Physics {
public:
    void initialize() noexcept;
    void update(float deltaTime) noexcept;
    void cleanup() noexcept;

private:
    void applyForces(float deltaTime) noexcept;
    void detectCollisions() noexcept;
    void resolveCollisions() noexcept;
};
