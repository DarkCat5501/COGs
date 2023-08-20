#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define NOBUILD_IMPLEMENTATION
#include "nobuild.h"

#define SRC_PATH "src"
#define MODULES_PATH "module"
#define BUILD_PATH "build"
#define BDEPS_PATH "build/deps"
#define BUILD_TYPE "debug"

//project stuff
#define SRC(X) PATH(SRC_PATH,X)
#define DEP(X) PATH(BUILD_PATH,X)
#define BDEP(X) PATH(BDEPS_PATH,X)
#define PROJECT_NAME "game"

//compiler stuff
#define CC "gcc"
#define CPP "g++"

#define CMFLAGS "-Wall",\
    "-Wextra","-pedantic","-ggdb"
#define CFLAGS "-std=c99"
#define CXXFLAGS "-std=c++17"
#define CDYNLINK "-fPIC"
#define LDYNLINK "-shared"
#define LFLAGS "-lSDL2"

#define INCLUDES CONCAT("-I",PATH(ENV_LIBS,"stb"))
                 // CONCAT("-I",PATH(ENV_LIBS,"eigen")),\
                 // CONCAT("-I",PATH(ENV_LIBS,"glm"))
                 // PATH(ENV_LIBS,"/sol2/include")

// char version[16] = {0};
// void build_module(void){
//   MKDIRS(BUILD_PATH);
//   CMD(CC, CFLAGS, CDYNLINK, "-c", SRC("module.c"), "-o",DEP("module.o"), INCLUDES);
// }

// void link_modules(void){
//   MKDIRS(MODULES_PATH);
//   CMD(CC,"-o",LDYNLINK,"libmodule1",
//     DEP("module.o")
//   );
//}

Cstr_Array split(char* data,Cstr delim){
  // Extract the first token
  Cstr_Array tokens = {0};
  char * token = strtok(data, delim);
  // loop through the string to extract all other tokens
  while( token != NULL ) {
    tokens = cstr_array_append(tokens, token);
    token = strtok(NULL, delim);
  }

  return tokens;
}

char* read_file(Cstr file_path){
  //read building dependencies of file
#ifndef _WIN32
  Fd file = fd_open_for_read(file_path);
  if(file < 0){
    WARN("Could not read %s because %s", file_path, strerror(errno));
    return NULL;
  }

  struct stat statbuf;
  if(fstat(file,&statbuf)==-1){
    fd_close(file);
    return NULL;
  }

  __off_t size = lseek(file, 0, SEEK_END);
  if(size == -1){ PANIC("Failed to read file %s\n",file_path); exit(-1); }
  if(lseek(file,0, SEEK_SET)){ PANIC("Failed to read file %s\n",file_path); exit(-1); }
  
  char* buffer = (char*)malloc(size + 1);
  memset(buffer,0,size + 1);
  size_t rd = read(file,buffer, size);
  fd_close(file);

  INFO("Checking dependencies file '%s' { s: %lld(b) r: %zu(b)}",file_path,(long long)size,rd);
  return buffer;

#else
assert(false); //NOT Implemented for windows
#endif
}

int check_build_deps(Cstr current, Cstr_Array deps){
  int needs_rebuild = 0;
  for(int i=0;i<deps.count;i++){
    Cstr dep = deps.elems[i];
    // INFO("checking %s",dep);
    if(is_path1_modified_after_path2(dep, current)){
      INFO("Dependency modified: '%s' > '%s'",dep,current);
      needs_rebuild = 1;
      break;
    }
  }
  return needs_rebuild;
}

int check_deps(Cstr bdeps_file_path){
  char* deps_data = read_file(bdeps_file_path);
  if(deps_data==NULL) return 0;

  Cstr_Array dt = split(deps_data, ": \\\n");
  if(dt.count < 1){
    WARN("No valid dependencies found: {%s}\n:---:\n%s\n:---:", strerror(errno),deps_data);
    exit(1);
  }

  Cstr dep_obj = dt.elems[0];
  int needs_rebuild = check_build_deps(dep_obj, dt);

  free(deps_data);
  return needs_rebuild;
}

int build_app(void) {
  Cstr main_c = SRC("main.c"), main_o = DEP("main.o"), main_d = BDEP("main.d");
  int total_rebuild = 0;

  if(PATH_EXISTS(main_d)){
    if(!check_deps(main_d)) goto end;
  }
  INFO("==== Compiling '%s'",main_c);
  CMD(CC, CMFLAGS, CFLAGS,"-MD","-MF",main_d,"-c", main_c, "-o", main_o, INCLUDES);

  end:
  return total_rebuild;
}

int link_app(void){
  Cstr app_name = CONCAT(PROJECT_NAME,".out");
  Cstr app_file = PATH(BUILD_PATH,BUILD_TYPE,app_name);

  #define DEPS DEP("main.o")

  if(PATH_EXISTS(app_file)){
    Cstr_Array deps_array = MAKE_CSTR_ARRAY(DEPS);
    if(!check_build_deps(app_file, deps_array)) return 0;
  }

  INFO("==== Linking '%s' ====",app_file);
  CMD(CC,LFLAGS, "-o", app_file,DEPS);

  #undef DEPS
  return 1;
}

int main(int argc, char** argv){
  GO_REBUILD_URSELF((size_t)argc, argv);

  //geting build version
  const clock_t start = T_NOW_MS;
  // snprintf(version,sizeof(version),"T%08lX",start);

  //make sure the build paths are present
  if(!path_exists(BUILD_PATH)) MKDIRS(BUILD_PATH);
  if(!path_exists(BDEPS_PATH)) MKDIRS(BDEPS_PATH);
  if(!path_exists(MODULES_PATH)) MKDIRS(MODULES_PATH);
  if(!path_exists(PATH(BUILD_PATH,BUILD_TYPE))) MKDIRS(BUILD_PATH,BUILD_TYPE);

  int total_build = build_app();
  total_build += link_app();

  const clock_t delta = (T_NOW_MS - start);
  INFO("Building %d packages took: %6.3f(s)\n",total_build, delta / 1000.0f);

  return 0;
}
