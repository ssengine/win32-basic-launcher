#include <ssengine/core.h>
#include <ssengine/log.h>
#include <Windows.h>

#include <assert.h>

#include <ssengine/macros.h>
#include "launcher.h"

#include <ssengine/uri.h>
#include <ssengine/render/device.h>
#include <ssengine/render/drawbatch.h>

#include <stdlib.h>

#ifndef NDEBUG
#include <crtdbg.h>
#endif

static void dbgstr_log(int level, const char* msg, size_t sz, void* userdata){
	wchar_t* wmsg = char2wchar_t(msg);
	OutputDebugStringW(wmsg);
	OutputDebugStringW(L"\n");
	delete[] wmsg;
}

static void dbgstr_wlog(int level, const wchar_t* msg, size_t sz, void* userdata){
	OutputDebugStringW(msg);
	OutputDebugStringW(L"\n");
}

static ss_logger logger = {
	NULL,
	dbgstr_log,
	dbgstr_wlog
};

static HWND hwnd;

static void WINAPI wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_CLOSE:
		//TODO: Send global event.
		PostQuitMessage(0);
		break;
	default:
		DefWindowProcW(hwnd, msg, wparam, lparam);
	}
}

static BOOL create_window(ss_core_context* C)
{
	assert(hwnd == NULL);
	if (hwnd == NULL)
	{
		LPCWSTR class_name = L"SSENGINE";

		long uiWidth = 640;
		long uiHeight = 480;

		WNDCLASSW wc;
		RECT wRect;
		HINSTANCE hInstance;

		wRect.left = 0L;
		ss_macro_eval(C, "WINDOW_WIDTH");
		wRect.right = (long)ss_macro_get_integer(C, "WINDOW_WIDTH");
		wRect.top = 0L;
		ss_macro_eval(C, "WINDOW_HEIGHT");
		wRect.bottom = (long)ss_macro_get_integer(C, "WINDOW_HEIGHT");

		hInstance = GetModuleHandle(NULL);

		ZeroMemory(&wc, sizeof(wc));
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		wc.lpfnWndProc = (WNDPROC)wndproc;
		wc.hInstance = hInstance;
		//wc.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON));
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.lpszClassName = class_name;

		RegisterClassW(&wc);

		AdjustWindowRectEx(&wRect, (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN)&(~WS_SIZEBOX), FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);

		uiWidth = wRect.right - wRect.left;
		uiHeight = wRect.bottom - wRect.top;

		ss_macro_eval(C, "WINDOW_TITLE");
		wchar_t* wsTitle = char2wchar_t(ss_macro_get_content(C, "WINDOW_TITLE").c_str());

		hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
			class_name, wsTitle,
			(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN) &(~WS_SIZEBOX), 0, 0, uiWidth, uiHeight, NULL, NULL, hInstance, NULL);

		free(wsTitle);
		ShowWindow(hwnd, SW_SHOW);

		return TRUE;
	}
	return false;
}

static void destroy_window(ss_core_context* C)
{
	assert(hwnd != NULL);
	if (hwnd != NULL)
	{
		DestroyWindow(hwnd);
		hwnd = NULL;
		UnregisterClassW(L"SSENGINE", GetModuleHandle(NULL));
	}
}

//static ss_render_device* s_render_device;

static bool create_render_device_from(ss_core_context* C, const char* libmname, const char* dtmname){
	ss_macro_eval(C, libmname);
	ss_macro_eval(C, dtmname);
	HMODULE hmod = LoadLibrary(ss_macro_get_content(C, libmname).c_str());
	if (hmod){
		ss_render_device_factory_type fac = (ss_render_device_factory_type)GetProcAddress(hmod, "ss_render_device_factory");
		if (fac){
			ss_render_device_type dt = (ss_render_device_type)ss_macro_get_integer(C, dtmname);
			ss_render_device* device = fac(dt, (uintptr_t)(hwnd));
			if (device){
				ss_set_render_device(C, device);
				return true;
			}
			FreeLibrary(hmod);
		}
	}

	SS_LOGE("Create render device %s(%d) create failed, try others", ss_macro_get_content(C, libmname).c_str(), ss_macro_get_integer(C, dtmname));
	return false;
}

static bool create_render_device(ss_core_context* C){
	//TODO: load device type from project configure.
	if (ss_macro_isdef(C, "RENDERER_LIB")){
		if (create_render_device_from(C, "RENDERER_LIB", "RENDERER_TYPE")){
			return true;
		}
		SS_LOGE("Default render device create failed, try others");
	}
	// try every renderer.
	for (int i = 0; i < 0x100; i++){
		char libname[32], dtname[32];
		sprintf(libname, "RENDERER_LIBS(%d)", i);
		sprintf(dtname, "RENDERER_TYPES(%d)", i);
		if (!ss_macro_isdef(C, libname) || !ss_macro_isdef(C, dtname)){
			break;
		}
		if (create_render_device_from(C, libname, dtname)){
			return true;
		}
	}
	SS_LOGE("No render device available.");
	return false;
}

static void destroy_render_device(ss_core_context* C){
	ss_render_device* device = ss_get_render_device(C);
	ss_set_render_device(C, NULL);
	device->destroy();
	device = NULL;
}

static void main_loop(ss_core_context* C)
{
	MSG msg;
	ss_macro_eval(C, "USE_DEBUG_BG");
	int debugbg = ss_macro_get_integer(C, "USE_DEBUG_BG");

	ss_render_device* device = ss_get_render_device(C);

	RECT rect;
	GetClientRect(hwnd, &rect);
	device->set_viewport(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);

	ss_texture2d* texture;
	{
		//Create a 64x64 texture.
		unsigned char pixels[64 * 64 * 4];
		for (size_t i = 0; i < 64 * 64; i++){
			pixels[i * 4] = rand() & 0xff;
			pixels[i * 4 + 1] = rand() & 0xff;
			pixels[i * 4 + 2] = rand() & 0xff;
			pixels[i * 4 + 3] = 0xff;
		}
		texture = device->create_texture2d(64, 64, SS_FORMAT_BYTE_RGBA, pixels);

		//device->set_ps_texture2d_resource(0, 1, &texture);
	}

	for (;;)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			//TODO: if paused, ignore enterframe and use GetMessage for less cpu usage.
			//seed_app_enterFrame(tick_count());
			if (debugbg){
				device->clear_color(1.f, 0.3f, 0.5f, 0.f);
				device->clear();
			}

			ss_db_draw_image_rect(C,
				texture, 
				-1, -1, 2, 2,
				0, 0, 1, 1
				);
			for (size_t i = 0; i < 100; i++){
				ss_db_draw_line(C, 
					rand() / (float)RAND_MAX, rand() / (float)RAND_MAX,
					rand() / (float)RAND_MAX, rand() / (float)RAND_MAX);
			}
			ss_db_flush(C);
			device->present();
		}
	}

	delete texture;
}


int WINAPI WinMain(_In_  HINSTANCE hInstance,
	_In_  HINSTANCE hPrevInstance,
	_In_  LPSTR lpCmdLine,
	_In_  int nCmdShow){

#ifndef NDEBUG
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
#endif
	ss_core_context* C = ss_create_context();

	::CoInitialize(NULL);

	ss_add_logger(&logger);

	int argc = __argc;
	char**argv = __argv;

	init_defines(C);

	//TODO: parse options from command-line arguments
	if (argc <= 1) {
		ss_macro_define(C, "PROJECT_FILE", "project.prj");
	} else {
		ss_macro_define(C, "PROJECT_FILE", argv[1]);
	}

	ss_macro_eval(C, "PROJECT_FILE");
	load_project(C, ss_macro_get_content(C, "PROJECT_FILE").c_str());
	ss_macro_eval(C, "USER_CONFIG_FILE");
	load_user_configure(C, ss_macro_get_content(C, "USER_CONFIG_FILE").c_str());

	//TODO: group initialize/finalize code to a simple func

	ss_run_script_from_macro(C, "SCRIPTS(onCreate)");

	if (create_window(C)){
		if (create_render_device(C)){
			main_loop(C);
			destroy_render_device(C);
		}

		destroy_window(C);
	}

	ss_run_script_from_macro(C, "SCRIPTS(onExit)");

	ss_destroy_context(C);

	::CoUninitialize();

	return 0;
}
