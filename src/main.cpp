#include <ssengine/ssengine.h>
#include <ssengine/log.h>
#include <Windows.h>

#include <assert.h>

#include <ssengine/macros.h>
#include "launcher.h"

#include <ssengine/uri.h>

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

static BOOL create_window()
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
		ss_macro_eval("WINDOW_WIDTH");
		wRect.right = (long)ss_macro_get_integer("WINDOW_WIDTH");
		wRect.top = 0L;
		ss_macro_eval("WINDOW_HEIGHT");
		wRect.bottom = (long)ss_macro_get_integer("WINDOW_HEIGHT");

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

		ss_macro_eval("WINDOW_TITLE");
		wchar_t* wsTitle = char2wchar_t(ss_macro_get_content("WINDOW_TITLE").c_str());

		hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
			class_name, wsTitle,
			(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN) &(~WS_SIZEBOX), 0, 0, uiWidth, uiHeight, NULL, NULL, hInstance, NULL);

		free(wsTitle);
		ShowWindow(hwnd, SW_SHOW);

		return TRUE;
	}
	return FALSE;
}

static void destroy_window()
{
	assert(hwnd != NULL);
	if (hwnd != NULL)
	{
		DestroyWindow(hwnd);
		hwnd = NULL;
		UnregisterClassW(L"SSENGINE", GetModuleHandle(NULL));
	}
}

static void main_loop()
{
	MSG msg;

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
			//device->swap_buffer();
		}
	}
}


int WINAPI WinMain(_In_  HINSTANCE hInstance,
	_In_  HINSTANCE hPrevInstance,
	_In_  LPSTR lpCmdLine,
	_In_  int nCmdShow){

#ifndef NDEBUG
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
#endif
	::CoInitialize(NULL);

	ss_add_logger(&logger);

	int argc = __argc;
	char**argv = __argv;

	init_defines();

	//TODO: parse options from command-line arguments
	if (argc <= 1) {
		ss_macro_define("PROJECT_FILE", "project.prj");
	} else {
		ss_macro_define("PROJECT_FILE", argv[1]);
	}
	
	ss_macro_eval("PROJECT_FILE");
	load_project(ss_macro_get_content("PROJECT_FILE").c_str());
	ss_macro_eval("USER_CONFIG_FILE");
	load_user_configure(ss_macro_get_content("USER_CONFIG_FILE").c_str());

	//TODO: group initialize/finalize code to a simple func
	ss_uri_init_schemas();

	ss_init_script_context();

	ss_run_script_from_macro("SCRIPTS(onCreate)");

	if (create_window()){
		main_loop();
		destroy_window();
	}

	ss_run_script_from_macro("SCRIPTS(onExit)");

	ss_destroy_script_context();

	::CoUninitialize();

	return 0;
}
