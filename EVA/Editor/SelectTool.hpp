#pragma once
#include <EVA/Editor/Tool.hpp>
#include <EVA/Math.hpp>

class SelectTool : public Tool {
public:
	ECLASS_COMMON();

	float3 m_gizmoCenter = {};

	SelectTool();
	virtual void Tick(double dt) override;
};
