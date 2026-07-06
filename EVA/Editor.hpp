#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct CSGBrush;
struct ECamera;

enum EdOpType {
	EdOpType_None   = 0,
	EdOpType_Brush  = 1,
	EdOpType_Stack  = 2,

	EdOpType_ENUM_SIZE,
};

struct EdOp {
	EdOp*                  parent           = nullptr;
	EdOpType               type             = EdOpType_None;
	std::vector<CSGBrush*> built            = {};
	bool                   subtract         = false;
	float3                 position         = {};
	float4x4               global_transform = {};

	std::vector<EdOp*>     children = {};      // for EdOpType_Stack
	CSGBrush*              brush    = nullptr; // for EdOpType_Brush
};

void EdInitialize();
void EdTick();

extern ECamera* g_editor_camera;