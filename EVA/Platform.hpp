#pragma once

struct SDL_Window;

extern SDL_Window* GameWindow;
extern int         WindowWidth;
extern int         WindowHeight;
extern bool        DoQuit;
extern double      DeltaTime;
extern bool        InMenu;
extern float       FPS;