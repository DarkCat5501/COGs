#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <game.h>
#include <modules.h>

#define MOD_CHECK_INTERVAL 1000
#define MOD_TICK_INTERVAL 100

Context ctx;
#define MOD(X) "module/"X".so"

Module modules[] = {
  {.path = MOD("worldgen")}
};
const size_t mods_count = sizeof(modules)/sizeof(Module);

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  SDL_Init(SDL_INIT_EVERYTHING);
  ctx.window = SDL_CreateWindow("app", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 500, SDL_WINDOW_RESIZABLE);
  ctx.renderer = SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_SOFTWARE);
  ctx.running = true;

  //load and init mods
  load_modules(modules,mods_count);
  //load and init mods
  init_modules(modules,mods_count,&ctx);
  printf("started\n");

  uint32_t last_checked = 0;
  uint32_t last_tick = 0;

  while (ctx.running) {
    uint32_t timer = SDL_GetTicks();
    if(timer > (last_checked+MOD_CHECK_INTERVAL)){
      last_checked = timer;
      check_modules(modules,mods_count,&ctx);
    }
    if(timer > (last_tick + MOD_TICK_INTERVAL)){
      last_tick = timer;
      update_modules(modules, mods_count, timer / MOD_TICK_INTERVAL);
    }

    while (SDL_PollEvent(&ctx.event)) {
      switch (ctx.event.type) {
      case SDL_QUIT:
        ctx.running = false;
        break;
      default:
        break;
      }
    }

    SDL_RenderPresent(ctx.renderer);
  }

  //destroy mods
  destroy_modules(modules, mods_count);
  SDL_DestroyRenderer(ctx.renderer);
  SDL_DestroyWindow(ctx.window);
  SDL_Quit();
  return 0;
}
