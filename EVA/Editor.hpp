#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/GameModes/GameMode.hpp>
#include <EVA/Math.hpp>
#include <EVA/Entities/Entity.hpp> // TODO: EntityType
#include <vector>

class Entity;
struct CSGBrush;
struct ECamera;

enum EdOpType {
	EdOpType_None   = 0,
	EdOpType_Brush  = 1,
	EdOpType_Stack  = 2,

	EdOpType_ENUM_SIZE,
};

struct EdOp {
	EdOp*                  parent           = nullptr;
	EdOpType               type             = EdOpType_None;
	std::vector<CSGBrush*> built            = {};
	bool                   subtract         = false;
	float3                 position         = {};
	float4x4               global_transform = {};

	std::vector<EdOp*>     children = {};      // for EdOpType_Stack
	CSGBrush*              brush    = nullptr; // for EdOpType_Brush
};

class Editor : public GameMode {


public:
	ECLASS_COMMON();

	ECamera* m_camera;
	void EdTick();

	virtual void OnBegin() override;
	virtual void OnEnd() override;
	virtual void OnTick(double dt) override;
	virtual Result LoadMap(String name) override;
};


void EdInitialize();
