#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <string>
#include <sstream>
#include <cassert>
#include <atomic>
#include <unordered_map>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <locale>
#include <codecvt>

#include "json.hpp"
#include "resource3.h"
#include "SG_InputBox.h"
#include "VDesktops.h"

#define SOL_ALL_SAFETIES_ON 1


// ------------------------------------------------------------------
// Defs
// ------------------------------------------------------------------
#define COMMERCIAL

// ------------------------------------------------------------------
// NT Aesthetic
// ------------------------------------------------------------------
#define RtlFormatPrint snprintf
#define RtlAlloc malloc
#define ForegroundClass 25

//------------------------------------------------------------------
// COM Variables
//------------------------------------------------------------------
#define ComOk(x) if (const char* com_error = x)\
	FailWithCode(com_error);

BOOL FirstDesktopRender;
//------------------------------------------------------------------
// Application Definitions
//------------------------------------------------------------------
#define NULL_STYLE 0
#define NULL_EX_STYLE 0
#define INIT_TILES_PER_WORKSPACE 4
#define MAX_WORKSPACES 10
#define MAX_INIT_TILES 40
#define ARRAY_SIZEOF(A) (sizeof(A)/sizeof(*A))
#define WIN3M_PIPE_NAME "\\\\.\\pipe\\Win3WMPipe"
#define MINIMUM_CONSIDERED_AREA 10000
#define DO_NOT_PASS_KEY TRUE

std::atomic<int> CanSwitch = TRUE;
std::atomic<BOOL> IsPressed;

//------------------------------------------------------------------
// Application Structs 
//------------------------------------------------------------------


//------------------------------------------------------------------
// Config
//------------------------------------------------------------------
using json = nlohmann::json;
std::unordered_map<std::string, unsigned int> KeyMap;


//------------------------------------------------------------------
// C generics
//------------------------------------------------------------------
#define ChangeWorkspaceEx(Number) VOID ChangeWorkspaceEx_##Number()\
	{											\
		if (Number == CurWk)	\
			return;								\
												\
		HandleChangeWorkspace(Number);			\
	}											\

#define MoveWorkspaceEx(Number) VOID MoveWorkspaceEx_##Number()\
	{											\
		if (Number == CurWk)	\
			return;								\
												\
		HandleMoveWindowWorkspace(Number);		\
	}											\

#define ParseConfigEx(Key, Value) CurrentKey = Key; \
		ParseConfig(JsonParsed[CurrentKey], Value); \

#define ParseBoolOptionEx(String, Option) ParseBoolOption(String, Option, CurrentKey)

#define TreeHas(Member) (Workspace->Tree && Workspace->Tree->##Member)


//-------------------------
// Hotkey stupid stuff
//-------------------------
enum MODKEY {
	ALT = 0,
	WIN = 1
};


//------------------------------------------------------------------
// Window Definitions
//------------------------------------------------------------------
CHAR AppWindowName[] = "Win3wmWindow";
CHAR DebugAppWindowName[] = "FocusDebugOverlay";
CHAR FocusAppWindowName[] = "FocusWin3WM";
CHAR NotifAppWindowName[] = "NotifWin3WM";
CHAR StatusBarWindowName[] = "Win3wmStatus";

// ------------------------------------------------------------------
// Window Globals
// ------------------------------------------------------------------
std::unordered_map<HWND, BUTTON_STATE> ButtonMap;
HWND MainWindow;
HWND DebugWindow;
HWND FocusWindow;
HWND NotifWindow;
HWND BtnToColor;
HWND CurrentFocusChild;
HWND TrayWindow;
RECT ShellRect;

ATOM WindowAtom;
ATOM DebugWindowAtom;
ATOM FocusAtom;
ATOM NotifAtom;
ATOM StatusBarAtom;
// change later to more C-ish datastructure (AKA don't use std::vector)
std::vector<TILE_INFO> WindowOrderList;
std::vector<WORKSPACE_INFO> WorkspaceList;
std::vector<HWND> TrayList;
std::vector<DISPLAY_INFO> DisplayList;
DISPLAY_INFO* PrimaryDisplay;
HANDLE x86ProcessHandle;
DWORD x86ProcessId;
HANDLE PipeHandle = INVALID_HANDLE_VALUE;
HANDLE PipeEvent;
INT CurWk = 1;
#define WM_INSTALL_HOOKS 0x8069
#define WM_SHUTDOWN 0x806C
#define WM_MOVE_TILE 0x806b
#define WM_USER_FOCUS 0x8096
#define MIN_ALL 419
#define ID_EXIT 6
#define TIMER_FOCUS 0x1234

// ------------------------------------------------------------------
// Screen Globals
// ------------------------------------------------------------------
HANDLE AppMutex;
INT ScreenWidth;
INT ScreenHeight;

INT RealScreenWidth;
INT RealScreenHeight;
CHAR AppCheckName[] = "Win3WM";

// ------------------------------------------------------------------
// Application Globals
// ------------------------------------------------------------------
typedef PVOID(*InitFn)(HWND WindowHandle);
typedef VOID(*HOTKEY_FN)();

typedef struct _HotKeyDispatch_
{
	BOOL ShiftCb;
	HOTKEY_FN HotKeyCb;
} HOTKEY_DISPATCH;

HMODULE ForceResize64;
HMODULE ForceResize86;
HMODULE WinHook64;
HOOKPROC DllHookProc;
HHOOK KeyboardHook;
HHOOK NewWindowHook;
BOOL ShouldRerender;

// ------------------------------------------------------------------
// Status Bar Globals
// ------------------------------------------------------------------
HWND StatusButtonList[10];

// ------------------------------------------------------------------
// Windows Stuff 
// ------------------------------------------------------------------

const wchar_t* Win32kDefaultWindowNamesDebug[] = { L"FocusWin3WM",  L"Win3wmStatusBar", L"FocusDebugOverlay", L"ConsoleWindowClass", L"Shell_TrayWnd", L"WorkerW", L"Progman", L"Win3wmWindow", L"NarratorHelperWindow", L"lul", L"Visual Studio", L"Windows.UI.Core.CoreWindow" };
std::vector<std::wstring> Win32kDefaultWindowNames = { L"Shell_SystemDialogProxy", L"Shell_SystemDim", L"The Event Managment Dashboard", L"FocusWin3WM",  L"Win3wmStatusBar", L"FocusDebugOverlay", L"Shell_TrayWnd", L"WorkerW", L"Progman", L"Win3wmWindow", L"NarratorHelperWindow", L"Windows.UI.Core.CoreWindow" };
const SPECIFIC_WINDOW WeirdWindowsList[] =
{
	{ L"ConsoleWindowClass", SW_MAXIMIZE }
};

// ------------------------------------------------------------------
// User Options
// ------------------------------------------------------------------
NODE_TYPE UserChosenLayout = VERTICAL_SPLIT;
MODKEY ModKey = ALT;
const char* StartCommand = "start cmd.exe";
const char* StartDirectory = "";
const char* LuaScriptPath = "";

LUA_OPT LuaOpt;
BOOL HasLua;
sol::state LuaState;
BOOL AdjustForNC;
BOOL IsGapsEnabled;
BOOL ShouldRemoveTitleBars;
BOOL IsFullScreenMax;
POINT NewOrigin;

std::vector<unsigned char> ColorActiveWorkspaceButton;
std::vector<unsigned char> ColorInactiveWorkspaceButton;
std::vector<unsigned char> ColorInactiveMonitorButton;
std::vector<unsigned char> ColorActiveButtonText;
std::vector<unsigned char> ColorInActiveButtonText;

DWORD ClrActWk;
DWORD ClrInActWk;
DWORD ClrInMt;
DWORD ClrActTxt;
DWORD ClrInActTxt;

INT OuterGapsVertical;
INT OuterGapsHorizontal;
INT InnerGapsVertical;
INT InnerGapsHorizontal;



// ------------------------------------------------------------------
// Code
// ------------------------------------------------------------------

void logc(unsigned char color, const char* format, ...)
{
#ifdef _DEBUG
	va_list args;
	va_start(args, format);

	HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	SetConsoleTextAttribute(console_handle, color);
	vprintf(format, args);
	SetConsoleTextAttribute(console_handle, 15);

	va_end(args);
#else
#endif

}


VOID FailWithCode(const char* ErrorMessage)
{

	CHAR ErrorMsgBuffer[1024] = { 0 };

	RtlFormatPrint(ErrorMsgBuffer, sizeof(ErrorMsgBuffer),
		"%s : %u", ErrorMessage, GetLastError());

	if (x86ProcessHandle)
		TerminateProcess(x86ProcessHandle, 327);

	logc(12, ErrorMsgBuffer);

	MessageBoxA(NULL, ErrorMsgBuffer, NULL, MB_OK);
	TerminateProcess(GetCurrentProcess(), 327);
}


VOID Fail(const char* ErrorMessage)
{

	if (x86ProcessHandle)
		TerminateProcess(x86ProcessHandle, 327);

	MessageBoxA(NULL, ErrorMessage, NULL, MB_OK);
	TerminateProcess(GetCurrentProcess(), 327);
}

using sol::lib;

template<class...Args>
void LuaDispatchEx(const char* functionName, Args &&... args)
{
	if (LuaOpt.On && LuaOpt.State[functionName] != sol::nil)
	{
		sol::protected_function Func = LuaOpt.State[functionName];

		auto Result = Func(std::forward<Args>(args)...);

		if (!Result.valid())
		{
			sol::error err = Result;
			std::string what = err.what();
			Fail(what.c_str());
		}

	}

}

VOID ShowTaskBar(BOOL SetTaskBar)
{
	TrayWindow = FindWindowA("Shell_TrayWnd", NULL);

	if (!TrayWindow)
		Fail((PCHAR)"Didn't Find TaskBar HWND");


	ShowWindow(TrayWindow, SetTaskBar ? SW_SHOW : SW_HIDE);
	UpdateWindow(TrayWindow);

}

const char* MakeFormatString(const char* Format, ...)
{
	char* StringMemory = (char*)malloc(1024);

	if (!StringMemory)
		Fail("Couldn't allocate memory for error print????");

	va_list Args;
	va_start(Args, Format);
	vsnprintf(StringMemory, 1024, Format, Args);
	va_end(Args);

	return StringMemory;

}

VOID EnterFullScreen()
{

	ShowTaskBar(TRUE);

	WINDOWPLACEMENT NewPlacement = { 0 };
	RECT FullScreenRect = { 0 };
	POINT FullScreenPoint = { 0 };

	SetRect(&FullScreenRect, 0, 0, GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN));

	NewPlacement.length = sizeof(WINDOWPLACEMENT);
	NewPlacement.flags = WPF_ASYNCWINDOWPLACEMENT;
	NewPlacement.showCmd = SW_SHOWNORMAL;
	NewPlacement.ptMinPosition = FullScreenPoint;
	NewPlacement.ptMaxPosition = FullScreenPoint;
	NewPlacement.rcNormalPosition = FullScreenRect;

	SetWindowPlacement(MainWindow, &NewPlacement);
	SetWindowLongPtrA(MainWindow, GWL_STYLE, NULL_STYLE);

}



BOOL AppRunningCheck()
{
	DWORD LastError;
	BOOL Result;

	AppMutex = CreateMutexA(NULL, FALSE, AppCheckName);
	LastError = GetLastError();
	Result = (LastError != 0);

	return Result;

}





VOID MainWindowToFore()
{
	HWND CurrentFgWindow;
	DWORD FgThreadID;
	DWORD CurrentThreadID;

	CurrentFgWindow = GetForegroundWindow();
	CurrentThreadID = GetCurrentThreadId();
	GetWindowThreadProcessId(CurrentFgWindow, &FgThreadID);

	AttachThreadInput(CurrentThreadID, FgThreadID, TRUE);
	SetWindowPos(MainWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	SetWindowPos(MainWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
	SetForegroundWindow(MainWindow);
	SetFocus(MainWindow);
	SetActiveWindow(MainWindow);
	AttachThreadInput(CurrentThreadID, FgThreadID, FALSE);
	ShowWindow(MainWindow, SW_SHOW);

}

VOID MoveWindowToBack()
{
	SetWindowPos(MainWindow, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
}

BOOL IsWindowRectVisible(HWND WindowHandle)
{
	RECT WindowRect;
	WINDOWPLACEMENT WindowPlacement;

	WindowPlacement.length = sizeof(WindowPlacement);

	if (!GetWindowPlacement(WindowHandle, &WindowPlacement))
		FailWithCode("Did not manage to get WindowRect");

	WindowRect = WindowPlacement.rcNormalPosition;

	INT Width = WindowRect.right - WindowRect.left;
	INT Height = WindowRect.bottom - WindowRect.top;
	INT Area = Width * Height;

	if (Area > 40000)
		return TRUE;

	return FALSE;

}

BOOL IsWindowCloaked(HWND hwnd)
{
	BOOL isCloaked = FALSE;
	return (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED,
		&isCloaked, sizeof(isCloaked))) && isCloaked);
}

BOOL IsWindowSecretCloaked(HWND hwnd)
{
	BOOL isCloaked = FALSE;
	return (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAK,
		&isCloaked, sizeof(isCloaked))) && isCloaked);
}

BOOL IsIgnoreWindow(HWND WindowHandle)
{
	WCHAR WindowClassText[1024] = { 0 };
	WCHAR WindowNameText[1024] = { 0 };

	GetClassNameW(WindowHandle, WindowClassText, 1024);
	GetWindowTextW(WindowHandle, WindowNameText, 1024);

	for (int i = 0; i < Win32kDefaultWindowNames.size(); i++)
	{
		if (wcsstr(WindowClassText, Win32kDefaultWindowNames[i].c_str()))
			return TRUE;

		if (wcsstr(WindowNameText, Win32kDefaultWindowNames[i].c_str()))
			return TRUE;
	}

	return FALSE;

}

BOOL TransparentWindow(HWND WindowHandle)
{
	if (GetWindowLongPtrA(WindowHandle, GWL_EXSTYLE) & WS_EX_LAYERED)
	{

		COLORREF WindowColorRef;
		BYTE WindowAlphaByte;
		DWORD Flags;

		if (!GetLayeredWindowAttributes(WindowHandle, &WindowColorRef, &WindowAlphaByte, &Flags))
			logc(5, "Secret Cloak : %u\n", IsWindowSecretCloaked(WindowHandle));


		logc(5, "Handle : %X : Layered Byte: %X : %X : %X\n", WindowHandle, WindowColorRef, WindowAlphaByte, Flags);
		if (Flags & LWA_ALPHA && !WindowAlphaByte)
			return TRUE;

	}

	return FALSE;
}

//------------------------------------------------------------------
// We're gonna wait for the pipe using WaitNamedPipe to connect 
// to the x86 hook process and send messages
//------------------------------------------------------------------
VOID ConnectTox86Pipe()
{

	DWORD RetVal = WaitForSingleObject(PipeEvent, 5000);

	switch (RetVal)
	{
	case WAIT_TIMEOUT:
		Fail("Win3WMServer did not signal event");
		break;
	case WAIT_FAILED:
		FailWithCode("Wait for event failed");
		break;
	case WAIT_ABANDONED:
		Fail("Wait Abandoned????? Contact Developor");
		break;
	case WAIT_OBJECT_0: //success
		break;
	default:
		Fail("Wait Event Magic Failure");
	}

	PipeHandle = CreateFileA(WIN3M_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (PipeHandle == INVALID_HANDLE_VALUE)
		FailWithCode("Could not open Win3WM PipeHandle");

}


//------------------------------------------------------------------
// We Create the child x86 process for Setting windows hooks on
// x86 processes
//------------------------------------------------------------------
VOID CreateX86Process()
{

	PipeEvent = CreateEventA(NULL, FALSE, 0, "Win3WMEvent");

	if (!PipeEvent)
		FailWithCode("Couldn't Create Pipe Event");

	STARTUPINFOA StartUpInfo = { 0 };
	PROCESS_INFORMATION ProcInfo = { 0 };

	StartUpInfo.cb = sizeof(StartUpInfo);

	StartUpInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	StartUpInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	StartUpInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	StartUpInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	StartUpInfo.wShowWindow = SW_HIDE;

	WCHAR DirLength[2048] = { 0 };

	GetCurrentDirectoryW(sizeof(DirLength), DirLength);

	logc(5, "%ls\n", DirLength);

	CHAR CmdLine[] = "";
	if (!CreateProcessA("win3wmipc.exe", CmdLine, NULL, NULL, NULL, NULL, NULL, NULL, &StartUpInfo, &ProcInfo))
		FailWithCode("Couldn't Start win3wmipc.exe");

	x86ProcessHandle = ProcInfo.hProcess;
	x86ProcessId = ProcInfo.dwProcessId;

}

//------------------------------------------------------------------
// This Function is meant to send the Window Handle to the x86
// child process since SetWindowsHookEx has to be done from a
// process of the same bitness
//------------------------------------------------------------------
VOID IPCWindowHandle(HWND WindowHandle)
{

	if (!x86ProcessHandle)
		Fail("Couldn't create x86 process earlier");

	if (PipeHandle == INVALID_HANDLE_VALUE)
		ConnectTox86Pipe();

	DWORD Message = (DWORD)WindowHandle;
	DWORD BytesWritten;
	DWORD RetVal = WriteFile(PipeHandle, &Message, sizeof(Message), &BytesWritten, NULL);

	if (!RetVal)
	{
		if (GetLastError() == ERROR_BROKEN_PIPE)
			Fail("Pipe closed on the other side");

		FailWithCode("WriteFile to Pipe failed");
	}

	if (BytesWritten != sizeof(Message))
		Fail("Did not write HWND Bytes to Pipe");

}

BOOL IsOwned(HWND WindowHandle)
{
	return (BOOL)GetWindow(WindowHandle, GW_OWNER);
}

BOOL IsPopup(HWND WindowHandle)
{

	// we don't want tool windows
	LONG_PTR Style = GetWindowLongPtrA(WindowHandle, GWL_STYLE);
	LONG_PTR ExStyle = GetWindowLongPtrA(WindowHandle, GWL_EXSTYLE);

	BOOL IsPopup = (Style & WS_POPUP);
	BOOL IsToolWindow = (ExStyle & WS_EX_TOOLWINDOW);

	return (IsPopup || IsToolWindow);
}

BOOL HasParentOrPopup(HWND WindowHandle)
{
	return (GetParent(WindowHandle) || IsPopup(WindowHandle));
}

VOID InstallWindowSizeHook()
{
	HMODULE ModuleBase = ForceResize64;

	if (!DllHookProc)
		DllHookProc = (HOOKPROC)GetProcAddress(ModuleBase, "HookProc");

	if (!DllHookProc)
		FailWithCode("Init not found in ForceResize64.dll");

	HHOOK DllHookAddress = SetWindowsHookExA(WH_CALLWNDPROC, DllHookProc, ModuleBase, 0);

	if (!DllHookAddress)
		FailWithCode("SetWindowsHookEx Failed");

}

VOID SortTrays(HWND WindowHandle)
{
	for (auto& Display : DisplayList)
	{
		if (Display.Handle == MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTOPRIMARY))
		{
			Display.TrayWindow = WindowHandle;
			return;
		}
	}

	Fail("Found no monitor for Tray Window?");

}


BOOL CALLBACK EnumWndProc(HWND WindowHandle, LPARAM LParam)
{

	WCHAR WindowText[1024] = { 0 };

	GetClassNameW(WindowHandle, WindowText, 1024);

	if (!wcscmp(WindowText, L"Shell_SecondaryTrayWnd"))
	{
		SortTrays(WindowHandle);
		return TRUE;
	}

	if (!IsIgnoreWindow(WindowHandle) &&
		!HasParentOrPopup(WindowHandle) &&
		!IsOwned(WindowHandle) &&
		IsWindowVisible(WindowHandle) &&
		IsWindowRectVisible(WindowHandle) &&
		!IsWindowCloaked(WindowHandle) &&
		!TransparentWindow(WindowHandle))
	{
		GetWindowTextW(WindowHandle, WindowText, 1024);
		logc(5, "Pushing Back : %X : %ls\n", WindowHandle, WindowText);
		TILE_INFO TileInfo = { 0 };

		TileInfo.WindowHandle = WindowHandle;
		TileInfo.PreWMInfo.WS_STYLE = GetWindowLongPtrA(WindowHandle, GWL_STYLE);
		TileInfo.PreWMInfo.WS_EX_STYLE = GetWindowLongPtrA(WindowHandle, GWL_EXSTYLE);
		TileInfo.PreWMInfo.OldPlacement.length = sizeof(WINDOWPLACEMENT);


		if (!GetWindowPlacement(WindowHandle, &TileInfo.PreWMInfo.OldPlacement))
			FailWithCode("Couldn't get Window Placement");


		DWORD WindowProcessId;

		if (!GetWindowThreadProcessId(TileInfo.WindowHandle, &WindowProcessId))
			FailWithCode(MakeFormatString("Couldnt get ProcessId for WindowHandle : %X ", TileInfo.WindowHandle));

		HANDLE ProcessHandle;

		ProcessHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, NULL, WindowProcessId);

		if (!ProcessHandle)
			FailWithCode(MakeFormatString("Couldnt get Process Handle for WindowHandle : %X "));

		BOOL IsX86Process;

		if (!IsWow64Process(ProcessHandle, &IsX86Process))
			FailWithCode(MakeFormatString("Couldnt get Bitness for WindowHandle : %X ", TileInfo.WindowHandle));

		if (IsX86Process)
			IPCWindowHandle(TileInfo.WindowHandle);
		else
			InstallWindowSizeHook();

		SendMessageW(TileInfo.WindowHandle, WM_INSTALL_HOOKS, NULL, (LPARAM)TileInfo.WindowHandle);

		BOOL DisableTransition = TRUE;

		DwmSetWindowAttribute(TileInfo.WindowHandle, DWMWA_TRANSITIONS_FORCEDISABLED, &DisableTransition, sizeof(DisableTransition));

		WindowOrderList.push_back(TileInfo);
	}

	return TRUE;
}

VOID GetOtherApplicationWindows()
{
	EnumWindows(EnumWndProc, NULL);
}

INT GetActiveWorkspace()
{

	INT Count = 0;

	for (int i = 1; i < WorkspaceList.size(); i++)
		if (WorkspaceList[i].Trees[WorkspaceList[i].Dsp->Handle].Root)
			Count++;

	return Count;
}

INT GetWindowCount(TILE_INFO* Tile)
{

	INT Count = 0;

	for (; Tile; Tile = Tile->ChildTile)
	{
		if (Tile->NodeType != TERMINAL)
			Count += GetWindowCount(Tile->BranchTile);

		Count++;
	}

	return Count;

}

INT GetLayoutAncestryHorizontal(TILE_INFO* CurrentTile, INT* LayoutWidth)
{
	INT NumTiles = 0;
	INT Width = 0;

	for (; CurrentTile; NumTiles++)
	{
		RECT CurrentRect = CurrentTile->Placement.rcNormalPosition;
		Width += (CurrentRect.bottom - CurrentRect.top);

		if (!CurrentTile->ParentTile && CurrentTile->BranchParent)
		{
			CurrentTile = CurrentTile->BranchParent;
			CurrentRect = CurrentTile->Placement.rcNormalPosition;
			// plus 2 because we break out of the one tile we woulda
			// got from the for loop increment
			NumTiles += 2;
			Width += (CurrentRect.bottom - CurrentRect.top);
			break;
		}

		CurrentTile = CurrentTile->ParentTile;

	}


	*LayoutWidth = Width;
	return NumTiles;
}

INT GetLayoutAncestryVertical(TILE_INFO* CurrentTile, INT* LayoutHeight)
{
	INT NumTiles = 0;
	INT Height = 0;

	for (; CurrentTile; NumTiles++)
	{
		RECT CurrentRect = CurrentTile->Placement.rcNormalPosition;
		Height += (CurrentRect.right - CurrentRect.left);

		if (!CurrentTile->ParentTile && CurrentTile->BranchParent)
		{
			CurrentTile = CurrentTile->BranchParent;
			CurrentRect = CurrentTile->Placement.rcNormalPosition;
			NumTiles += 2;
			Height += (CurrentRect.right - CurrentRect.left);
			break;
		}

		CurrentTile = CurrentTile->ParentTile;

	}


	*LayoutHeight = Height;
	return NumTiles;
}


//Includes the current tile
INT GetDescendantCount(TILE_INFO* Tile)
{
	int i = 0;
	for (; Tile; i++)
	{
		Tile = Tile->ChildTile;
	}

	return i;
}


//Includes the current tile
INT GetAncestryCount(TILE_INFO* Tile)
{
	int i = 0;
	for (; Tile; i++)
	{
		Tile = Tile->ParentTile;
	}

	return i;
}

TILE_INFO* GetAncestry(TILE_INFO* CurrentTile)
{

	for (TILE_INFO* Tile = CurrentTile;; Tile = Tile->ParentTile)
		if (!Tile->ParentTile)
			return Tile;

}

TILE_INFO* GetBranchAncestry(TILE_INFO* CurrentTile)
{

	for (TILE_INFO* Tile = CurrentTile;; Tile = Tile->BranchParent)
		if (!Tile->BranchParent)
			return Tile;

}

INT GetBranchAncestryCount(TILE_INFO* Tile)
{
	int i = 0;
	for (; Tile; i++)
	{
		Tile = Tile->BranchParent;
	}

	return i;
}

TILE_INFO* GetDescendant(TILE_INFO* CurrentTile)
{

	for (TILE_INFO* Tile = CurrentTile;; Tile = Tile->ChildTile)
		if (!Tile->ChildTile)
			return Tile;

}



//------------------------------------------------------------------
// Split Functions
// Splits tiles vertically/horizontally in a workspace.
// The tiles must have parent tiles.
//------------------------------------------------------------------

VOID SplitTileHorizontallyNoBranch(WORKSPACE_INFO* Workspace, TILE_INFO* TileToInsert)
{
	assert(TileToInsert->ParentTile || TileToInsert->BranchParent);

	RECT SplitRect = TileToInsert->Placement.rcNormalPosition;
	RECT ParentRect = TileToInsert->ParentTile->Placement.rcNormalPosition;


	INT BranchWidth;
	INT NumTiles = GetLayoutAncestryHorizontal(TileToInsert, &BranchWidth);
	INT DeltaWindow = BranchWidth / NumTiles;
	INT LastBottom;

	SplitRect.top = (ParentRect.bottom - DeltaWindow);
	SplitRect.bottom = ParentRect.bottom;
	SplitRect.right = ParentRect.right;
	SplitRect.left = ParentRect.left;
	LastBottom = SplitRect.top;

	TileToInsert->ParentTile->Placement.rcNormalPosition = ParentRect;
	TileToInsert->Placement.rcNormalPosition = SplitRect;

	TILE_INFO* ParentTile = TileToInsert->ParentTile;

	while (ParentTile)
	{
		ParentRect.bottom = LastBottom;
		ParentRect.top = LastBottom - DeltaWindow;
		LastBottom = ParentRect.top;
		ParentTile->Placement.rcNormalPosition = ParentRect;

		if (ParentTile->BranchParent)
		{

			TILE_INFO* BranchParent = ParentTile->BranchParent;
			RECT* BranchRect = &BranchParent->Placement.rcNormalPosition;

			BranchRect->top = LastBottom - DeltaWindow;
			BranchRect->bottom = LastBottom;
			LastBottom = BranchRect->top;
		}

		ParentTile = ParentTile->ParentTile;
	}

	//Modify Parent rect to move the right border to the left border of split
	// rect.

	TileToInsert->Placement.rcNormalPosition = SplitRect;


}


INT GetColumnWidth(TILE_INFO* BranchBegin)
{
	INT Width = 0;
	for (TILE_INFO* Tile = BranchBegin; Tile; Tile = Tile->ChildTile)
	{
		RECT* TileRect = &Tile->Placement.rcNormalPosition;
		Width += (TileRect->right - TileRect->left);
	}

	return Width;

}

INT GetColumnHeight(TILE_INFO* BranchBegin)
{
	INT Height = 0;
	for (TILE_INFO* Tile = BranchBegin; Tile; Tile = Tile->ChildTile)
	{
		RECT* TileRect = &Tile->Placement.rcNormalPosition;
		Height += (TileRect->bottom - TileRect->top);
	}

	return Height;

}


VOID RecurseTileVertically(WORKSPACE_INFO* Workspace, TILE_INFO* TileToRecurse)
{
	assert(TileToRecurse->ParentTile || TileToRecurse->BranchParent);

	RECT* SplitRect = &TileToRecurse->Placement.rcNormalPosition;
	//RECT *ParentRect = &TileToRecurse->BranchParent->Placement.rcNormalPosition;
	RECT* ParentRect = &Workspace->Tree->Focus->Placement.rcNormalPosition;

	SplitRect->right = ParentRect->right;
	SplitRect->left = (ParentRect->left + ParentRect->right) / 2;
	ParentRect->right = SplitRect->left;
	SplitRect->top = ParentRect->top;
	SplitRect->bottom = ParentRect->bottom;

}

VOID RecurseTileHorizontally(WORKSPACE_INFO* Workspace, TILE_INFO* TileToRecurse)
{
	assert(TileToRecurse->ParentTile || TileToRecurse->BranchParent);

	RECT* SplitRect = &TileToRecurse->Placement.rcNormalPosition;
	//RECT* ParentRect = &TileToRecurse->BranchParent->Placement.rcNormalPosition;
	RECT* ParentRect = &Workspace->Tree->Focus->Placement.rcNormalPosition;

	SplitRect->bottom = ParentRect->bottom;
	SplitRect->top = (ParentRect->bottom + ParentRect->top) / 2;
	ParentRect->bottom = SplitRect->top;
	SplitRect->left = ParentRect->left;
	SplitRect->right = ParentRect->right;

}

VOID ResortRecursive(TILE_INFO* BranchBegin, RECT* Box)
{

	TILE_INFO* BranchNode = BranchBegin->BranchParent;

	INT RightEncap = GetDescendantCount(BranchBegin);
	INT Width = Box->right - Box->left;
	INT Height = Box->bottom - Box->top;

	INT DeltaWidth = Width / RightEncap;
	INT DeltaHeight = Height / RightEncap;
	RECT Encap;

	if (BranchNode->NodeType == VERTICAL_SPLIT)
	{
		Encap.left = Box->left;
		Encap.right = Box->left + DeltaWidth;
		Encap.top = Box->top;
		Encap.bottom = Box->bottom;
	}
	else
	{
		Encap.top = Box->top;
		Encap.bottom = Box->top + DeltaHeight;
		Encap.left = Box->left;
		Encap.right = Box->right;
	}

	INT i = 0;
	for (TILE_INFO* Tile = BranchBegin; Tile; Tile = Tile->ChildTile)
	{
		RECT* Rect = &Tile->Placement.rcNormalPosition;

		if (BranchNode->NodeType == VERTICAL_SPLIT)
		{

			Rect->left = Encap.left + (i * DeltaWidth);
			Rect->right = Encap.right + (i * DeltaWidth);
			Rect->top = Encap.top;
			Rect->bottom = Encap.bottom;
		}
		else
		{
			Rect->top = Encap.top + (i * DeltaHeight);
			Rect->bottom = Encap.bottom + (i * DeltaHeight);
			Rect->left = Encap.left;
			Rect->right = Encap.right;
		}

		if (Tile->NodeType != TERMINAL)
			ResortRecursive(Tile->BranchTile, &Tile->Placement.rcNormalPosition);

		i++;
	}

}

VOID ResortTiles(TILE_TREE* Tree)
{

	Tree->NeedsRendering = TRUE;

	if (!Tree->Root)
		return;

	DISPLAY_INFO* Display = Tree->Display;
	TILE_INFO* Ancestor = Tree->Root;
	INT RightEncap = GetDescendantCount(Ancestor);
	INT Width = Display->ScreenWidth;
	INT Height = Display->ScreenHeight;

	INT DeltaWidth = Width / RightEncap;
	INT DeltaHeight = Height / RightEncap;

	TILE_INFO* Tile = Ancestor;
	RECT Encap;

	if (Tree->Layout == VERTICAL_SPLIT)
	{


		Encap.left = Display->Rect.left;
		Encap.top = Display->Rect.top;
		Encap.right = Display->Rect.left + DeltaWidth;
		Encap.bottom = Display->Rect.top + Height;

	}
	else
	{

		Encap.left = Display->Rect.left;
		Encap.top = Display->Rect.top;
		Encap.right = Display->Rect.left + Width;
		Encap.bottom = Display->Rect.top + DeltaHeight;
	}


	INT i = 0;
	for (Tile = Tree->Root; Tile; Tile = Tile->ChildTile)
	{
		RECT* Rect = &Tile->Placement.rcNormalPosition;

		if (Tree->Layout == VERTICAL_SPLIT)
		{

			Rect->left = Encap.left + (i * DeltaWidth);
			Rect->right = Encap.right + (i * DeltaWidth);
			Rect->top = Encap.top;
			Rect->bottom = Encap.bottom;
		}
		else
		{
			Rect->top = Encap.top + (i * DeltaHeight);
			Rect->bottom = Encap.bottom + (i * DeltaHeight);
			Rect->left = Encap.left;
			Rect->right = Encap.right;
		}

		if (Tile->NodeType != TERMINAL)
			ResortRecursive(Tile->BranchTile, &Tile->Placement.rcNormalPosition);


		i++;
	}


}

// The  Main Function to add a Tile to a workspace
// It does not render the workspace.
// call RenderWorkspace to render the workspace after adding a tile.
VOID AddTileToWorkspace(TILE_TREE* Tree, TILE_INFO* TileToAdd)
{

	ResortTiles(Tree);


	Tree->Focus = TileToAdd;
	IsPressed = FALSE;


}

VOID UpgradeNode(TILE_TREE* Tree, TILE_INFO* Node)
{

	assert(Node->BranchParent);

	if (Node->BranchParent->ParentTile)
	{
		Node->BranchParent->ParentTile->ChildTile = Node;
		Node->ParentTile = Node->BranchParent->ParentTile;
	}
	else
		Node->ParentTile = NULL;

	if (Node->BranchParent->ChildTile)
	{
		Node->BranchParent->ChildTile->ParentTile = Node;
		Node->ChildTile = Node->BranchParent->ChildTile;
	}
	else
		Node->ChildTile = NULL;

	TILE_INFO* BranchParent = Node->BranchParent;

	if (Node->BranchParent->BranchParent)
	{
		Node->BranchParent = Node->BranchParent->BranchParent;
		Node->BranchParent->BranchTile = GetAncestry(Node);
	}
	else if (Tree->Root == Node->BranchParent)
	{
		Tree->Root = Node;
		Node->BranchParent = NULL;
	}
	else
		Node->BranchParent = NULL;


	RtlZeroMemory(BranchParent, sizeof(TILE_INFO));
	free(BranchParent);

}

TILE_INFO* UnlinkNode(TILE_TREE* Tree, TILE_INFO* Node)
{

	assert(Node->NodeType == TERMINAL);

	TILE_INFO* FocusTile = NULL;

	if (Node->BranchParent)
	{
		if (Node->BranchParent->BranchTile == Node)
		{
			assert(Node->ChildTile);

			//Triangle has 3 branches we're removing the first one to relink
			// the triangle to the 2nd branch 
			if (Node->ChildTile->ChildTile)
			{
				Node->BranchParent->BranchTile = Node->ChildTile;
				Node->ChildTile->ParentTile = NULL;
				FocusTile = Node->ChildTile;
			}
			else
			{
				// Upgrade since this triangle only has 2 branches and we're removing one
				UpgradeNode(Tree, Node->ChildTile);
				FocusTile = Node->ChildTile;
			}
		}
		else //We're not at the very left of a node
		{
			assert(Node->ParentTile);

			//for a triangle to be upgraded it must remove 1 branch
			// of a 2 branch triangle split
			// and if the branchparent->branch tile isn't this node
			// it means it's the one on the right, so it has to have
			// no children and it it has to have Ancestry of 2
			// Ancestry includes itself
			if (GetAncestryCount(Node) == 2 && !Node->ChildTile)
			{
				// Upgrade since this triangle only has 2 branches and we're removing one
				UpgradeNode(Tree, Node->ParentTile);
				FocusTile = Node->ParentTile;
			}
			else
			{
				if (Node->ChildTile)
				{
					Node->ParentTile->ChildTile = Node->ChildTile;
					Node->ChildTile->ParentTile = Node->ParentTile;
					FocusTile = Node->ChildTile;
				}
				else
				{
					Node->ParentTile->ChildTile = NULL;
					FocusTile = Node->ParentTile;
				}

			}

		}

	}
	else //we're at the top level
	{

		//Top Level and last node
		if (Tree->Root == Node && !Node->ChildTile)
		{
			Tree->Root = NULL;
			FocusTile = NULL;
			Tree->IsFullScreen = FALSE;
			return NULL;
		}
		else if (Node == Tree->Root)
		{
			Tree->Root = Node->ChildTile;
			FocusTile = Node->ChildTile;
			Node->ChildTile->ParentTile = NULL;
		}
		else if (Node->ParentTile && Node->ChildTile)
		{
			Node->ParentTile->ChildTile = Node->ChildTile;
			Node->ChildTile->ParentTile = Node->ParentTile;
			FocusTile = Node->ChildTile;

		}
		else if (Node->ParentTile)
		{
			Node->ParentTile->ChildTile = NULL;
			FocusTile = Node->ParentTile;
		}


		//Upgrade Node since ROOT brnach is a split which means root has to change
		if (Tree->Root == Node && !Node->ChildTile && Node->NodeType != TERMINAL)
		{
			Tree->Root = Tree->Root->BranchTile;
			Tree->Root->BranchParent = NULL;
			FocusTile = Tree->Root;

			for (TILE_INFO* Tile = Tree->Root; Tile; Tile = Tile->ChildTile)
				Tile->BranchParent = NULL;

		}

	}

	return FocusTile;

}

BOOL IsWorkspaceRootTile(TILE_TREE* Tree)
{

	return !(Tree->Root->ChildTile || Tree->Root->BranchTile);

}

VOID FollowFocusToTerminal(WORKSPACE_INFO* Workspace)
{

	while (Workspace->Tree->Focus && Workspace->Tree->Focus->NodeType != TERMINAL)
		Workspace->Tree->Focus = Workspace->Tree->Focus->BranchTile;

}

VOID RemoveTileFromTree(WORKSPACE_INFO* Workspace, TILE_INFO* TileToRemove)
{

	TileToRemove->WindowHandle = NULL;

	Workspace->Tree->Focus = UnlinkNode(Workspace->Tree, TileToRemove);

	FollowFocusToTerminal(Workspace);
	ResortTiles(Workspace->Tree);

	free(TileToRemove);

}

VOID LinkNode(TILE_TREE* Tree, TILE_INFO* TileToAdd)
{

	DISPLAY_INFO* Display = Tree->Display;

	if (!Tree->Root)
	{
		TileToAdd->NodeType = TERMINAL;
		Tree->Root = TileToAdd;
		Tree->Layout = UserChosenLayout;
	}
	else if (!IsPressed || IsWorkspaceRootTile(Tree))
	{

		TileToAdd->NodeType = TERMINAL;

		if (Tree->Focus->ChildTile)
		{
			TileToAdd->ChildTile = Tree->Focus->ChildTile;
			TileToAdd->ChildTile->ParentTile = TileToAdd;
		}

		Tree->Focus->ChildTile = TileToAdd;
		TileToAdd->ParentTile = Tree->Focus;

		if (TileToAdd->ParentTile->BranchParent)
			TileToAdd->BranchParent = TileToAdd->ParentTile->BranchParent;

	}
	else
	{

		TILE_INFO* NodeTile = (TILE_INFO*)malloc(sizeof(TILE_INFO));
		RtlZeroMemory(NodeTile, sizeof(TILE_INFO));

		if (Tree->Focus->ChildTile)
		{
			NodeTile->ChildTile = Tree->Focus->ChildTile;
			NodeTile->ChildTile->ParentTile = NodeTile;
		}

		if (Tree->Focus->ParentTile)
		{
			Tree->Focus->ParentTile->ChildTile = NodeTile;
			NodeTile->ParentTile = Tree->Focus->ParentTile;
			NodeTile->BranchParent = NodeTile->ParentTile->BranchParent;
		}
		else
		{
			if (Tree->Focus == Tree->Root)
				Tree->Root = NodeTile;
			else
			{
				Tree->Focus->BranchParent->BranchTile = NodeTile;
				NodeTile->BranchParent = Tree->Focus->BranchParent;
			}


		}


		NodeTile->NodeType = UserChosenLayout;

		NodeTile->BranchTile = Tree->Focus;


		Tree->Focus->BranchParent = NodeTile;
		Tree->Focus->ChildTile = TileToAdd;
		Tree->Focus->ParentTile = NULL;
		TileToAdd->ParentTile = Tree->Focus;
		TileToAdd->BranchParent = NodeTile;

	}

}



//Remove Title Bar from window, User Option.
VOID RemoveTitleBar(HWND WindowHandle)
{
	LONG_PTR WindowStyle = GetWindowLongPtrA(WindowHandle, GWL_STYLE);

	WindowStyle &= ~WS_OVERLAPPEDWINDOW;

	SetWindowLongPtrA(WindowHandle, GWL_STYLE, WindowStyle);
	
}

BOOL IsOnPrimaryDisplay(TILE_INFO* Tile)
{
	HMONITOR Monitor = MonitorFromWindow(Tile->WindowHandle, NULL);

	if (Monitor == PrimaryDisplay->Handle)
		return TRUE;

	return FALSE;
}

// Seperate the all the windows we got into tiles. there are 9 workspaces.
// and upon init regrouping there are 4 windows per workspaces. If there
// is more than 40 total windows that have to be grouped. recalcualte
// tiles per workspace.
//
VOID PerformInitialRegroupring()
{

	INT TilesPerWorkspace;
	INT NumTiles;
	INT WorkspaceIndex;
	INT NumWorkspace;
	INT MaxInitTiles;

	NumTiles = WindowOrderList.size();
	NumWorkspace = WorkspaceList.size() - 1;
	MaxInitTiles = NumWorkspace * 4;

#ifdef COMMERCIAL
	if (NumTiles > MaxInitTiles)
		TilesPerWorkspace = (NumTiles / NumWorkspace) + 1;
	else if (NumTiles > INIT_TILES_PER_WORKSPACE)
		TilesPerWorkspace = INIT_TILES_PER_WORKSPACE;
	else
		TilesPerWorkspace = NumTiles;
#else
	TilesPerWorkspace = 3;

	if (NumTiles > TilesPerWorkspace * NumWorkspace)
		NumTiles = TilesPerWorkspace * NumWorkspace;
#endif

	WorkspaceIndex = CurWk;

	INT TilesLeft = NumTiles;

	for (int i = 0; i < NumTiles; i += TilesPerWorkspace)
	{

		INT ActualTilesLeft = NumTiles - (i * TilesPerWorkspace);
		WORKSPACE_INFO* CurrentWorkspace = &WorkspaceList[WorkspaceIndex];


		int j = 0;
		for (; j < TilesPerWorkspace && TilesLeft; j++)
		{

			TILE_INFO* CurrentTileInfo = &WindowOrderList[i + j];
			TILE_INFO* TileToAdd = (TILE_INFO*)malloc(sizeof(TILE_INFO));
			RtlZeroMemory(TileToAdd, sizeof(TILE_INFO));

			*TileToAdd = *CurrentTileInfo;
			TileToAdd->BranchTile = NULL;

			if (ShouldRemoveTitleBars)
				RemoveTitleBar(TileToAdd->WindowHandle);

			if (!IsOnPrimaryDisplay(TileToAdd))
				TileToAdd->IsDisplayChanged = TRUE;

			LinkNode(CurrentWorkspace->Tree, TileToAdd);
			AddTileToWorkspace(CurrentWorkspace->Tree, TileToAdd);
			MoveWindowToVDesktop(TileToAdd->WindowHandle, CurrentWorkspace->VDesktop);

			TilesLeft--;
		}

		WorkspaceIndex++;

	}

}

INT ForceToForeground(HWND WindowHandle)
{
	SwitchToThisWindow(WindowHandle, TRUE);
	return 1;
}

DWORD HandleWeirdWindowState(HWND WindowHandle)
{
	WCHAR CurrentWindowText[1024] = { 0 };
	DWORD ShowMask = SW_RESTORE;
	GetClassNameW(WindowHandle, CurrentWindowText, 1024);

	INT ListLength = ARRAY_SIZEOF(WeirdWindowsList);
	for (int i = 0; i < ListLength; i++)
	{
		if (!wcscmp(CurrentWindowText, WeirdWindowsList[i].ClassName))
		{
			ShowMask = WeirdWindowsList[i].ShowMask;
		}
	}

	return ShowMask;

}

VOID RenderFocusWindow(TILE_INFO* Tile)
{

	HWND WindowHandle = Tile->WindowHandle;

	if (GetForegroundWindow() != Tile->WindowHandle)
	{
		ForceToForeground(WindowHandle);
		logc(7, "CurrentFgWindow : %X\n", GetForegroundWindow());
	}

	logc(6, "RenderFocusWindow : %X\n", WindowHandle);

	RECT ClientRect;
	PRECT TileRect = &Tile->Placement.rcNormalPosition;

	GetClientRect(WindowHandle, &ClientRect);

	logc(6, "%lu %lu %lu %lu\n", ClientRect.left, ClientRect.top, ClientRect.right, ClientRect.bottom);

	INT Width = ClientRect.right - ClientRect.left;
	INT BorderSize = 5;

	POINT TopLeftClientArea = { 0 };

	if (!ClientToScreen(WindowHandle, &TopLeftClientArea))
		return;

	logc(6, "TL - X : %lu, TL - Y : %lu\n", TopLeftClientArea.x, TopLeftClientArea.y);

	ClientRect.left += TopLeftClientArea.x;
	ClientRect.top += TopLeftClientArea.y;
	ClientRect.bottom += TopLeftClientArea.y;

	logc(6, "%lu %lu %lu %lu\n", ClientRect.left, ClientRect.bottom - BorderSize, Width, BorderSize);

	if (!SetWindowPos(FocusWindow, HWND_TOPMOST, ClientRect.left, ClientRect.bottom - BorderSize, Width, BorderSize, 0))
		FailWithCode("SetWindowPos Focus Window");

	if (!SetWindowPos(FocusWindow, HWND_NOTOPMOST, ClientRect.left, ClientRect.bottom - BorderSize, Width, BorderSize, 0))
		FailWithCode("SetWindowPos Focus Window");

	ShowWindow(FocusWindow, SW_SHOW);

}

VOID RenderFocusWindowEx(WORKSPACE_INFO* Workspace)
{
	if (TreeHas(Focus) && !Workspace->Tree->IsFullScreen)
		RenderFocusWindow(Workspace->Tree->Focus);
	else
		SetWindowPos(FocusWindow, HWND_BOTTOM, 0, 0, 0, 0, SWP_HIDEWINDOW);
}


VOID AdjustForGaps(RECT* PrintRect, DISPLAY_INFO* Display)
{

	//Adjust for OuterGaps
	PrintRect->left +=OuterGapsHorizontal / 2;
	PrintRect->right += OuterGapsHorizontal / 2;

	PrintRect->top += OuterGapsVertical / 2;
	PrintRect->bottom += OuterGapsVertical / 2;

	//Adjust for InnerGaps
	PrintRect->left += InnerGapsHorizontal / 2;
	PrintRect->right -= InnerGapsHorizontal / 2;

	PrintRect->top += InnerGapsVertical / 2;
	PrintRect->bottom -= InnerGapsVertical / 2;

}

VOID AdjustForBorder(TILE_INFO* Tile, RECT* PrintRect)
{

	RECT WindowRect, FrameRect;
	HRESULT Result;
	HWND WindowHandle;

	WindowHandle = Tile->WindowHandle;

	Result = DwmGetWindowAttribute(WindowHandle,
		DWMWA_EXTENDED_FRAME_BOUNDS,
		&FrameRect,
		sizeof(FrameRect));

	if (Result != S_OK)
		return;

	if (!GetWindowRect(WindowHandle, &WindowRect))
		return;

	//Adjust for NC area of Windows
	RECT BorderRect;

	BorderRect.left = FrameRect.left - WindowRect.left;
	BorderRect.top = FrameRect.top - WindowRect.top;
	BorderRect.right = WindowRect.right - FrameRect.right;
	BorderRect.bottom = WindowRect.bottom - FrameRect.bottom;

	PrintRect->left -= BorderRect.left;
	PrintRect->top -= BorderRect.top;

	PrintRect->right += BorderRect.right;
	PrintRect->bottom += BorderRect.bottom;

}

INT RenderWindows(TILE_INFO* Tile, DISPLAY_INFO* Display)
{

	INT Count = 0;

	for (; Tile; Tile = Tile->ChildTile)
	{

		if (Tile->NodeType != TERMINAL)
		{
			Count += RenderWindows(Tile->BranchTile, Display);
			continue;
		}

		RECT PrintRect = Tile->Placement.rcNormalPosition;

		if (AdjustForNC && !Tile->IsDisplayChanged)
			AdjustForBorder(Tile, &PrintRect);

		if (IsGapsEnabled)
			AdjustForGaps(&PrintRect, Display);


		WINDOWPLACEMENT WndPlacement = { 0 };

		WndPlacement.length = sizeof(WINDOWPLACEMENT);
		WndPlacement.flags = 0;
		WndPlacement.showCmd = SW_RESTORE;
		WndPlacement.rcNormalPosition = PrintRect;

		RECT DpiRect;

		DWORD RetVal = SetWindowPlacement(Tile->WindowHandle, &WndPlacement);

		if (!RetVal)
		{
			if (GetLastError() == ERROR_INVALID_WINDOW_HANDLE)
				return ++Count;

			FailWithCode(MakeFormatString("SetWindowPos : %X\n", Tile->WindowHandle));

		}

		if (Tile->IsDisplayChanged)
		{

			INT Width = (PrintRect.right - PrintRect.left);
			INT Height = (PrintRect.bottom - PrintRect.top);

			if (AdjustForNC)
				AdjustForBorder(Tile, &PrintRect);

			SetWindowPos(Tile->WindowHandle, HWND_TOP, PrintRect.left, PrintRect.top, Width, Height, 0);
		}


		Count++;
	}

	return Count;

}

VOID RenderDebugWindow(TILE_INFO* FocusTile)
{

	return;

	if (!FocusTile)
		return;


	RECT* FocusRect = &FocusTile->Placement.rcNormalPosition;

	INT Width = FocusRect->right - FocusRect->left;
	INT Height = FocusRect->bottom - FocusRect->top;

	if (!SetWindowPos(DebugWindow, HWND_TOPMOST, FocusRect->left, FocusRect->top, Width, Height, 0))
		FailWithCode("Couldn't move DebugWindow??");

	ShowWindow(DebugWindow, SW_SHOW);

}

VOID RenderFullscreenWindow(WORKSPACE_INFO* Workspace, DISPLAY_INFO* Display)
{
	assert(Workspace->Tree->IsFullScreen &&
		Workspace->Tree->Focus);

	ForceToForeground(Workspace->Tree->Focus->WindowHandle);

	RECT PrintRect;

	PrintRect.left = Display->Rect.left;
	PrintRect.top = Display->Rect.top;

	PrintRect.right = Display->Rect.left + Display->RealScreenWidth;
	PrintRect.bottom = Display->Rect.top + Display->RealScreenHeight;

	if (AdjustForNC && !IsFullScreenMax)
		AdjustForBorder(Workspace->Tree->Focus, &PrintRect);

	INT Width = PrintRect.right - PrintRect.left;
	INT Height = PrintRect.bottom - PrintRect.top;

	if (IsFullScreenMax)
	{
		WCHAR ClassName[1024];
		GetClassNameW(Workspace->Tree->Focus->WindowHandle, ClassName, 1024);
		
		//If not Console Window
		if (wcscmp(ClassName, L"ConsoleWindowClass"))
			Height += (Display->TrayRect.bottom - Display->TrayRect.top);

		Workspace->FullScreenStyle = GetWindowLongPtrA(Workspace->Tree->Focus->WindowHandle, GWL_STYLE);
		SetWindowLongPtrA(Workspace->Tree->Focus->WindowHandle, GWL_STYLE, Workspace->FullScreenStyle & ~WS_OVERLAPPEDWINDOW);


	}

	DWORD RetVal = SetWindowPos(Workspace->Tree->Focus->WindowHandle, HWND_TOP, PrintRect.left, PrintRect.top, Width, Height, 0);

	if (!RetVal)
		FailWithCode(MakeFormatString("SetWindowPos : %u\n", Workspace->Tree->Focus->WindowHandle));

}

VOID RenderStatusBar()
{
	unsigned char Idx = 1;
	static char LastWorkspaces[10] = { 0 };
	char Workspaces[10] = { 0 };
	std::vector<DIFF_STATE> DiffVector;
	DiffVector.reserve(10);

	BOOL IsActiveWorkspace = FALSE;

	for (int i = 1; i < WorkspaceList.size(); i++)
	{
		WORKSPACE_INFO* Workspace = &WorkspaceList[i];
		BUTTON_STATE& ButtonState = ButtonMap[PrimaryDisplay->StatusBar[i]];

		if (TreeHas(Root) || i == CurWk)
		{
			Workspaces[Idx] = i;
			Idx++;
		}
	}

	for (int i = Idx; i < WorkspaceList.size(); i++)
		Workspaces[i] = -1;

	for (int Idx = 0; Idx < WorkspaceList.size(); Idx++)
	{

		BUTTON_STATE& ButtonState = ButtonMap[PrimaryDisplay->StatusBar[Idx]];
		if (Workspaces[Idx] != LastWorkspaces[Idx])
		{
			DiffVector.push_back({ Workspaces[Idx], LastWorkspaces[Idx], Idx});
			LastWorkspaces[Idx] = Workspaces[Idx];
		}
	}

	BOOL RenderTxt = FALSE;
	BOOL RenderBg = FALSE;
	BOOL HideWindow = FALSE;

	for (int i = 0; i < DiffVector.size(); i++)
	{

		DIFF_STATE* Diff = &DiffVector[i];

		// First time Rendering
		if (!Diff->PrevTxt)
			continue;


		if (Diff->NewTxt == -1)
		{
			HideWindow = TRUE;
		}
		else if (Diff->NewTxt > 0 && Diff->PrevTxt > 0)
		{
			RenderTxt = TRUE;
		}
		else if (Diff->PrevTxt == -1)
		{
			RenderTxt = TRUE;
			RenderBg = TRUE;
		}

		for (auto& Display : DisplayList)
		{
			if (HideWindow)
				ShowWindow(Display.StatusBar[Diff->Slot], SW_HIDE);

			BUTTON_STATE& ButtonState = ButtonMap[Display.StatusBar[Diff->Slot]];
			ButtonState.RenderBg = RenderBg;
			ButtonState.RenderTxt = RenderTxt;

			if (RenderTxt || RenderBg)
			{

				if (RenderTxt)
					ButtonState.ButtonText[0] = Diff->NewTxt + '0';

				InvalidateRect(Display.StatusBar[Diff->Slot], NULL, FALSE);

				if (RenderBg)
					ShowWindow(Display.StatusBar[Diff->Slot], SW_SHOW);
			}
		}
	}

	for (auto& Display : DisplayList)
	{
		for (int i = 1; i < WorkspaceList.size(); i++)
		{
			BUTTON_STATE& ButtonState = ButtonMap[Display.StatusBar[i]];

			if (ButtonState.IsActiveWorkspace)
			{
				if (ButtonState.ButtonText[0] - '0' != CurWk)
				{
					ButtonState.IsActiveWorkspace = FALSE;
					InvalidateRect(Display.StatusBar[i], NULL, FALSE);
				}
				else
				{
					Display.BtnToColor = Display.StatusBar[i];
					InvalidateRect(Display.StatusBar[i], NULL, FALSE);
				}
			}
			else if (ButtonState.ButtonText[0] - '0' == CurWk)
			{

				ButtonState.IsActiveWorkspace = TRUE;
					Display.BtnToColor = Display.StatusBar[i];
				InvalidateRect(Display.StatusBar[i], NULL, FALSE);
			}
		}
	}
}

VOID RenderNoWindow()
{
	SendMessage(TrayWindow, WM_COMMAND, MIN_ALL, 0);
}

VOID RenderWorkspace(INT WorkspaceNumber)
{

	//the window about to be rendered is the window in focus;
	CurWk = WorkspaceNumber;

	WORKSPACE_INFO* Workspace = &WorkspaceList[WorkspaceNumber];

	for (auto KeyValue : Workspace->Trees)
	{
		TILE_TREE* CurrentTree = &KeyValue.second;

		if (!CurrentTree->NeedsRendering)
			continue;

		//logc(13, "Rendering Workspace ; %u\n", WorkspaceNumber);

		INT Count = 1;

		if (CurrentTree->IsFullScreen)
			RenderFullscreenWindow(Workspace, CurrentTree->Display);
		else if (CurrentTree->Root)
			Count = RenderWindows(CurrentTree->Root, CurrentTree->Display);

		Workspace->Tree->NeedsRendering = FALSE;

		//logc(12, "Total Length Of Workspace : %u\n", Count);
	}


	RenderStatusBar();
	RenderFocusWindowEx(Workspace);


}

VOID InitWorkspaceList()
{
#ifdef COMMERCIAL
	WorkspaceList.resize(10);
#else
	WorkspaceList.resize(3);
#endif
}

VOID LoadNecessaryModules()
{
	ForceResize64 = LoadLibraryA("ForceResize64.dll");

	if (!ForceResize64)
		FailWithCode("Couldn't load ForceResize64.dll");

}

UINT_PTR KeyboardLookupTable[0x100];
HOTKEY_DISPATCH HotKeyCallbackTable[0x100][2];


NODE_TYPE GetBranchLayout(TILE_TREE* Tree, TILE_INFO* Tile)
{
	if (Tile->BranchParent)
		return Tile->BranchParent->NodeType;
	else
		return Tree->Layout;
}

VOID FocusLeft(WORKSPACE_INFO* Workspace)
{

	assert(Workspace->Tree->Focus->NodeType == TERMINAL);

	for (TILE_INFO* Tile = Workspace->Tree->Focus; Tile; Tile = Tile->BranchParent)
	{
		if (GetBranchLayout(Workspace->Tree, Tile) == VERTICAL_SPLIT)
		{
			if (Tile->ParentTile)
			{
				Workspace->Tree->Focus = Tile->ParentTile;
				break;
			}
		}

	}

	FollowFocusToTerminal(Workspace);


}

VOID FocusRight(WORKSPACE_INFO* Workspace)
{

	assert(Workspace->Tree->Focus->NodeType == TERMINAL);

	for (TILE_INFO* Tile = Workspace->Tree->Focus; Tile; Tile = Tile->BranchParent)
	{
		if (GetBranchLayout(Workspace->Tree, Tile) == VERTICAL_SPLIT)
		{
			if (Tile->ChildTile)
			{
				Workspace->Tree->Focus = Tile->ChildTile;
				break;
			}
		}

	}

	FollowFocusToTerminal(Workspace);

}


VOID FocusTop(WORKSPACE_INFO* Workspace)
{

	assert(Workspace->Tree->Focus->NodeType == TERMINAL);

	for (TILE_INFO* Tile = Workspace->Tree->Focus; Tile; Tile = Tile->BranchParent)
	{
		if (GetBranchLayout(Workspace->Tree, Tile) == HORIZONTAL_SPLIT)
		{
			if (Tile->ParentTile)
			{
				Workspace->Tree->Focus = Tile->ParentTile;
				break;
			}
		}

	}

	FollowFocusToTerminal(Workspace);

}


VOID FocusBottom(WORKSPACE_INFO* Workspace)
{

	assert(Workspace->Tree->Focus->NodeType == TERMINAL);

	for (TILE_INFO* Tile = Workspace->Tree->Focus; Tile; Tile = Tile->BranchParent)
	{
		if (GetBranchLayout(Workspace->Tree, Tile) == HORIZONTAL_SPLIT)
		{
			if (Tile->ChildTile)
			{
				Workspace->Tree->Focus = Tile->ChildTile;
				break;
			}
		}

	}

	FollowFocusToTerminal(Workspace);

}

VOID FocusRoot(TILE_TREE* Tree)
{

	if (Tree->Focus->BranchParent)
		UserChosenLayout = Tree->Focus->BranchParent->NodeType;
	else
		UserChosenLayout = Tree->Layout;
}

BOOL ButtonDown(UINT_PTR Button)
{
	return (Button == WM_KEYDOWN || Button == WM_SYSKEYDOWN);
}

VOID HandleLeft(WORKSPACE_INFO* Workspace, BOOL Swap)
{

	if (!Workspace->Tree->Focus)
		return;

	TILE_INFO* PrevTile = Workspace->Tree->Focus;

	if (Swap)
	{
		HWND TargetWindow = Workspace->Tree->Focus->WindowHandle;
		FocusLeft(Workspace);

		if (Workspace->Tree->Focus != PrevTile)
		{
			PrevTile->WindowHandle = Workspace->Tree->Focus->WindowHandle;
			Workspace->Tree->Focus->WindowHandle = TargetWindow;
			Workspace->Tree->NeedsRendering = TRUE;
			RenderWorkspace(CurWk);
		}
	}
	else
		FocusLeft(Workspace);

	FocusRoot(Workspace->Tree);
	RenderFocusWindow(Workspace->Tree->Focus);
	RenderDebugWindow(Workspace->Tree->Focus);
	ForceToForeground(Workspace->Tree->Focus->WindowHandle);

}


VOID HandleRight(WORKSPACE_INFO* Workspace, BOOL Swap)
{

	if (!Workspace->Tree->Focus)
		return;

	TILE_INFO* PrevTile = Workspace->Tree->Focus;

	if (Swap)
	{
		HWND TargetWindow = Workspace->Tree->Focus->WindowHandle;
		FocusRight(Workspace);

		if (Workspace->Tree->Focus != PrevTile)
		{
			PrevTile->WindowHandle = Workspace->Tree->Focus->WindowHandle;
			Workspace->Tree->Focus->WindowHandle = TargetWindow;
			Workspace->Tree->NeedsRendering = TRUE;
			RenderWorkspace(CurWk);
		}
	}
	else
		FocusRight(Workspace);

	FocusRoot(Workspace->Tree);
	RenderFocusWindow(Workspace->Tree->Focus);
	RenderDebugWindow(Workspace->Tree->Focus);
	ForceToForeground(Workspace->Tree->Focus->WindowHandle);

}

VOID HandleTop(WORKSPACE_INFO* Workspace, BOOL Swap)
{

	if (!Workspace->Tree->Focus)
		return;

	TILE_INFO* PrevTile = Workspace->Tree->Focus;

	if (Swap)
	{
		HWND TargetWindow = Workspace->Tree->Focus->WindowHandle;
		FocusTop(Workspace);

		if (Workspace->Tree->Focus != PrevTile)
		{
			PrevTile->WindowHandle = Workspace->Tree->Focus->WindowHandle;
			Workspace->Tree->Focus->WindowHandle = TargetWindow;
			Workspace->Tree->NeedsRendering = TRUE;
			RenderWorkspace(CurWk);
		}
	}
	else
		FocusTop(Workspace);

	FocusRoot(Workspace->Tree);
	RenderFocusWindow(Workspace->Tree->Focus);
	RenderDebugWindow(Workspace->Tree->Focus);
	ForceToForeground(Workspace->Tree->Focus->WindowHandle);

}


VOID HandleBottom(WORKSPACE_INFO* Workspace, BOOL Swap)
{


	if (!Workspace->Tree->Focus)
		return;

	TILE_INFO* PrevTile = Workspace->Tree->Focus;

	if (Swap)
	{
		HWND TargetWindow = Workspace->Tree->Focus->WindowHandle;
		FocusBottom(Workspace);

		if (Workspace->Tree->Focus != PrevTile)
		{
			PrevTile->WindowHandle = Workspace->Tree->Focus->WindowHandle;
			Workspace->Tree->Focus->WindowHandle = TargetWindow;
			Workspace->Tree->NeedsRendering = TRUE;
			RenderWorkspace(CurWk);
		}
	}
	else
		FocusBottom(Workspace);

	//This Shit Works because the graph is left/down-adjusted to say so??
	//when you destroy a node the next one goes down and so the swap
	// happens regardless

	FocusRoot(Workspace->Tree);
	RenderFocusWindow(Workspace->Tree->Focus);
	RenderDebugWindow(Workspace->Tree->Focus);
	ForceToForeground(Workspace->Tree->Focus->WindowHandle);

}

INT HideWindows(TILE_INFO* Tile)
{

	INT Count = 0;

	for (; Tile; Tile = Tile->ChildTile)
	{

		//Recursively Hide Splits
		if (Tile->NodeType != TERMINAL)
		{
			Count += HideWindows(Tile->BranchTile);
			continue;
		}

		//SetWindowPos(Tile->WindowHandle, 0, 0, 0, 0, 0, 0);
		ShowWindow(Tile->WindowHandle, SW_MINIMIZE);

	}

	return Count;

}

VOID HandleSwitchDesktop(INT WorkspaceNumber)
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[WorkspaceNumber];

	//Atomically wait while a new window is being created
	while (!CanSwitch)
	{
	}

	RenderFocusWindowEx(Workspace);

	//The first time you switch to a VDesktop it minizimes all windows
	//Unless you render the desktop during the switch, for every workspace
	//that isn't Workspace 1, it's fine since they get rendered on switch
	//but Workspace 1 gets rendered on App Start, but the first time we 
	// switch to it we have to re-render.
	// No idea why it works like that
	if (WorkspaceNumber == 1 && !FirstDesktopRender)
	{
		Workspace->Tree->NeedsRendering = TRUE;
		FirstDesktopRender = TRUE;
	}

	RenderWorkspace(WorkspaceNumber);

	HRESULT Result = VDesktopManagerInternal->SwitchDesktop(Workspace->VDesktop);

	if (TreeHas(Focus))
		ForceToForeground(Workspace->Tree->Focus->WindowHandle);

	if (FAILED(Result))
		Fail("SwitchDesktop");

	LuaDispatchEx("on_change_workspace", CurWk);

}

VOID HandleShutdown()
{

	ShowWindow(TrayWindow, SW_SHOW);

	if (x86ProcessHandle)
		TerminateProcess(x86ProcessHandle, 369);

	NOTIFYICONDATA NotifyData;
	RtlZeroMemory(&NotifyData, sizeof(NotifyData));

	NotifyData.hWnd = NotifWindow;
	NotifyData.cbSize = sizeof(NotifyData);
	NotifyData.uID = 69;

	Shell_NotifyIcon(NIM_DELETE, &NotifyData);

	for (int i = 2; i < WorkspaceList.size(); i++)
		if (WorkspaceList[i].VDesktop)
		{
			HRESULT Result = VDesktopManagerInternal->RemoveDesktop(WorkspaceList[i].VDesktop, WorkspaceList[1].VDesktop);

			if (FAILED(Result))
				MessageBoxA(NULL, "wtf RemoveDesktop", NULL, MB_OK);
			WorkspaceList[i].VDesktop->Release();
		}

	if (WorkspaceList[1].VDesktop)
		WorkspaceList[1].VDesktop->Release();

	ViewCollection->Release();
	VDesktopManagerInternal->Release();
	VDesktopManager->Release();
	ServiceProvider->Release();

	TerminateProcess(GetCurrentProcess(), 420);

}

VOID HandleChangeWorkspace(INT WorkspaceNumber)
{
	if (WorkspaceNumber == CurWk)
		return;

	PostThreadMessageA(GetCurrentThreadId(), WM_SWITCH_DESKTOP, WorkspaceNumber, NULL);
}

VOID HandleMoveWindowWorkspace(INT WorkspaceNumber)
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
	WORKSPACE_INFO* TargetWorkspace = &WorkspaceList[WorkspaceNumber];

	TILE_INFO* Node = Workspace->Tree->Focus;
	TILE_INFO* NewNode;

	if (!Node)
		return;

	NewNode = (TILE_INFO*)RtlAlloc(sizeof(TILE_INFO));
	RtlZeroMemory(NewNode, sizeof(TILE_INFO));

	//Copy Over Important Stuff of the 
	NewNode->WindowHandle = Node->WindowHandle;
	NewNode->PreWMInfo = Node->PreWMInfo;

	PostThreadMessageA(GetCurrentThreadId(), WM_MOVE_TILE, (WPARAM)NewNode->WindowHandle, (LPARAM)TargetWorkspace->VDesktop);
	//ComOk(MoveWindowToVDesktop(NewNode->WindowHandle, TargetWorkspace->VDesktop));

	RemoveTileFromTree(Workspace, Node);

	if (TreeHas(Root) && IsWorkspaceRootTile(Workspace->Tree))
		Workspace->Tree->IsFullScreen = FALSE;

	LinkNode(TargetWorkspace->Tree, NewNode);
	AddTileToWorkspace(TargetWorkspace->Tree, NewNode);
	RenderWorkspace(CurWk);

	if (Workspace->Tree->Focus)
		ForceToForeground(Workspace->Tree->Focus->WindowHandle);

}


INT HandleKeyStatesConfig(PKBDLLHOOKSTRUCT KeyboardData, UINT_PTR KeyState)
{

	BOOL ModDown;
	if (ModKey == ALT)
		ModDown = (KeyboardLookupTable[KeyboardData->vkCode] == WM_SYSKEYDOWN);
	else
		ModDown = KeyboardLookupTable[VK_LWIN] == WM_KEYDOWN &&
		KeyboardLookupTable[KeyboardData->vkCode] == WM_KEYDOWN;

	BOOL ShiftDown = ButtonDown(KeyboardLookupTable[VK_LSHIFT]);
	INT SkipKey = TRUE;

	if (!ModDown)
		return FALSE;

	if (ModDown == ALT && KeyboardData->vkCode == VK_LMENU)
		return TRUE;

	if (ModDown == WIN && KeyboardData->vkCode == VK_LWIN)
		return TRUE;

	HOTKEY_DISPATCH HotkeyDispatch = HotKeyCallbackTable[KeyboardData->vkCode][ShiftDown];

	if (HotkeyDispatch.HotKeyCb)
	{
		HotkeyDispatch.HotKeyCb();
		return TRUE;
	}

	if (ModKey == ALT)
		return TRUE;

	return FALSE;

}

LRESULT WINAPI KeyboardCallback(int nCode, WPARAM WParam, LPARAM LParam)
{

	if (nCode < 0)
		return CallNextHookEx(KeyboardHook, nCode, WParam, LParam);

	if (nCode != HC_ACTION)
		return CallNextHookEx(KeyboardHook, nCode, WParam, LParam);

	PKBDLLHOOKSTRUCT KeyboardData = (PKBDLLHOOKSTRUCT)LParam;
	UINT_PTR KeyState = WParam;

	KeyboardLookupTable[KeyboardData->vkCode] = KeyState;

	if (HandleKeyStatesConfig(KeyboardData, KeyState))
	{
		WCHAR WindowText[1024] = { 0 };
		HWND WindowHandle;

		return DO_NOT_PASS_KEY;
	}


	//logc(11, "Layout : %u\n", UserChosenLayout);

	return CallNextHookEx(KeyboardHook, nCode, WParam, LParam);

}

VOID InstallKeyboardHooks()
{
	KeyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardCallback, NULL, 0);

	if (!KeyboardHook)
		FailWithCode("Couldn't set keyboard hook");
}

LONG WINAPI OnCrash(PEXCEPTION_POINTERS* ExceptionInfo)
{

	ShowWindow(TrayWindow, SW_SHOW);

	if (x86ProcessHandle)
		TerminateProcess(x86ProcessHandle, 369);

	NOTIFYICONDATA NotifyData;
	RtlZeroMemory(&NotifyData, sizeof(NotifyData));

	NotifyData.uID = 69;
	Shell_NotifyIcon(NIM_DELETE, &NotifyData);

	for (int i = 2; i < WorkspaceList.size(); i++)
		if (WorkspaceList[i].VDesktop)
		{
			VDesktopManagerInternal->RemoveDesktop(WorkspaceList[i].VDesktop, WorkspaceList[1].VDesktop);
			WorkspaceList[i].VDesktop->Release();
		}

	WorkspaceList[1].VDesktop->Release();

	ViewCollection->Release();
	VDesktopManagerInternal->Release();
	VDesktopManager->Release();
	ServiceProvider->Release();

	return EXCEPTION_CONTINUE_SEARCH;

}

VOID SetCrashRoutine()
{

	//AddVectoredExceptionHandler(FALSE, (PVECTORED_EXCEPTION_HANDLER)OnCrash);
}

VOID GetRidOfFade(HWND WindowHandle)
{
	BOOL DisableTransition = TRUE;
	DwmSetWindowAttribute(WindowHandle, DWMWA_TRANSITIONS_FORCEDISABLED, &DisableTransition, sizeof(DisableTransition));
}

extern "C" __declspec(dllexport) VOID OnNewWindow(HWND WindowHandle)
{


	if (IsIgnoreWindow(WindowHandle) || !IsWindow(WindowHandle))
		return;

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
#ifndef COMMERCIAL
	if (GetWindowCount(Workspace->Tree->Root) > 2)
		return;
#endif

	CanSwitch = FALSE;

	if (ShouldRemoveTitleBars)
		RemoveTitleBar(WindowHandle);

	InstallWindowSizeHook();
	GetRidOfFade(WindowHandle);
	SendMessageW(WindowHandle, WM_INSTALL_HOOKS, NULL, (LPARAM)WindowHandle);

	BOOL IsFirstTile = (!(Workspace->Tree && Workspace->Tree->Root));

	TILE_INFO* TileToAdd = (TILE_INFO*)malloc(sizeof(TILE_INFO));
	RtlZeroMemory(TileToAdd, sizeof(TILE_INFO));

	TileToAdd->WindowHandle = WindowHandle;

	if (Workspace->Tree->Display->Handle != PrimaryDisplay->Handle)
		TileToAdd->IsDisplayChanged = TRUE;

	LinkNode(Workspace->Tree, TileToAdd);

	// if it's the first one in the workspace then don't realloc
	if (IsFirstTile)
		Workspace->Tree->Root = TileToAdd;

	if (!Workspace->Tree->Root)
		FailWithCode("realloc Tiles failed\n");

	AddTileToWorkspace(Workspace->Tree, TileToAdd);
	RenderWorkspace(CurWk);
	CanSwitch = TRUE;

	LuaDispatchEx("on_new_window", (PVOID)WindowHandle);

}


TILE_INFO* VerifyExistingWindow(TILE_INFO* Tiles, HWND WindowHandle)
{
	for (TILE_INFO* Tile = Tiles; Tile; Tile = Tile->ChildTile)
	{
		if (Tile->BranchTile)
		{
			TILE_INFO* RecursiveTile = VerifyExistingWindow(Tile->BranchTile, WindowHandle);

			if (RecursiveTile)
				return RecursiveTile;
		}

		if (WindowHandle == Tile->WindowHandle)
			return Tile;
	}

	return NULL;

}

extern "C" __declspec(dllexport) VOID OnDestroyWindow(HWND WindowHandle)
{

	if (IsIgnoreWindow(WindowHandle))
		return;


	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
	TILE_INFO* TileToRemove = NULL;

	// If the tile doesn't exist in the workspace something wrong happened.

	if (Workspace->Tree)
		TileToRemove = VerifyExistingWindow(Workspace->Tree->Root, WindowHandle);

	if (!TileToRemove)
		return;

	//FailWithCode("Got tile to remove but couldn't find it");

	RemoveTileFromTree(Workspace, TileToRemove);
	RenderWorkspace(CurWk);

	RenderFocusWindowEx(Workspace);

	LuaDispatchEx("on_destroy_window", (PVOID)WindowHandle);

}


VOID CreateStatusButton(DISPLAY_INFO* Display, HWND ParentHandle, INT Slot, const char* ButtonText)
{

	INT ButtonStyle = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW;

	Display->StatusButton[Slot] = CreateWindowExA(
		NULL,
		"BUTTON",  // Predefined class; Unicode assumed 
		ButtonText,      // Button text 
		ButtonStyle,  // Styles 
		0,         // x position 
		0,         // y position 
		25,        // Button width
		25,        // Button height
		ParentHandle,     // Parent window
		NULL,       // No menu.
		NULL,
		NULL);      // Pointer not needed.

	if (!Display->StatusButton[Slot])
		FailWithCode("Couldn't Create Status Bar Button");

}

INT ButtonIdx = 1;
HBRUSH ButtonBrush;
HBRUSH ButtonMonBrush;
HBRUSH DefaultBrush;
CHAR GlobalButtonText[2] = { '?', '\0' };

LRESULT CALLBACK StatusBarMsgHandler(
	HWND WindowHandle,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{


	switch (Message)
	{
	case WM_CREATE:
	{
		LPCREATESTRUCTA CreateStruct = (LPCREATESTRUCTA)LParam;
		DISPLAY_INFO* Display = (DISPLAY_INFO*)CreateStruct->lpCreateParams;
		CreateStatusButton(Display, WindowHandle, ButtonIdx, GlobalButtonText);
		ButtonIdx++;
	}

		//IApplicationView* ApplicationView;
		//if (FAILED(ViewCollection->GetViewForHwnd(WindowHandle, &ApplicationView)))
		//	FailWithCode("StatusBar GetView");

		//PinnedApps->PinView(ApplicationView);
		//ApplicationView->Release();
		break;
	case WM_DRAWITEM:
	{
		PDRAWITEMSTRUCT DrawItem = (PDRAWITEMSTRUCT)LParam;
		BUTTON_STATE& ButtonState = ButtonMap[WindowHandle];

		if (ButtonState.IsActiveWorkspace)
		{
			DWORD ClrButton = ClrInMt;

			if (WindowHandle == PrimaryDisplay->BtnToColor)
				ClrButton = ClrActWk;

			SetBkColor(DrawItem->hDC, ClrButton);
			SetTextColor(DrawItem->hDC, ClrActTxt);
		}
		else
		{
			SetBkColor(DrawItem->hDC, ClrInActWk);
			SetTextColor(DrawItem->hDC, ClrInActTxt);
		}

		if (ButtonState.RenderTxt)
			DrawTextA(DrawItem->hDC, ButtonState.ButtonText, 1, &DrawItem->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		return TRUE;
		break;
	}
	case WM_CTLCOLORBTN:
	{
		BOOL IsActiveWindow = FALSE;
		BUTTON_STATE& ButtonState = ButtonMap[WindowHandle];

		if (ButtonState.IsActiveWorkspace)
		{
			if (WindowHandle == PrimaryDisplay->BtnToColor)
				return (LRESULT)ButtonBrush;
			return (LRESULT)ButtonMonBrush;
		}
		else
		{
			//Red Brush.
			return (LRESULT)DefaultBrush;
		}
	}
		break;
	default:
		return DefWindowProcA(WindowHandle, Message, WParam, LParam);
	}

	return 0;
}

LRESULT CALLBACK WindowMessageHandler2(
	HWND WindowHandle,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{
	return DefWindowProcA(WindowHandle, Message, WParam, LParam);
}

VOID OnTrayRightClick()
{
	HMENU PopupHandle = NULL;

	PopupHandle = CreatePopupMenu();

	InsertMenuW(PopupHandle, 0, MF_BYPOSITION, ID_EXIT, L"Exit");

	SetMenuDefaultItem(PopupHandle, ID_EXIT, 0);

	POINT CurPos;

	GetCursorPos(&CurPos);

	BOOL cmd = TrackPopupMenu(PopupHandle, TPM_LEFTALIGN | TPM_RIGHTBUTTON
		| TPM_RETURNCMD | TPM_NONOTIFY,
		CurPos.x, CurPos.y, 0, NotifWindow, NULL);

	SendMessage(NotifWindow, WM_COMMAND, cmd, 0);

	DestroyMenu(PopupHandle);


}


LRESULT CALLBACK NotifMessageHandler(
	HWND WindowHandle,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{

	switch (Message)
	{
	case WM_COMMAND:
		if (LOWORD(WParam) == ID_EXIT)
		{
			if (x86ProcessHandle)
				TerminateProcess(x86ProcessHandle, 0);
			TerminateProcess(GetCurrentProcess(), 0);
		}
		else
			return DefWindowProcA(WindowHandle, Message, WParam, LParam);
	case WM_APP:
		switch (LParam)
		{
		case WM_RBUTTONUP:
			OnTrayRightClick();
			return 0;
		default:
			return DefWindowProcA(WindowHandle, Message, WParam, LParam);
		}
		break;
	default:
		return DefWindowProcA(WindowHandle, Message, WParam, LParam);
	}

	return 0;

}

LRESULT CALLBACK WindowMessageHandler(
	HWND WindowHandle,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{
	return DefWindowProcA(WindowHandle, Message, WParam, LParam);
}

LRESULT CALLBACK FocusMessageHandler(
	HWND WindowHandle,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{

	WORKSPACE_INFO* Workspace;
	INT Width;
	INT Height;

	switch (Message)
	{
	case WM_TIMER:
	{
		UINT TimerId = (UINT)WParam;

		switch (TimerId)
		{
		case TIMER_FOCUS:

			Workspace = &WorkspaceList[CurWk];



			if (TreeHas(Focus))
			{

				RECT TileRect;

				GetWindowRect(Workspace->Tree->Focus->WindowHandle, &TileRect);

				//There is a fullscreen window
				//don't render focus bar
				if ((TileRect.bottom - TileRect.top) > RealScreenHeight)
					return 0;
			}

			RECT FocusRect;

			GetWindowRect(FocusWindow, &FocusRect);

			Width = FocusRect.right - FocusRect.left;
			Height = FocusRect.bottom - FocusRect.top;

			if (!SetWindowPos(FocusWindow, HWND_TOPMOST, FocusRect.left, FocusRect.top, Width, Height, 0))
				FailWithCode("SetWindowPos Focus Window");

			if (!SetWindowPos(FocusWindow, HWND_NOTOPMOST, FocusRect.left, FocusRect.top, Width, Height, 0))
				FailWithCode("SetWindowPos Focus Window");

			return 0;
		default:
			return DefWindowProcA(WindowHandle, Message, WParam, LParam);
		}

	}

	default:
		return DefWindowProcA(WindowHandle, Message, WParam, LParam);
	}
}

LRESULT CALLBACK NewWindowHookProc(int nCode, WPARAM WParam, LPARAM LParam)
{
	if (nCode < 0)
		return CallNextHookEx(KeyboardHook, nCode, WParam, LParam);

	if (nCode != HCBT_CREATEWND || nCode != HCBT_DESTROYWND)
		return CallNextHookEx(KeyboardHook, nCode, WParam, LParam);

	if (nCode == HCBT_CREATEWND)
	{
		logc(9, "NewWindow : %X\n", WParam);
		OnNewWindow((HWND)WParam);
	}

	if (nCode == HCBT_DESTROYWND)
		OnDestroyWindow((HWND)WParam);

	return CallNextHookEx(KeyboardHook, nCode, WParam, LParam);

}

VOID InstallNewWindowHook()
{

	WinHook64 = LoadLibraryA("winhook64.dll");

	if (!WinHook64)
		FailWithCode("Couldn't load WinHook64.dll");

	HOOKPROC NewWindowHookProc = (HOOKPROC)GetProcAddress(WinHook64, "OnWindowAction");

	if (!NewWindowHookProc)
		FailWithCode("Couldn't find (x64)NewWindowHookProc");

	NewWindowHook = SetWindowsHookExA(WH_SHELL, NewWindowHookProc, WinHook64, 0);

	logc(5, "NewWindowHook : %p\n", NewWindowHook);

	if (!NewWindowHook)
		FailWithCode("Couldn't set NewWindowHook");

}

VOID HandleWindowMessage(MSG* Message)
{

	HWND TargetWindow = (HWND)Message->lParam;

	//The WindowHandle is being destroyed
	if (Message->wParam)
	{
		logc(6, "DestroyWindow : %X\n", TargetWindow);
		OnDestroyWindow(TargetWindow);
	}
	else
	{
		logc(9, "NewWindow : %X\n", TargetWindow);
		OnNewWindow(TargetWindow);
	}
}

VOID CreateInitialWindow()
{
	WNDCLASSEXA WindowClass;

	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = NULL_STYLE;
	WindowClass.lpfnWndProc = WindowMessageHandler2;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = NULL;
	WindowClass.hIcon = NULL;
	WindowClass.hCursor = NULL;
	WindowClass.hbrBackground = NULL;
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = AppWindowName;
	WindowClass.hIconSm = NULL;

	WindowAtom = RegisterClassExA(&WindowClass);

	if (!WindowAtom)
		FailWithCode("Couldn't Register Window Class");

	MainWindow = CreateWindowExA(
		NULL_EX_STYLE,
		AppWindowName,
		"Win3wm",
		NULL_STYLE,
		0,
		0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		NULL,
		NULL,
		NULL,
		NULL
	);

	if (!MainWindow)
		FailWithCode("Could not create main window!");

	if (!ChangeWindowMessageFilterEx(MainWindow, WM_INSTALL_HOOKS, MSGFLT_ALLOW, NULL))
		FailWithCode("Change Window Perms");

}


LRESULT CALLBACK DebugMessageHandler(
	HWND WindowHandle,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{
	return DefWindowProcA(WindowHandle, Message, WParam, LParam);
}

VOID CreateDebugOverlay()
{

	return;

	WNDCLASSEXA WindowClass;

	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = NULL;
	WindowClass.lpfnWndProc = WindowMessageHandler;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = NULL;
	WindowClass.hIcon = NULL;
	WindowClass.hCursor = NULL;
	WindowClass.hbrBackground = CreateSolidBrush(RGB(0xff, 0, 0));
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = DebugAppWindowName;
	WindowClass.hIconSm = NULL;

	DebugWindowAtom = RegisterClassExA(&WindowClass);

	if (!DebugWindowAtom)
		FailWithCode("Couldn't Register DebugWindow Class");

	DebugWindow = CreateWindowExA(
		WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
		DebugAppWindowName,
		"FocusDebugOverlayWindow",
		0,
		0,
		0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		NULL,
		NULL,
		NULL,
		NULL
	);

	if (!DebugWindow)
		FailWithCode("DebugWindow creation failed");

	if (!SetLayeredWindowAttributes(DebugWindow, 0, 100, LWA_ALPHA))
		FailWithCode("SetWindowlayered attribs failed");


	ShowWindow(DebugWindow, SW_SHOW);
	SetWindowLongPtrA(DebugWindow, GWL_STYLE, 0);
	SetWindowLongPtrA(DebugWindow, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
	UpdateWindow(DebugWindow);




}

VOID CreateNotificationWindow()
{

	WNDCLASSEXA WindowClass;

	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = NULL;
	WindowClass.lpfnWndProc = NotifMessageHandler;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = NULL;
	WindowClass.hIcon = NULL;
	WindowClass.hCursor = NULL;
	WindowClass.hbrBackground = NULL;
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = NotifAppWindowName;
	WindowClass.hIconSm = NULL;

	NotifAtom = RegisterClassExA(&WindowClass);

	if (!NotifAtom)
		FailWithCode("Couldn't Register FocusWindow Class");

	NotifWindow = CreateWindowExA(
		0,
		NotifAppWindowName,
		"NotifWin3WM",
		0,
		0,
		0,
		0,
		0,
		NULL,
		NULL,
		NULL,
		NULL
	);

	if (!NotifWindow)
		FailWithCode("FocusWindow creation failed");

	ShowWindow(NotifWindow, SW_HIDE);

}

VOID CreateFocusOverlay()
{

	WNDCLASSEXA WindowClass;

	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = NULL;
	WindowClass.lpfnWndProc = FocusMessageHandler;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = NULL;
	WindowClass.hIcon = NULL;
	WindowClass.hCursor = NULL;
	WindowClass.hbrBackground = CreateSolidBrush(RGB(0xff, 0xff, 0));
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = FocusAppWindowName;
	WindowClass.hIconSm = NULL;

	FocusAtom = RegisterClassExA(&WindowClass);

	if (!FocusAtom)
		FailWithCode("Couldn't Register FocusWindow Class");

	FocusWindow = CreateWindowExA(
		WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
		FocusAppWindowName,
		"FocusWin3WM",
		0,
		0,
		0,
		0,
		0,
		NULL,
		NULL,
		NULL,
		NULL
	);

	if (!FocusWindow)
		FailWithCode("FocusWindow creation failed");

	SetWindowLongPtrA(FocusWindow, GWL_STYLE, WS_POPUP);
	SetWindowLongPtrA(FocusWindow, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
	ShowWindow(FocusWindow, SW_SHOW);
	UpdateWindow(FocusWindow);

	SetTimer(FocusWindow, TIMER_FOCUS, 750, NULL);

}

VOID InitScreenGlobals()
{

	for (auto& Display : DisplayList)
	{

		if (!Display.TrayWindow)
			Fail("Missing Tray Window");

		GetWindowRect(Display.TrayWindow, &Display.TrayRect);

		Display.ScreenWidth = Display.Rect.right - Display.Rect.left;
		Display.ScreenHeight = Display.Rect.bottom - Display.Rect.top;
		Display.ScreenHeight -= (Display.TrayRect.bottom - Display.TrayRect.top);

		Display.RealScreenWidth = Display.ScreenWidth;
		Display.RealScreenHeight = Display.ScreenHeight;

		if (IsGapsEnabled)
		{

			if (OuterGapsVertical > Display.RealScreenHeight)
				Fail("outer_gaps_vertical cannot be bigger than screen height");

			if (OuterGapsHorizontal > Display.RealScreenWidth)
				Fail("outer_gaps_horizontal cannot be bigger than screen height");

			if (InnerGapsVertical > Display.RealScreenHeight)
				Fail("inner_gaps_vertical cannot be bigger than screen height");

			if (InnerGapsHorizontal > Display.RealScreenWidth)
				Fail("inner_gaps_horizontal cannot be bigger than screen height");

			Display.HorizontalScalar = ((Display.Rect.right - Display.Rect.left) / (PrimaryDisplay->Rect.right - PrimaryDisplay->Rect.left));
			Display.VerticalScalar = ((Display.Rect.bottom - Display.Rect.top) / (PrimaryDisplay->Rect.bottom - PrimaryDisplay->Rect.top));

			Display.ScreenWidth -= (Display.HorizontalScalar * OuterGapsHorizontal);
			Display.ScreenHeight -= (Display.VerticalScalar * OuterGapsVertical);
		}
	}

	ClrActWk = RGB(ColorActiveWorkspaceButton[0],
		ColorActiveWorkspaceButton[1],
		ColorActiveWorkspaceButton[2]);

	ClrInActWk = RGB(ColorInactiveWorkspaceButton[0],
		ColorInactiveWorkspaceButton[1],
		ColorInactiveWorkspaceButton[2]);

	ClrInMt = RGB(ColorInactiveMonitorButton[0],
		ColorInactiveMonitorButton[1],
		ColorInactiveMonitorButton[2]);

	ClrActTxt = RGB(ColorActiveButtonText[0],
		ColorActiveButtonText[1],
		ColorActiveButtonText[2]);

	ClrInActTxt = RGB(ColorInActiveButtonText[0],
		ColorInActiveButtonText[1],
		ColorInActiveButtonText[2]);

	ButtonBrush = CreateSolidBrush(ClrActWk);
	ButtonMonBrush = CreateSolidBrush(ClrInMt);
	DefaultBrush = CreateSolidBrush(ClrInActWk);

	logc(9, "ScreenHeight: %lu - ScreenWidth : %lu\n", ScreenHeight, ScreenWidth);

}


VOID InitStatusBar()
{

	INT WorkspaceCount = GetActiveWorkspace();

	WNDCLASSEXA WindowClass;

	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = NULL_STYLE;
	WindowClass.lpfnWndProc = StatusBarMsgHandler;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = NULL;
	WindowClass.hIcon = NULL;
	WindowClass.hCursor = NULL;
	WindowClass.hbrBackground = CreateSolidBrush(RGB(0x22, 0x22, 0x22));
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = StatusBarWindowName;
	WindowClass.hIconSm = NULL;

	StatusBarAtom = RegisterClassExA(&WindowClass);

	for (int Idx = 0; Idx < DisplayList.size(); Idx++)
	{

		HWND SearchBarWindow;
		POINT StatusBarPosition;
		RECT SearchBarRect;
		DISPLAY_INFO* Display = &DisplayList[Idx];

		if (Display->Handle == PrimaryDisplay->Handle)
		{
			SearchBarWindow = FindWindowExW(TrayWindow, NULL, L"TrayDummySearchControl", NULL);
			assert(SearchBarWindow);
			GetWindowRect(SearchBarWindow, &SearchBarRect);
		}
		else
		{
			SearchBarWindow = Display->TrayWindow;
			assert(SearchBarWindow);
			GetWindowRect(SearchBarWindow, &SearchBarRect);

		}

		StatusBarPosition.x = SearchBarRect.left;
		StatusBarPosition.y = SearchBarRect.top;

		StatusBarPosition.x -= 25;
		StatusBarPosition.y -= 25;

		for (int i = 1; i < 10; i++)
		{

			GlobalButtonText[0] = '0' + i;

			Display->StatusBar[i] = CreateWindowExA(
				WS_EX_TOOLWINDOW,
				StatusBarWindowName,
				"Win3wmStatusBar",
				WS_POPUP,
				StatusBarPosition.x + i * 25,
				StatusBarPosition.y,
				25,
				25,
				NULL,
				NULL,
				NULL,
				(PVOID)Display
			);

			BUTTON_STATE& ButtonState = ButtonMap[Display->StatusBar[i]];

			ButtonState.ButtonText[0] = '0' + i;

			//INT What = ButtonMap[Display->StatusBar[i]].ButtonText[0];

			if (i == CurWk)
			{
				Display->BtnToColor = Display->StatusBar[CurWk];
				ButtonState.IsPrimaryDisplay = TRUE;
			}

			if (!Display->StatusBar[i])
				FailWithCode("Could not create Status Bar Window!");

			SetWindowLongPtrA(Display->StatusBar[i], GWL_EXSTYLE, WS_EX_TOOLWINDOW);
			SetWindowLongPtrA(Display->StatusBar[i], GWL_STYLE, WS_POPUP);
			SetWindowPos(Display->StatusBar[i], HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			UpdateWindow(Display->StatusBar[i]);

			if (i - 1 < WorkspaceCount)
			{
				ButtonState.RenderBg = TRUE;
				ButtonState.RenderTxt = TRUE;
				ShowWindow(Display->StatusBar[i], SW_SHOW);
			}
			else
			{
				ButtonState.RenderBg = FALSE;
				ButtonState.RenderTxt = FALSE;
				ShowWindow(Display->StatusBar[i], SW_HIDE);
			}
		}
	}
}

VOID DpiSet()
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
	//SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

VOID InitIcon()
{

	assert(NotifWindow);

	NOTIFYICONDATA NotifyData;
	RtlZeroMemory(&NotifyData, sizeof(NotifyData));

	NotifyData.cbSize = sizeof(NotifyData);
	NotifyData.hWnd = NotifWindow;
	NotifyData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	NotifyData.uID = 69;
	NotifyData.uCallbackMessage = WM_APP;

	HICON hIcon = (HICON)LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(TRAY_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
	//HICON hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1));
	//HICON hIcon = LoadIcon((HINSTANCE)GetWindowLong(NotifWindow, GWLP_HINSTANCE), MAKEINTRESOURCE(TRAY_ICON));

	NotifyData.hIcon = hIcon;

	if (!Shell_NotifyIcon(NIM_ADD, &NotifyData))
		FailWithCode("Shell_NotifyIconW");

}

namespace nlohmann
{

	bool exists(const json& j, const std::string& key)
	{
		return j.find(key) != j.end();
	}

}

VOID ParseToken(char* Token, PBOOL isShift)
{

	if (!strcmp(Token, "shift"))
	{
		*isShift = TRUE;
		return;
	}

}

char ParseHotkeyWord(const char* WordBinding)
{
	return KeyMap[WordBinding];
}

VOID ParseConfig(std::string HotkeyString, HOTKEY_FN Callback)
{

	BOOL isShift = strstr(HotkeyString.c_str(), "shift") != NULL;
	char Hotkey;

	if (isShift)
	{
		const char* PlusLoc = strstr(HotkeyString.c_str(), "+");

		if (!PlusLoc)
			Fail(MakeFormatString("Couldn't parse \"%s\"", HotkeyString.c_str()));

		if (!(*++PlusLoc))
			Fail("No key for config");

		const char* NextToken = PlusLoc;

		Hotkey = *PlusLoc;

		if (strlen(NextToken) != 1)
			Hotkey = ParseHotkeyWord(NextToken);

	}
	else
	{
		Hotkey = HotkeyString[0];

		if (!Hotkey)
			Fail("Empty string");

		if (strlen(HotkeyString.c_str()) != 1)
		{

			//Check for word bindings 
			Hotkey = ParseHotkeyWord(HotkeyString.c_str());

			if (!Hotkey)
				Fail(MakeFormatString("Failed to parse Config \"%s\"", HotkeyString.c_str()));

		}

	}

	if (islower(Hotkey))
		Hotkey = toupper(Hotkey);

	if (HotKeyCallbackTable[Hotkey][isShift].HotKeyCb)
		Fail(MakeFormatString("Duplicate Key Bindings for binding \"%s\"", HotkeyString.c_str()));

	HotKeyCallbackTable[Hotkey][isShift].HotKeyCb = Callback;

}

//Wrapper Callbacks for 
VOID DestroyTileEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!TreeHas(Focus))
		return;

	Workspace->Tree->IsFullScreen;

	SendMessage(Workspace->Tree->Focus->WindowHandle, WM_CLOSE, 0, 0);

	if (GetParent(Workspace->Tree->Focus->WindowHandle))
		OnDestroyWindow(Workspace->Tree->Focus->WindowHandle);

}

VOID SetLayoutVerticalEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
	IsPressed = TRUE;
	UserChosenLayout = HORIZONTAL_SPLIT;
	if (IsWorkspaceRootTile(Workspace->Tree))
		Workspace->Tree->Layout = UserChosenLayout;

}

ChangeWorkspaceEx(1);
ChangeWorkspaceEx(2);
ChangeWorkspaceEx(3);
ChangeWorkspaceEx(4);
ChangeWorkspaceEx(5);
ChangeWorkspaceEx(6);
ChangeWorkspaceEx(7);
ChangeWorkspaceEx(8);
ChangeWorkspaceEx(9);
MoveWorkspaceEx(1);
MoveWorkspaceEx(2);
MoveWorkspaceEx(3);
MoveWorkspaceEx(4);
MoveWorkspaceEx(5);
MoveWorkspaceEx(6);
MoveWorkspaceEx(7);
MoveWorkspaceEx(8);
MoveWorkspaceEx(9);

VOID ShutdownEx()
{

	PostThreadMessageA(GetCurrentThreadId(), WM_SHUTDOWN, 0, 0);
}

VOID VerifyWorkspaceRecursive(WORKSPACE_INFO* Workspace, TILE_INFO* Tile, std::vector<TILE_INFO*>& HandleList, std::unordered_map<HWND, TILE_INFO*>& DupeMap)
{

	TILE_INFO* ChildTile = NULL;

	for (; Tile; Tile = ChildTile)
	{

		ChildTile = Tile->ChildTile;

		if (Tile->BranchTile)
			VerifyWorkspaceRecursive(Workspace, Tile->BranchTile, HandleList, DupeMap);
		else if (!IsWindow(Tile->WindowHandle))
		{
			Workspace->Tree->Focus = UnlinkNode(Workspace->Tree, Tile);

			while (Workspace->Tree->Focus && Workspace->Tree->Focus->NodeType != TERMINAL)
				Workspace->Tree->Focus = Workspace->Tree->Focus->BranchTile;

			free(Tile);
			continue;
		}

		TILE_INFO* AlreadyExists = DupeMap[Tile->WindowHandle];

		if (AlreadyExists)
			HandleList.push_back(Tile);
	}

}

BOOL FlipRecursive(TILE_INFO* Tile)
{
	BOOL Changed = FALSE;

	for (; Tile; Tile = Tile->ChildTile)
	{
		if (Tile->BranchTile)
		{

			if (Tile->NodeType == VERTICAL_SPLIT)
				Tile->NodeType = HORIZONTAL_SPLIT;
			else
				Tile->NodeType = VERTICAL_SPLIT;

			Changed |= FlipRecursive(Tile->BranchTile);
		}
	}

	return Changed;
	
}

VOID FlipEx()
{

	BOOL Changed = FALSE;
	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!TreeHas(Root) || IsWorkspaceRootTile(Workspace->Tree))
		return;

	if (Workspace->Tree->Layout == HORIZONTAL_SPLIT)
		Workspace->Tree->Layout = VERTICAL_SPLIT;
	else
		Workspace->Tree->Layout = HORIZONTAL_SPLIT;

	for (TILE_INFO* Tile = Workspace->Tree->Root; Tile; Tile = Tile->ChildTile)
	{
		if (Tile->BranchTile)
		{

			if (Tile->NodeType == VERTICAL_SPLIT)
				Tile->NodeType = HORIZONTAL_SPLIT;
			else
				Tile->NodeType = VERTICAL_SPLIT;

			FlipRecursive(Tile->BranchTile);

		}
	}

	ResortTiles(Workspace->Tree);
	RenderWorkspace(CurWk);

}


VOID VerifyWorkspaceEx()
{
	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
	TILE_INFO* Tiles = Workspace->Tree->Root;

	std::unordered_map<HWND, TILE_INFO*> DupeMap;
	std::vector<TILE_INFO*> TileList;

 VerifyWorkspaceRecursive(Workspace, Tiles, TileList, DupeMap);

	for (auto DupeTile : TileList)
	{
			Workspace->Tree->Focus = UnlinkNode(Workspace->Tree, DupeTile);

			while (Workspace->Tree->Focus && Workspace->Tree->Focus->NodeType != TERMINAL)
				Workspace->Tree->Focus = Workspace->Tree->Focus->BranchTile;

			free(DupeTile);
	}

	ResortTiles(Workspace->Tree);
	RenderWorkspace(CurWk);

}

VOID GoFullscreenEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
	//Single tile no need to 
	if (Workspace->Tree->Root && !Workspace->Tree->Root->ChildTile && Workspace->Tree->Root->NodeType == TERMINAL)
		return;

	if (Workspace->Tree->Focus)
		Workspace->Tree->IsFullScreen = !Workspace->Tree->IsFullScreen;
	else
		Workspace->Tree->IsFullScreen = FALSE;

	if (Workspace->Tree->Focus && Workspace->FullScreenStyle)
	{
		SetWindowLongPtr(Workspace->Tree->Focus->WindowHandle, GWL_STYLE, Workspace->FullScreenStyle);
		Workspace->FullScreenStyle = NULL;
	}


	Workspace->Tree->NeedsRendering = TRUE;
	RenderWorkspace(CurWk);
	PVOID LuaHWND = (PVOID)(Workspace->Tree->Focus) ? Workspace->Tree->Focus->WindowHandle : NULL;
	LuaDispatchEx("on_fullscreen", Workspace->Tree->IsFullScreen, LuaHWND);

}

VOID SetLayoutHorizontalEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
	IsPressed = TRUE;
	UserChosenLayout = VERTICAL_SPLIT;
	if (IsWorkspaceRootTile(Workspace->Tree))
		Workspace->Tree->Layout = UserChosenLayout;

}

VOID HandleLeftEx()
{
	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!Workspace->Tree->IsFullScreen)
		HandleLeft(Workspace, FALSE);

}

VOID SwapLeftEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!Workspace->Tree->IsFullScreen)
		HandleLeft(Workspace, TRUE);

}

VOID HandleRightEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!Workspace->Tree->IsFullScreen)
		HandleRight(Workspace, FALSE);

}

VOID SwapRightEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!Workspace->Tree->IsFullScreen)
		HandleRight(Workspace, TRUE);
}


VOID HandleDownEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!Workspace->Tree->IsFullScreen)
		HandleBottom(Workspace, FALSE);

}

VOID SwapDownEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!Workspace->Tree->IsFullScreen)
		HandleBottom(Workspace, TRUE);

}


VOID HandleUpEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!Workspace->Tree->IsFullScreen)
		HandleTop(Workspace, FALSE);

}

VOID SwapUpEx()
{

	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];

	if (!Workspace->Tree->IsFullScreen)
		HandleTop(Workspace, TRUE);

}

VOID CreateTileEx()
{
	system(StartCommand);
}

VOID ParseModifier(std::string ModifierString)
{
	if (ModifierString != "alt" && ModifierString != "win")
		Fail("\"modifier\" is not \"alt\" or \"win\" in configs.json");

	if (ModifierString == "alt")
		ModKey = ALT;
	else
		ModKey = WIN;

}

VOID InitKeyMap()
{

	//oh boi keymap emplace
	KeyMap.emplace("left", VK_LEFT);
	KeyMap.emplace("right", VK_RIGHT);
	KeyMap.emplace("up", VK_UP);
	KeyMap.emplace("down", VK_DOWN);
	KeyMap.emplace("enter", VK_RETURN);

}

VOID ParseBoolOption(std::string UserInput, PBOOL Option, const char* OptionName)
{

	if (UserInput == "y")
		*Option = TRUE;
	else if (UserInput == "n")
		*Option = FALSE;
	else
		Fail(MakeFormatString("\"%s\" in configs.json can only be \"y\" or \"n\"", OptionName));


}


VOID MoveMonitorLeft(BOOL ShouldMove)
{
	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
	DISPLAY_INFO* Display = Workspace->Dsp;
	INT DispIdx = -1;

	for (int Idx = 0; Idx < DisplayList.size(); Idx++)
	{
		if (DisplayList[Idx].Handle == Display->Handle)
		{
			DispIdx = Idx;
			break;
		}
	}

	if (DispIdx == -1)
		Fail("Couldn't find a monitor for display??");

	if (!DispIdx)
		return;

	if (!ShouldMove)
	{
		Workspace->Dsp = &DisplayList[--DispIdx];
		Workspace->Tree = &Workspace->Trees[Workspace->Dsp->Handle];
		RenderFocusWindowEx(Workspace);
		return;
	}

	TILE_TREE* Tree = Workspace->Tree;
	TILE_TREE* TargetTree = &Workspace->Trees[DisplayList[--DispIdx].Handle];


	TILE_INFO* NewNode;
	TILE_INFO* Node = Tree->Focus;

	if (!Node)
		return;

	NewNode = (TILE_INFO*)RtlAlloc(sizeof(TILE_INFO));
	RtlZeroMemory(NewNode, sizeof(TILE_INFO));

	//Copy Over Important Stuff of the 
	NewNode->WindowHandle = Node->WindowHandle;
	NewNode->PreWMInfo = Node->PreWMInfo;
	NewNode->IsDisplayChanged = TRUE;

	RemoveTileFromTree(Workspace, Node);
	if (TreeHas(Root) && IsWorkspaceRootTile(Workspace->Tree))
		Workspace->Tree->IsFullScreen = FALSE;
	LinkNode(TargetTree, NewNode);
	AddTileToWorkspace(TargetTree, NewNode);
	RenderWorkspace(CurWk);

}

VOID MoveMonitorRight(BOOL ShouldMove)
{
	WORKSPACE_INFO* Workspace = &WorkspaceList[CurWk];
	DISPLAY_INFO* Display = Workspace->Dsp;
	INT DispIdx = -1;

	for (int Idx = 0; Idx < DisplayList.size(); Idx++)
	{
		if (DisplayList[Idx].Handle == Display->Handle)
		{
			DispIdx = Idx;
			break;
		}
	}

	if (DispIdx == -1)
		Fail("Couldn't find a monitor for display??");


	// if is last return, if is invalid then return
	if ((DispIdx + 1) >= DisplayList.size())
		return;

	if (!ShouldMove)
	{
		Workspace->Dsp = &DisplayList[++DispIdx];
		Workspace->Tree = &Workspace->Trees[Workspace->Dsp->Handle];
		RenderFocusWindowEx(Workspace);
		return;
	}

	TILE_TREE* Tree = Workspace->Tree;
	TILE_TREE* TargetTree = &Workspace->Trees[DisplayList[++DispIdx].Handle];


	TILE_INFO* NewNode;
	TILE_INFO* Node = Tree->Focus;

	if (!Node)
		return;

	NewNode = (TILE_INFO*)RtlAlloc(sizeof(TILE_INFO));
	RtlZeroMemory(NewNode, sizeof(TILE_INFO));

	//Copy Over Important Stuff of the 
	NewNode->WindowHandle = Node->WindowHandle;
	NewNode->PreWMInfo = Node->PreWMInfo;
	NewNode->IsDisplayChanged = TRUE;

	RemoveTileFromTree(Workspace, Node);
	if (TreeHas(Root) && IsWorkspaceRootTile(Workspace->Tree))
		Workspace->Tree->IsFullScreen = FALSE;
	LinkNode(TargetTree, NewNode);
	AddTileToWorkspace(TargetTree, NewNode);
	RenderWorkspace(CurWk);

}

VOID MoveMonitorLeftEx()
{
	MoveMonitorLeft(FALSE);
}

VOID MoveWindowMonitorLeftEx()
{
	MoveMonitorLeft(TRUE);
}

VOID MoveMonitorRightEx()
{
	MoveMonitorRight(FALSE);
}

VOID MoveWindowMonitorRightEx()
{
	MoveMonitorRight(TRUE);
}

VOID InitConfig()
{

	InitKeyMap();

	if (!PathFileExistsW(L"config.json"))
		FailWithCode("Couldn't find config.json");

	HANDLE ConfigHandle = CreateFileW(L"config.json",
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (ConfigHandle == INVALID_HANDLE_VALUE)
		FailWithCode("Config Json CreateFileW");

	PBYTE JsonBuffer = (PBYTE)RtlAlloc(16384);
	RtlZeroMemory(JsonBuffer, 16384);
	DWORD BytesRead;

	if (!ReadFile(ConfigHandle, JsonBuffer, 16384, &BytesRead, NULL))
		FailWithCode("Config Json ReadFile");

	json JsonParsed;

	try
	{
		JsonParsed = json::parse(JsonBuffer);
	}
	catch (json::parse_error& e)
	{
		Fail(MakeFormatString("Couldn't parse json - %s", e.what()));
	}


	const char* CurrentKey = NULL;

	auto GetJsonEx = [&CurrentKey, &JsonParsed](const char* JsonKey)
	{
		CurrentKey = JsonKey;
		return JsonParsed[CurrentKey];
	};

	try
	{

		ParseConfigEx("destroy_tile", DestroyTileEx);
		ParseConfigEx("create_tile", CreateTileEx);
		ParseConfigEx("set_layout_vertical", SetLayoutVerticalEx);
		ParseConfigEx("set_layout_horizontal", SetLayoutHorizontalEx);
		ParseConfigEx("change_workspace_1", ChangeWorkspaceEx_1);
		ParseConfigEx("change_workspace_2", ChangeWorkspaceEx_2);
		ParseConfigEx("change_workspace_3", ChangeWorkspaceEx_3);
		ParseConfigEx("change_workspace_4", ChangeWorkspaceEx_4);
		ParseConfigEx("change_workspace_5", ChangeWorkspaceEx_5);
		ParseConfigEx("change_workspace_6", ChangeWorkspaceEx_6);
		ParseConfigEx("change_workspace_7", ChangeWorkspaceEx_7);
		ParseConfigEx("change_workspace_8", ChangeWorkspaceEx_8);
		ParseConfigEx("change_workspace_9", ChangeWorkspaceEx_9);

		ParseConfigEx("focus_left", HandleLeftEx);
		ParseConfigEx("focus_right", HandleRightEx);
		ParseConfigEx("focus_up", HandleUpEx);
		ParseConfigEx("focus_down", HandleDownEx);

		ParseConfigEx("swap_left", SwapLeftEx);
		ParseConfigEx("swap_right", SwapRightEx);
		ParseConfigEx("swap_up", SwapUpEx);
		ParseConfigEx("swap_down", SwapDownEx);

		ParseConfigEx("move_workspace_1", MoveWorkspaceEx_1);
		ParseConfigEx("move_workspace_2", MoveWorkspaceEx_2);
		ParseConfigEx("move_workspace_3", MoveWorkspaceEx_3);
		ParseConfigEx("move_workspace_4", MoveWorkspaceEx_4);
		ParseConfigEx("move_workspace_5", MoveWorkspaceEx_5);
		ParseConfigEx("move_workspace_6", MoveWorkspaceEx_6);
		ParseConfigEx("move_workspace_7", MoveWorkspaceEx_7);
		ParseConfigEx("move_workspace_8", MoveWorkspaceEx_8);
		ParseConfigEx("move_workspace_9", MoveWorkspaceEx_9);

		ParseConfigEx("flip_workspace", FlipEx);
		ParseConfigEx("verify_workspace", VerifyWorkspaceEx);
		ParseConfigEx("fullscreen", GoFullscreenEx);
		ParseConfigEx("shutdown", ShutdownEx);

		ParseConfigEx("go_monitor_left", MoveMonitorLeftEx);
		ParseConfigEx("go_monitor_right", MoveMonitorRightEx);

		ParseConfigEx("move_monitor_left", MoveWindowMonitorLeftEx);
		ParseConfigEx("move_monitor_right", MoveWindowMonitorRightEx);

		ParseModifier(GetJsonEx("modifier"));

		CurrentKey = "windows_to_ignore";
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::vector<std::string> WindowsToIgnore = JsonParsed[CurrentKey];

		for (auto& Title : WindowsToIgnore)
		{
			std::wstring wide_string = converter.from_bytes(Title);
			Win32kDefaultWindowNames.push_back(wide_string);
		}

		std::string start_command = GetJsonEx("start_command");
		StartCommand = _strdup(start_command.c_str());

		ParseBoolOptionEx(GetJsonEx("adjust_for_nc"), &AdjustForNC);
		ParseBoolOptionEx(GetJsonEx("gaps_enabled"), &IsGapsEnabled);
		ParseBoolOptionEx(GetJsonEx("remove_titlebars_experimental"), &ShouldRemoveTitleBars);
		ParseBoolOptionEx(GetJsonEx("true_fullscreen"), &IsFullScreenMax);
		StartDirectory = _strdup(static_cast<std::string>(GetJsonEx("start_directory")).c_str());
		LuaScriptPath = _strdup(static_cast<std::string>(GetJsonEx("lua_script_path")).c_str());

		if (IsGapsEnabled)
		{
			OuterGapsVertical = GetJsonEx("outer_gaps_vertical");
			OuterGapsHorizontal = GetJsonEx("outer_gaps_horizontal");
			InnerGapsVertical = GetJsonEx("inner_gaps_vertical");
			InnerGapsHorizontal = GetJsonEx("inner_gaps_horizontal");
		}

		CurrentKey = "active_workspace_color_button";
		ColorActiveWorkspaceButton = JsonParsed[CurrentKey].get<std::vector<unsigned char>>();
		//ColorActiveWorkspaceButton = GetJsonEx("active_workspace_color_button");

		if (ColorActiveWorkspaceButton.size() != 3)
			Fail("active_workspace_color_button should be an array of 3 unsigned chars");

		CurrentKey = "inactive_workspace_color_button";
		ColorInactiveWorkspaceButton = JsonParsed[CurrentKey].get<std::vector<unsigned char>>();

		if (ColorInactiveWorkspaceButton.size() != 3)
			Fail("inactive_workspace_color_button should be an array of 3 unsigned chars");

		CurrentKey = "inactive_monitor_color_button";
		ColorInactiveMonitorButton = JsonParsed[CurrentKey].get<std::vector<unsigned char>>();;

		if (ColorInactiveMonitorButton.size() != 3)
			Fail("inactive_monitor_color_button should be an array of 3 unsigned chars");

		CurrentKey = "active_text_color_button";
		ColorActiveButtonText = JsonParsed[CurrentKey].get<std::vector<unsigned char>>();;

		if (ColorActiveButtonText.size() != 3)
			Fail("active_text_color_button should be an array of 3 unsigned chars");

		CurrentKey = "inactive_text_color_button";
		ColorInActiveButtonText = JsonParsed[CurrentKey].get<std::vector<unsigned char>>();;

		if (ColorInActiveButtonText.size() != 3)
			Fail("active_text_color_button should be an array of 3 unsigned chars");

	}
	catch (json::type_error& e)
	{
		Fail(MakeFormatString("Couldn't parse config.json - %s : %s", CurrentKey, e.what()));
	}

	free(JsonBuffer);

}

VOID InitConfigFree()
{

	HotKeyCallbackTable['Q'][TRUE].HotKeyCb = DestroyTileEx;
	HotKeyCallbackTable[VK_RETURN][FALSE].HotKeyCb = CreateTileEx;

	HotKeyCallbackTable['V'][FALSE].HotKeyCb = SetLayoutVerticalEx;
	HotKeyCallbackTable['H'][FALSE].HotKeyCb = SetLayoutHorizontalEx;

	HotKeyCallbackTable['1'][FALSE].HotKeyCb = ChangeWorkspaceEx_1;
	HotKeyCallbackTable['2'][FALSE].HotKeyCb = ChangeWorkspaceEx_2;

	HotKeyCallbackTable['1'][TRUE].HotKeyCb = MoveWorkspaceEx_1;
	HotKeyCallbackTable['2'][TRUE].HotKeyCb = MoveWorkspaceEx_2;

	HotKeyCallbackTable[VK_LEFT][FALSE].HotKeyCb = HandleLeftEx;
	HotKeyCallbackTable[VK_LEFT][TRUE].HotKeyCb = SwapLeftEx;
	HotKeyCallbackTable[VK_RIGHT][FALSE].HotKeyCb = HandleRightEx;
	HotKeyCallbackTable[VK_RIGHT][TRUE].HotKeyCb = SwapRightEx;
	HotKeyCallbackTable[VK_UP][FALSE].HotKeyCb = HandleUpEx;
	HotKeyCallbackTable[VK_UP][TRUE].HotKeyCb = SwapUpEx;
	HotKeyCallbackTable[VK_DOWN][FALSE].HotKeyCb = HandleDownEx;
	HotKeyCallbackTable[VK_DOWN][TRUE].HotKeyCb = SwapDownEx;
	HotKeyCallbackTable['R'][FALSE].HotKeyCb = VerifyWorkspaceEx;
	HotKeyCallbackTable['F'][FALSE].HotKeyCb = GoFullscreenEx;
	HotKeyCallbackTable['T'][FALSE].HotKeyCb = ShutdownEx;
}

BOOL VerifyLicense()
{

	WCHAR LicensePath[2048] = { 0 };

	PWSTR AppFolderPath;

	if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &AppFolderPath) != S_OK)
		FailWithCode("Error 55");

	wcscat(LicensePath, AppFolderPath);
	wcscat(LicensePath, L"\\a6js4r.txt");

	if (PathFileExistsW(LicensePath))
	{
		HANDLE LicenseFile = CreateFileW(LicensePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

		if (LicenseFile == INVALID_HANDLE_VALUE)
		{
			CloseHandle(LicenseFile);
			Fail("Error 54");
		}

		CHAR LicenseContent[40];
		DWORD BytesRead;

		if (!ReadFile(LicenseFile, LicenseContent, 40, &BytesRead, NULL))
		{
			CloseHandle(LicenseFile);
			FailWithCode("Error 53");
		}

		CloseHandle(LicenseFile);

		if (strstr(LicenseContent, "Win32kWindows"))
			return TRUE;

		return FALSE;
	}

	LPSTR result = SG_InputBox::GetString("WinWM", "Please Enter the WinWM License\nCopyright Ayaz Mammadov 2020", "");


	if (!strlen(result))
		TerminateProcess(GetCurrentProcess(), 369);

	if (strcmp(result, "athk3kf459idxz"))
		TerminateProcess(GetCurrentProcess(), 369);

	HANDLE LicenseFile = CreateFileW(LicensePath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);

	if (LicenseFile == INVALID_HANDLE_VALUE)
	{
		CloseHandle(LicenseFile);
		FailWithCode("Error 52");
	}

	DWORD BytesWritten;

	if (!WriteFile(LicenseFile, "Win32kWindows", sizeof("Win32kWindows"), &BytesWritten, NULL))
		FailWithCode("Error 52");

	CloseHandle(LicenseFile);

	return TRUE;

}

VOID SetPWD()
{

	if (strlen(StartDirectory))
		SetCurrentDirectory(StartDirectory);

}


VOID InitLua()
{
	// no lua script
	if (!strlen(LuaScriptPath))
		return;

	LuaOpt.On = TRUE;

	LuaOpt.State.open_libraries(lib::base,
		lib::bit32,
		lib::coroutine,
		lib::debug,
		lib::ffi,
		lib::io,
		lib::jit,
		lib::math,
		lib::os,
		lib::package,
		lib::string,
		lib::table,
		lib::utf8);

	try {
		LuaOpt.State.script_file(LuaScriptPath);
	}
	catch (const std::exception& LuaException)
	{
		Fail(LuaException.what());
	}

}

BOOL DisplayProc(HMONITOR DisplayHandle, HDC DC, LPRECT DisplayRect, LPARAM Context)
{
	DISPLAY_INFO Display = { 0 };
	Display.Handle = DisplayHandle;
	Display.Rect = *DisplayRect;
	DisplayList.push_back(Display);
	return TRUE;
}

BOOL CALLBACK TrayProc(HWND WindowHandle, LPARAM Param)
{
	WCHAR WindowText[1024] = { 0 };

	GetClassNameW(WindowHandle, WindowText, 1024);

	if (!wcscmp(WindowText, L"Shell_SecondaryTrayWnd"))
		SortTrays(WindowHandle);

	return TRUE;

}

VOID GetTrays()
{
	EnumWindows(TrayProc, NULL);

	TrayWindow = FindWindowA("Shell_TrayWnd", NULL);

	HWND ShellWindow = TrayWindow;

	if (!ShellWindow)
		FailWithCode("No Primary Tray Window Found");

	SortTrays(TrayWindow);

}

VOID GetMonitors()
{
	EnumDisplayMonitors(NULL, NULL, DisplayProc, NULL);
	HMONITOR PrimaryMonitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);

	for (int Idx = 0; Idx < DisplayList.size(); Idx++)
	{
		DISPLAY_INFO* Display = &DisplayList[Idx];

		for (auto& Workspace : WorkspaceList)
		{
			Workspace.Trees[Display->Handle].Display = Display;
			Workspace.Trees[Display->Handle].NeedsRendering = TRUE;
		}

		if (Display->Handle == PrimaryMonitor)
			PrimaryDisplay = Display;
	}

	if (!PrimaryDisplay)
		Fail("No Primary Display?");

	for (auto& Workspace : WorkspaceList)
	{
		Workspace.Tree = &Workspace.Trees[PrimaryDisplay->Handle];
		Workspace.Dsp = PrimaryDisplay;
	}
}


INT main()
{

	if (AppRunningCheck())
		Fail("Only a single instance of Win3m can be run");

	DpiSet();
	FreeConsole();
	SetCrashRoutine();
	ComOk(InitCom());
	InitWorkspaceList();
	GetMonitors();
	GetTrays();

#ifdef COMMERCIAL
	if (!VerifyLicense())
		TerminateProcess(GetCurrentProcess(), 3069);

	InitConfig();
	InitLua();
#else
	InitConfigFree();
#endif
	InitScreenGlobals();
	ComOk(RemoveOtherVDesktops());
	ComOk(SetupVDesktops(WorkspaceList));
	CreateInitialWindow();
	LoadNecessaryModules();
	CreateX86Process();
	GetOtherApplicationWindows();
	PerformInitialRegroupring();
	InstallNewWindowHook();
	InstallKeyboardHooks();
	CreateDebugOverlay();
	CreateNotificationWindow();
	InitIcon();
	CreateFocusOverlay();
	WorkspaceList[CurWk].Tree->NeedsRendering = TRUE;
	InitStatusBar();
	RenderWorkspace(CurWk);
	SetPWD();

	MSG Message;
	while (int RetVal = GetMessageA(&Message, NULL, 0, 0))
	{

		if (RetVal == -1)
		{
			PostQuitMessage(325);
			continue;
		}

		switch (Message.message)
		{
		case WM_SHUTDOWN:
			HandleShutdown();
			continue;
		case WM_SWITCH_DESKTOP:
			HandleSwitchDesktop(Message.wParam);
			continue;
		case WM_MOVE_TILE:
			ComOk(MoveWindowToVDesktop((HWND)Message.wParam, (IVirtualDesktop*)Message.lParam));
			continue;
		case WM_INSTALL_HOOKS:
			HandleWindowMessage(&Message);
			continue;
		default:
			break;
		}

		TranslateMessage(&Message);
		DispatchMessage(&Message);

	}

	return 0;

}



