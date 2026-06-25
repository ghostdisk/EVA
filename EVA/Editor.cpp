#include <EVA/Arena.hpp>
#include <EVA/Editor.hpp>
#include <EVA/Console.hpp>
#include <EVA/UI.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Game.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Input.hpp>
#include <tracy/Tracy.hpp>
#include <stdarg.h>

enum SelectionType
{
	SelectionType_CSGStack,
	SelectionType_CSGBrush,
};

struct Selection
{
	SelectionType type;
	void* data;
};

std::vector<Selection> selection_list;
std::vector<const char*> screen_log;

Ray hover_ray;
CSGBrush* hover_brush;

ConVar show_fps = {
	.name = "show_fps",
	.help = "show fps on screen",
	.value = {
		.type = ConValueType_Number,
		.number = 0,
	},
};

void EditorInitialize()
{
	ConRegisterVar(&show_fps);
}

Selection* IsSelected(void* stack)
{
	for (Selection& s : selection_list)
	{
		if (s.data == stack)
		{
			return &s;
		}
	}
	return nullptr;
}

void DeselectAll()
{
	selection_list.clear();
}

Selection* Select(SelectionType type, void* data, bool selected)
{
	Selection* existing = nullptr;
	for (Selection& s : selection_list)
	{
		if (s.data == data)
		{
			existing = &s;
			break;
		}
	}

	if (existing && !selected)
	{
		*existing = selection_list.back();
		selection_list.pop_back();
	}
	else if (!existing && selected)
	{
		selection_list.push_back({
			.type = type,
			.data = data,
		});
		existing = &selection_list.back();
	}
	return existing;
}

void OutlineCSGPlane(CSGPlane& plane)
{
	for (int i = 0; i < plane.points.size(); i++)
	{
		DrawLine(plane.points[i], plane.points[(i + 1) % plane.points.size()], {1,1,1,1});
	}
}

void InspectorCSGBrush(CSGBrush* brush)
{
	UIPushId(brush);
	DEFER(UIPopId());

	UITreeNodeFlags flags = 0;
	if (IsSelected(brush)) flags |= UITreeNodeFlags_Selected;
	UITreeNodeStatus status = UIBeginTreeNode("CSG Brush", flags);
	Select(SelectionType_CSGBrush, brush, status.selected);

	if (status.open)
	{
		for (CSGPlane& plane : brush->planes)
		{
			UIPushId(&plane);
			DEFER(UIPopId());

			char buf[256];
			snprintf(buf, 256, "Plane %.1f,%.1f,%.1f:%.2f", plane.plane.normal.x, plane.plane.normal.y, plane.plane.normal.z, plane.plane.distance);
			UITreeNodeStatus plane_status = UIBeginTreeNode(buf);

			if (plane_status.hover)
				OutlineCSGPlane(plane);

			if (plane_status.open)
			{
				for (float3& p : plane.points)
				{
					UIPushId(&p);
					DEFER(UIPopId());

					snprintf(buf, 256, "Point %.1f,%.1f,%.1f", p.x, p.y, p.z);
					UITreeNodeStatus point_status = UIBeginTreeNode(buf, UITreeNodeFlags_Leaf);
					if (point_status.hover)
					{
						DrawPoint(p, {1,1,1,1});
					}
				}

				UIEndTreeNode();
			}

		}

		UIEndTreeNode();
	}
}

void InspectorCSGStack(CSGStack* stack)
{
	UIPushId(stack);
	DEFER(UIPopId());

	UITreeNodeFlags flags = 0;
	if (IsSelected(stack)) flags |= UITreeNodeFlags_Selected;

	UITreeNodeStatus status = UIBeginTreeNode("CSG Stack", flags);
	Select(SelectionType_CSGStack, stack, status.selected);

	if (status.open)
	{
		if (UIBeginTreeNode("Nodes"))
		{
			for (CSGStackNode& child : stack->nodes)
			{
				switch (child.type)
				{
					case CSGStackNodeType_Brush:
					{
						InspectorCSGBrush(child.brush);
						break;
					}
					case CSGStackNodeType_Stack:
					{
						InspectorCSGStack(child.stack);
						break;
					}
					default:
					{
						assert(0);
					}

				}
			}
			UIEndTreeNode();
		}
		if (UIBeginTreeNode("Built"))
		{
			for (CSGBrush* brush : stack->built_brushes)
			{
				InspectorCSGBrush(brush);
			}
		}
		UIEndTreeNode();
	}
}

void EditorEarlyTick()
{
	if (0)
	{
		ZoneScopedN("CSG Rebuild");
		CSGBuildStack(ActiveGame->csg);
		for (CSGBrush* b : ActiveGame->csg->built_brushes)
		{
			CSGBuildBrushMesh(b);
		}
	}
}

void EditorLateTick()
{
	{ // status
		UIBox* status = UIBeginBox()
			->SetPadding(8)
			->SetFlex(UIAxis_Vertical)
			->SetGap(4)
			->SetSize(400, 0)
			->SetPosition((float)(WindowWidth-400), 0)
			->SetColor({ 0, 0, 0, .2 });
		
		char buf[512];

		if (show_fps.value.number)
		{
			snprintf(buf, 512, "FPS: %.1f", FPS);
			UILabel(buf);
		}

		for (const char* text : screen_log)
		{
			UILabel(text);
		}

		UIEndBox();
	}

	{
		UIBeginBox()->SetSize(400, 0);
		UIButton("Cube");
		UIEndBox();
	}

	DrawSetLayer(Layer_Main);
	// DrawGrid(100);

	// UIBeginTreeList();
	// InspectorCSGStack(ActiveGame->csg);
	// UIEndTreeList();

	screen_log.clear();

	DrawSetLayer(Layer_Overlay);
	for (Selection& sel : selection_list)
	{
		if (sel.type == SelectionType_CSGBrush)
		{
			for (CSGPlane& plane : ((CSGBrush*)sel.data)->planes)
				OutlineCSGPlane(plane);
		}
	}
	DrawSetLayer(Layer_Main);

	if (InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT))
	{
		DeselectAll();

		hover_ray = CameraScreenToRay(ActiveGame->camera, InputMousePosition);
		hover_brush = nullptr;
		float hover_t = FLT_MAX;
		for (const CSGStackNode& node : ActiveGame->csg->nodes)
		{
			CSGBrush* brush = 0;
			if (node.type == CSGStackNodeType_Brush)
			{
				brush = node.brush;
			}
			if (brush)
			{
				float t = Intersect(hover_ray, brush);
				if (hover_t > t && t >= 0)
				{
					hover_t = t;
					hover_brush = brush;
				}
			}
		}

		if (hover_brush)
		{
			Select(SelectionType_CSGBrush, hover_brush, true);
		}
	}
}

void LogToScreen(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	screen_log.push_back(ArenaVprintf(FrameArena, fmt, args));
	va_end(args);
}