#pragma once
#include <EVA/Entities/Entity.hpp>

class ECharacter : public Entity {
public:
	ECLASS_COMMON(ECharacter);

	virtual void OnActivate(const EntityCallbackInfo& ci) override;
	virtual void OnUpdate(const EntityCallbackInfo& ci) override;
	virtual void OnFixedUpdate(const EntityCallbackInfo& ci) override;
};
