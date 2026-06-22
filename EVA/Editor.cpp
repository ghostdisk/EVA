#include <EVA/Editor.hpp>
#include <EVA/UI.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Game.hpp>
#include <EVA/Renderer.hpp>

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

void InspectorCSGBrush(UIContext& ui, CSGBrush* brush)
{
	UIPushId(ui, brush);
	DEFER(UIPopId(ui));

	UITreeNodeFlags flags = 0;
	if (Selected(brush)) flags |= UITreeNodeFlags_Selected;
	UITreeNodeStatus status = UIBeginTreeNode(ui, "CSG Brush", flags);
	Select(SelectionType_CSGBrush, brush, status.selected);

	if (status.hover || status.selected)
		for (CSGPlane& plane : brush->planes)
			OutlineCSGPlane(plane);

	if (status.open)
	{
		for (CSGPlane& plane : brush->planes)
		{
			UIPushId(ui, &plane);
			DEFER(UIPopId(ui));

			char buf[256];
			snprintf(buf, 256, "Plane %.1f,%.1f,%.1f:%.2f", plane.plane.normal.x, plane.plane.normal.y, plane.plane.normal.z, plane.plane.distance);
			UITreeNodeStatus plane_status = UIBeginTreeNode(ui, buf);

			if (plane_status.hover)
				OutlineCSGPlane(plane);

			if (plane_status.open)
			{
				for (float3& p : plane.points)
				{
					UIPushId(ui, &p);
					DEFER(UIPopId(ui));

					snprintf(buf, 256, "Point %.1f,%.1f,%.1f", p.x, p.y, p.z);
					UITreeNodeStatus point_status = UIBeginTreeNode(ui, buf, UITreeNodeFlags_Leaf);
					if (point_status.hover)
					{
						DrawPoint(p, {1,1,1,1});
					}
				}

				UIEndTreeNode(ui);
			}

		}

		UIEndTreeNode(ui);
	}
}

void InspectorCSGStack(UIContext& ui, CSGStack* stack)
{
	UIPushId(ui, stack);
	DEFER(UIPopId(ui));

	UITreeNodeFlags flags = 0;
	if (Selected(stack)) flags |= UITreeNodeFlags_Selected;
	UITreeNodeStatus status = UIBeginTreeNode(ui, "CSG Stack", flags);
	Select(SelectionType_CSGStack, stack, status.selected);

	if (status.open)
	{
		if (UIBeginTreeNode(ui, "Nodes"))
		{
			for (CSGStackNode& child : stack->nodes)
			{
				switch (child.type)
				{
					case CSGStackNodeType_Brush:
					{
						InspectorCSGBrush(ui, child.brush);
						break;
					}
					case CSGStackNodeType_Stack:
					{
						InspectorCSGStack(ui, child.stack);
						break;
					}
					default:
					{
						assert(0);
					}

				}
			}
			UIEndTreeNode(ui);
		}
		if (UIBeginTreeNode(ui, "Built"))
		{
			for (CSGBrush* brush : stack->built_brushes)
			{
				InspectorCSGBrush(ui, brush);
			}
		}
		UIEndTreeNode(ui);
	}
}

void Inspector(UIContext& ui)
{
	UIBeginTreeList(ui);
	InspectorCSGStack(ui, ActiveGame->csg);
	UIEndTreeList(ui);
}

void EditorTick()
{

	Inspector(main_ui);
}
