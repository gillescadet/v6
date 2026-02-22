/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <Windowsx.h>
#include <v6/core/windows_end.h>

#include <v6/core/time.h>
#include <v6/core/win.h>

#define V6_EMULATE_FULLSCREEN 0

BEGIN_V6_NAMESPACE

static const u32 WINDOW_MAX_COUNT = 1;

Win_s*	s_windowToRegister = nullptr;
Win_s*	s_windows[WINDOW_MAX_COUNT] = {};
u32		s_windowCount = 0;

static Win_s* Win_FindWindowByHandle( HWND hWnd )
{
	for ( u32 winID = 0; winID < s_windowCount; ++winID )
	{
		if ( (HWND)s_windows[winID]->hWnd == hWnd )
			return s_windows[winID];
	}

	return nullptr;
}

void Win_CaptureMouse( Win_s* win )
{
	POINT cursorPos;
	GetCursorPos( &cursorPos );
	win->mouseCursorPos.x = cursorPos.x;
	win->mouseCursorPos.y = cursorPos.y;

	SetCapture( (HWND)win->hWnd ) ;
	
	ShowCursor( false );
	
	win->mouseCaptured = true;
}

void Win_ReleaseMouse( Win_s* win )
{
	if ( win->mouseCaptured )
	{
		SetCursorPos( win->mouseCursorPos.x, win->mouseCursorPos.y );
		
		ShowCursor( true );
		
		ReleaseCapture();
		
		win->mouseCaptured = false;
	}
}

static LRESULT CALLBACK Win_Proc( HWND hWnd, u32 message, WPARAM wParam, LPARAM lParam )
{
	Win_s* win = Win_FindWindowByHandle( hWnd );

	switch ( message )
	{
	case WM_ACTIVATE:
	case WM_KILLFOCUS:
	case WM_SETFOCUS:
		{
			V6_ASSERT( win ); 
			Win_ReleaseMouse( win );
		}
		break;

	case WM_CREATE:
		{
			V6_ASSERT( s_windowToRegister );
			V6_ASSERT( s_windowCount < WINDOW_MAX_COUNT );
			s_windowToRegister->hWnd = hWnd;
			s_windows[s_windowCount] = s_windowToRegister;
			++s_windowCount;
			s_windowToRegister = nullptr;
		}
		break;

	case WM_DESTROY:
		{
			V6_ASSERT( win ); 
			for ( u32 winID = 0; winID < s_windowCount; ++winID )
			{
				if ( s_windows[winID] == win )
				{
					s_windows[winID] = s_windows[s_windowCount-1];
					--s_windowCount;
					break;
				}
			}
			win->hWnd = 0;
			if ( win->isMain )
				PostQuitMessage( 0 );
		}
		break;

	case WM_INPUT: 
		{
			V6_ASSERT( win );

			u32 dwSize;
			GetRawInputData( (HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
		
			LPBYTE lpb[4096];
			V6_ASSERT( dwSize <= sizeof( lpb ) );
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER) );

			RAWINPUT* raw = (RAWINPUT*)lpb;

			if ( raw->header.dwType == RIM_TYPEKEYBOARD ) 
			{
				if ( win->onKeyEvent )
				{
					KeyEvent_s keyEvent = { win };

					keyEvent.key = raw->data.keyboard.VKey & 0xFF;
					keyEvent.pressed = raw->data.keyboard.Message == 0x100;
					
					win->onKeyEvent( &keyEvent );

#if 0
					V6_DEVMSG( "Kbd: make=%04x Flags:%04x Reserved:%04x ExtraInformation:%08x, msg=%04x VK=%04x\n",
						raw->data.keyboard.MakeCode, 
						raw->data.keyboard.Flags, 
						raw->data.keyboard.Reserved, 
						raw->data.keyboard.ExtraInformation, 
						raw->data.keyboard.Message, 
						raw->data.keyboard.VKey );
#endif
				}
			}
			else if ( raw->header.dwType == RIM_TYPEMOUSE ) 
			{
				if ( win->onMouseEvent )
				{
					MouseEvent_s mouseEvent = { win };
				
					POINT cursorPos;
					GetCursorPos( &cursorPos );
					ScreenToClient( (HWND)win->hWnd, &cursorPos );
					mouseEvent.posX = (int)cursorPos.x; 
					mouseEvent.posY = (int)cursorPos.y;

					mouseEvent.deltaX = raw->data.mouse.lLastX; 
					mouseEvent.deltaY = raw->data.mouse.lLastY;

					if ( (raw->data.mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_DOWN) != 0 )
					{
						const u64 clickTime = Tick_GetCount();
						if ( Abs( win->mouseClickPos.x - mouseEvent.posX ) <= 1 && Abs( win->mouseClickPos.y - mouseEvent.posY ) <= 1 && Tick_ConvertToSeconds( clickTime - win->mouseClickTime ) < 0.5f )
						{
							win->mouseClickPos.x = 0;
							win->mouseClickPos.y = 0;
							win->mouseClickTime = 0;
							mouseEvent.leftButton = MOUSE_BUTTON_DOUBLE_CLICK;
						}
						else
						{
							win->mouseClickPos.x = mouseEvent.posX;
							win->mouseClickPos.y = mouseEvent.posY;
							win->mouseClickTime = clickTime;
							mouseEvent.leftButton = MOUSE_BUTTON_DOWN;
						}
					}
					else if ( (raw->data.mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_UP) != 0 )
						mouseEvent.leftButton = MOUSE_BUTTON_UP;

					if ( (raw->data.mouse.ulButtons & RI_MOUSE_RIGHT_BUTTON_DOWN) != 0 )
						mouseEvent.rightButton = MOUSE_BUTTON_DOWN;
					else if ( (raw->data.mouse.ulButtons & RI_MOUSE_RIGHT_BUTTON_UP) != 0 )
						mouseEvent.rightButton = MOUSE_BUTTON_UP;

					if ( (raw->data.mouse.ulButtons & RI_MOUSE_WHEEL) != 0 )
						mouseEvent.deltaWheel = (short)raw->data.mouse.usButtonData;

					win->onMouseEvent( &mouseEvent );

#if 0
					V6_DEVMSG( "Mouse: usFlags=%04x ulButtons=%04x usButtonFlags=%04x usButtonData=%04x ulRawButtons=%04x lLastX=%04x lLastY=%04x ulExtraInformation=%04x\n",
						raw->data.mouse.usFlags, 
						raw->data.mouse.ulButtons, 
						raw->data.mouse.usButtonFlags, 
						raw->data.mouse.usButtonData, 
						raw->data.mouse.ulRawButtons, 
						raw->data.mouse.lLastX, 
						raw->data.mouse.lLastY, 
						raw->data.mouse.ulExtraInformation );
#endif
				}
			}
		}
		break;
	
	case WM_GETMINMAXINFO:
		{
			MINMAXINFO* minMaxInfo = (MINMAXINFO*)lParam;
			minMaxInfo->ptMinTrackSize.x = (long)800;
			minMaxInfo->ptMinTrackSize.y = (long)600;
		}
		return 0;

	case WM_SIZE:
		if ( wParam != SIZE_MINIMIZED && win->onWindowResized )
		{
			win->size.x = LOWORD( lParam );
			win->size.y = HIWORD( lParam );
			win->onWindowResized( win );
		}
		break;

	default:
		return DefWindowProcA( hWnd, message, wParam, lParam );
	}

	return 0;
}

bool Win_Create( Win_s* win, void* owner, const char* title, int x, int y, int width, int height, u32 winFlags )
{
	if ( s_windowCount == WINDOW_MAX_COUNT )
	{
		V6_ERROR( "Too much windows: %d!\n", s_windowCount );
		return false;
	}

	memset( win, 0, sizeof( *win ) );

	WNDCLASSEXA wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = Win_Proc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = nullptr;
	wcex.hIcon = nullptr;
	wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
	wcex.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = "v6";
	wcex.hIconSm = nullptr;

	if ( !RegisterClassExA(&wcex) )
	{
		V6_ERROR( "Call to RegisterClassEx failed!\n" );
		return false;
	}

#if V6_EMULATE_FULLSCREEN  == 1
	const int style = WS_POPUP;
#else
	int style = WS_POPUP | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	if ( winFlags & WIN_FLAG_RESIZABLE )
		style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
#endif
		
	RECT rect = { 0, 0, width, height };
	AdjustWindowRect( &rect, style, false );
	const Vec2u dim = Vec2u_Make( rect.right - rect.left, rect.bottom - rect.top );

	win->owner = owner;
	win->size = Vec2i_Make( width, height );
	win->isMain = (winFlags & WIN_FLAG_IS_MAIN) != 0;
	s_windowToRegister = win;

	HWND hWnd = CreateWindowA(
		"v6",
		title,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		dim.x, dim.y,
		nullptr,
		nullptr,
		nullptr,
		nullptr
		);

	if ( x == -1 )
	{
		const u32 monitorW = GetSystemMetrics( SM_CXSCREEN );
		x = (monitorW - dim.x) / 2;
	}
	else
	{
		x -= dim.x - width;
	}

	if ( y == -1 )
	{
		const u32 monitorH = GetSystemMetrics( SM_CYSCREEN );
		y = (monitorH - dim.y) / 2;
	}
	else
	{
		y -= dim.y - height;
	}
	
	V6_ASSERT( win->hWnd == hWnd );
	SetWindowPos( hWnd, nullptr, x, y, dim.x, dim.y, 0 );

	RECT r;
	GetClientRect( hWnd, &r );
	V6_MSG( "win.resolution: %dx%d\n", r.right - r.left, r.bottom - r.top );

	RAWINPUTDEVICE rid[2];

	rid[0].usUsagePage = 0x01; 
	rid[0].usUsage = 0x02; 
	//rid[0].dwFlags = RIDEV_NOLEGACY;   // adds HID mouse and also ignores legacy mouse messages
	rid[0].dwFlags = 0;
	rid[0].hwndTarget = hWnd;

	rid[1].usUsagePage = 0x01; 
	rid[1].usUsage = 0x06; 
	rid[1].dwFlags = RIDEV_NOLEGACY;   // adds HID keyboard and also ignores legacy keyboard messages
	rid[1].hwndTarget = hWnd;

	if ( RegisterRawInputDevices( rid, 2, sizeof( rid[0] ) ) == false )
	{
		V6_ERROR( "Call to RegisterRawInputDevices failed!\n" );
		return false;
	}

	V6_MSG( "Window created\n" );

	return true;
}

void Win_Release( Win_s* win )
{
	if ( win->hWnd )
		DestroyWindow( (HWND)win->hWnd );
}

void Win_RegisterKeyEvent( Win_s* win, OnKeyEvent_f onKeyEvent )
{
	win->onKeyEvent = onKeyEvent;
}

void Win_RegisterMouseEvent( Win_s* win, OnMouseEvent_f onMouseEvent )
{
	win->onMouseEvent = onMouseEvent;
}

void Win_RegisterResizeEvent( Win_s* win, OnWindowResized_f onWindowResized )
{
	win->onWindowResized = onWindowResized;
}

void Win_Show( Win_s* win, bool show )
{
#if V6_EMULATE_FULLSCREEN  == 1
	ShowWindow( (HWND)win->hWnd, show ? SW_SHOWMAXIMIZED : SW_HIDE );
#else
	ShowWindow( (HWND)win->hWnd, show ? SW_SHOWNORMAL : SW_HIDE );
#endif
	if ( show )
		UpdateWindow( (HWND)win->hWnd );
}

bool Win_ProcessMessagesAndShouldQuit( Win_s* win )
{
	MSG msg;
	while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		TranslateMessage( &msg );
		DispatchMessageA( &msg );

		if ( msg.message == WM_QUIT )
			return true;
	}

	return false;
}

void Win_SetTitle( Win_s* win, const char* title )
{
	SetWindowTextA( (HWND)win->hWnd, title );
}

void Win_ShowWaitCursor( Win_s* win, bool show )
{
	SetCursor( LoadCursor( nullptr, show ? IDC_ARROW : IDC_WAIT) );
}

void Win_ShowMessage( Win_s* win, const char* str, const char* title )
{
	MessageBoxA( (HWND)win->hWnd, str, title, MB_OK );
}

void Win_Terminate( Win_s* win )
{
	ExitProcess( 1 );
}

END_V6_NAMESPACE
