#pragma once
#include <EVA/Core/Object.hpp>

class Editor;

class Tool : public Object {
public:
	ECLASS_COMMON(Tool);

	Editor*  m_editor    = nullptr;
	ZTString m_name      = {};
	ZTString m_shortName = {};
	int      m_order     = 0;

	virtual void OnActivate() {}
	virtual void OnDeactivate() {}
	virtual void Tick(double dt) = 0;
	virtual void DrawSidebar() {}
};
