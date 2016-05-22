/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <Windowsx.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/core/win.h>
#include <v6/graphic/gpu.h>

#pragma comment( lib, "d3d11.lib" )

BEGIN_V6_NAMESPACE

struct Player_s
{
	Win_s				win;
};

Player_s s_player;

static void Player_OnKeyEvent( const KeyEvent_s* keyEvent )
{
	switch( keyEvent->key )
	{
	case 0x1B:
		Win_Release( &s_player.win );
		break;
	}
}

static void Player_ProcessFrame( u32 )
{
	GPUSurfaceContext_Present();
}

END_V6_NAMESPACE

int main()
{
	const v6::u32 width = 1280;
	const v6::u32 height = 720;

	if ( !v6::Win_Create( &v6::s_player.win, "V6 Player", 40, 40, width, height, true ) )
		return 1;

	v6::GPUDevice_CreateWithSurfaceContext( width, height, v6::s_player.win.hWnd, false );

	v6::Win_Show( &v6::s_player.win, true );
	v6::Win_RegisterKeyEvent( &v6::s_player.win, v6::Player_OnKeyEvent );
	
	v6::u32 frameID = 0;
	while ( !Win_ProcessMessagesAndShouldQuit( &v6::s_player.win ) )
	{
		v6::Player_ProcessFrame( frameID );
		++frameID;
	}

	v6::GPUDevice_Release();

	return 0;
}