#include <ssengine/macros.h>

#include "launcher.h"

#include <Windows.h>

void init_defines(ss_core_context* C){
	ss_macro_define(C, "LAUNCHER_NAME", "Win32 Basic Launcher");
	ss_macro_define(C, "PLATFORM", "Win32");

	ss_macro_define(C, "USER_CONFIG_FILE", "{PROJECT_FILE}.{OS_USERNAME}.user");
	
	ss_macro_define(C, "WINDOW_TITLE", "{EMULATORS({EMULATOR})(title)}");
	ss_macro_define(C, "WINDOW_WIDTH", "{EMULATORS({EMULATOR})(width)}");
	ss_macro_define(C, "WINDOW_HEIGHT", "{EMULATORS({EMULATOR})(height)}");

	ss_macro_define(C, "USE_DEBUG_BG", "1");

	//ss_macro_define("RENDERER_LIB", "SSGLRenderer.dll");
	//ss_macro_define("RENDERER_TYPE", "32");

	ss_macro_define(C, "RENDERER_LIBS(0)", "SSGLRenderer.dll");
	ss_macro_define(C, "RENDERER_TYPES(0)", "16");

	char Buf[257];
	DWORD sz = 257;
	if (GetUserNameA(Buf, &sz)){
		ss_macro_define(C, "OS_USERNAME", Buf);
	}
}
