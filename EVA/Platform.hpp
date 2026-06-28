#pragma once
#include <EVA/math.hpp>

struct SDL_Window;
typedef union SDL_Event SDL_Event;

void PlatformInitialize();
void PlatformBeginFrame();
bool PlatformProcessSDLEvent(SDL_Event* event);


extern double      g_delta_time;
extern float2      g_window_size;
extern float       g_fps;
extern bool        g_quit;
extern SDL_Window* g_game_window;