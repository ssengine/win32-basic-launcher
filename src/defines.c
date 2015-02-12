#include <ssengine/macros.h>

#include "launcher.h"

#include <Windows.h>

void init_defines(){
	ss_macro_define("LAUNCHER_NAME", "Win32 Basic Launcher");
	ss_macro_define("PLATFORM", "Win32");

	ss_macro_define("USER_CONFIG_FILE", "{PROJECT_FILE}.{OS_USERNAME}.user");
	
	ss_macro_define("WINDOW_TITLE", "{EMULATORS({EMULATOR})(title)}");
	ss_macro_define("WINDOW_WIDTH", "{EMULATORS({EMULATOR})(width)}");
	ss_macro_define("WINDOW_HEIGHT", "{EMULATORS({EMULATOR})(height)}");


	char Buf[257];
	DWORD sz = 257;
	if (GetUserNameA(Buf, &sz)){
		ss_macro_define("OS_USERNAME", Buf);
	}
}
