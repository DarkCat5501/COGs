#ifndef CORE_H_
#define CORE_H_

#include "modules.h"
#include <stdbool.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>

typedef struct {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Event event;
  bool running;
} Context;

#endif // CORE_H_
