#include <windows.h>
#include <tlhelp32.h>
#include <glib.h>

#define SKYPE_WIN32_CLASS_NAME "Skype-libpurple-Joiner"

static LRESULT APIENTRY Skype_WindowProc(HWND hWindow, UINT uiMessage,
					 WPARAM uiParam, LPARAM ulParam);
static void win32_message_loop(void);

HINSTANCE hInit_ProcessHandle = NULL;
static UINT uiGlobal_MsgID_SkypeControlAPIAttach = 0;
static UINT uiGlobal_MsgID_SkypeControlAPIDiscover = 0;
static HWND hInit_MainWindowHandle = NULL;
static HWND hGlobal_SkypeAPIWindowHandle = NULL;
HANDLE hEvent = NULL;

static gboolean
skype_connect()
{
	int i = 0;
	PDWORD_PTR sendMessageResult = NULL;

	if (!uiGlobal_MsgID_SkypeControlAPIAttach)
		uiGlobal_MsgID_SkypeControlAPIAttach = RegisterWindowMessage("SkypeControlAPIAttach");
	if (!uiGlobal_MsgID_SkypeControlAPIDiscover)
		uiGlobal_MsgID_SkypeControlAPIDiscover = RegisterWindowMessage("SkypeControlAPIDiscover");

	if (!hInit_ProcessHandle)
		hInit_ProcessHandle = (HINSTANCE)OpenProcess( PROCESS_DUP_HANDLE, FALSE, GetCurrentProcessId());
	skype_debug_info("skype_win32", "ProcessId %d\n", GetCurrentProcessId());
	skype_debug_info("skype_win32", "hInit_ProcessHandle %d\n", hInit_ProcessHandle);

	g_thread_create((GThreadFunc)win32_message_loop, NULL, FALSE, NULL);
	while(hInit_MainWindowHandle == NULL)
	{
		Sleep(10);
	}

	skype_debug_info("skype_win32", "hInit_MainWindowHandle %d\n", hInit_MainWindowHandle);
	skype_debug_info("skype_win32", "Sending broadcast message\n");
	SendMessageTimeout( HWND_BROADCAST, uiGlobal_MsgID_SkypeControlAPIDiscover, (WPARAM)hInit_MainWindowHandle, 0, SMTO_NORMAL, 1000, sendMessageResult);
	skype_debug_info("skype_win32", "Broadcast message sent\n");

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
	static gboolean message_loop_started = FALSE;

	if (message_loop_started)
		return;
	message_loop_started = TRUE;

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
	skype_debug_info("skype_win32", "Finished message loop\n");
	DestroyWindow(hInit_MainWindowHandle);
	hInit_MainWindowHandle = NULL;
	message_loop_started = FALSE;
}

static void
skype_disconnect()
{
	UnregisterClass(SKYPE_WIN32_CLASS_NAME, hInit_ProcessHandle);
	CloseHandle(hInit_ProcessHandle);
	hInit_ProcessHandle = NULL;

	if (hInit_MainWindowHandle != NULL)
	{
		//tell win32_message_loop() thread to die gracefully
		PostMessage(hInit_MainWindowHandle, WM_QUIT, 0, 0);
	}
}

static void
send_message(char* message)
{
	int message_num;
	char *error_return;

	COPYDATASTRUCT oCopyData;
	oCopyData.dwData = 0;
	oCopyData.lpData = (void *)message;
	oCopyData.cbData = strlen(message) + 1;

	if (SendMessage( hGlobal_SkypeAPIWindowHandle, WM_COPYDATA, 
						(WPARAM)hInit_MainWindowHandle, (LPARAM)&oCopyData) == FALSE)
	{
		hGlobal_SkypeAPIWindowHandle = NULL;
		//There was an error
		if (message[0] == '#')
		{
			//And we're expecting a response
			sscanf(message, "#%d ", &message_num);
			error_return = g_strdup_printf("#%d ERROR WIN32", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)error_return, FALSE, NULL);
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
		skype_debug_info("skype_win32", "Attached process %d %d\n", uiParam, ulParam);
		if (ulParam == 0)
			skype_debug_info("skype_win32", "Attach success\n");
		else if (ulParam == 1)
			skype_debug_info("skype_win32", "Pending auth\n");
		else if (ulParam == 2)
			skype_debug_info("skype_win32", "Refused\n");
		else if (ulParam == 3)
			skype_debug_info("skype_win32", "Not ready\n");
		else if (ulParam == 0x8001)
			skype_debug_info("skype_win32", "Skype became ready\n");
		return 1;
	}
	return DefWindowProc(hWindow, uiMessage, uiParam, ulParam);
}

static void
hide_skype()
{
	//don't need to since SILENT_MODE ON works
	return;
}

static gboolean
exec_skype()
{
	DWORD size = 0;
	gchar *path, *pathtemp;
	HKEY regkey;
	gboolean success = FALSE;

	//HKCU\Software\Skype\Phone\SkypePath or HKLM\Software\Skype\Phone\SkypePath
	
	RegOpenKey(HKEY_CURRENT_USER, "Software\\Skype\\Phone", &regkey);
	RegQueryValueEx(regkey, "SkypePath", NULL, NULL, NULL, &size);
	if (size != 0)
	{
		path = g_new(gchar, size);
		RegQueryValueEx(regkey, "SkypePath", NULL, NULL, (LPBYTE)path, &size);
	} else {
		RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\Skype\\Phone", &regkey);
		RegQueryValueEx(regkey, "SkypePath", NULL, NULL, NULL, &size);
		if (size != 0)
		{
			path = g_new(gchar, size);
			RegQueryValueEx(regkey, "Software\\Skype\\Phone\\SkypePath", NULL, NULL, (LPBYTE)path, &size);
		} else {
			path = g_strdup("C:\\Program Files\\Skype\\Phone\\Skype.exe");
		}
	}

	pathtemp = g_strconcat("\"", path, "\" /nosplash /minimized", NULL);
	skype_debug_info("skype_win32", "Path to Skype: %s\n", pathtemp);
	g_free(path);

	success = g_spawn_command_line_async(pathtemp, NULL);
	g_free(pathtemp);
	return success;
}

static gboolean
is_skype_running()
{
#ifdef _TLHELP32_H
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	HANDLE temp = NULL;
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	Process32First(snapshot, &entry);
	do {
		if (g_str_equal("Skype.exe", entry.szExeFile))
		{
			temp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
			CloseHandle(snapshot);
			return TRUE;
		}
	} while (Process32Next(snapshot, &entry));
	CloseHandle(snapshot);
#endif
	return FALSE;
}
