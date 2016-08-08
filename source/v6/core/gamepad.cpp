/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <Windowsx.h>
#include <Xinput.h>
#include <v6/core/windows_end.h>

#include <v6/core/gamepad.h>
#include <v6/core/string.h>

#pragma comment( lib, "Xinput.lib" )

BEGIN_V6_NAMESPACE

#define XINPUT_BUTTONS_LLEFT	0x0004
#define XINPUT_BUTTONS_LRIGHT	0x0008
#define XINPUT_BUTTONS_LBOTTOM	0x0002
#define XINPUT_BUTTONS_LTOP		0x0001
#define XINPUT_BUTTONS_LINDEX	0x0100
#define XINPUT_BUTTONS_LSTICK	0x0040
#define XINPUT_BUTTONS_LOPTION	0x0020

#define XINPUT_BUTTONS_RLEFT	0x4000
#define XINPUT_BUTTONS_RRIGHT	0x2000
#define XINPUT_BUTTONS_RBOTTOM	0x1000
#define XINPUT_BUTTONS_RTOP		0x8000
#define XINPUT_BUTTONS_RINDEX	0x0200
#define XINPUT_BUTTONS_RSTICK	0x0080
#define XINPUT_BUTTONS_ROPTION	0x0010

static const u32	s_gamePadMaxCount = 1;
static XINPUT_STATE	s_states[s_gamePadMaxCount] = {};

void Gamepad_Init( Gamepad_s* gamepad, u32 index, void* owner )
{
	V6_ASSERT( index < s_gamePadMaxCount );

	memset( gamepad, 0, sizeof( *gamepad ) );
	gamepad->owner = owner;
	gamepad->index = 0;
}

void Gamepad_Release( Gamepad_s* gamepad )
{
	memset( gamepad, 0, sizeof( *gamepad ) );
}

bool Gamepad_UpdateState( Gamepad_s* gamepad )
{
	gamepad->leftButtonWasDown = gamepad->leftButtonIsDown;
	gamepad->rightButtonWasDown = gamepad->rightButtonIsDown;

	memset( &gamepad->leftButtonIsDown, 0, sizeof( gamepad->leftButtonIsDown ) );
	memset( &gamepad->rightButtonIsDown, 0, sizeof( gamepad->rightButtonIsDown ) );

	memset( &s_states[gamepad->index], 0, sizeof( XINPUT_STATE ) );

	if ( XInputGetState( gamepad->index, &s_states[gamepad->index] ) != ERROR_SUCCESS )
		return false;

	const XINPUT_GAMEPAD* inputState = &s_states[gamepad->index].Gamepad;

	gamepad->leftButtonIsDown.L = (inputState->wButtons & XINPUT_BUTTONS_LLEFT) != 0;
	gamepad->leftButtonIsDown.R = (inputState->wButtons & XINPUT_BUTTONS_LRIGHT) != 0;
	gamepad->leftButtonIsDown.B = (inputState->wButtons & XINPUT_BUTTONS_LBOTTOM) != 0;
	gamepad->leftButtonIsDown.T = (inputState->wButtons & XINPUT_BUTTONS_LTOP) != 0;
	gamepad->leftButtonIsDown.I = (inputState->wButtons & XINPUT_BUTTONS_LINDEX) != 0;
	gamepad->leftButtonIsDown.S = (inputState->wButtons & XINPUT_BUTTONS_LSTICK) != 0;
	gamepad->leftButtonIsDown.O = (inputState->wButtons & XINPUT_BUTTONS_LOPTION) != 0;

	gamepad->rightButtonIsDown.L = (inputState->wButtons & XINPUT_BUTTONS_RLEFT) != 0;
	gamepad->rightButtonIsDown.R = (inputState->wButtons & XINPUT_BUTTONS_RRIGHT) != 0;
	gamepad->rightButtonIsDown.B = (inputState->wButtons & XINPUT_BUTTONS_RBOTTOM) != 0;
	gamepad->rightButtonIsDown.T = (inputState->wButtons & XINPUT_BUTTONS_RTOP) != 0;
	gamepad->rightButtonIsDown.I = (inputState->wButtons & XINPUT_BUTTONS_RINDEX) != 0;
	gamepad->rightButtonIsDown.S = (inputState->wButtons & XINPUT_BUTTONS_RSTICK) != 0;
	gamepad->rightButtonIsDown.O = (inputState->wButtons & XINPUT_BUTTONS_ROPTION) != 0;

	if ( gamepad->onGamepadButtonEvent )
	{
		GamepadButtons_s leftButtonIsChanged;
		leftButtonIsChanged.bits = gamepad->leftButtonIsDown.bits ^ gamepad->leftButtonWasDown.bits;

		GamepadButtons_s rightButtonIsChanged;
		rightButtonIsChanged.bits = gamepad->rightButtonIsDown.bits ^ gamepad->rightButtonWasDown.bits;

		if ( leftButtonIsChanged.bits || rightButtonIsChanged.bits )
			gamepad->onGamepadButtonEvent( gamepad, leftButtonIsChanged, rightButtonIsChanged );
	}

	return true;
}

const char* Gamepad_DumpState( Gamepad_s* gamepad )
{
	const XINPUT_GAMEPAD* inputState = &s_states[gamepad->index].Gamepad;

	return String_Format(
		"Gamepad %d:\n"
		"  packet        : 0x%08X\n"
		"  wButtons      : 0x%04X\n"
		"  bLeftTrigger  : 0x%02X\n"
		"  bRightTrigger : 0x%02X\n"
		"  sThumbLX      : 0x%04X\n"
		"  sThumbLY      : 0x%04X\n"
		"  sThumbRX      : 0x%04X\n"
		"  sThumbRY      : 0x%04X\n",
		gamepad->index,
		s_states[gamepad->index].dwPacketNumber,
		inputState->wButtons,
		inputState->bLeftTrigger,
		inputState->bRightTrigger,
		inputState->sThumbLX,
		inputState->sThumbLY,
		inputState->sThumbRX,
		inputState->sThumbRY );
}

void Gamepad_RegisterButtonEvent( Gamepad_s* gamepad, OnGamepadButtonEvent_f onGamepadButtonEvent )
{
	gamepad->onGamepadButtonEvent = onGamepadButtonEvent;
}

END_V6_NAMESPACE
