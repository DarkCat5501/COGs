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

// project stuff
#define SRC(X) PATH(SRC_PATH, X)
#define DEP(X) PATH(BUILD_PATH, X)
#define BDEP(X) PATH(BDEPS_PATH, X)
#define PROJECT_NAME "game"

// compiler stuff
#define CC "gcc"
#define CPP "g++"

#define CMFLAGS "-Wall", "-Wextra", "-pedantic", "-ggdb"
#define CFLAGS "-std=c11"
#define CXXFLAGS "-std=c++17"
#define CDYNLINK "-fPIC"
#define LDYNLINK "-shared"
#define LFLAGS "-lSDL2"

#define INC(X) CONCAT("-I", X)
#define INCLUDES                                                               \
  INC("./includes"), INC(PATH(ENV_LIBS, "stb")), INC(PATH(ENV_LIBS, "glm"))
// PATH(ENV_LIBS,"/sol2/include")
// CONCAT("-I",PATH(ENV_LIBS,"eigen")),\

// char version[16] = {0};

enum Modules { Module_All, Module_WorldGen };
enum Builds {
  Build_Invalid,
  Build_All,
  Build_Modules,
  Build_Module,
  Build_App
};
int build = Build_Invalid;
int build_module = Module_All;

Cstr_Array split(char *data, Cstr delim) {
  // Extract the first token
  Cstr_Array tokens = {0};
  char *token = strtok(data, delim);
  // loop through the string to extract all other tokens
  while (token != NULL) {
    tokens = cstr_array_append(tokens, token);
    token = strtok(NULL, delim);
  }

  return tokens;
}

char *read_file(Cstr file_path) {
  // read building dependencies of file
#ifndef _WIN32
  Fd file = fd_open_for_read(file_path);
  if (file < 0) {
    WARN("Could not read %s because %s", file_path, strerror(errno));
    return NULL;
  }

  struct stat statbuf;
  if (fstat(file, &statbuf) == -1) {
    fd_close(file);
    return NULL;
  }

  __off_t size = lseek(file, 0, SEEK_END);
  if (size == -1) {
    PANIC("Failed to read file %s\n", file_path);
    exit(-1);
  }
  if (lseek(file, 0, SEEK_SET)) {
    PANIC("Failed to read file %s\n", file_path);
    exit(-1);
  }

  char *buffer = (char *)malloc(size + 1);
  memset(buffer, 0, size + 1);
  size_t rd = read(file, buffer, size);
  fd_close(file);

  INFO("Checking dependencies file '%s' { s: %lld(b) r: %zu(b)}", file_path,
       (long long)size, rd);
  return buffer;

#else
  assert(false); // NOT Implemented for windows
#endif
}

int check_build_deps(Cstr current, Cstr_Array deps) {
  int needs_rebuild = 0;
  for (int i = 0; i < deps.count; i++) {
    Cstr dep = deps.elems[i];
    // INFO("checking %s",dep);
    if (is_path1_modified_after_path2(dep, current)) {
      INFO("Dependency modified: '%s' > '%s'", dep, current);
      needs_rebuild = 1;
      break;
    }
  }
  return needs_rebuild;
}

int check_deps(Cstr bdeps_file_path) {
  char *deps_data = read_file(bdeps_file_path);
  if (deps_data == NULL)
    return 0;

  Cstr_Array dt = split(deps_data, ": \\\n");
  if (dt.count < 1) {
    WARN("No valid dependencies found: {%s}\n:---:\n%s\n:---:", strerror(errno),
         deps_data);
    exit(1);
  }

  Cstr dep_obj = dt.elems[0];
  int needs_rebuild = check_build_deps(dep_obj, dt);

  free(deps_data);
  return needs_rebuild;
}

int build_modules(void) {
  int total_rebuild = 0;
  // worldgen
  if (build_module == Module_All || build_module == Module_WorldGen) {
    Cstr worldgen_c = SRC("core/worldgen.c"), worldgen_o = DEP("worldgen.o"),
         worldgen_d = BDEP("worldgen.d");

    if (PATH_EXISTS(worldgen_d)) {
      if (!check_deps(worldgen_d))
        goto worldgen_end;
    }
    INFO("==== Compiling '%s'", worldgen_c);
    CMD(CC, CMFLAGS, CFLAGS, CDYNLINK, "-MD", "-MF", worldgen_d, "-c",
        worldgen_c, "-o", worldgen_o, INCLUDES);
  worldgen_end : {};
  }
  return total_rebuild;
}

int link_modules(void) {
  int total_linked = 0;
  if (build_module == Module_All || build_module == Module_WorldGen) {
    Cstr worldgen_lib = CONCAT("worldgen", ".so");
    Cstr worldgen_file = PATH(MODULES_PATH, worldgen_lib);
#define DEPS DEP("worldgen.o")
    if (PATH_EXISTS(worldgen_file)) {
      Cstr_Array deps_array = MAKE_CSTR_ARRAY(DEPS);
      if (!check_build_deps(worldgen_file, deps_array))
        goto worldgen_end;
    }
    INFO("==== Linking '%s' ====", worldgen_file);
    // CMD(CC,"-o", app_file,DEPS,LFLAGS);
    CMD(CC, LDYNLINK, "-o", worldgen_file, DEPS, LFLAGS);
    total_linked++;
  worldgen_end : {};
#undef DEPS
  }
  return total_linked;
}

int build_app(void) {
  Cstr main_c = SRC("main.c"), main_o = DEP("main.o"), main_d = BDEP("main.d");
  int total_rebuild = 0;

  if (PATH_EXISTS(main_d)) {
    if (!check_deps(main_d))
      goto end;
  }
  INFO("==== Compiling '%s'", main_c);
  CMD(CC, CMFLAGS, CFLAGS, "-MD", "-MF", main_d, "-c", main_c, "-o", main_o,
      INCLUDES, "-DMODULE_IMPLEMENTATION");

end:
  return total_rebuild;
}

int link_app(void) {
  Cstr app_name = CONCAT(PROJECT_NAME, ".out");
  Cstr app_file = PATH(BUILD_PATH, BUILD_TYPE, app_name);
#define DEPS DEP("main.o")

  if (PATH_EXISTS(app_file)) {
    Cstr_Array deps_array = MAKE_CSTR_ARRAY(DEPS);
    if (!check_build_deps(app_file, deps_array))
      return 0;
  }

  INFO("==== Linking '%s' ====", app_file);
  CMD(CC, "-o", app_file, DEPS, LFLAGS);

#undef DEPS
  return 1;
}

void show_usage(Cstr program) {
  printf("Usage:\n\t%s [all|modules|module|app] {options}\n", program);
}

int main(int argc, char **argv) {
  GO_REBUILD_URSELF((size_t)argc, argv);

  if (argc < 1) {
    PANIC("Program not initialized correctly");
    exit(-1);
  }

  Cstr nobuild_exec = shift_args(&argc, &argv);
  if (argc < 1) {
    show_usage(nobuild_exec);
    exit(1);
  } else {
    Cstr action = shift_args(&argc, &argv);
    if (strcmp(action, "all") == 0)
      build = Build_All;
    else if (strcmp(action, "modules") == 0)
      build = Build_Modules;
    else if (strcmp(action, "app") == 0)
      build = Build_App;
    else if (strcmp(action, "module") == 0) {
      build = Build_Module;
      if (argc < 1) {
        ERRO("No module provided!");
        exit(1);
      }
      Cstr module = shift_args(&argc, &argv);
      if (strcmp(module, "worldgen") == 0)
        build_module = Module_WorldGen;
      else {
        ERRO("Invalid module %s", module);
        exit(1);
      }
    }
  }

  // geting build version
  const clock_t start = T_NOW_MS;
  // snprintf(version,sizeof(version),"T%08lX",start);

  // make sure the build paths are present
  if (!path_exists(BUILD_PATH))
    MKDIRS(BUILD_PATH);
  if (!path_exists(BDEPS_PATH))
    MKDIRS(BDEPS_PATH);
  if (!path_exists(MODULES_PATH))
    MKDIRS(MODULES_PATH);
  if (!path_exists(PATH(BUILD_PATH, BUILD_TYPE)))
    MKDIRS(BUILD_PATH, BUILD_TYPE);

  int total_build = 0;

  if (build == Build_All || build == Build_App) {
    total_build += build_app();
    total_build += link_app();
  }
  if (build == Build_All || build == Build_Module || build == Build_Modules) {
    total_build += build_modules();
    total_build += link_modules();
  }

  const clock_t delta = (T_NOW_MS - start);
  INFO("Building %d packages took: %6.3f(s)\n", total_build, delta / 1000.0f);

  return 0;
}
