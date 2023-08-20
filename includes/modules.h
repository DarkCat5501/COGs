#ifndef MODULES_H_
#define MODULES_H_
#include <stdint.h>
#include <dlfcn.h>
#include <stdio.h>

typedef int (*ModuleInit)(void* self, void* ctx);
typedef void (*ModuleUpdate)(void* self,uint32_t delta);
typedef void (*ModuleDestroy)(void* self);
typedef int (*ModuleHandleEvent)(void* self);

typedef struct {
  const char* path;
  ModuleInit init;
  ModuleUpdate update;
  ModuleDestroy destroy;
  ModuleHandleEvent handle;
  void* lib;
  int l_time;
  int needs_reload;
  uint32_t upf,l_updated; //update frequency (ms / ticks)
} Module;

#define Mod(X) ((Module*)X)

#ifdef MODULE_IMPLEMENTATION
static int void_init(void* self, void* ctx){ printf("invalid module {Init} '%s'\n",Mod(self)->path); (void)ctx; return 0; }
static void void_update(void* self,uint32_t delta){ (void)delta;(void)self; printf("module updated %s\n",Mod(self)->path); }
static void void_destroy(void* self){ printf("invalid module {Destroy} '%s'\n",Mod(self)->path); }
static int void_handle(void* self){ (void)self;return 0; }

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#define PATH_SEP "/"
typedef pid_t Pid;
typedef int Fd;
#endif

#if defined(__GNUC__) || defined(__clang__)
// https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html
#define NOBUILD_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (printf, STRING_INDEX, FIRST_TO_CHECK)))
#else
#define NOBUILD_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK)
#endif
typedef const char* Cstr;
// void VLOG(FILE *stream, Cstr tag, Cstr fmt, va_list args);
// void INFO(Cstr fmt, ...) NOBUILD_PRINTF_FORMAT(1, 2);
// void WARN(Cstr fmt, ...) NOBUILD_PRINTF_FORMAT(1, 2);
// void ERRO(Cstr fmt, ...) NOBUILD_PRINTF_FORMAT(1, 2);
// void PANIC(Cstr fmt, ...) NOBUILD_PRINTF_FORMAT(1, 2);
#define PANIC(fmt,...) fprintf(stderr, fmt,__VA_ARGS__);

enum {
  MODULE_OK,
  MODULE_ERROR_LOADING,
  MODULE_ERROR_INIT,
  MODULE_ERROR_DESTROY,
};

static void start_module(Module* module) {
  //start all module functions to valid functions 
  if(!module->init) module->init = &void_init;
  if(!module->update) module->update = &void_update;
  if(!module->destroy) module->destroy = &void_destroy;
  if(!module->handle) module->handle = &void_handle;
}

static void clean_module(Module* module) {
  if(module->destroy) module->destroy(module);
  if(module->lib){
    dlclose(module->lib);
    printf("Closing handle of %s: %s\n", module->path, dlerror());
    module->lib = NULL;
  }
  start_module(module);
}

static int load_module(Module* module) {
  printf("loading module: %s\n", module->path);
  start_module(module); //always try to make a valid module
  
  clean_module(module);
  void* new_lib = dlopen(module->path,RTLD_NOW);

  if(!new_lib){
    fprintf(stderr, "Error loading library '%s':\n\t%s\n", module->path, dlerror());
    return MODULE_ERROR_LOADING;
  } else module->lib = new_lib;

  ModuleInit init = (ModuleInit)dlsym(new_lib, "init");
  if(!init){
    fprintf(stderr, "Error loading init function of '%s': %s\n", module->path, dlerror());
    return MODULE_ERROR_INIT;
  } else module->init = init;

  ModuleDestroy destroy = (ModuleDestroy)dlsym(new_lib, "destroy");
  if(!destroy){
    fprintf(stderr, "Error loading init function of '%s': %s\n", module->path, dlerror());
    return MODULE_ERROR_DESTROY;
  } else module->destroy = destroy;

  ModuleUpdate update = (ModuleUpdate)dlsym(new_lib, "update");
  if(!update){
    fprintf(stderr, "Error loading update function of '%s': %s\n", module->path, dlerror());
  } else module->update = update;

  ModuleHandleEvent handle = (ModuleHandleEvent)dlsym(new_lib, "handle");
  if(!handle){
    fprintf(stderr, "Warning: handle function of '%s' not found: %s\n", module->path, dlerror());
  } else module->handle = handle;

  return MODULE_OK;
}

static int path_exists(Cstr path) {
#ifdef _WIN32
    DWORD dwAttrib = GetFileAttributes(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES);
#else
    struct stat statbuf = {0};
    if (stat(path, &statbuf) < 0) {
        if (errno == ENOENT) {
            errno = 0;
            return 0;
        }

        PANIC("could not retrieve information about file %s: %s",
              path, strerror(errno));
    }

    return 1;
#endif
}

static int get_last_modifed_time(Cstr path){
    struct stat statbuf = {0};
    if (stat(path, &statbuf) < 0) {
        PANIC("could not stat %s: %s\n", path, strerror(errno));
    }
    return statbuf.st_mtime;
} 

static void load_modules(Module modules[], size_t modules_count) {
  for(size_t i =0;i<modules_count;i++){
    load_module(&modules[i]);  
    if(path_exists(modules[i].path)){
      modules[i].l_time = get_last_modifed_time(modules[i].path);
    }
  }
}

static void check_modules(Module modules[], size_t modules_count, void* ctx) {
  for(size_t i =0;i<modules_count;i++){
    if(path_exists(modules[i].path)){
      Module* module = &modules[i];
      int l_time = get_last_modifed_time(module->path);
      if(module->needs_reload){
        load_module(module);
        module->l_time = l_time;
        module->needs_reload = 0;
        //reinitialize the module
        module->init(module,ctx);
      } else if(l_time > module->l_time){
        printf("found an outdated module: %s\n", module->path);
        module->needs_reload = 1;
      }
    }
  }
}

static void init_modules(Module modules[], size_t modules_count,void* ctx) {
  for(size_t i =0;i<modules_count;i++){
    Module* module = &modules[i];
    module->init(module,ctx);
  }
}

static void update_modules(Module modules[], size_t modules_count,uint32_t d) {
  for(size_t i =0;i<modules_count;i++){
    Module* module = &modules[i];
    if(module->upf == 0) continue;
    if(d > module->l_updated+module->upf){
      module->l_updated = d;
      module->update(module,d);
    }
  }
}

static void destroy_modules(Module modules[], size_t modules_count) {
  for(size_t i =0;i<modules_count;i++){
    Module* module = &modules[i];
    module->destroy(module);
  }
}

static void handle_actions(Module modules[], size_t modules_count) {
  for(size_t i =0;i<modules_count;i++){
    Module* module = &modules[i];
    if(modules->handle(module)) break;
  }
}

#endif //MODULE_IMPLEMENTATION
#endif //MODULES_H_

