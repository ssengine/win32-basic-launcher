#pragma once

#if defined(__cplusplus)
extern "C"{
#endif

void load_project(const char* path);
void load_user_configure(const char* path);

void init_defines();

#if defined(__cplusplus)
}
#endif