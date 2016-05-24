/*V6*/

#pragma once

#ifndef __V6_CORE_WIN_H__
#define __V6_CORE_WIN_H__

#include <v6/core/vec2i.h>

BEGIN_V6_NAMESPACE

struct Win_s;

struct KeyEvent_s
{
	Win_s*	win;
	u8		key;
	bool	pressed;
};

enum MouseButtonEvent_e
{
	MOUSE_BUTTON_NONE,
	MOUSE_BUTTON_DOWN,
	MOUSE_BUTTON_UP,
};

struct MouseEvent_s
{
	Win_s*				win;
	MouseButtonEvent_e	leftButton;
	MouseButtonEvent_e	rightButton;
	int					deltaX;
	int					deltaY;
};

typedef void (*OnKeyEvent_f)( const KeyEvent_s* keyEvent );
typedef void (*OnMouseEvent_f)( const MouseEvent_s* mouseEvent );

struct Win_s
{
	void*			owner;
	void*			hWnd;
	OnKeyEvent_f	onKeyEvent;
	OnMouseEvent_f	onMouseEvent;
	bool			isMain;
	Vec2i			mouseCursorPos;
	bool			mouseCaptured;
};

void Win_CaptureMouse( Win_s* win );
bool Win_Create( Win_s* win, void* owner, const char* title, int x, int y, int width, int height, bool isMain );
bool Win_ProcessMessagesAndShouldQuit( Win_s* win );
void Win_RegisterKeyEvent( Win_s* win, OnKeyEvent_f onKeyEvent );
void Win_RegisterMouseEvent( Win_s* win, OnMouseEvent_f onMouseEvent );
void Win_Release( Win_s* win );
void Win_ReleaseMouse( Win_s* win );
void Win_SetTitle( Win_s* win, const char* title );
void Win_Show( Win_s* win, bool show );

END_V6_NAMESPACE

#endif // __V6_CORE_WIN_H__
