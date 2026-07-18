#pragma once
#include <EVA/GameModes/GameMode.hpp>

class BasePlayableGameMode : GameMode {

	char map_name[32] = {};
	ECamera* m_camera = nullptr;

public:
	ECLASS_COMMON(BasePlayableGameMode);

	virtual void OnBegin() override;
	virtual void OnEnd() override;
	virtual void OnTick(double dt) override;
	virtual Result LoadMap(String name) override;
};
