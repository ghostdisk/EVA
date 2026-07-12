#include <EVA/Assets/Asset.hpp>
#include <EVA/Assets/Mesh.hpp>
#include <EVA/Editor/Editor.hpp>
#include <EVA/Assets/Material.hpp>
#include <EVA/App.hpp>
#include <EVA/Platform.hpp>
#include <EVA/GameModes/EditorGameMode.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Console.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Library.hpp>
#include <EVA/Input.hpp>
#include <EVA/Game.hpp>
#include <EVA/UI.hpp>
#include <EVA/Core/Hashing.hpp>
#include <cglm/quat.h>
#include <algorithm>

static const float ED_GRID_SIZES[] = {
	0.03125f,
	0.0625f,
	0.125f,
	0.25f,
	0.5f,
	1.0f,
	2.0f,
	4.0f,
	8.0f,
};


static ConVar cvar_ed_show_sub = {
	.name = "ed_show_sub",
	.help = "show subtract brushes",
	.fvalue = 0,
};

static float4 brush_colors[] = {
	{ 0.910f, 0.450f, 0.450f, 1.0f },  // red
	{ 0.910f, 0.542f, 0.450f, 1.0f },
	{ 0.910f, 0.634f, 0.450f, 1.0f },
	{ 0.910f, 0.726f, 0.450f, 1.0f },
	{ 0.910f, 0.818f, 0.450f, 1.0f },
	{ 0.910f, 0.910f, 0.450f, 1.0f },  // yellow
	{ 0.818f, 0.910f, 0.450f, 1.0f },
	{ 0.726f, 0.910f, 0.450f, 1.0f },
	{ 0.634f, 0.910f, 0.450f, 1.0f },
	{ 0.542f, 0.910f, 0.450f, 1.0f },
	{ 0.450f, 0.910f, 0.450f, 1.0f },  // green
	{ 0.450f, 0.910f, 0.542f, 1.0f },
	{ 0.450f, 0.910f, 0.634f, 1.0f },
	{ 0.450f, 0.910f, 0.726f, 1.0f },
	{ 0.450f, 0.910f, 0.818f, 1.0f },
	{ 0.450f, 0.910f, 0.910f, 1.0f },  // cyan
	{ 0.450f, 0.818f, 0.910f, 1.0f },
	{ 0.450f, 0.726f, 0.910f, 1.0f },
	{ 0.450f, 0.634f, 0.910f, 1.0f },
	{ 0.450f, 0.542f, 0.910f, 1.0f },
	{ 0.450f, 0.450f, 0.910f, 1.0f },  // blue
	{ 0.542f, 0.450f, 0.910f, 1.0f },
	{ 0.634f, 0.450f, 0.910f, 1.0f },
	{ 0.726f, 0.450f, 0.910f, 1.0f },
	{ 0.818f, 0.450f, 0.910f, 1.0f },
	{ 0.910f, 0.450f, 0.910f, 1.0f },  // magenta
	{ 0.910f, 0.450f, 0.818f, 1.0f },
	{ 0.910f, 0.450f, 0.726f, 1.0f },
	{ 0.910f, 0.450f, 0.634f, 1.0f },
	{ 0.910f, 0.450f, 0.542f, 1.0f },
};


static Result GetEditor(Editor** out) {
	EditorGameMode* game_mode = dynamic_cast<EditorGameMode*>(GameMode::GetCurrent());
	*out = game_mode ? game_mode->editor : nullptr;
	return *out ? Success() : Err("no active editor");
}

EdOp* EdCreateOp() {
	EdOp* op = new EdOp();
	return op;
}

bool EdShouldSnap(Editor* ed) {
	return ed->m_snap && !InputGetButton(SDL_SCANCODE_LALT);
}

void EdDeselect(Editor* ed) {
	ed->m_selection.clear();
}

void EdSelectPlane(Editor* ed, EdOp* op, int plane, bool additive = false) {
	assert(op);
	if (!additive) EdDeselect(ed);
	if (ed->m_selection.size() && ed->m_selection[0].type != EdSelectionType_BrushPlane) return;

	ed->m_selection.push_back(EdSelection{
		.type = EdSelectionType_BrushPlane,
		.op = op,
		.index = plane,
	});
}

void EdSelectOp(Editor* ed, EdOp* op, bool additive = false) {
	if (!additive) EdDeselect(ed);
	if (!op || op == ed->m_root) return;

	for (const EdSelection& sel : ed->m_selection)
	{
		if (sel.type != EdSelectionType_Node) { EdDeselect(ed);  break; }
		if (sel.op->parent != op->parent) { EdDeselect(ed);  break; }
	}

	ed->m_selection.push_back(EdSelection{
		.type = EdSelectionType_Node,
		.op = op,
	});
}

void EdSelectEntity(Editor* ed, Entity* entity, bool additive = false) {
	if (!additive) EdDeselect(ed);

	for (const EdSelection& sel : ed->m_selection) {
		if (sel.type != EdSelectionType_Entity) { EdDeselect(ed);  break; }
	}

	ed->m_selection.push_back(EdSelection{
		.type = EdSelectionType_Entity,
		.entity = entity,
	});
}

bool EdIsSelected(Editor* ed, EdOp* op) {
	for (const EdSelection& sel : ed->m_selection)
		if (sel.type == EdSelectionType_Node && sel.op == op)
			return true;
	return false;
}

bool EdIsSelected(Editor* ed, Entity* entity) {
	for (const EdSelection& sel : ed->m_selection)
		if (sel.type == EdSelectionType_Entity && sel.entity == entity)
			return true;
	return false;
}

std::vector<EdOp*> EdGetSelectedOps(Editor* ed) {
	if (ed->m_selection.size() == 0 || ed->m_selection[0].type != EdSelectionType_Node) return {};
	std::vector<EdOp*> selection;
	for (const EdSelection& sel : ed->m_selection) {
		assert(sel.type == EdSelectionType_Node);
		assert(!selection.size() || selection[0]->parent == sel.op->parent);
		selection.push_back(sel.op);
	}
	return selection;
}

void EdRemoveOpFromTree(EdOp* op) {
	if (op->parent) {
		for (int i = 0; i < op->parent->children.size(); i++) {
			if (op->parent->children[i] == op) {
				op->parent->children.erase(op->parent->children.begin() + i);
				break;
			}
		}
		op->parent = nullptr;
	}
}

Entity* EdCreateEntity(Editor* ed, Type* type, float3 pos) {
	Entity* entity = (Entity*)type->Instantiate(Allocator::HeapAllocator);
	entity->position = pos;
	ed->m_entityManager->RegisterEntity(entity, ed->m_nextEID++);
	return entity;
}

void EdDestroyEntity(Editor* ed, Entity* entity) {
	ed->m_entityManager->DestroyEntity(entity);
}

void EdDestroyOp(EdOp* op) {
	if (op->brush) CSGDestroyBrush(op->brush);
	for (CSGBrush* brush : op->built) CSGDestroyBrush(brush);

	for (EdOp* child : op->children) {
		child->parent = nullptr; // zero this out so the child doesn't try to remove itself from op->children while we're iterating it
		EdDestroyOp(child);
	}

	EdRemoveOpFromTree(op);
	delete op;
}

void EdDestroySelection(Editor* ed) {
	bool changed_geometry = false;

	for (EdSelection& sel : ed->m_selection) {
		switch (sel.type) {
			case EdSelectionType_Entity: {
				EdDestroyEntity(ed, sel.entity);
				break;
			}
			case EdSelectionType_Node: {
				changed_geometry = true;
				EdDestroyOp(sel.op);
				break;
			}
			default: break;
		}
	}

	EdDeselect(ed);
}

void EdDrawBrushPlaneOutline(CSGBrush* b, int i, const float4x4& transform, const float4& color) {
	const CSGPlane& plane = b->planes[i];
	for (int i = 0; i < plane.points.size(); i++) {
		float3 p1 = transform.TransformPosition(plane.points[i]);
		float3 p2 = transform.TransformPosition(plane.points[(i + 1) % plane.points.size()]);
		DrawLine(p1, p2, color);
	}
}

void EdDrawBrushOutline(CSGBrush* b, const float4x4& transform, const float4& color) {
	for (int i = 0; i < b->planes.size(); i++)
		EdDrawBrushPlaneOutline(b, i, transform, color);
}

void EdDrawOutline(EdOp* op, const float4& color) {
	switch (op->type) {
		case EdOpType_Brush: {
			EdDrawBrushOutline(op->brush, op->global_transform, color);
			break;
		}
		case EdOpType_Stack: {
			for (EdOp* child : op->children) {
				EdDrawOutline(child, color);
			}
			break;
		}
		default: break;
	}
}

template <typename F>
void EdForeach(F&& func, EdOp* op) {
	if (!func(op))
		return;
	for (EdOp* child : op->children) EdForeach(func, child);
}

int EdGetSiblingIndex(EdOp* op) {
	if (!op->parent) return -1;
	for (int i = 0; i < op->parent->children.size(); i++)
		if (op->parent->children[i] == op)
			return i;
	assert(0);
	return -1;
}

void EdOrderMove(Editor* ed, int offset) {
	std::vector<EdOp*> selection = EdGetSelectedOps(ed);
	if (selection.size() == 0) return;

	EdOp* parent = selection[0]->parent;

	std::sort(selection.begin(), selection.end(), [](EdOp* a, EdOp* b) { return EdGetSiblingIndex(a) < EdGetSiblingIndex(b); });

	int idx = EdGetSiblingIndex(selection[0]) + offset;
	if (idx < 0) idx = 0;
	if (idx >= parent->children.size()) idx = parent->children.size() - 1;

	for (EdOp* op : selection) {
		parent->children.erase(parent->children.begin() + EdGetSiblingIndex(op));
	}
	parent->children.insert(parent->children.begin() + idx, selection.begin(), selection.end());
}

void EdOpAddChild(EdOp* parent, EdOp* child, int idx = -1) {
	assert(!child->parent);

	if (idx >= 0) {
		parent->children.insert(parent->children.begin() + idx, child);
	} else {
		parent->children.push_back(child);
	}
	child->parent = parent;
}

EdOp* EdGroup(std::vector<EdOp*> ops) {
	if (ops.size() == 0) return nullptr;
	if (ops.size() == 1) return ops[0];

	EdOp* parent = ops[0]->parent;

	std::sort(ops.begin(), ops.end(), [](EdOp* a, EdOp* b) { return EdGetSiblingIndex(a) < EdGetSiblingIndex(b); });
	int idx = EdGetSiblingIndex(ops[0]);

	for (EdOp* op : ops) {
		assert(op->parent == parent);
		EdRemoveOpFromTree(op);
	}

	EdOp* group = EdCreateOp();
	group->type = EdOpType_Stack;

	for (EdOp* op : ops) EdOpAddChild(group, op);
	EdOpAddChild(parent, group, idx);

	return group;
}

void EdUngroup(EdOp* group) {
	if (group->type != EdOpType_Stack) return;
	assert(group->parent);

	EdOp* parent = group->parent;
	int idx = EdGetSiblingIndex(group);

	std::vector<EdOp*> ops = std::move(group->children);
	group->children	 = {};
	for (EdOp* child : ops) {
		child->parent = group->parent;
	}
	EdRemoveOpFromTree(group);

	parent->children.insert(parent->children.begin() + idx, ops.begin(), ops.end());
	EdDestroyOp(group);
}

void EdBuild(Editor* ed, EdOp* op) {
	for (CSGBrush* brush : op->built) CSGDestroyBrush(brush);
	op->built.clear();

	switch (op->type) {
		case EdOpType_Brush: {
			op->built = { CSGCloneBrush(op->brush) };
			op->built[0]->sources[0] = op;
			CSGBrushTransform(op->built[0], op->global_transform);
			break;
		}
		case EdOpType_Stack: {
			for (EdOp* child : op->children) {
				float4x4 child_transform = float4x4::Identity();
				glm_translate(child_transform, child->position);
				// glm_rotate(m, angle, (vec3){ ax, ay, az });
				// glm_scale(m, (vec3){ sx, sy, sz });

				child->global_transform = child_transform * op->global_transform;
				EdBuild(ed, child);
				for (CSGBrush* b : child->built) {
					std::vector<CSGBrush*> new_set;
					for (CSGBrush* a : op->built) {
						int old_size = new_set.size();
						CSGDifference(a, b, new_set);
					}
					if (!child->subtract) {
						CSGBrush* clone = CSGCloneBrush(b);
						clone->sources[0] = child;
						new_set.push_back(clone);
					}
					op->built = new_set;
				}
			}
			break;
		}
		default: assert(0); break;
	}

	if (op == ed->m_root) {
		for (CSGBrush* b : op->built) CSGBuildBrushMesh(b);
	}
}

EdOp* EdCloneOp(EdOp* orig) {
	EdOp* clone = EdCreateOp();
	clone->type = orig->type;
	clone->subtract = orig->subtract;
	clone->position = orig->position;

	switch (orig->type) {
		case EdOpType_Brush: {
			clone->brush = CSGCloneBrush(orig->brush);
			CSGBuildBrush(clone->brush);
			break;
		}
		case EdOpType_Stack: {
			for (EdOp* orig_child : orig->children) {
				EdOpAddChild(clone, EdCloneOp(orig_child));
			}
			break;
		}
		default: assert(0);
	}
	return clone;
}

struct Res2 {
	EdOp* op       = nullptr;
	float t        = FLT_MAX;
	bool  sub      = false;
};

Res2 EdRaycastAgainstSubtractOpsImpl(const Ray& ray, EdOp* current) {
	switch (current->type) {
		case EdOpType_Brush: {
			int plane;
			float t = Intersect(ray, current->brush, current->global_transform, &plane);
			if (t >= 0.0f) return { current, t, current->subtract };
			else return {};
		}
		case EdOpType_Stack: {
			Res2 best_match = {};

			if (current->subtract) {
				for (EdOp* child : current->children) {
					Res2 candidate = EdRaycastAgainstSubtractOpsImpl(ray, child);
					if (candidate.t < best_match.t)
						best_match = candidate;
				}
				best_match.op = current;
				best_match.sub = true;
			}
			else {
				for (EdOp* child : current->children) {
					Res2 candidate = EdRaycastAgainstSubtractOpsImpl(ray, child);
					if (candidate.t < best_match.t && (candidate.sub || !best_match.sub))
						best_match = candidate;
				}
			}
			return best_match;
		}
		default: break;
	}
	assert(0);
	return {};
}

bool EdRaycastAgainstSubtractOps(Editor* ed, const Ray& ray, float* out_t, EdOp** out_hit) {
	auto res = EdRaycastAgainstSubtractOpsImpl(ray, ed->m_root);
	if (res.op) {
		*out_hit = res.op;
		*out_t = res.t;
		return true;
	}
	else return false;
}

bool EdRaycastAgainstBuiltBrushes(Editor* ed, const Ray& ray, float* out_t, EdOp** out_op_hit, CSGBrush** out_built_brush_hit) {
	float min_t = FLT_MAX;
	CSGBrush* hit = nullptr;
	for (CSGBrush* brush : ed->m_root->built) {
		int plane_hit;
		float t = Intersect(ray, brush, float4x4::Identity(), &plane_hit);
		if (t < min_t && t >= 0) {
			hit = brush;
			min_t = t;
		}
	}

	if (min_t < FLT_MAX) {
		if (out_t) *out_t = min_t;
		if (out_op_hit) *out_op_hit = hit->sources[0];
		if (out_built_brush_hit) *out_built_brush_hit = hit;
		return true;
	} else {
		if (out_t) *out_t = FLT_MAX;
		if (out_op_hit) *out_op_hit = nullptr;
		if (out_built_brush_hit) *out_built_brush_hit = nullptr;
		return false;
	}
}

AABB EdGetEntityAABB(Entity* entity) {
	float3 center = entity->position;
	AABB aabb;
	aabb.min = center - float3{0.5, 0.5, 0.5} / 2.0f;
	aabb.max = center + float3{0.5, 0.5, 0.5} / 2.0f;
	return aabb;
}

bool EdRaycastAgainstEntities(Editor* ed, const Ray& ray, float* out_t, Entity** out_hit) {
	float min_t = FLT_MAX;
	Entity* hit = nullptr;
	ed->m_entityManager->Iterate([&](Entity* entity) {
		float t = Intersect(ray, EdGetEntityAABB(entity));
		if (t >= 0 && t < min_t)
		{
			min_t = t;
			hit = entity;
		}
	});

	if (hit) {
		if (out_t) *out_t = min_t;
		if (out_hit) *out_hit = hit;
		return true;
	} else {
		if (out_t) *out_t = FLT_MAX;
		if (out_hit) *out_hit = nullptr;
		return false;
	}
}

void EdOpGUI(Editor* ed, EdOp* op) {
	UIPushId(op);
	DEFER(UIPopId());

	UITreeNodeFlags flags = 0;
	if (EdIsSelected(ed, op)) flags |= UITreeNodeFlags_Selected;
	if (op->type == EdOpType_Brush) flags |= UITreeNodeFlags_Leaf;
	flags |= UITreeNodeFlags_DefaultOpen;

	const char* name = op->type == EdOpType_Brush ? "Brush" : "Stack";

	UIBox* tree_node = 0;
	if (UIBeginTreeNode(name, &tree_node, flags)) {
		switch (op->type) {
			case EdOpType_Stack: {
				for (EdOp* child : op->children) {
					EdOpGUI(ed, child);
				}
				break;
			}
			default: break;
		}
		UIEndTreeNode();
	}
	if (tree_node->Clicked()) {
		EdSelectOp(ed, op, InputGetButton(SDL_SCANCODE_LCTRL));
	}
}

float3 EdSnapToGrid(EdGrid& grid, float3 p) {
	// TODO: make a 3x3 matrix for the grid, do a basis change, figure out which axis we're snapping on, blablabla
	p.x = round(p.x / grid.size) * grid.size;
	p.y = round(p.y / grid.size) * grid.size;
	p.z = round(p.z / grid.size) * grid.size;
	return p;
}

void EdArrowGizmo(Editor* ed, Hash hash, float3& pos, float3 direction, float4 color, float base_scale = 1, bool hidden = false, bool force_activate = false) {
	U32 id = ed->m_hashStack.Push(hash);
	DEFER(ed->m_hashStack.Pop());

	const float scale = Distance(pos, ed->m_camera->position) * 0.1;
	const float cone_scale = 0.075f;

	const float3 a = pos;
	const float3 b = pos + direction * scale * base_scale;

	const float3 a_screen = CameraWorldToScreen(*ed->m_camera, a);
	const float3 b_screen = CameraWorldToScreen(*ed->m_camera, b);
	const float2 nearest_screen = NearestPointToLineSegment(a_screen.xy(), b_screen.xy(), g_mouse_position);

	const float nearest_screen_t = Unlerp(a_screen.xy(), nearest_screen, b_screen.xy());

	const float screen_dist = Distance(nearest_screen, g_mouse_position);
	const Ray ray_to_nearest = CameraScreenToRay(*ed->m_camera, nearest_screen);

	if (!hidden && screen_dist < 20 && nearest_screen_t >= 0.0f && nearest_screen_t <= (1.0f + 0.2 / base_scale) && screen_dist < ed->m_newHoverGizmoState.screen_dist) {
		ed->m_newHoverGizmoState.id = id;
		ed->m_newHoverGizmoState.screen_dist = screen_dist;
	}

	if (ed->m_activeGizmoState.id == id || ed->m_hoverGizmoState.id == id) {
		color.x += 0.7;
		color.y += 0.7;
		color.z += 0.7;
	}

	if (!hidden) { // draw:
		DrawSetLayer(Layer_Overlay);
		float4 cone_rotation;
		glm_quat_from_vecs(float3(0,0,1), (b - a).Normalized(), cone_rotation);
		DrawLine(a, b, color);
		DrawMesh(Library::mesh_cone, nullptr, float4x4::FromTransform(b, cone_rotation, float3(cone_scale * scale, cone_scale * scale, cone_scale * scale)), color);
	}

	float t;
	DistanceToLineSegment(ray_to_nearest, a, b, nullptr, &t);
	float3 nearest_world = a + (b - a).Normalized() * t;

	if (force_activate || (ed->m_hoverGizmoState.id == id && !ed->m_activeGizmoState.id && !UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT))) {
		ed->m_activeGizmoState.id = id;
		ed->m_activeGizmoState.offset = nearest_world - pos;
	}

	if (ed->m_activeGizmoState.id == id) {
		pos = nearest_world - ed->m_activeGizmoState.offset;
		if (EdShouldSnap(ed)) pos = EdSnapToGrid(ed->m_grid, pos);
	}
}

void EdTranslationGizmo(Editor* ed, Hash hash, float3& pos) {
	ed->m_hashStack.Push(hash);
	EdArrowGizmo(ed, 1, pos, float3(1, 0, 0), float4(1, 0, 0, 1));
	EdArrowGizmo(ed, 2, pos, float3(0, 1, 0), float4(0, 1, 0, 1));
	EdArrowGizmo(ed, 3, pos, float3(0, 0, 1), float4(0, 0, 1, 1));
	ed->m_hashStack.Pop();
}

void EdDrawSelectionOutline(Editor* ed, float4 color) {
	for (const EdSelection& sel : ed->m_selection) {
		switch (sel.type) {
			case EdSelectionType_Node: {
				EdDrawOutline(sel.op, color);
				break;
			}
			case EdSelectionType_BrushPlane: {
				EdDrawBrushPlaneOutline(sel.op->brush, sel.index, sel.op->global_transform, color);
				break;
			}
			case EdSelectionType_Entity: {
				DrawAABB(sel.entity->position, {0.5, 0.5, 0.5}, color);
				break;
			}
			default: {
				break;
			}
		}
	}
}

bool EdDoPlaneDragGizmo(Editor* ed, EdOp* op, CSGBrush* brush, int idx) {
	CSGPlane& plane = brush->planes[idx];
	if (plane.points.size() == 0) return false;

	ed->m_hashStack.Push(brush);
	ed->m_hashStack.Push(EdSelectionType_BrushPlane);
	DEFER(ed->m_hashStack.Pop());
	DEFER(ed->m_hashStack.Pop());


	float3 c1 = (plane.points[0] - plane.points[1]).Normalized();
	float3 c2 = Cross(c1, plane.plane.normal).Normalized();

	c1 *= 0.05f;
	c2 *= 0.05f;

	float3 com = {};
	for (float3 p : plane.points) com += p;
	com /= plane.points.size();

	float3 comx = op->global_transform.TransformPosition(com);

	float3 p00 = op->global_transform.TransformPosition(com - c1 - c2);
	float3 p01 = op->global_transform.TransformPosition(com - c1 + c2);
	float3 p10 = op->global_transform.TransformPosition(com + c1 - c2);
	float3 p11 = op->global_transform.TransformPosition(com + c1 + c2);

	ed->m_hashStack.Push((int)EdSelectionType_BrushPlane);
	ed->m_hashStack.Push(op);
	U32 id = ed->m_hashStack.Push(idx);
	DEFER(ed->m_hashStack.Pop());
	DEFER(ed->m_hashStack.Pop());
	DEFER(ed->m_hashStack.Pop());

	Ray mouse_ray = CameraScreenToRay(*ed->m_camera, g_mouse_position);

	float screen_dist = Distance(CameraWorldToScreen(*ed->m_camera, comx).xy(), g_mouse_position);
	if (screen_dist < 10 || IntersectTriangle(mouse_ray, p00, p01, p11) > 0.0f || IntersectTriangle(mouse_ray, p00, p11, p10) > 0.0f) {
		if (screen_dist < ed->m_newHoverGizmoState.screen_dist) {
			ed->m_newHoverGizmoState.id = id;
			ed->m_newHoverGizmoState.screen_dist = screen_dist;
		}
	}

	float4 color = ed->m_hoverGizmoState.id == id ? float4(1,0,1,1) : float4(1,1,1,.5);

	DrawSetLayer(Layer_Overlay);
	DrawLine(p00, p10, color);
	DrawLine(p00, p01, color);
	DrawLine(p10, p11, color);
	DrawLine(p01, p11, color);

	bool activate = !ed->m_activeGizmoState.id && ed->m_hoverGizmoState.id == id && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT);

	float3 new_comx = comx;
	EdArrowGizmo(ed, idx, new_comx, plane.plane.normal, {1,1,0,1}, 1.0f, true, activate);
	float3 d = new_comx - comx;

	float add = Dot(d, plane.plane.normal);
	if (abs(add) > 0.001f) {
		plane.plane.distance += add;
		CSGBuildBrush(brush);
		return true;
	} else {
		return false;
	}
}

void EdDrawGrid(EdGrid& grid, float4 color) {
	int S = 50;

	float3 fwd = grid.forward * grid.size;
	float3 rgh = grid.right * grid.size;

	for (int i = -S; i <= S; i++) {
		DrawLine(
			grid.center - fwd * S + rgh * i,
			grid.center + fwd * S + rgh * i,
			color);
		DrawLine(
			grid.center - rgh * S + fwd * i,
			grid.center + rgh * S + fwd * i,
			color);
	}
}

void SelectTool::Tick(double dt) {
	Editor* ed = m_editor;
	Ray mouse_ray = CameraScreenToRay(*ed->m_camera, g_mouse_position);
	bool dirty = false;

	std::vector<EdOp*> selected_ops = {};
	for (const EdSelection& sel : ed->m_selection) {
		if (sel.type == EdSelectionType_Node) {
			if (sel.op->type == EdOpType_Brush) {
				for (int i = 0; i < sel.op->brush->planes.size(); i++) {
					if (EdDoPlaneDragGizmo(ed, sel.op, sel.op->brush, i)) dirty = true;
				}
			}
			selected_ops.push_back(sel.op);
		}
		if (sel.type == EdSelectionType_BrushPlane && !EdIsSelected(ed, sel.op)) {
			if (EdDoPlaneDragGizmo(ed, sel.op, sel.op->brush, sel.index)) dirty = true;
		}
	}

	if (selected_ops.size()) {
		if (!ed->m_activeGizmoState.id) {
			m_gizmoCenter = {};
			for (EdOp* op : selected_ops)
			{
				m_gizmoCenter += op->global_transform.column(3).xyz();
			}
			m_gizmoCenter /= selected_ops.size();
		}

		ed->m_hashStack.Push(&m_gizmoCenter);
		DEFER(ed->m_hashStack.Pop());

		float3 old_center = m_gizmoCenter;
		EdTranslationGizmo(ed, &m_gizmoCenter, m_gizmoCenter);
		float3 d = m_gizmoCenter - old_center;

		if (abs(d.Length()) > 0.0001f)
		{
			for (EdOp* op : selected_ops) op->position += d;
			dirty = true;
		}
	}
	
	if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT) && !ed->m_activeGizmoState.id) {
		float min_t = FLT_MAX;
		EdOp* hit_op = nullptr;
		CSGBrush* hit_brush = nullptr;

		bool hit = false;
		if (InputGetButton(SDL_SCANCODE_LALT))
			hit = EdRaycastAgainstSubtractOps(ed, mouse_ray, &min_t, &hit_op);
		else
			hit = EdRaycastAgainstBuiltBrushes(ed, mouse_ray, &min_t, &hit_op, nullptr);

		float entity_t = FLT_MAX;
		Entity* hit_entity = nullptr;
		bool hit_ent = EdRaycastAgainstEntities(ed, mouse_ray, &entity_t, &hit_entity);

		if (hit_ent && (!hit || entity_t < min_t)) 
			EdSelectEntity(ed, hit_entity, InputGetButton(SDL_SCANCODE_LSHIFT));
		else if (hit)
			EdSelectOp(ed, hit_op, InputGetButton(SDL_SCANCODE_LSHIFT));
		else if (!InputGetButton(SDL_SCANCODE_LSHIFT))
			EdDeselect(ed);
	}

	if (dirty)
		EdBuild(ed, ed->m_root);
}

void BrushTool::Tick(double dt)
{
	Editor* ed = m_editor;

	Ray mouse_ray = CameraScreenToRay(*ed->m_camera, g_mouse_position);

	float min_t = FLT_MAX;
	bool hit = EdRaycastAgainstBuiltBrushes(ed, mouse_ray, &min_t, nullptr, nullptr);
	float3 p = {};
	if (hit) {
		p = mouse_ray.Evaluate(min_t);
		if (EdShouldSnap(ed)) p = EdSnapToGrid(ed->m_grid, p);
	}

	if (InputGetButton(SDL_SCANCODE_ESCAPE)) {
		m_phase = Phase_Inactive;
	}

	if (hit) {
		DrawSetLayer(Layer_Overlay);
		DrawPoint(m_start, {0,0,1,0.1});
		DrawSetLayer(Layer_Main);
		DrawPoint(m_start, {0,0,1,1});
	}

	Step:
	switch (m_phase) {
		case Phase_Inactive: {
			if (hit) {
				m_start = p;
				if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT)) {
					m_phase = Phase_InitialDraw;
				}
			}
			break;
		}
		case Phase_InitialDraw: {
			m_end = p;
			if (InputGetButtonUp(INPUT_BUTTON_MOUSE_LEFT)) {
				if (hit) {
					m_justEnteredPhase = false;
					m_phase = Phase_X;
				} else {
					m_phase = Phase_Inactive;
				}
				goto Step;
			}
			break;
		}
		case Phase_X:
		case Phase_Y:
		case Phase_Z: {
			float3 dir = {};
			dir[m_phase - Phase_X] = 1.0f;

			if (!m_justEnteredPhase) {
				m_justEnteredPhase = true;
				float len = Dot(dir, m_end - m_start);
				if (len != 0)
				{
					m_justEnteredPhase = false;
					m_phase = (Phase)(m_phase + 1);
					goto Step;
				}

				m_refA = m_end;
				m_refB = m_refA + dir;
				m_axisStart = m_end;
			}

			float t1, t2;
			DistanceToLineSegment(mouse_ray, m_refA, m_refB, &t1, &t2);

			m_end = m_axisStart + t2 * dir;
			if (EdShouldSnap(ed)) m_end = EdSnapToGrid(ed->m_grid, m_end);
			if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT)) {
				m_justEnteredPhase = false;
				m_phase = (Phase)(m_phase + 1);
			}
			break;
		}
		case Phase_Finalize: {
			if (m_end.x < m_start.x) std::swap(m_start.x, m_end.x);
			if (m_end.y < m_start.y) std::swap(m_start.y, m_end.y);
			if (m_end.z < m_start.z) std::swap(m_start.z, m_end.z);

			CSGBrush* cube = CSGCreateCube(m_end - m_start);
			CSGBuildBrush(cube);
			EdOp* op = EdCreateOp();
			op->type = EdOpType_Brush;
			op->brush = cube;
			op->position = m_start;
			EdOpAddChild(ed->m_root, op);
			EdBuild(ed, ed->m_root);
			EdSelectOp(ed, op);
			m_phase = Phase_Inactive;
			break;
		}
	}

	if (m_phase != Phase_Inactive) {
		if (!(m_phase == Phase_InitialDraw && !hit)) {
			DrawSetLayer(Layer_Overlay);
			DrawBox(m_start, m_end, {0, 0, 1, 0.1 });
			DrawSetLayer(Layer_Main);
			DrawBox(m_start, m_end, {0, 0, 1, 1 });
		}
	}
}

void EntityTool::Tick(double dt) {
	Editor* ed = m_editor;
	Ray mouse_ray = CameraScreenToRay(*ed->m_camera, g_mouse_position);

	Entity* hit_entity = nullptr;
	if (EdRaycastAgainstEntities(ed, mouse_ray, nullptr, &hit_entity)) {
		if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT)) {
			EdSelectEntity(ed, hit_entity);
		}
		return;
	}

	float min_t = FLT_MAX;
	if (EdRaycastAgainstBuiltBrushes(ed, mouse_ray, &min_t, nullptr, nullptr)) {
		float3 p = {};
		p = mouse_ray.Evaluate(min_t);
		if (EdShouldSnap(ed)) p = EdSnapToGrid(ed->m_grid, p);
		DrawPoint(p, {0,1,0,1});

		if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT)) {
			if (m_entityType) EdCreateEntity(ed, m_entityType, p);
			EdDeselect(ed);
		}
	}
}

void EntityTool::DrawSidebar() {
	if (UIBeginTreeNode("Entity Type", nullptr, UITreeNodeFlags_DefaultOpen)) {
		for (Type* type : Entity::StaticClass()->subclasses) {
			UIButtonFlags flags = UIButtonFlags_Small;
			if (m_entityType == type) flags |= UIButtonFlags_Toggle;
			if (UIButton(type->name, flags)) m_entityType = type;
		}
		UIEndTreeNode();
	}
}

void BrushTool::OnDeactivate() {
	m_phase = Phase_Inactive;
}

void Editor::SetTool(Tool* tool) {
	if (!tool || m_tool == tool) return;
	if (m_tool) m_tool->OnDeactivate();
	m_tool = tool;
	m_tool->OnActivate();
	ScreenLog("Tool: %s", m_tool->Name().c_str());
}

Editor::Editor(Game* game, EntityManager* entity_manager)
	: m_game(game), m_entityManager(entity_manager) {
	m_camera = new ECamera();
	m_camera->eid = EID_DefaultCamera;
	CameraInit(*m_camera);
	m_camera->position.y = -10;
	m_camera->position.z = 3;
	m_game->m_activeCamera = m_camera;
	m_grid.size = ED_GRID_SIZES[m_gridSizeIdx];
	m_root = EdCreateOp();
	m_root->type = EdOpType_Stack;
	m_root->global_transform = float4x4::Identity();

	for (Type* tool_type : Tool::StaticClass()->subclasses) {
		Tool* tool = (Tool*)tool_type->Instantiate(Allocator::HeapAllocator);
		tool->m_editor = this;
		m_tools.push_back(tool);
	}
	std::sort(m_tools.begin(), m_tools.end(), [](Tool* a, Tool* b) {
		return a->GetOrder() < b->GetOrder();
	});

	for (Tool* tool : m_tools) {
		if (tool->GetClass() == SelectTool::StaticClass()) {
			SetTool(tool);
			break;
		}
	}
}

Editor::~Editor() {
	if (m_tool) m_tool->OnDeactivate();
	for (Tool* tool : m_tools) {
		tool->~Tool();
		Allocator::HeapAllocator.Free(tool);
	}
	delete m_camera;
}

void Editor::Tick(double dt) {
	Editor* ed = this;
	CameraFly(*m_camera);
	CameraUpdateMatrices(*m_camera);

	DrawSetLayer(Layer_Main);
	// EdDrawGrid(m_grid, float4{1,1,1,0.3});

	m_hashStack.Reset();
	m_newHoverGizmoState = {};
	m_newHoverGizmoState.screen_dist = FLT_MAX;
	m_newHoverGizmoState.world_dist = FLT_MAX;

	if (m_tool) m_tool->Tick(dt);

	m_entityManager->Iterate([&](Entity* ent) {
		DrawAABB(ent->position, {0.5,0.5,0.5}, {0,1,0,1});
		DrawPoint(ent->position, {0,1,0,1});
	});

	DrawSetLayer(Layer_Main);
	EdDrawSelectionOutline(ed, {1,1,1,1});
	DrawSetLayer(Layer_Overlay);
	EdDrawSelectionOutline(ed, {1,1,1,0.1});

	if (cvar_ed_show_sub.fvalue) {
		EdForeach([](EdOp* op) {
			if (op->subtract)
			{
				for (CSGBrush* b : op->built)
				{
					EdDrawBrushOutline(b, op->global_transform, {1,0,0.2,0.3});
				}
			}
			return true;
		}, ed->m_root);
	}

	{ // Toolbar
		UIBeginBox()
			->SetSize(g_window_size.x, 0)
			->SetFlex(UIAxis_Horizontal, UIAlignment_Start, UIAlignment_Center)
			->SetColor(COLOR_BG)
			->SetGap(4)
			->SetPadding(4);
		DEFER(UIEndBox());

		auto spacer = []() {
			UIBeginBox()->SetSize(9, 0)->SetFlex(UIAxis_Horizontal, UIAlignment_Center, UIAlignment_Center);
			UIBeginBox()->SetSize(3, 3)->SetColor(COLOR_BUTTON);
			UIEndBox();
			UIEndBox();
		};

		char buf[32];
		snprintf(buf, 32, "Snap: %.3f", m_grid.size);
		if (UIButton(buf, UIButtonFlags_Small | ((EdShouldSnap(this)) ? UIButtonFlags_Toggle : 0))) m_snap = !m_snap;

		if (UIButton("-", UIButtonFlags_Small | (m_snap ? 0 : UIButtonFlags_Disabled))) ConExec("ed_grid_size_inc -1");
		if (UIButton("+", UIButtonFlags_Small | (m_snap ? 0 : UIButtonFlags_Disabled))) ConExec("ed_grid_size_inc +1");

		spacer();

		for (Tool* tool : m_tools) {
			UIButtonFlags flags = UIButtonFlags_Small;
			if (tool == m_tool) flags |= UIButtonFlags_Toggle;
			if (UIButton(tool->GetShortName().c_str(), flags)) SetTool(tool);
		}

		spacer();

		EdOp* first_sel = (ed->m_selection.size() && ed->m_selection[0].type == EdSelectionType_Node) ? ed->m_selection[0].op : nullptr;
		bool sub = first_sel && first_sel->subtract;

		if (UIButton("Sub", UIButtonFlags_Small | (first_sel ? 0 : UIButtonFlags_Disabled) | (sub ? UIButtonFlags_Toggle : 0u))) {
			for (EdSelection& sel : ed->m_selection)
				if (sel.type == EdSelectionType_Node)
					sel.op->subtract = !sub;
			EdBuild(ed, ed->m_root);
		}

		if (UIButton("X", UIButtonFlags_Small | (first_sel ? 0 : UIButtonFlags_Disabled))) {
			ConExec("ed_del");
		}
	}

	{ // Sidebar
		UIBeginBox()
			->SetPosition(0, 30)
			->SetSize(200, g_window_size.y)
			->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch)
			->SetColor(COLOR_BG);
		DEFER(UIEndBox());


		if (m_tool) m_tool->DrawSidebar();

		if (UIBeginTreeNode("Brushes")) {
			for (EdOp* child : ed->m_root->children)
				EdOpGUI(ed, child);
			UIEndTreeNode();
		}

		if (UIBeginTreeNode("Entities")) {
			m_entityManager->Iterate([&](Entity* entity) {
				UIPushId(entity);
				DEFER(UIPopId());

				UITreeNodeFlags flags = UITreeNodeFlags_Leaf;
				if (EdIsSelected(ed, entity)) flags |= UITreeNodeFlags_Selected;

				UIBox* tree_node = nullptr;
				if (UIBeginTreeNode("Entity", &tree_node, flags)) UIEndTreeNode();

				if (tree_node->Clicked())
				{
					EdSelectEntity(ed, entity, InputGetButton(SDL_SCANCODE_LCTRL));
				}
			});
			UIEndTreeNode();
		}
	}

	DrawSetLayer(Layer_Main);
	for (int i = 0; i < ed->m_root->built.size(); i++) {
		CSGBrush* b = ed->m_root->built[i];
		// DrawMesh(b->mesh, Library::mat_brush, float4x4::Identity(), brush_colors[i % EVA_ARRAYSIZE(brush_colors)]);
		DrawMesh(b->mesh, Library::mat_brush, float4x4::Identity(), COLOR_WHITE);
	}

	m_hoverGizmoState = m_newHoverGizmoState;
	if (InputGetButtonUp(INPUT_BUTTON_MOUSE_LEFT))
		m_activeGizmoState = {};
}

void EdIndent(FILE* f, int indent) {
	for (int i = 0; i < indent; i++) fprintf(f, "\t");
}

void EdSaveOp(FILE* f, EdOp* op, int indent) {
	EdIndent(f, indent); fprintf(f, "op %d\n", op->type);

	indent++;

	EdIndent(f, indent); fprintf(f, "subtract %d\n", op->subtract);
	EdIndent(f, indent); fprintf(f, "position %f %f %f\n", XYZ(op->position));

	switch (op->type) {
		case EdOpType_Brush: {
			EdIndent(f, indent); fprintf(f, "planes %d\n", (int)op->brush->planes.size());
			indent++;
			for (int i = 0; i < op->brush->planes.size(); i++)
			{
				CSGPlane& plane = op->brush->planes[i];
				EdIndent(f, indent); fprintf(f, "plane %f %f %f %f\n", XYZ(plane.plane.normal), plane.plane.distance);
			}
			indent--;
			break;
		}
		case EdOpType_Stack: {
			EdIndent(f, indent); fprintf(f, "children %d\n", (int)op->children.size());
			for (int i = 0; i < op->children.size(); i++) {
				EdSaveOp(f, op->children[i], indent + 1);
			}
			break;
		}
		default: assert(0);
	}

	indent--;
	EdIndent(f, indent); fprintf(f, "op_end\n");
}

EdOp* EdLoadOp(FILE* f) {
	int n;
	EdOp* op = EdCreateOp();
	n = fscanf(f, "op %d\n", &op->type);
	assert(n == 1);

	switch (op->type) {
		case EdOpType_Brush: {
			op->brush = CSGCreateBrush();
			break;
		}
		default: break;
	}
	assert(op->type > EdOpType_None && op->type < EdOpType_ENUM_SIZE);

	for (;;) {
		char t[32] = {};
		n = fscanf(f, "%s", t);
		assert(n == 1);

		if (strcmp(t, "subtract") == 0) {
			int sub;
			n = fscanf(f, "%d\n", &sub);
			assert(n == 1);
			op->subtract = sub;
		} else if (strcmp(t, "position") == 0) {
			n = fscanf(f, "%f %f %f\n", XYZ(&op->position));
			assert(n == 3);
		} else if (strcmp(t, "planes") == 0) {
			assert(op->type == EdOpType_Brush);

			int num_planes;
			n = fscanf(f, "%d\n", &num_planes);
			assert(n == 1);

			for (int i = 0; i < num_planes; i++)
			{
				Plane plane;
				n = fscanf(f, "plane %f %f %f %f\n", XYZ(&plane.normal), &plane.distance);
				assert(n == 4);

				op->brush->planes.push_back({ .plane = plane });
			}
		} else if (strcmp(t, "children") == 0) {
			int num_children;
			n = fscanf(f, "%d\n", &num_children);
			assert(num_children >= 0 && num_children < 1000);
			for (int i = 0; i < num_children; i++)
			{
				EdOp* child = EdLoadOp(f);
				op->children.push_back(child);
				child->parent = op;
			}
		} else if (strcmp(t, "op_end") == 0) {
			fscanf(f, "\n");
			break;
		}
	}

	switch (op->type) {
		case EdOpType_Brush: {
			CSGBuildBrush(op->brush);
			break;
		}
		default: break;
	}

	return op;
}

Result EdSaveMap(Editor* ed, const char* name) {
	char path[256];
	snprintf(path, 256, "%s/Assets/%s.mpe", EVA_BASE_DIR, name);
	FILE* f = fopen(path, "wb");
	if (!f) return Err("Failed to open %s", name);
	DEFER(fclose(f));

	fprintf(f, "type mpe\n");
	fprintf(f, "version 1\n");
	EdSaveOp(f, ed->m_root, 0);

	int num_entities = 0;
	ed->m_entityManager->Iterate([&](Entity* entity) { num_entities++; });
	fprintf(f, "entities %d\n", num_entities);
	ed->m_entityManager->Iterate([&](Entity* entity) { EntitySave(f, entity, 1); });

	snprintf(ed->m_loadedMapName, sizeof(ed->m_loadedMapName), "%s", name);
	return Success();
}

void EdUnloadMap(Editor* ed) {
	if (ed->m_root) {
		EdDestroyOp(ed->m_root);
		ed->m_root = nullptr;
	}

	ed->m_entityManager->Reset();
	ed->m_entityManager->Init();
}

Result Editor::LoadMap(String name) {
	Editor* ed = this;
	char path[256];
	snprintf(path, 256, "%s/Assets/%.*s.mpe", EVA_BASE_DIR, STRING_PRINTF_ARGS(name));
	FILE* f = fopen(path, "rb");
	if (!f) return Err("Failed to open %s", name);
	DEFER(fclose(f));

	EdUnloadMap(this);

	int version = 0;
	fscanf(f, "type mpe\n");
	fscanf(f, "version %d\n", &version);
	if (version != 1) {
		return Err("map %s is version %d, expected %d", name, version, 1);
	}

	ed->m_nextEID = EID_MapStart;

	ed->m_root = EdLoadOp(f);
	ed->m_root->global_transform = float4x4::Identity();

	int num_entities;
	int n = fscanf(f, "entities %d\n", &num_entities);
	if (n != 1) return Err("failed to load map");
	assert(num_entities >= 0 && num_entities < 1000);

	for (int i = 0; i < num_entities; i++) {
		Entity* ent = nullptr;
		TRY(EntityLoad(&ent, m_entityManager, f));
		if (ent->eid >= ed->m_nextEID) ed->m_nextEID = ent->eid + 1;
	}

	EdBuild(ed, ed->m_root);
	snprintf(ed->m_loadedMapName, sizeof(ed->m_loadedMapName), "%.*s", STRING_PRINTF_ARGS(name));

	return Success();
}

Result EdCompileMap(Editor* ed) {
	int indent = 0;

	char path[256];
	assert(ed->m_loadedMapName[0]);
	snprintf(path, 256, "%s/Assets/%s.map", EVA_BASE_DIR, ed->m_loadedMapName);
	FILE* f = fopen(path, "wb");
	if (!f) return Err("Failed to open %s", path);
	DEFER(fclose(f));

	EdBuild(ed, ed->m_root);

	fprintf(f, "type map\n");
	fprintf(f, "version 1\n");

	std::vector<MeshVertex> mesh_vertices;
	std::vector<U32> mesh_indices;

	for (CSGBrush* brush : ed->m_root->built) {
		for (const CSGPlane& plane : brush->planes) {
			int start = mesh_vertices.size();
			for (int i = 0; i < plane.points.size(); i++) {
				mesh_vertices.push_back({
					.position = plane.points[i],
					.normal = plane.plane.normal,
				});
			}
			for (int i = 2; i < plane.points.size(); i++) {
				mesh_indices.push_back(start);
				mesh_indices.push_back(start + i - 1);
				mesh_indices.push_back(start + i);
			}
		}
	}

	EdIndent(f, indent); fprintf(f, "vertices %d\n", (int)mesh_vertices.size());
	for (const MeshVertex& vert : mesh_vertices) {
		EdIndent(f, indent); fprintf(f, "%f %f %f %f %f %f ", XYZ(vert.position), XYZ(vert.normal));
	}
	fprintf(f, "\n");

	EdIndent(f, indent); fprintf(f, "indices %d ", (int)mesh_indices.size());
	for (U32 idx : mesh_indices) {
		fprintf(f, "%u ", idx);
	}
	fprintf(f, "\n");

	int num_entities = 0;
	ed->m_entityManager->Iterate([&](Entity* entity) { num_entities++; });
	fprintf(f, "entities %d\n", num_entities);
	ed->m_entityManager->Iterate([&](Entity* entity) { EntitySave(f, entity, 1); });

	return Success();
}

void EdInitialize() {
	ConRegisterVar(&cvar_ed_show_sub);

	ConRegisterCommand("ed_cube", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		EdOp* op = EdCreateOp();
		op->brush = CSGCreateCube({ parser.FloatArg(1.0f), parser.FloatArg(1.0f), parser.FloatArg(1.0f) });
		op->type = EdOpType_Brush;
		EdOpAddChild(ed->m_root, op);
		EdSelectOp(ed, op);
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: create a cube");

	ConRegisterCommand("ed_cylinder", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		EdOp* op = EdCreateOp();
		int nseg = parser.FloatArg(12.0f);
		float rad  = parser.FloatArg(1.0f);
		float height = parser.FloatArg(1.0f);
		op->brush = CSGCreateCylinder(nseg, rad, height);
		op->type = EdOpType_Brush;
		EdOpAddChild(ed->m_root, op);
		EdSelectOp(ed, op);
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: create a cylinder");

	ConRegisterCommand("ed_move", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		float3 offset = { parser.FloatArg(0.0f), parser.FloatArg(0.0f), parser.FloatArg(0.0f) };
		for (EdSelection& sel : ed->m_selection)
			if (sel.type == EdSelectionType_Node)
				sel.op->position += offset;
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: create a cube");

	ConRegisterCommand("ed_build", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: rebuild csg");

	ConRegisterCommand("ed_add", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		for (EdSelection& sel : ed->m_selection)
			if (sel.type == EdSelectionType_Node)
				sel.op->subtract = false;
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: set selected to add");

	ConRegisterCommand("ed_sub", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		for (EdSelection& sel : ed->m_selection)
			if (sel.type == EdSelectionType_Node)
				sel.op->subtract = true;
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: set selected to subtract");

	ConRegisterCommand("ed_order_move", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		EdOrderMove(ed, parser.IntArg(1));
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: move selection up/down in stack");

	ConRegisterCommand("ed_group", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		EdSelectOp(ed, EdGroup(EdGetSelectedOps(ed)));
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: group selection into an object");

	ConRegisterCommand("ed_ungroup", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		for (EdSelection& sel : ed->m_selection)
			if (sel.type == EdSelectionType_Node)
				EdUngroup(sel.op);
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: group ungroup selected objects");

	ConRegisterCommand("ed_clone", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		std::vector<EdOp*> selection = EdGetSelectedOps(ed);
		EdDeselect(ed);
		for (EdOp* orig : selection)
		{
			EdOp* clone = EdCloneOp(orig);
			EdOpAddChild(orig->parent, clone, EdGetSiblingIndex(orig) + 1);
			EdSelectOp(ed, clone, true);
		}
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: clone selection");

	ConRegisterCommand("ed_del", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		EdDestroySelection(ed);
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: delete subtract");

	ConRegisterCommand("ed_grid_size_inc", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		ed->m_gridSizeIdx += parser.IntArg(1);
		if (ed->m_gridSizeIdx < 0) ed->m_gridSizeIdx = 0;
		if (ed->m_gridSizeIdx >= EVA_ARRAYSIZE(ED_GRID_SIZES)) ed->m_gridSizeIdx = EVA_ARRAYSIZE(ED_GRID_SIZES) - 1;
		ed->m_grid.size = ED_GRID_SIZES[ed->m_gridSizeIdx];
		Library::mat_brush->texture_scale = 1.0f / ed->m_grid.size / 4.0f;
		ScreenLog("Grid Size: %g", ed->m_grid.size);
		EdBuild(ed, ed->m_root);
		return Success();
	}, "editor: increase/decrease grid size");

	ConRegisterCommand("ed_save", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));

		const char* name = parser.StringArg();
		if (!name) name = ed->m_loadedMapName;
		if (!name[0]) return Err("no map opened");

		return EdSaveMap(ed, name);
	}, "editor: save map .mpe");

	ConRegisterCommand("ed_compile", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		return EdCompileMap(ed);
	}, "editor: compile map");

	ConRegisterCommand("ed", [](ConParser& parser) {
		if (g_active_game) {
			g_active_game->SetGameMode(EditorGameMode::StaticClass());
			return Success();
		} else {
			return Err("no active game");
		}
	}, "editor: open editor");

	ConRegisterCommand("ed_tool", [](ConParser& parser) {
		Editor* ed;
		TRY(GetEditor(&ed));
		int index = parser.IntArg(0);
		if (index < 0 || index >= ed->m_tools.size()) return Err("invalid tool index");
		ed->SetTool(ed->m_tools[index]);
		return Success();
	}, "editor: set tool");

}
