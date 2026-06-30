#include <EVA/App.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Editor.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Collision.hpp>
#include <EVA/Console.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Library.hpp>
#include <EVA/Input.hpp>
#include <EVA/Game.hpp>
#include <EVA/UI.hpp>
#include <EVA/Hashing.hpp>
#include <cglm/quat.h>
#include <algorithm>

enum EdSelectionType
{
	EdSelectionType_None,
	EdSelectionType_Node,
	EdSelectionType_BrushPlane,
};

struct EdSelection
{
	EdSelectionType type   = EdSelectionType_None;
	EdOp*           op     = 0;
	int             index  = 0;
};

Camera g_editor_camera = {};

static EdOp*                    g_root                 = nullptr;
static std::vector<EdSelection> g_selection            = {};
static char                     g_loaded_map_name[64]  = {};

static ConVar cvar_ed_show_sub = {
	.name = "ed_show_sub",
	.help = "show subtract brushes",
	.fvalue = 0,
};

struct EdGizmoState
{
	U32      id             = 0;
	float    world_dist     = 0;
	float    screen_dist    = 0;
	float3   offset         = {};
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

EdGizmoState g_active_gizmo_state = {};
EdGizmoState g_hover_gizmo_state = {};
EdGizmoState g_new_hover_gizmo_state = {};

HashStack hash_stack;

EdOp* EdCreateOp()
{
	EdOp* op = new EdOp();
	return op;
}

void EdDeselect()
{
	g_selection.clear();
}

void EdSelectPlane(EdOp* op, int plane, bool additive = false)
{
	assert(op);
	if (!additive) EdDeselect();
	if (g_selection.size() && g_selection[0].type != EdSelectionType_BrushPlane) return;

	g_selection.push_back(EdSelection{
		.type = EdSelectionType_BrushPlane,
		.op = op,
		.index = plane,
	});
}

void EdSelectOp(EdOp* op, bool additive = false)
{
	assert(op);
	if (!additive) EdDeselect();
	if (op == g_root) return;

	for (const EdSelection& sel : g_selection)
	{
		if (sel.type != EdSelectionType_Node) { EdDeselect();  break; }
		if (sel.op->parent != op->parent) { EdDeselect();  break; }
	}

	g_selection.push_back(EdSelection{
		.type = EdSelectionType_Node,
		.op = op,
	});
}

bool EdIsSelected(EdOp* op)
{
	for (const EdSelection& sel : g_selection)
		if (sel.type == EdSelectionType_Node && sel.op == op)
			return true;
	return false;
}

std::vector<EdOp*> EdGetSelectedOps()
{
	if (g_selection.size() == 0 || g_selection[0].type != EdSelectionType_Node) return {};
	std::vector<EdOp*> selection;
	for (const EdSelection& sel : g_selection)
	{
		assert(sel.type == EdSelectionType_Node);
		assert(!selection.size() || selection[0]->parent == sel.op->parent);
		selection.push_back(sel.op);
	}
	return selection;
}

void EdRemoveOpFromTree(EdOp* op)
{
	if (op->parent)
	{
		for (int i = 0; i < op->parent->children.size(); i++)
		{
			if (op->parent->children[i] == op)
			{
				op->parent->children.erase(op->parent->children.begin() + i);
				break;
			}
		}
		op->parent = nullptr;
	}
}

void EdDestroyOp(EdOp* op)
{
	if (op->brush) CSGDestroyBrush(op->brush);
	for (CSGBrush* brush : op->built) CSGDestroyBrush(brush);
	for (EdOp* child : op->children) EdDestroyOp(child);
	EdRemoveOpFromTree(op);
	delete op;
}

void EdDestroySelection()
{
	bool did_something = false;
	std::vector<EdOp*> selection = EdGetSelectedOps();
	for (EdOp* op : selection) EdDestroyOp(op);
	if (selection.size()) EdDeselect();
}

void EdDrawBrushPlaneOutline(CSGBrush* b, int i, const float4x4& transform, const float4& color)
{
	const CSGPlane& plane = b->planes[i];
	for (int i = 0; i < plane.points.size(); i++)
	{
		float3 p1 = transform.TransformPosition(plane.points[i]);
		float3 p2 = transform.TransformPosition(plane.points[(i + 1) % plane.points.size()]);
		DrawLine(p1, p2, color);
	}
}

void EdDrawBrushOutline(CSGBrush* b, const float4x4& transform, const float4& color)
{
	for (int i = 0; i < b->planes.size(); i++)
		EdDrawBrushPlaneOutline(b, i, transform, color);
}

void EdDrawOutline(EdOp* op, const float4& color)
{
	switch (op->type)
	{
		case EdOpType_Brush:
		{
			EdDrawBrushOutline(op->brush, op->global_transform, color);
			break;
		}
		case EdOpType_Stack:
		{
			for (EdOp* child : op->children)
			{
				EdDrawOutline(child, color);
			}
			break;
		}
		default: break;
	}
}

template <typename F>
void EdForeach(F&& func, EdOp* op)
{
	if (!func(op))
		return;
	for (EdOp* child : op->children) EdForeach(func, child);
}

int EdGetSiblingIndex(EdOp* op)
{
	if (!op->parent) return -1;
	for (int i = 0; i < op->parent->children.size(); i++)
	{
		if (op->parent->children[i] == op)
		{
			return i;
		}
	}
	assert(0);
	return -1;
}

void EdOrderMove(int offset)
{
	std::vector<EdOp*> selection = EdGetSelectedOps();
	if (selection.size() == 0) return;

	EdOp* parent = selection[0]->parent;

	std::sort(selection.begin(), selection.end(), [](EdOp* a, EdOp* b) { return EdGetSiblingIndex(a) < EdGetSiblingIndex(b); });

	int idx = EdGetSiblingIndex(selection[0]) + offset;
	if (idx < 0) idx = 0;
	if (idx >= parent->children.size()) idx = parent->children.size() - 1;

	for (EdOp* op : selection)
	{
		parent->children.erase(parent->children.begin() + EdGetSiblingIndex(op));
	}
	parent->children.insert(parent->children.begin() + idx, selection.begin(), selection.end());
}

void EdOpAddChild(EdOp* parent, EdOp* child, int idx = -1)
{
	assert(!child->parent);

	if (idx >= 0)
	{
		parent->children.insert(parent->children.begin() + idx, child);
	}
	else
	{
		parent->children.push_back(child);
	}
	child->parent = parent;
}

EdOp* EdGroup(std::vector<EdOp*> ops)
{
	if (ops.size() == 0) return nullptr;
	if (ops.size() == 1) return ops[0];

	EdOp* parent = ops[0]->parent;

	std::sort(ops.begin(), ops.end(), [](EdOp* a, EdOp* b) { return EdGetSiblingIndex(a) < EdGetSiblingIndex(b); });
	int idx = EdGetSiblingIndex(ops[0]);

	for (EdOp* op : ops)
	{
		assert(op->parent == parent);
		EdRemoveOpFromTree(op);
	}

	EdOp* group = EdCreateOp();
	group->type = EdOpType_Stack;

	for (EdOp* op : ops) EdOpAddChild(group, op);
	EdOpAddChild(parent, group, idx);

	return group;
}

void EdUngroup(EdOp* op)
{

}

void EdBuild(EdOp* op)
{
	for (CSGBrush* brush : op->built) CSGDestroyBrush(brush);
	op->built.clear();

	switch (op->type)
	{
		case EdOpType_Brush:
		{
			op->built = { CSGCloneBrush(op->brush) };
			op->built[0]->source = op;
			break;
		}
		case EdOpType_Stack:
		{
			for (EdOp* child : op->children)
			{
				float4x4 child_transform = float4x4::Identity();
				glm_translate(child_transform, child->position);
				// glm_rotate(m, angle, (vec3){ ax, ay, az });
				// glm_scale(m, (vec3){ sx, sy, sz });

				child->global_transform = op->global_transform * child_transform;
				EdBuild(child);
				for (CSGBrush* b : child->built)
				{
					std::vector<CSGBrush*> new_set;
					for (CSGBrush* a : op->built)
					{
						int old_size = new_set.size();
						CSGDifference(a, b, child->global_transform, new_set);
						for (int i = old_size; i < new_set.size(); i++) new_set[i]->source = a->source;
					}
					if (!child->subtract)
					{
						CSGBrush* clone = CSGCloneBrush(b);
						clone->source = b->source;
						CSGBrushTransform(clone, child->global_transform);
						new_set.push_back(clone);
					}
					op->built = new_set;
				}
			}
			break;
		}
		default: assert(0); break;
	}

	if (op == g_root)
	{
		for (CSGBrush* b : op->built) CSGBuildBrushMesh(b);
	}
}

void EdMousePickRecursive(EdOp* op, const Ray& mouse_ray, float* min_t, EdOp** out_op, int* out_plane)
{
	switch (op->type)
	{
		case EdOpType_Brush:
		{
			int plane_hit = -1;
			float t = Intersect(mouse_ray, op->brush, op->global_transform, &plane_hit);
			if (t < *min_t && t >= 0.0f)
			{
				*min_t = t;
				*out_op = op;
				*out_plane = plane_hit;
				break;
			}
			else
			{
				break;
			}
		}
		case EdOpType_Stack:
		{
			EdOp* picked = nullptr;
			for (EdOp* child : op->children)
				EdMousePickRecursive(child, mouse_ray, min_t, out_op, out_plane);
			break;
		}
		default: assert(0);
	}
}

void EdOpGUI(EdOp* op)
{
	UIPushId(op);
	DEFER(UIPopId());

	UITreeNodeFlags flags = 0;
	if (EdIsSelected(op)) flags |= UITreeNodeFlags_Selected;
	if (op->type == EdOpType_Brush) flags |= UITreeNodeFlags_Leaf;
	flags |= UITreeNodeFlags_DefaultOpen;

	const char* name = op->type == EdOpType_Brush ? "Brush" : "Stack";

	UIBox* tree_node = 0;
	if (UIBeginTreeNode(name, &tree_node, flags))
	{
		switch (op->type)
		{
			case EdOpType_Stack:
			{
				for (EdOp* child : op->children)
				{
					EdOpGUI(child);
				}
				break;
			}
			default: break;
		}
		UIEndTreeNode();
	}
	if (tree_node->Clicked())
	{
		EdSelectOp(op, InputGetButton(SDL_SCANCODE_LCTRL));
	}
}

void EdArrowGizmo(Hash hash, float3& pos, float3 direction, float4 color, float base_scale = 1)
{
	U32 id = hash_stack.Push(hash);
	DEFER(hash_stack.Pop());

	const float scale = Distance(pos, g_editor_camera.position) * 0.1;
	const float cone_scale = 0.075f;

	const float3 a = pos;
	const float3 b = pos + direction * scale * base_scale;

	const float3 a_screen = CameraWorldToScreen(g_editor_camera, a);
	const float3 b_screen = CameraWorldToScreen(g_editor_camera, b);
	const float2 nearest_screen = NearestPointToLineSegment(a_screen.xy(), b_screen.xy(), InputMousePosition);

	const float nearest_screen_t = Unlerp(a_screen.xy(), nearest_screen, b_screen.xy());

	const float screen_dist = Distance(nearest_screen, InputMousePosition);
	const Ray ray_to_nearest = CameraScreenToRay(g_editor_camera, nearest_screen);

	if (screen_dist < 30 && nearest_screen_t >= 0.0f && nearest_screen_t <= (1.0f + 0.2 / base_scale) && screen_dist < g_new_hover_gizmo_state.screen_dist)
	{
		g_new_hover_gizmo_state.id = id;
		g_new_hover_gizmo_state.screen_dist = screen_dist;
	}

	if (g_active_gizmo_state.id == id || g_hover_gizmo_state.id == id)
	{
		color.x += 0.7;
		color.y += 0.7;
		color.z += 0.7;
	}

	{ // draw:
		DrawSetLayer(Layer_Overlay);
		float4 cone_rotation;
		glm_quat_from_vecs(float3(0,0,1), (b - a).Normalized(), cone_rotation);
		DrawLine(a, b, color);
		DrawMesh(Library::mesh_cone, nullptr, float4x4::FromTransform(b, cone_rotation, float3(cone_scale * scale, cone_scale * scale, cone_scale * scale)), color);
	}

	float t;
	DistanceToLineSegment(ray_to_nearest, a, b, nullptr, &t);
	float3 nearest_world = a + (b - a).Normalized() * t;

	if (g_hover_gizmo_state.id == id && !g_active_gizmo_state.id && !UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT))
	{
		g_active_gizmo_state = g_hover_gizmo_state;
		g_active_gizmo_state.offset = nearest_world - pos;
	}

	if (g_active_gizmo_state.id == id)
	{
		pos = nearest_world - g_active_gizmo_state.offset;
	}
}

void EdTranslationGizmo(Hash hash, float3& pos)
{
	hash_stack.Push(hash);
	EdArrowGizmo(1, pos, float3(1, 0, 0), float4(1, 0, 0, 1));
	EdArrowGizmo(2, pos, float3(0, 1, 0), float4(0, 1, 0, 1));
	EdArrowGizmo(3, pos, float3(0, 0, 1), float4(0, 0, 1, 1));
	hash_stack.Pop();
}

void EdDrawSelectionOutline(float4 color)
{
	for (const EdSelection& sel : g_selection)
	{
		switch (sel.type)
		{
			case EdSelectionType_Node:
			{
				EdDrawOutline(sel.op, color);
				break;
			}
			case EdSelectionType_BrushPlane:
			{
				EdDrawBrushPlaneOutline(sel.op->brush, sel.index, sel.op->global_transform, color);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

bool EdDoPlaneDragGizmo(EdOp* op, CSGBrush* brush, int idx)
{
	CSGPlane& plane = brush->planes[idx];

	float3 center = {};
	for (float3 p : plane.points) center += p;
	center /= plane.points.size();
	center += op->global_transform.column(3).xyz();

	hash_stack.Push(brush);
	hash_stack.Push(EdSelectionType_BrushPlane);
	DEFER(hash_stack.Pop());
	DEFER(hash_stack.Pop());

	float3 old_center = center;
	EdArrowGizmo(idx, center, plane.plane.normal, {1,1,0,1}, 0.3);
	float3 d = center - old_center;

	float add = Dot(d, plane.plane.normal);
	if (abs(add) > 0.001f)
	{
		plane.plane.distance += add;
		CSGBuildBrush(brush);
		return true;
	}
	else
	{
		return false;
	}
}

void EdTick()
{
	CameraFly(g_editor_camera);
	CameraUpdateMatrices(g_editor_camera);


	hash_stack.Reset();
	g_new_hover_gizmo_state = {};
	g_new_hover_gizmo_state.screen_dist = FLT_MAX;
	g_new_hover_gizmo_state.world_dist = FLT_MAX;

	Ray mouse_ray = CameraScreenToRay(g_editor_camera, InputMousePosition);

	std::vector<EdOp*> selected_ops = {};
	bool dirty = false;

	for (const EdSelection& sel : g_selection)
	{
		if (sel.type == EdSelectionType_Node)
		{

			if (sel.op->type == EdOpType_Brush)
			{
				for (int i = 0; i < sel.op->brush->planes.size(); i++)
				{
					if (EdDoPlaneDragGizmo(sel.op, sel.op->brush, i)) dirty = true;
				}
			}

			selected_ops.push_back(sel.op);
		}

		if (sel.type == EdSelectionType_BrushPlane && !EdIsSelected(sel.op))
		{
			if (EdDoPlaneDragGizmo(sel.op, sel.op->brush, sel.index)) dirty = true;
		}
	}

	if (selected_ops.size())
	{
		static float3 center = {};
		if (!g_active_gizmo_state.id)
		{
			center = {};
			for (EdOp* op : selected_ops)
			{
				center += op->global_transform.column(3).xyz();
			}
			center /= selected_ops.size();
		}

		hash_stack.Push(&center);
		DEFER(hash_stack.Pop());

		float3 old_center = center;
		EdTranslationGizmo(&center, center);
		float3 d = center - old_center;

		if (abs(d.Length()) > 0.0001f)
		{
			for (EdOp* op : selected_ops) op->position += d;
			dirty = true;
		}
	}
	
	if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT) && !g_active_gizmo_state.id)
	{
		float min_t = FLT_MAX;
		EdOp* hit = nullptr;
		int hit_plane = 0;

		EdMousePickRecursive(g_root, mouse_ray, &min_t, &hit, &hit_plane);
		if (hit)
		{
			if (InputGetButton(SDL_SCANCODE_LCTRL))
			{
				EdSelectPlane(hit, hit_plane, InputGetButton(SDL_SCANCODE_LSHIFT));
			}
			else
			{
				EdSelectOp(hit, InputGetButton(SDL_SCANCODE_LSHIFT));
			}
		}
		else
		{
			if (!InputGetButton(SDL_SCANCODE_LSHIFT)) EdDeselect();
		}
	}

	if (dirty)
		EdBuild(g_root);

	DrawSetLayer(Layer_Main);
	EdDrawSelectionOutline({1,1,1,1});
	DrawSetLayer(Layer_Overlay);
	EdDrawSelectionOutline({1,1,1,0.1});

	if (cvar_ed_show_sub.fvalue)
	{
		EdForeach([](EdOp* op)
			{
				if (op->subtract)
				{
					for (CSGBrush* b : op->built)
					{
						EdDrawBrushOutline(b, op->global_transform, {1,0,0.2,0.3});
					}
				}
				return true;
			}, g_root);
	}

	{ // Sidebar
		UIBeginBox()
			->SetSize(200, g_window_size.y)
			->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch)
			->SetColor(COLOR_BUTTON);

		for (EdOp* child : g_root->children)
		{
			EdOpGUI(child);
		}

		UIEndBox();
	}

	DrawSetLayer(Layer_Main);
	for (int i = 0; i < g_root->built.size(); i++)
	{
		CSGBrush* b = g_root->built[i];
		DrawMesh(b->mesh, Library::mat_brush, float4x4::Identity(), brush_colors[i % EVA_ARRAYSIZE(brush_colors)]);
		// DrawMesh(b->mesh, Library::mat_brush, float4x4::Identity(), brush_colors[1]);
	}

	g_hover_gizmo_state = g_new_hover_gizmo_state;
	if (InputGetButtonUp(INPUT_BUTTON_MOUSE_LEFT))
	{
		g_active_gizmo_state = {};
	}
}

void EdIndent(FILE* f, int indent)
{
	for (int i = 0; i < indent; i++) fprintf(f, "\t");
}

void EdSaveOp(FILE* f, EdOp* op, int indent)
{
	EdIndent(f, indent); fprintf(f, "op %d\n", op->type);

	indent++;

	EdIndent(f, indent); fprintf(f, "subtract %d\n", op->subtract);
	EdIndent(f, indent); fprintf(f, "position %f %f %f\n", XYZ(op->position));

	switch (op->type)
	{
		case EdOpType_Brush:
		{
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
		case EdOpType_Stack:
		{
			EdIndent(f, indent); fprintf(f, "children %d\n", (int)op->children.size());
			for (int i = 0; i < op->children.size(); i++)
			{
				EdSaveOp(f, op->children[i], indent + 1);
			}
			break;
		}
		default: assert(0);
	}

	indent--;
	EdIndent(f, indent); fprintf(f, "op_end\n");
}

EdOp* EdLoadOp(FILE* f)
{
	int n;
	EdOp* op = EdCreateOp();
	n = fscanf(f, "op %d\n", &op->type);
	assert(n == 1);

	switch (op->type)
	{
		case EdOpType_Brush:
		{
			op->brush = CSGCreateBrush();
			break;
		}
		default: break;
	}
	assert(op->type > EdOpType_None && op->type < EdOpType_ENUM_SIZE);

	for (;;)
	{
		char t[32] = {};
		n = fscanf(f, "%s", t);
		assert(n == 1);

		if (strcmp(t, "subtract") == 0)
		{
			int sub;
			n = fscanf(f, "%d\n", &sub);
			assert(n == 1);
			op->subtract = sub;
		}
		else if (strcmp(t, "position") == 0)
		{
			n = fscanf(f, "%f %f %f\n", XYZ(&op->position));
			assert(n == 3);
		}
		else if (strcmp(t, "planes") == 0)
		{
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
		}
		else if (strcmp(t, "children") == 0)
		{
			int num_children;
			n = fscanf(f, "%d\n", &num_children);
			assert(num_children >= 0 && num_children < 1000);
			for (int i = 0; i < num_children; i++)
			{
				op->children.push_back(EdLoadOp(f));
			}
		}
		else if (strcmp(t, "op_end") == 0)
		{
			fscanf(f, "\n");
			break;
		}
	}

	switch (op->type)
	{
		case EdOpType_Brush:
		{
			CSGBuildBrush(op->brush);
			break;
		}
		default: break;
	}

	return op;
}

void EdSaveMap(const char* name)
{
	char path[256];
	snprintf(path, 256, "%s/Assets/%s.mpe", EVA_BASE_DIR, name);
	FILE* f = fopen(path, "wb");
	if (!f)
	{
		ConError("Failed to open %s", name);
		return;
	}
	DEFER(fclose(f));
	fprintf(f, "type mpe\n");
	fprintf(f, "version 1\n");
	EdSaveOp(f, g_root, 0);

	snprintf(g_loaded_map_name, sizeof(g_loaded_map_name), "%s", name);
}

void EdLoadMap(const char* name)
{
	if (g_root)
	{
		EdDestroyOp(g_root);
		g_root = nullptr;
	}

	char path[256];
	snprintf(path, 256, "%s/Assets/%s.mpe", EVA_BASE_DIR, name);
	FILE* f = fopen(path, "rb");
	if (!f)
	{
		ConError("Failed to open %s", name);
		return;
	}
	DEFER(fclose(f));

	int version = 0;
	fscanf(f, "type mpe\n");
	fscanf(f, "version %d\n", &version);
	if (version != 1)
	{
		ConError("map %s is version %d, expected %d", name, version, 1);
		return;
	}

	g_root = EdLoadOp(f);
	g_root->global_transform = float4x4::Identity();
	EdBuild(g_root);

	snprintf(g_loaded_map_name, sizeof(g_loaded_map_name), "%s", name);
}

void EdCompileMap()
{
	int indent = 0;
	char path[256];
	assert(g_loaded_map_name[0]);
	snprintf(path, 256, "%s/Assets/%s.map", EVA_BASE_DIR, g_loaded_map_name);
	FILE* f = fopen(path, "wb");
	if (!f)
	{
		ConError("Failed to open %s", path);
		return;
	}
	DEFER(fclose(f));

	EdBuild(g_root);

	fprintf(f, "type map\n");
	fprintf(f, "version 1\n");

	std::vector<MeshVertex> mesh_vertices;
	std::vector<U32> mesh_indices;

	for (CSGBrush* brush : g_root->built)
	{
		for (const CSGPlane& plane : brush->planes)
		{
			int start = mesh_vertices.size();
			for (int i = 0; i < plane.points.size(); i++)
			{
				mesh_vertices.push_back({
					.position = plane.points[i],
					.normal = plane.plane.normal,
				});
			}
			for (int i = 2; i < plane.points.size(); i++)
			{
				mesh_indices.push_back(start);
				mesh_indices.push_back(start + i - 1);
				mesh_indices.push_back(start + i);
			}
		}
	}

	EdIndent(f, indent); fprintf(f, "vertices %d\n", (int)mesh_vertices.size());
	for (const MeshVertex& vert : mesh_vertices)
	{
		EdIndent(f, indent); fprintf(f, "%f %f %f %f %f %f ", XYZ(vert.position), XYZ(vert.normal));
	}
	fprintf(f, "\n");

	EdIndent(f, indent); fprintf(f, "indices %d ", (int)mesh_indices.size());
	for (U32 idx : mesh_indices)
	{
		fprintf(f, "%u ", idx);
	}
	fprintf(f, "\n");
}

void EdInitialize()
{
	ConRegisterVar(&cvar_ed_show_sub);
	ConRegisterCommand("ed_cube",
		[](ConParser& parser)
		{
			EdOp* op = EdCreateOp();
			op->brush = CSGCreateCube({ parser.FloatArg(1.0f), parser.FloatArg(1.0f), parser.FloatArg(1.0f) });
			op->type = EdOpType_Brush;
			EdOpAddChild(g_root, op);
			EdSelectOp(op);
			EdBuild(g_root);
		}, "editor: create a cube");
	ConRegisterCommand("ed_cylinder",
		[](ConParser& parser)
		{
			EdOp* op = EdCreateOp();
			int nseg = parser.FloatArg(12.0f);
			float rad  = parser.FloatArg(1.0f);
			float height = parser.FloatArg(1.0f);
			op->brush = CSGCreateCylinder(nseg, rad, height);
			op->type = EdOpType_Brush;
			EdOpAddChild(g_root, op);
			EdSelectOp(op);
			EdBuild(g_root);
		}, "editor: create a cylinder");
	ConRegisterCommand("ed_move",
		[](ConParser& parser)
		{
			float3 offset = { parser.FloatArg(0.0f), parser.FloatArg(0.0f), parser.FloatArg(0.0f) };
			for (EdSelection& sel : g_selection)
				if (sel.type == EdSelectionType_Node)
					sel.op->position += offset;
			EdBuild(g_root);
		}, "editor: create a cube");
	ConRegisterCommand("ed_build",
		[](ConParser& parser)
		{
			EdBuild(g_root);
		}, "editor: rebuild csg");
	ConRegisterCommand("ed_add",
		[](ConParser& parser)
		{
			for (EdSelection& sel : g_selection)
				if (sel.type == EdSelectionType_Node)
					sel.op->subtract = false;
			EdBuild(g_root);
		}, "editor: set selected to add");
	ConRegisterCommand("ed_sub",
		[](ConParser& parser)
		{
			for (EdSelection& sel : g_selection)
				if (sel.type == EdSelectionType_Node)
					sel.op->subtract = true;
			EdBuild(g_root);
		}, "editor: set selected to subtract");
	ConRegisterCommand("ed_order_move", [](ConParser& parser)
		{
			EdOrderMove(parser.IntArg(1));
			EdBuild(g_root);
		}, "editor: move selection up/down in stack");
	ConRegisterCommand("ed_group", [](ConParser& parser)
		{
			EdGroup(EdGetSelectedOps());
			EdBuild(g_root);
		}, "editor: group selection into an object");
	ConRegisterCommand("ed_ungroup", [](ConParser& parser)
		{
			for (EdSelection& sel : g_selection)
				if (sel.type == EdSelectionType_Node)
					EdUngroup(sel.op);
			EdBuild(g_root);
		}, "editor: group ungroup selected objects");
	ConRegisterCommand("ed_del",
		[](ConParser& parser)
		{
			EdDestroySelection();
			EdBuild(g_root);
		}, "editor: delete subtract");
	ConRegisterCommand("ed_save",
		[](ConParser& parser)
		{
			const char* name = parser.StringArg();
			if (!name || name[0] == '\0')
			{
				name = g_loaded_map_name;
			}
			if (!name[0])
			{
				ConError("no map opened");
				return;
			}
			EdSaveMap(name);
		}, "editor: save map .mpe");
	ConRegisterCommand("ed_load",
		[](ConParser& parser)
		{
			const char* name = parser.StringArg();
			if (!name || name[0] == '\0')
			{
				ConError("required arg missing");
				return;
			}
			EdLoadMap(name);
		}, "editor: load a map .mpe");
	ConRegisterCommand("ed_compile", [](ConParser& parser)
		{
			EdCompileMap();
		}, "editor: compile map");
	ConRegisterCommand("ed", [](ConParser& parser)
		{
			AppSetMode(AppMode_Editor, nullptr);
		}, "editor: open editor");

	g_root = EdCreateOp();
	g_root->type = EdOpType_Stack;
	g_root->global_transform = float4x4::Identity();

	CameraInit(g_editor_camera);
	g_editor_camera.position.y = -10;
	g_editor_camera.position.z = 3;
}