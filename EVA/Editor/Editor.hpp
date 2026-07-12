#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Core/Object.hpp>
#include <EVA/Core/Hashing.hpp>
#include <EVA/Math.hpp>
#include <EVA/Entities/Entity.hpp> // TODO: EntityType
#include <vector>

class Entity;
class EntityManager;
class Game;
struct CSGBrush;
struct ECamera;

struct EdGrid {
	float3 center = {};
	float3 forward = { 0, 1, 0 };
	float3 right = { 1, 0, 0 };
	float size = 0.25f;
};

struct EdGizmoState {
	U32 id = 0;
	float world_dist = 0;
	float screen_dist = 0;
	float3 offset = {};
};

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

class Editor;

class Tool : public Object {
public:
	ECLASS_COMMON();

	Editor* m_editor = nullptr;

	virtual ZTString Name() = 0;
	virtual ZTString GetShortName() = 0;
	virtual int GetOrder() = 0;
	virtual void OnActivate() {}
	virtual void OnDeactivate() {}
	virtual void Tick(double dt) = 0;
	virtual void DrawSidebar() {}
};

class SelectTool : public Tool {
public:
	ECLASS_COMMON();
	virtual ZTString Name() override { return "Select"; }
	virtual ZTString GetShortName() override { return "SEL"; }
	virtual int GetOrder() override { return 1; }
	virtual void Tick(double dt) override;

	float3 m_gizmoCenter = {};
};

class EntityTool : public Tool {
public:
	ECLASS_COMMON();
	virtual ZTString Name() override { return "Entity"; }
	virtual ZTString GetShortName() override { return "ENT"; }
	virtual int GetOrder() override { return 2; }
	virtual void Tick(double dt) override;
	virtual void DrawSidebar() override;

	Type* m_entityType = nullptr;
};

class BrushTool : public Tool {
public:
	ECLASS_COMMON();
	virtual ZTString Name() override { return "Brush"; }
	virtual ZTString GetShortName() override { return "BSH"; }
	virtual int GetOrder() override { return 3; }
	virtual void OnDeactivate() override;
	virtual void Tick(double dt) override;

	enum Phase {
		Phase_Inactive,
		Phase_InitialDraw,
		Phase_X,
		Phase_Y,
		Phase_Z,
		Phase_Finalize,
	};

	Phase  m_phase = Phase_Inactive;
	float3 m_start = {};
	float3 m_end = {};
	bool   m_justEnteredPhase = false;
	float3 m_refA = {};
	float3 m_refB = {};
	float3 m_axisStart = {};
};

class Editor {
public:
	Editor(Game* game, EntityManager* entity_manager);
	~Editor();

	Game*                    m_game              = nullptr;
	EntityManager*           m_entityManager     = nullptr;
	ECamera*                 m_camera            = nullptr;
	int                      m_gridSizeIdx       = 3;
	bool                     m_snap              = true;
	HashStack                m_hashStack         = {};
	EdGrid                   m_grid              = {};
	EdGizmoState             m_activeGizmoState  = {};
	EdGizmoState             m_hoverGizmoState   = {};
	EdGizmoState             m_newHoverGizmoState = {};
	EID                      m_nextEID           = EID_MapStart;
	EdOp*                    m_root              = nullptr;
	std::vector<EdSelection> m_selection         = {};
	char                     m_loadedMapName[64] = {};
	std::vector<Tool*>       m_tools             = {};
	Tool*                    m_tool              = nullptr;

	void Tick(double dt);
	void SetTool(Tool* tool);
	Result LoadMap(String name);


	void Deselect();
	void SelectPlane(EdOp* op, int plane, bool additive = false);
	void SelectEntity(Entity* entity, bool additive = false);
	void SelectOp(EdOp* op, bool additive = false);
	void DestroySelection();
	bool ShouldSnap() const;
	bool IsSelected(EdOp* op) const;
	bool IsSelected(Entity* entity) const;
	std::vector<EdOp*> GetSelectedOps() const;
	Entity* CreateEntity(Type* type, float3 pos);
	void DestroyEntity(Entity* entity);
	void OrderMove(int offset);
	void Build(EdOp* op);
	bool RaycastAgainstSubtractOps(const Ray& ray, float* out_t, EdOp** out_hit);
	bool RaycastAgainstBuiltBrushes(const Ray& ray, float* out_t, EdOp** out_op_hit, CSGBrush** out_built_brush_hit);
	bool RaycastAgainstEntities(const Ray& ray, float* out_t, Entity** out_hit);
	void DrawOpGUI(EdOp* op);
	float3 SnapToGrid(float3 p) const;
	void ArrowGizmo(Hash hash, float3& pos, float3 direction, float4 color, float base_scale = 1, bool hidden = false, bool force_activate = false);
	void TranslationGizmo(Hash hash, float3& pos);
	void DrawSelectionOutline(float4 color);
	bool DoPlaneDragGizmo(EdOp* op, CSGBrush* brush, int idx);
	void DrawGrid(float4 color);
	Result SaveMap(const char* name);
	void UnloadMap();
	Result CompileMap();

};


void EdInitialize();
