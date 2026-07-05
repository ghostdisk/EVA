#include <EVA/Entities/ECharacter.hpp>

void ECharacter::OnActivate(const EntityCallbackInfo& ci) {
	RequestUpdateCallback(ci);
	RequestFixedUpdateCallback(ci);
}

void ECharacter::OnUpdate(const EntityCallbackInfo& ci) {
}

void ECharacter::OnFixedUpdate(const EntityCallbackInfo& ci) {
}
