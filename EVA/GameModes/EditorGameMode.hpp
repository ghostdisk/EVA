#pragma once
#include <EVA/GameModes/GameMode.hpp>

class Editor;

class EditorGameMode : public GameMode {
public:
	ECLASS_COMMON();

	Editor* editor = nullptr;

	virtual void OnBegin() override;
	virtual void OnEnd() override;
	virtual void OnTick(double dt) override;
	virtual Result LoadMap(String name) override;
};
