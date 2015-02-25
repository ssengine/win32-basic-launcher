#pragma once
#include <ssengine/core.h>

#if defined(__cplusplus)
extern "C"{
#endif

void load_project(ss_core_context* C, const char* path);
void load_user_configure(ss_core_context* C, const char* path);

void init_defines(ss_core_context* C);

#if defined(__cplusplus)
}
#endif