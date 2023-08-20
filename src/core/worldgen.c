#include <SDL2/SDL_render.h>
#include <core.h>
#include <modules.h>

Context* ctx;

int init(void* self, void* ctx_ref){ 
  printf("{WorldGen 2} '%s'\n",__DATE__);
  ctx = ctx_ref;
  //enable updating every 10 ticks
  Mod(self)->upf = 30;
  return 0;
}

void update(void* self,uint32_t delta){
  (void)delta;
  (void)self;
  // printf("Checking world border %s\n",Mod(self)->path);
  SDL_SetRenderDrawColor(ctx->renderer, 255,100,100,255);
  SDL_RenderClear(ctx->renderer);
}

int handle(void* self){
  (void)self;
  return 0;
}

void destroy(void* self){
  printf("{Destroy WorldGen} '%s'\n",__DATE__);
}


