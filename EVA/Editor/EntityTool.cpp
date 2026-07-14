#include <EVA/Editor/EntityTool.hpp>
#include <EVA/Editor/Editor.hpp>
#include <EVA/Entities/ECamera.hpp>
#include <EVA/Input.hpp>
#include <EVA/Platform.hpp>
#include <EVA/GFX/Renderer.hpp>
#include <EVA/UI.hpp>

EntityTool::EntityTool() {
	m_name = "Entity";
	m_shortName = "ENT";
	m_order = 2;
}

void EntityTool::Tick(double dt) {
	Editor* ed = m_editor;
	Ray mouse_ray = CameraScreenToRay(*ed->m_camera, g_mouse_position);
	Entity* hit_entity = nullptr;
	if (ed->RaycastAgainstEntities(mouse_ray, nullptr, &hit_entity)) {
		if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT)) ed->SelectEntity(hit_entity);
		return;
	}

	float min_t = FLT_MAX;
	if (ed->RaycastAgainstBuiltBrushes(mouse_ray, &min_t, nullptr, nullptr)) {
		float3 p = mouse_ray.Evaluate(min_t);
		if (ed->ShouldSnap()) p = ed->SnapToGrid(p);
		DrawPoint(p, {0,1,0,1});
		if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT)) {
			if (m_entityType) ed->CreateEntity(m_entityType, p);
			ed->Deselect();
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
