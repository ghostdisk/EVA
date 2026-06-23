#include <EVA/Arena.hpp>
#include <EVA/Editor.hpp>
#include <EVA/UI.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Game.hpp>
#include <EVA/Renderer.hpp>
#include <EVA/Platform.hpp>
#include <EVA/IO.hpp>
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
std::vector<char> console_input;
static bool console_open = false;

void EditorInitialize()
{
}

Selection* Selected(void* stack)
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
	if (Selected(brush)) flags |= UITreeNodeFlags_Selected;
	UITreeNodeStatus status = UIBeginTreeNode("CSG Brush", flags);
	Select(SelectionType_CSGBrush, brush, status.selected);

	if (status.hover || status.selected)
		for (CSGPlane& plane : brush->planes)
			OutlineCSGPlane(plane);

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
	if (Selected(stack)) flags |= UITreeNodeFlags_Selected;
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
	if (1)
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
		snprintf(buf, 512, "FPS: %.1f", FPS);
		UILabel(buf);

		for (const char* text : screen_log)
		{
			UILabel(text);
		}

		UIEndBox();
	}

	{ // sidebar
		UIBox* sidebar = UIBeginBox()
			->SetPosition(0, 0)
			->SetSize( (float)300, (float)WindowHeight );
		UIEndBox();
	}

	{ // console
		bool just_opened = false;
		if (IOGetButtonDown(SDL_SCANCODE_GRAVE))
		{
			console_open = !console_open;
			just_opened = true;
		}

		if (console_open)
		{
			UIPushId("console");
			DEFER(UIPopId());

			UIBeginBox()
				->SetFlex(UIAxis_Vertical, UIAlignment_Stretch, UIAlignment_Stretch)
				->SetSize(800, 600)
				->SetPosition((float)WindowWidth/2 - 400, (float)WindowHeight/2 - 300)
				->SetColor(COLOR_RGB(57, 9, 23));
			DEFER(UIEndBox());

			{ // titlebar
				UIBeginBox()
					->SetFlex(UIAxis_Horizontal, UIAlignment_Stretch, UIAlignment_Stretch);
				UILabel("Console")->SetPadding(8);
				UIFlexSpacer();
				if (UIButton("X")) console_open = false;
				UIEndBox();
			}

			UIBeginBox()
				->SetFlex(UIAxis_Vertical, UIAlignment_Stretch, UIAlignment_Stretch)
				->SetFlexGrow(1)
				->SetGap(8)
				->SetPadding(8);

			{ // main content
				UIBeginBox()
					->SetFlexGrow(1)
					->SetColor(COLOR_RGB(0,0,255));
				UIEndBox();
			}

			{ // input bar
				UIBeginBox()
					->SetFlex(UIAxis_Horizontal, UIAlignment_Stretch, UIAlignment_Stretch)
					->SetGap(8);

				char buf[64];
				UIPushId("input");
				UIBox* text_input = UITextInput(console_input)->SetFlexGrow(1);
				if (just_opened) UIFocus(text_input);
				UIPopId();

				UIButton("Submit");
				UIEndBox();
			}
			UIEndBox();
		}
	}

	// UIBeginTreeList();
	// InspectorCSGStack(ActiveGame->csg);
	// UIEndTreeList();

	screen_log.clear();
}

void LogToScreen(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	screen_log.push_back(ArenaVprintf(FrameArena, fmt, args));
	va_end(args);
}