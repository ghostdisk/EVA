#pragma once
#include <EVA/Editor/Tool.hpp>

class Type;

class EntityTool : public Tool {
public:
	ECLASS_COMMON(EntityTool);

	Type* m_entityType = nullptr;

	EntityTool();
	virtual void Tick(double dt) override;
	virtual void DrawSidebar() override;
};
