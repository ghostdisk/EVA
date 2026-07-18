#pragma once
#include <EVA/Core/Basic.hpp>

class Game;
class EntityManager;
class ECamera;

class GameMode : Object {

public:
	Game*          m_game          = nullptr;
	EntityManager* m_entityManager = nullptr;

	ECLASS_COMMON(GameMode);

	void Init(Game* game, EntityManager* entityManager);

	static GameMode* GetCurrent();

	virtual void OnBegin() {}
	virtual void OnEnd() {}
	virtual void OnTick(double dt) {}
	virtual Result LoadMap(String name);
};
