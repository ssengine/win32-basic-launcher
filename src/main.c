#include <ssengine/ssengine.h>
#include <ssengine/log.h>
#include <Windows.h>

#include <assert.h>

static void dbgstr_log(int level, const char* msg, size_t sz, void* userdata){
	OutputDebugStringA(msg);
	OutputDebugStringA("\n");
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
		wRect.right = (long)640;
		wRect.top = 0L;
		wRect.bottom = (long)960;

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

		hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
			class_name, L"",
			(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN) &(~WS_SIZEBOX), 0, 0, uiWidth, uiHeight, NULL, NULL, hInstance, NULL);

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
	ss_add_logger(&logger);

	if (create_window()){
		main_loop();
		destroy_window();
	}

	return 0;
}
