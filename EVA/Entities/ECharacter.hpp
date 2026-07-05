#pragma once
#include <EVA/Entities/Entity.hpp>

struct ECharacter : Entity {
	virtual void OnActivate(const EntityCallbackInfo& ci) override;
	virtual void OnUpdate(const EntityCallbackInfo& ci) override;
	virtual void OnFixedUpdate(const EntityCallbackInfo& ci) override;
};