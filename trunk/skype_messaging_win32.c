#include <windows.h>
#include <glib.h>

#define SKYPE_WIN32_CLASS_NAME "Skype-libpurple-Joiner"

static LRESULT APIENTRY Skype_WindowProc(HWND hWindow, UINT uiMessage,
					 WPARAM uiParam, LPARAM ulParam);
static void win32_message_loop(void);

HINSTANCE hInit_ProcessHandle;
UINT uiGlobal_MsgID_SkypeControlAPIAttach;
UINT uiGlobal_MsgID_SkypeControlAPIDiscover;
static HWND hInit_MainWindowHandle = NULL;
static HWND hGlobal_SkypeAPIWindowHandle = NULL;
HANDLE hEvent = NULL;

static gboolean
skype_connect()
{
	int i = 0;

	uiGlobal_MsgID_SkypeControlAPIAttach = RegisterWindowMessage("SkypeControlAPIAttach");
	uiGlobal_MsgID_SkypeControlAPIDiscover = RegisterWindowMessage("SkypeControlAPIDiscover");

	hInit_ProcessHandle = (HINSTANCE)OpenProcess( PROCESS_DUP_HANDLE, FALSE, GetCurrentProcessId());
	purple_debug_info("skype_win32", "ProcessId %d\n", GetCurrentProcessId());
	purple_debug_info("skype_win32", "hInit_ProcessHandle %d\n", hInit_ProcessHandle);

	g_thread_create((GThreadFunc)win32_message_loop, NULL, FALSE, NULL);
	while(hInit_MainWindowHandle == NULL)
	{
		Sleep(10);
	}

	purple_debug_info("skype_win32", "Sending broadcast message\n");
	SendMessage( HWND_BROADCAST, uiGlobal_MsgID_SkypeControlAPIDiscover, (WPARAM)hInit_MainWindowHandle, 0);
	purple_debug_info("skype_win32", "Broadcast message sent\n");

	while(hGlobal_SkypeAPIWindowHandle == NULL && i < 100)
	{
		i++;
		Sleep(10);
	}
	
	if (hGlobal_SkypeAPIWindowHandle == NULL)
		return FALSE;

	return TRUE;
}

static void
win32_message_loop(void)
{
	MSG msg;
	WNDCLASS oWindowClass;
	int classRegistration;

	oWindowClass.style = CS_HREDRAW|CS_VREDRAW;
	oWindowClass.lpfnWndProc = (WNDPROC)&Skype_WindowProc;
	oWindowClass.cbClsExtra = 0;
	oWindowClass.cbWndExtra = 0;
	oWindowClass.hInstance = hInit_ProcessHandle;
	oWindowClass.hIcon = NULL;
	oWindowClass.hCursor = NULL;
	oWindowClass.hbrBackground = NULL;
	oWindowClass.lpszMenuName = NULL;
	oWindowClass.lpszClassName = SKYPE_WIN32_CLASS_NAME;
	classRegistration = RegisterClass(&oWindowClass);
	hInit_MainWindowHandle = CreateWindow(SKYPE_WIN32_CLASS_NAME, SKYPE_WIN32_CLASS_NAME, 
											WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, 
											NULL, NULL, hInit_ProcessHandle, NULL);
	
	ShowWindow(hInit_MainWindowHandle, SW_HIDE);
	UpdateWindow(hInit_MainWindowHandle);
	while (GetMessage(&msg, NULL, 0, 0) != 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static void
skype_disconnect()
{
	UnregisterClass(SKYPE_WIN32_CLASS_NAME, hInit_ProcessHandle);
	CloseHandle(hInit_ProcessHandle);
	DestroyWindow(hInit_MainWindowHandle);
	hInit_ProcessHandle = NULL;
	hInit_MainWindowHandle = NULL;
	hGlobal_SkypeAPIWindowHandle = NULL;
}

static void
send_message(char* message)
{
	int message_num;
	char error_return[15];

	COPYDATASTRUCT oCopyData;
	oCopyData.dwData = 0;
	oCopyData.lpData = (void *)message;
	oCopyData.cbData = strlen(message) + 1;

	if (SendMessage( hGlobal_SkypeAPIWindowHandle, WM_COPYDATA, 
						(WPARAM)hInit_MainWindowHandle, (LPARAM)&oCopyData) == FALSE)
	{
		//There was an error
		if (message[0] == '#')
		{
			//And we're expecting a response
			sscanf(message, "#%d ", &message_num);
			sprintf(error_return, "#%d ERROR", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)g_strdup(error_return), FALSE, NULL);
		}
	}
}

static LRESULT CALLBACK
Skype_WindowProc(HWND hWindow, UINT uiMessage, WPARAM uiParam, LPARAM ulParam)
{
		
	if(uiMessage == WM_COPYDATA && hGlobal_SkypeAPIWindowHandle == (HWND)uiParam)
	{
		PCOPYDATASTRUCT poCopyData = (PCOPYDATASTRUCT)ulParam;
		g_thread_create((GThreadFunc)skype_message_received, (void *)g_strdup(poCopyData->lpData), FALSE, NULL);
		return 1;
	} else if (uiMessage == uiGlobal_MsgID_SkypeControlAPIAttach) {
		hGlobal_SkypeAPIWindowHandle = (HWND)uiParam;
		purple_debug_info("skype_win32", "Attached process %d %d\n", uiParam, ulParam);
		return 1;
	}
	return DefWindowProc(hWindow, uiMessage, uiParam, ulParam);
}

static void
hide_skype
{
	return;
}

