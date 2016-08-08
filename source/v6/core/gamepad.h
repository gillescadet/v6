/*V6*/

#pragma once

#ifndef __V6_CORE_GAMEPAD_H__
#define __V6_CORE_GAMEPAD_H__

BEGIN_V6_NAMESPACE

struct GamepadButtons_s
{
	union
	{
		struct
		{
			u8 L : 1;
			u8 R : 1;
			u8 B : 1;
			u8 T : 1;
			u8 I : 1;
			u8 S : 1;
			u8 O : 1;
		};
		u32 bits;
	};
};

struct Gamepad_s;

typedef void (*OnGamepadButtonEvent_f)( const Gamepad_s* gamepad, GamepadButtons_s leftButtonIsChanged, GamepadButtons_s rightButtonIsChanged );

struct Gamepad_s
{
	void*					owner;
	OnGamepadButtonEvent_f	onGamepadButtonEvent;
	u32						index;

	GamepadButtons_s		leftButtonIsDown;
	GamepadButtons_s		leftButtonWasDown;
	GamepadButtons_s		rightButtonIsDown;
	GamepadButtons_s		rightButtonWasDown;
};

const char* Gamepad_DumpState( Gamepad_s* gamepad );
void		Gamepad_Init( Gamepad_s* gamepad, u32 index, void* owner );
void		Gamepad_RegisterButtonEvent( Gamepad_s* gamepad, OnGamepadButtonEvent_f onGamepadButtonEvent );
void		Gamepad_Release( Gamepad_s* gamepad );
bool		Gamepad_UpdateState( Gamepad_s* gamepad );

END_V6_NAMESPACE

#endif // __V6_CORE_GAMEPAD_H__
