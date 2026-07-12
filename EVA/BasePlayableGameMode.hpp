#pragma once
#include <EVA/GameMode.hpp>

class BasePlayableGameMode : GameMode {

	char map_name[32] = {};

public:
	ECLASS_COMMON();

	virtual void OnBegin() override;
	virtual void OnEnd() override;
	virtual void OnTick(double dt) override;
	virtual Result LoadMap(String name) override;
};
