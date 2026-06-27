#include <EVA/Editor.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Console.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Library.hpp>
#include <EVA/Input.hpp>
#include <EVA/Game.hpp>
#include <EVA/UI.hpp>
#include <cglm/quat.h>

static EdOp* root = nullptr;

float grid_size = 0.25;

static ConVar cvar_ed_show_sub = {
	.name = "ed_show_sub",
	.help = "show subtract brushes",
	.fvalue = 0,
};

struct EdTranslationGizmoState
{
	float3 offset;
	bool active = false;
	int active_axis = -1;
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

EdTranslationGizmoState g_gizmo = {};

EdOp* EdCreateOp()
{
	EdOp* op = new EdOp();
	return op;
}

void EdDeselectRecursive(EdOp* op)
{
	for (EdOp* child : op->children)
	{
		EdDeselectRecursive(child);
	}
	op->selected = false;
}

void EdDeselectAll()
{
	EdDeselectRecursive(root);
}

void EdSelect(EdOp* op, bool additive = false)
{
	if (!additive) EdDeselectAll();
	if (op) op->selected = true;
}

void EdDestroyOp(EdOp* op)
{
	if (op->brush) CSGDestroyBrush(op->brush);
	for (CSGBrush* brush : op->built) CSGDestroyBrush(brush);
	for (EdOp* child : op->children) EdDestroyOp(child);
	delete op;
}

bool EdDestroySelectedRecursively(EdOp* op)
{
	if (op->selected)
	{
		EdDestroyOp(op);
		return true;
	}
	for (int i = 0; i < op->children.size(); i++)
	{
		if (EdDestroySelectedRecursively(op->children[i]))
		{
			op->children.erase(op->children.begin() + i);
		}
	}
	return false;
}

void EdOutlineBrush(CSGBrush* b, const float4x4& transform, float4 color)
{
	for (const CSGPlane& plane : b->planes)
	{
		for (int i = 0; i < plane.points.size(); i++)
		{
			float3 p1 = transform.TransformPosition(plane.points[i]);
			float3 p2 = transform.TransformPosition(plane.points[(i + 1) % plane.points.size()]);
			DrawLine(p1, p2, color);
		}
	}
}


void EdOutlineSelectionRecursively(EdOp* op, float4 color)
{
	if (op->selected) for (CSGBrush* b : op->built) EdOutlineBrush(b, op->global_transform, color);
	for (EdOp* child : op->children) EdOutlineSelectionRecursively(child, color);
}

template <typename F>
void EdForeach(F&& func, EdOp* op)
{
	if (!func(op))
		return;
	for (EdOp* child : op->children) EdForeach(func, child);
}

template <typename F>
void EdForeachSelected(F&& func, EdOp* op)
{
	if (op->selected)
	{
		if (!func(op))
			return;
	}
	for (EdOp* child : op->children) EdForeachSelected(func, child);
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

	if (op == root)
	{
		for (CSGBrush* b : op->built) CSGBuildBrushMesh(b);
	}
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
			root->children.push_back(op);
			EdSelect(op);
			EdBuild(root);
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
			root->children.push_back(op);
			EdSelect(op);
			EdBuild(root);
		}, "editor: create a cylinder");
	ConRegisterCommand("ed_move",
		[](ConParser& parser)
		{
			float3 offset = { parser.FloatArg(0.0f), parser.FloatArg(0.0f), parser.FloatArg(0.0f) };
			EdForeachSelected([&](EdOp* op)
				{
					op->position += offset;
					return false;
				}, root);
			EdBuild(root);
		}, "editor: create a cube");
	ConRegisterCommand("ed_build",
		[](ConParser& parser)
		{
			EdBuild(root);
		}, "editor: rebuild csg");
	ConRegisterCommand("ed_add",
		[](ConParser& parser)
		{
			EdForeachSelected([](EdOp* op) { op->subtract = false; return false; }, root);
			EdBuild(root);
		}, "editor: set selected to add");
	ConRegisterCommand("ed_sub",
		[](ConParser& parser)
		{
			EdForeachSelected([](EdOp* op) { op->subtract = true; return false; }, root);
			EdBuild(root);
		}, "editor: set selected to subtract");
	ConRegisterCommand("ed_del",
		[](ConParser& parser)
		{
			EdDestroySelectedRecursively(root);
			EdBuild(root);
		}, "editor: delete subtract");

	root = EdCreateOp();
	root->type = EdOpType_Stack;
	root->global_transform = float4x4::Identity();
}

EdOp* EdMousePickRecursive(EdOp* op, const Ray& mouse_ray, float* min_t)
{
	switch (op->type)
	{
		case EdOpType_Brush:
		{
			float t = Intersect(mouse_ray, op->brush, op->global_transform);
			if (t < *min_t && t >= 0.0f)
			{
				*min_t = t;
				return op;
			}
			else
			{
				return nullptr;
			}
		}
		case EdOpType_Stack:
		{
			EdOp* picked = nullptr;
			for (EdOp* child : op->children)
			{
				EdOp* candidate = EdMousePickRecursive(child, mouse_ray, min_t);
				if (candidate) picked = candidate;
			}
			return picked;
		}
		default:
		{
			assert(0);
			return nullptr;
		}
	}
}

void EdOpGUI(EdOp* op)
{
	UIPushId(op);
	DEFER(UIPopId());

	UITreeNodeFlags flags = 0;
	if (op->selected) flags |= UITreeNodeFlags_Selected;
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
		EdSelect(op);
	}
}

float Unlerp(float2 a, float2 m, float2 b)
{
	if (abs(m.x - a.x) > 0.01f)
	{
		return (m.x - a.x) / (b.x - a.x);
	}
	else
	{
		return (m.y - a.y) / (b.y - a.y);
	}
}

void EdTranslationGizmo(EdTranslationGizmoState& gizmo, float3& pos, const Ray& mouse_ray, bool mouse_down)
{
	int new_active_axis = -1;
	float min_distp = FLT_MAX;
	float3 new_offset;
	float3 point_along_line;

	float dist = Distance(pos, ActiveGame->camera.position) * 0.1;
	float scale = dist;
	float cone_scale = 0.075f;

	auto picker = [&](int axis, float3 a, float3 b, float4 color, Ray ray)
	{
		b *= scale;
		a += pos;
		b += pos;

		float3 a_screen = CameraWorldToScreen(ActiveGame->camera, a);
		float3 b_screen = CameraWorldToScreen(ActiveGame->camera, b);
		float2 nearest_screen = NearestPointToLineSegment(a_screen.xy(), b_screen.xy(), InputMousePosition);
		float nearest_screen_t = Unlerp(a_screen.xy(), nearest_screen, b_screen.xy());

		float dist_screen = Distance(nearest_screen, InputMousePosition);
		Ray ray_to_nearest = CameraScreenToRay(ActiveGame->camera, nearest_screen);

		float t2;
		DistanceToLineSegment(ray_to_nearest, a, b, nullptr, &t2);
		float3 nearest_world = a + (b - a).Normalized() * t2;

		if (!gizmo.active && dist_screen < 30 && dist_screen < min_distp && nearest_screen_t >= 0.0f && nearest_screen_t <= (1.0f + cone_scale * 2))
		{
			min_distp = dist_screen;
			new_active_axis = axis;
			point_along_line = nearest_world;
			new_offset = nearest_world - pos;
		}
		if (axis == gizmo.active_axis)
		{
			point_along_line = nearest_world;
			color = COLOR_WHITE;
		}

		float4 cone_rotation;
		glm_quat_from_vecs(float3(0,0,1), (b - a).Normalized(), cone_rotation);

		DrawLine(a, b, color);
		DrawMesh(Library::mesh_cone, nullptr, float4x4::FromTransform(b, cone_rotation, float3(cone_scale * scale, cone_scale * scale, cone_scale * scale)), color);
	};

	DrawSetLayer(Layer_Overlay);
	picker(0, {0,0,0}, {1,0,0}, {1,0,0,1}, mouse_ray);
	picker(1, {0,0,0}, {0,1,0}, {0,1,0,1}, mouse_ray);
	picker(2, {0,0,0}, {0,0,1}, {0,0,1,1}, mouse_ray);
	DrawSetLayer(Layer_Main);

	if (!gizmo.active) gizmo.active_axis = new_active_axis;
	if (mouse_down && new_active_axis >= 0)
	{
		gizmo.active = true;
		gizmo.offset = new_offset;
	}

	if (gizmo.active)
	{
		pos = point_along_line - gizmo.offset;

		if (InputGetButton(SDL_SCANCODE_LCTRL))
		{
			pos = pos / grid_size;
			if (gizmo.active_axis == 0) pos.x = round(pos.x);
			if (gizmo.active_axis == 1) pos.y = round(pos.y);
			if (gizmo.active_axis == 2) pos.z = round(pos.z);
			pos = pos * grid_size;
		}

		if (InputGetButtonUp(INPUT_BUTTON_MOUSE_LEFT))
		{
			gizmo.active = false;
			gizmo.offset = {};
			gizmo.active_axis = -1;
		}
	}
}

EdOp* EdGetFirstSelected(EdOp* el)
{
	if (el->selected) return el;
	for (EdOp* child : el->children)
	{
		EdOp* sel = EdGetFirstSelected(child);
		if (sel) return sel;
	}
	return nullptr;
}

void EdTick()
{

	bool select = false;
	if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT))
	{
		select = true;
	}

	Ray mouse_ray = CameraScreenToRay(ActiveGame->camera, InputMousePosition);
	EdOp* sel = EdGetFirstSelected(root);

	if (sel)
	{
		EdTranslationGizmo(g_gizmo, sel->position, mouse_ray, select);
		if (g_gizmo.active)
		{
			select = false;
			EdBuild(root);
		}
	}
	

	if (select)
	{

		float min_t = FLT_MAX;
		EdOp* selected = EdMousePickRecursive(root, mouse_ray, &min_t);

		EdSelect(selected, InputGetButton(SDL_SCANCODE_LCTRL));
	}

	DrawSetLayer(Layer_Overlay);
	EdOutlineSelectionRecursively(root, {1,1,1,0.1});
	DrawSetLayer(Layer_Main);
	EdOutlineSelectionRecursively(root, {1,1,1,1});

	if (cvar_ed_show_sub.fvalue)
	{
		EdForeach([](EdOp* op)
			{
				if (op->subtract)
				{
					for (CSGBrush* b : op->built)
					{
						EdOutlineBrush(b, op->global_transform, {1,0,0.2,0.3});
					}
				}
				return true;
			}, root);
	}

	{
		UIBeginBox()->SetSize(200, 0)->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch);
		EdOpGUI(root);
		UIEndBox();
	}

	DrawSetLayer(Layer_Main);
	for (int i = 0; i < root->built.size(); i++)
	{
		CSGBrush* b = root->built[i];
		DrawMesh(b->mesh, Library::mat_brush, float4x4::Identity(), brush_colors[i % EVA_ARRAYSIZE(brush_colors)]);
		DrawMesh(b->mesh, Library::mat_brush, float4x4::Identity(), brush_colors[1]);
	}
}
