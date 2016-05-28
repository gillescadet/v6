/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <Windowsx.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/codec/decoder.h>
#include <v6/core/memory.h>
#include <v6/core/string.h>
#include <v6/core/time.h>
#include <v6/core/win.h>
#include <v6/graphic/gpu.h>
#include <v6/graphic/scene.h>
#include <v6/graphic/trace.h>
#include <v6/graphic/view.h>
#include <v6/player/player_shared.h>

#pragma comment( lib, "d3d11.lib" )

#define V6_D3D_DEBUG			0

BEGIN_V6_NAMESPACE

extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_deviceContext;

static const float ZNEAR					= 10.0f;
static const float MOUSE_ROTATION_SPEED		= 0.5f;
static const float KEY_TRANSLATION_SPEED	= 200.0f;

enum
{
	CONSTANT_BUFFER_BASIC,

	CONSTANT_BUFFER_COUNT
};

enum
{
	SHADER_BASIC,

	SHADER_COUNT
};

struct FrameTimer_s
{
	u64						frameTickLast;
	float					fpsMax;
	float					dtMin;
	float					dtMax;
};

enum CommandAction_e
{
	COMMAND_ACTION_NONE,
	COMMAND_ACTION_LOAD_SEQUENCE,
	COMMAND_ACTION_UNLOAD_SEQUENCE,
};

struct CommandBuffer_s
{
	CommandAction_e			action;
	char					arg[256];
};

struct Player_s
{
	IAllocator*				heap;
	IStack*					stack;
	CommandBuffer_s			commandBuffer;
	Win_s					win;
	GPURenderTargetSet_s	mainRenderTargetSet;
	Camera_s				camera;
	Scene_s					scene;
	Sequence_s				sequence;
	TraceContext_s			traceContext;
	TraceFrameData_s		traceFrameData;
	u32						traceFrameID;

	// inputs
	bool					mousePressed;
	float					mouseDeltaX;
	float					mouseDeltaY;
	int						keyLeftPressed;
	int						keyRightPressed;
	int						keyUpPressed;
	int						keyDownPressed;
};

//----------------------------------------------------------------------------------------------------

static void FrameTimer_Init( FrameTimer_s* frameTimer, float fpsMax, float dtMax )
{
	frameTimer->frameTickLast = GetTickCount();
	frameTimer->fpsMax = Max( 1.0f, fpsMax );
	frameTimer->dtMin = 1.0f / frameTimer->fpsMax;
	frameTimer->dtMax = Max( frameTimer->dtMin, dtMax );
}

static float FrameTimer_ComputeInterval( FrameTimer_s* frameTimer )
{
	const u64 frameTick = GetTickCount();
	u64 frameUpdatedTick = frameTick;
	float dt = 0.0f;
	for (;;)
	{
		const u64 frameDelta = frameUpdatedTick - frameTimer->frameTickLast;
		dt = Min( ConvertTicksToSeconds( frameDelta ), frameTimer->dtMax );
//#if !V6_USE_HMD
		if ( dt + 0.0001f >= frameTimer->dtMin )
			break;
		SwitchToThread();
		frameUpdatedTick = GetTickCount();
//#endif // #if !V6_USE_HMD
	}
	frameTimer->frameTickLast = frameTick;

	return dt;
}

//----------------------------------------------------------------------------------------------------

static void PlayerMaterial_DrawBasic( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags )
{
	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	hlsl::CBBasic* cbBasic = (hlsl::CBBasic*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC] );

	Mat4x4 worlMatrix;
	Mat4x4_Scale( &worlMatrix, entity->scale );
	Mat4x4_SetTranslation( &worlMatrix, entity->pos );

	// use this order because one matrix is "from" local space and the other is "to" local space
	Mat4x4 objectToViewMatrix;
	Mat4x4_Mul( &objectToViewMatrix, view->viewMatrix, worlMatrix );
	
	cbBasic->c_basicObjectToView = objectToViewMatrix;
	cbBasic->c_basicViewToProj = view->projMatrix;

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC] );

	g_deviceContext->VSSetConstantBuffers( hlsl::CBBasicSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC].buf );
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &shaderContext->shaders[SHADER_BASIC];
	GPUMesh_Draw( mesh, 1, shader );
}

//----------------------------------------------------------------------------------------------------

static void PlayerScene_Create( Player_s* player )
{
	Camera_Create( &player->camera, &Vec3_Zero(), ZNEAR, DegToRad( 90.0f ), (float)player->mainRenderTargetSet.width / player->mainRenderTargetSet.height );
	
	Scene_Create( &player->scene );

	const u32 meshWireFrameBoxID = Scene_GetNewMeshID( &player->scene );
	GPUMesh_CreateBox( &player->scene.meshes[meshWireFrameBoxID], Color_Make( 255, 255, 255, 255 ), true );

	const u32 materialBasicID = Scene_GetNewMaterialID( &player->scene );
	Material_Create( &player->scene.materials[materialBasicID], PlayerMaterial_DrawBasic );
		
	const u32 mainBoxID = Scene_GetNewEntityID( &player->scene );
	Entity_Create( &player->scene.entities[mainBoxID], materialBasicID, meshWireFrameBoxID, Vec3_Make( 0.0f, 0.0f, 0.0f), ZNEAR * 2.0f );
}

static void PlayerScene_Release( Player_s* player )
{
	Scene_Release( &player->scene );
}

static void PlayerScene_Draw( Player_s* player, View_s* view )
{
	GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
	renderTargetSetBindingDesc.clear = true;
	renderTargetSetBindingDesc.useMSAA = true;

	GPURenderTargetSet_Bind( &player->mainRenderTargetSet, &renderTargetSetBindingDesc, 0 );

	Scene_Draw( &player->scene, view, 0 );

	GPURenderTargetSet_Unbind( &player->mainRenderTargetSet );
}

//----------------------------------------------------------------------------------------------------

static void PlayerDevice_Create( Player_s* player, u32 width, u32 height )
{
	bool debugDevice = false;
#if V6_D3D_DEBUG == 1
	debugDevice = true;
#endif
	GPUDevice_CreateWithSurfaceContext( width, height, player->win.hWnd, debugDevice );
	
	GPURenderTargetSetCreationDesc_s renderTargetSetCreationDesc = {};
	renderTargetSetCreationDesc.name = "main";
	renderTargetSetCreationDesc.width = width;
	renderTargetSetCreationDesc.height = height;
	renderTargetSetCreationDesc.supportMSAA = true;
	renderTargetSetCreationDesc.bindable = true;
	renderTargetSetCreationDesc.writable = true;

	GPURenderTargetSet_Create( &player->mainRenderTargetSet, &renderTargetSetCreationDesc );

	GPUShaderContext_CreateEmpty();

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	static_assert( CONSTANT_BUFFER_COUNT <= GPUShaderContext_s::CONSTANT_BUFFER_MAX_COUNT, "Out of constant buffer" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC], sizeof( hlsl::CBBasic ), "basic" );

	{
		ScopedStack scopedStack( player->stack );
		
		static_assert( SHADER_COUNT <= GPUShaderContext_s::SHADER_MAX_COUNT, "Out of shader" );
		GPUShader_Create( &shaderContext->shaders[SHADER_BASIC], "player_basic_vs.cso", "player_basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, player->stack );
	}
}

static void PlayerDevice_Release( Player_s* player )
{
	GPURenderTargetSet_Release( &player->mainRenderTargetSet );

	GPUDevice_Release();
}

//----------------------------------------------------------------------------------------------------

static bool PlayerSequence_Create( Player_s* player, const char* sequenceFilename )
{
	if ( !Sequence_Load( sequenceFilename, &player->sequence, player->heap, player->stack ) )
		return false;
	
	TraceDesc_s traceDesc = {};
	traceDesc.screenWidth = player->mainRenderTargetSet.width;
	traceDesc.screenHeight = player->mainRenderTargetSet.height;
	traceDesc.stereo = false;

	TraceContext_Create( &player->traceContext, &traceDesc, &player->sequence );

	player->traceFrameID = 0;

	return true;
}

static void PlayerSequence_Release( Player_s* player )
{
	TraceContext_Release( &player->traceContext );
	Sequence_Release( &player->sequence, player->heap );
	player->sequence.buffer = nullptr;
}

static bool PlayerSequence_IsValid( Player_s* player )
{
	return player->sequence.buffer != nullptr;
}

//----------------------------------------------------------------------------------------------------

static void PlayerCommandBuffer_Process( Player_s* player )
{
	switch ( player->commandBuffer.action )
	{
	case COMMAND_ACTION_LOAD_SEQUENCE:
		{
			if ( PlayerSequence_IsValid( player ) )
				PlayerSequence_Release( player );
			if ( !PlayerSequence_Create( player, player->commandBuffer.arg ) )
			{
				V6_ERROR( "Unable to load sequence %s\n", player->commandBuffer.arg );
			}
			else
			{
				player->camera.pos = player->sequence.frameDescArray[0].origin;
				player->camera.yaw = 0.0f;
				player->camera.pitch = 0.0f;
				V6_MSG( "Loaded sequence %s\n", player->commandBuffer.arg );
			}
		}
		break;
	case COMMAND_ACTION_UNLOAD_SEQUENCE:
		{
			if ( PlayerSequence_IsValid( player ) )
				PlayerSequence_Release( player );
		}
		break;
	}

	player->commandBuffer.action = COMMAND_ACTION_NONE;
}

//----------------------------------------------------------------------------------------------------

static void Player_OnKeyEvent( const KeyEvent_s* keyEvent )
{
	Player_s* player = (Player_s*)keyEvent->win->owner;

	switch( keyEvent->key )
	{
	case 0x1B:
		Win_Release( &player->win );
		break;
	case 'A': player->keyLeftPressed = keyEvent->pressed; break;
	case 'D': player->keyRightPressed = keyEvent->pressed; break;
	case 'L':
		{
			player->commandBuffer.action = COMMAND_ACTION_LOAD_SEQUENCE;
			//strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), "D:/media/obj/crytek-sponza/sponza.v6s" ); break;
			strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), "D:/tmp/v6/ue.v6s" ); break;
		}
		break;
	case 'S': player->keyDownPressed = keyEvent->pressed; break;
	case 'U':
		{
			player->commandBuffer.action = COMMAND_ACTION_UNLOAD_SEQUENCE;
		}
		break;
	case 'W': player->keyUpPressed = keyEvent->pressed; break;
	}
}

static void Player_OnMouseEvent( const MouseEvent_s* mouseEvent )
{
	Player_s* player = (Player_s*)mouseEvent->win->owner;

	if ( mouseEvent->rightButton == MOUSE_BUTTON_DOWN )
	{
		Win_CaptureMouse( &player->win );
		player->mousePressed = true;
	}
	else if ( mouseEvent->rightButton == MOUSE_BUTTON_UP )
	{
		Win_ReleaseMouse( &player->win );
		player->mousePressed = false;
	}

	if ( player->mousePressed )
	{
		player->mouseDeltaX += mouseEvent->deltaX;
		player->mouseDeltaY += mouseEvent->deltaY;
	}
}

static void Player_ProcessInputs( Player_s* player, float dt )
{
	{
		float mouseDeltaX = 0;
		float mouseDeltaY = 0;

		if ( player->mousePressed )
		{		
			mouseDeltaX = player->mouseDeltaX;
			mouseDeltaY = player->mouseDeltaY;
			player->mouseDeltaX = 0;
			player->mouseDeltaY = 0;
		}

		player->camera.yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
		player->camera.pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;
	}

	{
		int keyDeltaX = 0;
		int keyDeltaZ = 0;

		if ( player->keyLeftPressed != player->keyRightPressed )
			keyDeltaX = player->keyLeftPressed ? -1 : 1;

		if ( player->keyDownPressed != player->keyUpPressed )
			keyDeltaZ = player->keyDownPressed ? -1 : 1;

	
		if ( keyDeltaX || keyDeltaZ )
		{
			player->camera.pos += player->camera.right * (float)keyDeltaX * KEY_TRANSLATION_SPEED * dt;
			player->camera.pos += player->camera.forward * (float)keyDeltaZ * KEY_TRANSLATION_SPEED * dt;
		}
	}

	Camera_UpdateBasis( &player->camera );

}

static void Player_ProcessFrame( Player_s* player, u32 frameID, float dt )
{
	String_ResetInternalBuffer();
	
	PlayerCommandBuffer_Process( player );

	Player_ProcessInputs( player, dt );

	Camera_UpdateBasis( &player->camera );

	View_s view;
	Camera_MakeView( &player->camera, &view );

	if ( PlayerSequence_IsValid( player ) )
	{
		if ( player->traceFrameID == 0 )
		{
			TraceContext_UpdateFrame( &player->traceContext, &player->traceFrameData, player->traceFrameID, player->stack );
			++player->traceFrameID;
		}
		TraceOptions_s traceOptions = {};
		TraceContext_DrawFrame( &player->traceContext, &player->mainRenderTargetSet, &player->traceFrameData, &view, &traceOptions );
	}
	else
	{
		PlayerScene_Draw( player, &view );
	}

	GPUColorRenderTarget_Copy( &GPUSurfaceContext_Get()->surface, &player->mainRenderTargetSet.colorBuffers[0] );
	GPUSurfaceContext_Present();
}

static bool Player_Create( Player_s* player, u32 width, u32 height, IAllocator* heap, IStack* stack )
{
	memset( player, 0, sizeof( *player ) );

	if ( !Win_Create( &player->win, player, "V6 Player", 40, 40, width, height, true ) )
		return false;
	Win_RegisterKeyEvent( &player->win, Player_OnKeyEvent );
	Win_RegisterMouseEvent( &player->win, Player_OnMouseEvent );

	player->heap = heap;
	player->stack = stack;

	PlayerDevice_Create( player, width, height );

	PlayerScene_Create( player );

	return true;
}

static void Player_Release( Player_s* player )
{
	if ( PlayerSequence_IsValid( player ) )
		PlayerSequence_Release( player );
	PlayerScene_Release( player );
	PlayerDevice_Release( player );
}

END_V6_NAMESPACE

//----------------------------------------------------------------------------------------------------

int main()
{
	v6::CHeap heap;
	v6::Stack stack( &heap, 100 * 1024 * 1024 );

	v6::Player_s* player = stack.newInstance< v6::Player_s >();

	const v6::u32 width = 1024;
	const v6::u32 height = 1024;

	if ( !v6::Player_Create( player, width, height, &heap, &stack ) )
		return -1;
	v6::Win_Show( &player->win, true );
	
	v6::FrameTimer_s frameTimer;
	v6::FrameTimer_Init( &frameTimer, 100.0f, 0.1f );
	v6::u32 frameID = 0;
	while ( !Win_ProcessMessagesAndShouldQuit( &player->win ) )
	{
		const float dt = FrameTimer_ComputeInterval( &frameTimer );

		v6::Player_ProcessFrame( player, frameID, dt );
		++frameID;
	}

	v6::Player_Release( player );

	return 0;
}