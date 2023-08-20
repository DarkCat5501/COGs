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
    cstr_array_append(tokens, token);
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
  if(lseek(bdeps_file,0, SEEK_SET)){ PANIC("Failed to read file %s\n",bdeps_file_path); exit(-1); }
  
  char* buffer = (char*)malloc(size + 1);
  memset(buffer,0,size + 1);
  size_t rd = read(bdeps_file,buffer, size);
  fd_close(bdeps_file);

  INFO("file content: size:%lld read: %zul\n%s",(long long)size,rd,buffer);
    return buffer;
  }
#else
assert(false); //NOT Implemented for windows
#endif
}

void check_bdeps(Cstr bdeps_file_path){
}

void build_app(void) {
  Cstr main_c = SRC("main.c");
  Cstr main_o = DEP("main.o");
  Cstr main_d = BDEP("main.d");

  if(PATH_EXISTS(main_d)){
    check_bdeps(main_d);
  }
  return;

  CMD(CC, CMFLAGS, CFLAGS,
      "-MD","-MF",main_d,
      "-c", main_c, "-o", main_o, INCLUDES);
}

void link_app(void){
  Cstr app_name = CONCAT(PROJECT_NAME,".out");
  CMD(CC,LFLAGS, "-o", PATH(BUILD_PATH,BUILD_TYPE,app_name),
    DEP("main.o")
  );
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

  build_app();
  link_app();

  const clock_t delta = (T_NOW_MS - start);
  INFO("Build took: %6.3f(s)\n", delta / 1000.0f);

  return 0;
}
