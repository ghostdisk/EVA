#include <EVA/GameModes/EditorGameMode.hpp>
#include <EVA/Editor/Editor.hpp>

void EditorGameMode::OnBegin() {
	editor = new Editor(m_game, m_entityManager);
}

void EditorGameMode::OnEnd() {
	delete editor;
	editor = nullptr;
}

void EditorGameMode::OnTick(double dt) {
	editor->Tick(dt);
}

Result EditorGameMode::LoadMap(String name) {
	return editor->LoadMap(name);
}
