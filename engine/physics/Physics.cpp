#include "engine/physics/Physics.h"

void Physics::initialize() noexcept {
}

void Physics::update(float deltaTime) noexcept {
    applyForces(deltaTime);
    detectCollisions();
    resolveCollisions();
}

void Physics::cleanup() noexcept {
}

void Physics::applyForces(float deltaTime) noexcept {
    (void)deltaTime;
}

void Physics::detectCollisions() noexcept {
}

void Physics::resolveCollisions() noexcept {
}
