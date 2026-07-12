#include <EVA/Game.hpp>
#include <EVA/GameModes/GameMode.hpp>

GameMode* GameMode::GetCurrent() {
	if (!g_active_game) return nullptr;
	return g_active_game->GetGameMode();
}

void GameMode::Init(Game* game, EntityManager* entityManager) {
	assert(!m_game && !m_entityManager);
	m_game = game;
	m_entityManager = entityManager;
}

Result GameMode::LoadMap(String name) {
	return Err("LoadMap not supported for this game mode");
}