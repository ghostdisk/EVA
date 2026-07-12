#include <EVA/Editor/SelectTool.hpp>
#include <EVA/Editor/Editor.hpp>
#include <EVA/Assets/Mesh.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Entities/ECamera.hpp>
#include <EVA/Input.hpp>
#include <EVA/Platform.hpp>
#include <EVA/UI.hpp>

SelectTool::SelectTool() {
	m_name = "Select";
	m_shortName = "SEL";
	m_order = 1;
}

void SelectTool::Tick(double dt) {
	Editor* ed = m_editor;
	Ray mouse_ray = CameraScreenToRay(*ed->m_camera, g_mouse_position);
	bool dirty = false;

	std::vector<EdOp*> selected_ops = {};
	for (const EdSelection& sel : ed->m_selection) {
		if (sel.type == EdSelectionType_Node) {
			if (sel.op->type == EdOpType_Brush) {
				for (int i = 0; i < sel.op->brush->planes.size(); i++)
					if (ed->DoPlaneDragGizmo(sel.op, sel.op->brush, i)) dirty = true;
			}
			selected_ops.push_back(sel.op);
		}
		if (sel.type == EdSelectionType_BrushPlane && !ed->IsSelected(sel.op))
			if (ed->DoPlaneDragGizmo(sel.op, sel.op->brush, sel.index)) dirty = true;
	}

	if (selected_ops.size()) {
		if (!ed->m_activeGizmoState.id) {
			m_gizmoCenter = {};
			for (EdOp* op : selected_ops) m_gizmoCenter += op->global_transform.column(3).xyz();
			m_gizmoCenter /= selected_ops.size();
		}

		ed->m_hashStack.Push(&m_gizmoCenter);
		DEFER(ed->m_hashStack.Pop());
		float3 old_center = m_gizmoCenter;
		ed->TranslationGizmo(&m_gizmoCenter, m_gizmoCenter);
		float3 d = m_gizmoCenter - old_center;
		if (abs(d.Length()) > 0.0001f) {
			for (EdOp* op : selected_ops) op->position += d;
			dirty = true;
		}
	}

	if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT) && !ed->m_activeGizmoState.id) {
		float min_t = FLT_MAX;
		EdOp* hit_op = nullptr;
		bool hit = InputGetButton(SDL_SCANCODE_LALT)
			? ed->RaycastAgainstSubtractOps(mouse_ray, &min_t, &hit_op)
			: ed->RaycastAgainstBuiltBrushes(mouse_ray, &min_t, &hit_op, nullptr);
		float entity_t = FLT_MAX;
		Entity* hit_entity = nullptr;
		bool hit_ent = ed->RaycastAgainstEntities(mouse_ray, &entity_t, &hit_entity);
		if (hit_ent && (!hit || entity_t < min_t)) ed->SelectEntity(hit_entity, InputGetButton(SDL_SCANCODE_LSHIFT));
		else if (hit) ed->SelectOp(hit_op, InputGetButton(SDL_SCANCODE_LSHIFT));
		else if (!InputGetButton(SDL_SCANCODE_LSHIFT)) ed->Deselect();
	}

	if (dirty) ed->Build(ed->m_root);
}
