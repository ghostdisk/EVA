#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Math.hpp>
#include <EVA/Entities/Entity.hpp> // TODO: EntityType
#include <vector>

class Entity;
class EntityManager;
class Game;
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

enum EdTool {
	EdTool_None = 0,
	EdTool_Select = 1,
	EdTool_Entity = 2,
	EdTool_Brush = 3,

	EdTool_ENUM_SIZE,
};

enum EdSelectionType {
	EdSelectionType_None,
	EdSelectionType_Node,
	EdSelectionType_BrushPlane,
	EdSelectionType_Entity,
};

struct EdSelection {
	EdSelectionType type   = EdSelectionType_None;
	EdOp*           op     = nullptr;
	Entity*         entity = nullptr;
	int             index  = 0;
};

enum EdBrushToolPhase {
	EdBrushToolPhase_Inactive    = 0,
	EdBrushToolPhase_InitialDraw = 1,
	EdBrushToolPhase_X           = 2,
	EdBrushToolPhase_Y           = 3,
	EdBrushToolPhase_Z           = 4,
	EdBrushToolPhase_Finalize    = 5,
};

class Editor {
public:
	Editor(Game* game, EntityManager* entity_manager);

	Game*          m_game          = nullptr;
	EntityManager* m_entityManager = nullptr;
	ECamera*       m_camera        = nullptr;
	EID            m_nextEID       = EID_MapStart;
	EdOp*          m_root          = nullptr;
	std::vector<EdSelection> m_selection = {};
	char           m_loadedMapName[64] = {};
	EdTool         m_tool          = EdTool_Select;
	float3         m_brushToolStart = {};
	float3         m_brushToolEnd   = {};
	EdBrushToolPhase m_brushToolPhase = EdBrushToolPhase_Inactive;
	Type*          m_entityToolType = nullptr;

	void Tick(double dt);
	Result LoadMap(String name);
};


void EdInitialize();
