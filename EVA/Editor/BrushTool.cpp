#include <EVA/Editor/BrushTool.hpp>
#include <EVA/Editor/Editor.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Entities/ECamera.hpp>
#include <EVA/Input.hpp>
#include <EVA/Platform.hpp>
#include <EVA/GFX/Renderer.hpp>
#include <EVA/UI.hpp>
#include <algorithm>

BrushTool::BrushTool() {
	m_name = "Brush";
	m_shortName = "BSH";
	m_order = 3;
}

void BrushTool::Tick(double dt) {
	Editor* ed = m_editor;
	Ray mouse_ray = CameraScreenToRay(*ed->m_camera, g_mouse_position);
	float min_t = FLT_MAX;
	bool hit = ed->RaycastAgainstBuiltBrushes(mouse_ray, &min_t, nullptr, nullptr);
	float3 p = {};
	if (hit) {
		p = mouse_ray.Evaluate(min_t);
		if (ed->ShouldSnap()) p = ed->SnapToGrid(p);
	}

	if (InputGetButton(SDL_SCANCODE_ESCAPE)) m_phase = Phase_Inactive;
	if (hit) {
		DrawSetLayer(Layer_Overlay);
		DrawPoint(m_start, {0,0,1,0.1});
		DrawSetLayer(Layer_Main);
		DrawPoint(m_start, {0,0,1,1});
	}

	Step:
	switch (m_phase) {
		case Phase_Inactive:
			if (hit) {
				m_start = p;
				if (!UICapturesMouse() && InputGetButtonDown(INPUT_BUTTON_MOUSE_LEFT)) m_phase = Phase_InitialDraw;
			}
			break;
		case Phase_InitialDraw:
			m_end = p;
			if (InputGetButtonUp(INPUT_BUTTON_MOUSE_LEFT)) {
				if (hit) { m_justEnteredPhase = false; m_phase = Phase_X; }
				else m_phase = Phase_Inactive;
				goto Step;
			}
			break;
		case Phase_X:
		case Phase_Y:
		case Phase_Z: {
			float3 dir = {};
			dir[m_phase - Phase_X] = 1.0f;
			if (!m_justEnteredPhase) {
				m_justEnteredPhase = true;
				if (Dot(dir, m_end - m_start) != 0) {
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
			if (ed->ShouldSnap()) m_end = ed->SnapToGrid(m_end);
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
			EdOp* op = new EdOp();
			op->type = EdOpType_Brush;
			op->brush = cube;
			op->position = m_start;
			ed->m_root->children.push_back(op);
			op->parent = ed->m_root;
			ed->Build(ed->m_root);
			ed->SelectOp(op);
			m_phase = Phase_Inactive;
			break;
		}
	}

	if (m_phase != Phase_Inactive && !(m_phase == Phase_InitialDraw && !hit)) {
		DrawSetLayer(Layer_Overlay);
		DrawBox(m_start, m_end, {0, 0, 1, 0.1});
		DrawSetLayer(Layer_Main);
		DrawBox(m_start, m_end, {0, 0, 1, 1});
	}
}

void BrushTool::OnDeactivate() {
	m_phase = Phase_Inactive;
}
