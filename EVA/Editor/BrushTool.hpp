#pragma once
#include <EVA/Editor/Tool.hpp>
#include <EVA/Math.hpp>

class BrushTool : public Tool {
public:
	ECLASS_COMMON();

	enum Phase {
		Phase_Inactive,
		Phase_InitialDraw,
		Phase_X,
		Phase_Y,
		Phase_Z,
		Phase_Finalize,
	};

	Phase  m_phase = Phase_Inactive;
	float3 m_start = {};
	float3 m_end = {};
	bool   m_justEnteredPhase = false;
	float3 m_refA = {};
	float3 m_refB = {};
	float3 m_axisStart = {};

	BrushTool();
	virtual void OnDeactivate() override;
	virtual void Tick(double dt) override;
};
