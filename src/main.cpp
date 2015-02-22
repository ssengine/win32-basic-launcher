#include <ssengine/ssengine.h>
#include <ssengine/log.h>
#include <Windows.h>

#include <assert.h>

#include <ssengine/macros.h>
#include "launcher.h"

#include <ssengine/uri.h>
#include <ssengine/render/device.h>

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
	return false;
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

static ss_render_device* s_render_device;

static bool create_render_device_from(const char* libmname, const char* dtmname){
	ss_macro_eval(libmname);
	ss_macro_eval(dtmname);
	HMODULE hmod = LoadLibrary(ss_macro_get_content(libmname).c_str());
	if (hmod){
		ss_device_factory_type fac = (ss_device_factory_type)GetProcAddress(hmod, "ss_device_factory");
		if (fac){
			ss_device_type dt = (ss_device_type)ss_macro_get_integer(dtmname);
			s_render_device = fac(dt, (uintptr_t)(hwnd));
			if (s_render_device){
				return true;
			}
			FreeLibrary(hmod);
		}
	}

	SS_LOGE("Create render device %s(%d) create failed, try others", ss_macro_get_content(libmname).c_str(), ss_macro_get_integer(dtmname));
	return false;
}

static bool create_render_device(){
	//TODO: load device type from project configure.
	if (ss_macro_isdef("RENDERER_LIB")){
		if (create_render_device_from("RENDERER_LIB", "RENDERER_TYPE")){
			return true;
		}
		SS_LOGE("Default render device create failed, try others");
	}
	// try every renderer.
	for (int i = 0; i < 0x100; i++){
		char libname[32], dtname[32];
		sprintf(libname, "RENDERER_LIBS(%d)", i);
		sprintf(dtname, "RENDERER_TYPES(%d)", i);
		if (!ss_macro_isdef(libname) || !ss_macro_isdef(dtname)){
			break;
		}
		if (create_render_device_from(libname, dtname)){
			return true;
		}
	}
	SS_LOGE("No render device available.");
	return false;
}

static void destroy_render_device(){
	s_render_device->destroy();
	s_render_device = NULL;
}

static void main_loop()
{
	MSG msg;
	ss_macro_eval("USE_DEBUG_BG");
	int debugbg = ss_macro_get_integer("USE_DEBUG_BG");

	RECT rect;
	GetClientRect(hwnd, &rect);
	s_render_device->set_viewport(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);

	ss_vertex_buffer_memory* buffer = s_render_device->create_memory_vertex_bufer(SS_VBF_FLOAT32_RGBA, 3);
	{
		float vertexes[] = {
			0, 1, 0, 1,
			-1, -1, 0, 1,
			1, -1, 0, 1,
		};
		memcpy(buffer->lock(), vertexes, sizeof(float)*12);
		buffer->unlock();

		ss_vertex_buffer* tmp = buffer;
		size_t stride = sizeof(float)* 4;
		size_t offset = 0;
		s_render_device->set_vertex_buffer(0, 1, &tmp, &stride, &offset);
	}

	ss_render_technique* tech = s_render_device->get_predefined_technique(SS_PDT_BLANK);

	ss_render_input_layout* layout;
	{
		ss_render_input_element layout_elements[] = {
			{ SS_USAGE_POSITION, 0, SS_VBF_FLOAT32_RGBA, 0, 0}
		};
		layout = tech->create_input_layout(layout_elements, 1);
		s_render_device->set_input_layout(layout);
	}

	s_render_device->set_primitive_type(SS_MT_TRIANGLELIST);

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
				s_render_device->clear_color(1.f, 0.3f, 0.5f, 0.f);
				s_render_device->clear();
			}

			for (size_t i = 0; i < tech->pass_count(); i++){
				ss_render_pass* pass = tech->get_pass(i);
				pass->begin();
				s_render_device->draw(3, 0);
				pass->end();
			}

			s_render_device->present();
		}
	}

	s_render_device->set_input_layout(nullptr);
	delete layout;

	s_render_device->unset_vertex_buffer(0, 1);
	delete buffer;
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

	ss_uri_init_schemas();
	
	ss_macro_eval("PROJECT_FILE");
	load_project(ss_macro_get_content("PROJECT_FILE").c_str());
	ss_macro_eval("USER_CONFIG_FILE");
	load_user_configure(ss_macro_get_content("USER_CONFIG_FILE").c_str());

	//TODO: group initialize/finalize code to a simple func
	ss_init_script_context();

	ss_run_script_from_macro("SCRIPTS(onCreate)");

	if (create_window()){
		if (create_render_device()){
			main_loop();
			destroy_render_device();
		}

		destroy_window();
	}

	ss_run_script_from_macro("SCRIPTS(onExit)");

	ss_destroy_script_context();

	::CoUninitialize();

	return 0;
}
