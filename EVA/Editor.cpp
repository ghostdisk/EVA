#include <EVA/Editor.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Console.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Library.hpp>
#include <EVA/Input.hpp>
#include <EVA/Game.hpp>
#include <cglm/affine.h>

static EdOp* root = nullptr;

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

EdOp* EdCreateCube(float3 size)
{
	EdOp* op = EdCreateOp();
	op->brush = CSGCreateCube(size);
	op->type = EdOpType_Brush;
	root->children.push_back(op);
	EdSelect(op);
	return {};
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
	ConRegisterCommand("ed_cube",
		[](int argc, ConValue* argv) -> ConValue
		{
			EdCreateCube({
				(argc > 0 && argv[0].number) ? argv[0].number : 1.0f,
				(argc > 1 && argv[1].number) ? argv[1].number : 1.0f,
				(argc > 2 && argv[2].number) ? argv[2].number : 1.0f,
			});
			EdBuild(root);
			return {};
		}, "editor: create a cube");
	ConRegisterCommand("ed_move",
		[](int argc, ConValue* argv) -> ConValue
		{
			float3 offset = {
				(argc > 0 && argv[0].number) ? argv[0].number : 0.0f,
				(argc > 1 && argv[1].number) ? argv[1].number : 0.0f,
				(argc > 2 && argv[2].number) ? argv[2].number : 0.0f,
			};
			EdForeachSelected([&](EdOp* op)
				{
					op->position += offset;
					return false;
				}, root);
			EdBuild(root);
			return {};
		}, "editor: create a cube");
	ConRegisterCommand("ed_build",
		[](int argc, ConValue* argv) -> ConValue
		{
			EdBuild(root);
			return {};
		}, "editor: rebuild csg");
	ConRegisterCommand("ed_add",
		[](int argc, ConValue* argv) -> ConValue
		{
			EdForeachSelected([](EdOp* op) { op->subtract = false; return false; }, root);
			EdBuild(root);
			return {};
		}, "editor: set selected to add");
	ConRegisterCommand("ed_sub",
		[](int argc, ConValue* argv) -> ConValue
		{
			EdForeachSelected([](EdOp* op) { op->subtract = true; return false; }, root);
			EdBuild(root);
			return {};
		}, "editor: set selected to subtract");
	ConRegisterCommand("ed_del",
		[](int argc, ConValue* argv) -> ConValue
		{
			EdDestroySelectedRecursively(root);
			EdBuild(root);
			return {};
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

void EdTick()
{
	DrawSetLayer(Layer_Main);
	for (int i = 0; i < root->built.size(); i++)
	{
		CSGBrush* b = root->built[i];
		DrawMesh(b->mesh, Library::mat_brush, float4x4::Identity(), brush_colors[i % EVA_ARRAYSIZE(brush_colors)]);
	}

	bool select = false;
	if (InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT))
	{
		select = true;
	}

	if (select)
	{
		Ray mouse_ray = CameraScreenToRay(ActiveGame->camera, InputMousePosition);

		float min_t = FLT_MAX;
		EdOp* selected = EdMousePickRecursive(root, mouse_ray, &min_t);

		EdSelect(selected, InputGetButton(SDL_SCANCODE_LCTRL));
	}

	DrawSetLayer(Layer_Overlay);
	EdOutlineSelectionRecursively(root, {1,1,1,0.1});
	DrawSetLayer(Layer_Main);
	EdOutlineSelectionRecursively(root, {1,1,1,1});

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
