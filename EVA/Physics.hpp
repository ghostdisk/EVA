#pragma once
#include <EVA/Common.hpp>

struct PhysicsWorld;

void PhysicsInitialize();
PhysicsWorld* PhysicsWorldCreate();
void PhysicsWorldDestroy(PhysicsWorld* world);
void PhysicsTick(PhysicsWorld* world, double dt);