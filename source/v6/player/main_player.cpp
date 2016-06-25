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
#include <v6/graphic/font.h>
#include <v6/graphic/gpu.h>
#include <v6/graphic/scene.h>
#include <v6/graphic/trace.h>
#include <v6/graphic/view.h>
#include <v6/player/player_shared.h>

#pragma comment( lib, "d3d11.lib" )

#define V6_D3D_DEBUG			1

BEGIN_V6_NAMESPACE

static const GPUEventID_t s_gpuEventDraw		= GPUEvent_Register( "Draw", true );
static const GPUEventID_t s_gpuEventCopy		= GPUEvent_Register( "Copy", true );
static const GPUEventID_t s_gpuEventPresent		= GPUEvent_Register( "Present", true );

extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_deviceContext;

static const u32	PLAY_RATE				= 75; 
static const float	ZNEAR					= 10.0f;
static const float	MOUSE_ROTATION_SPEED	= 0.5f;
static const float	KEY_TRANSLATION_SPEED	= 200.0f;

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

struct FrameInfo_s
{
	u32		frameID;
	float	dt;
	float	averageFPS;
};

struct FrameTimer_s
{
	u32		frameID;
	u64		frameTickLast;
	float	fpsMax;
	float	dtMin;
	float	dtMax;
	u64		frameDurations[32];
	u64		frameDurationSum;
};

enum CommandAction_e
{
	COMMAND_ACTION_NONE,
	COMMAND_ACTION_COMMAND_LINE,
	COMMAND_ACTION_LOAD_STREAM,
	COMMAND_ACTION_UNLOAD_STREAM,
	COMMAND_ACTION_PLAY,
	COMMAND_ACTION_BEGIN_FRAME,
	COMMAND_ACTION_END_FRAME,
	COMMAND_ACTION_PREV_FRAME,
	COMMAND_ACTION_NEXT_FRAME,
	COMMAND_ACTION_TRACE_OPTION_BUCKET,
	COMMAND_ACTION_TRACE_OPTION_LOG,
	COMMAND_ACTION_TRACE_OPTION_OVERDRAW,
	COMMAND_ACTION_TRACE_OPTION_MIP,
	COMMAND_ACTION_TRACE_OPTION_TSAA,
	COMMAND_ACTION_UI,
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
	VideoStream_s			stream;
	FontContext_s			fontContext;
	TraceContext_s			traceContext;
	TraceOptions_s			traceOptions;
	bool					hideUI;
	float					curFrameID;
	u32						targetFrameID;

	// inputs
	bool					mousePressed;
	float					mouseDeltaX;
	float					mouseDeltaY;
	int						keyLeftPressed;
	int						keyRightPressed;
	int						keyUpPressed;
	int						keyDownPressed;
	char					commandLine[256];
	u32						commandLineSize;
};

//----------------------------------------------------------------------------------------------------

static u32 CheckMemory( const u8* p, u8 val, u32 size )
{
	for ( u32 i = 0; i < size; ++i )
	{
		if ( p[i] != val )
			return i;
	}

	return size;
}

//----------------------------------------------------------------------------------------------------

static void FrameTimer_Init( FrameTimer_s* frameTimer, float fpsMax, float dtMax )
{
	frameTimer->frameTickLast = GetTickCount();
	frameTimer->fpsMax = Max( 1.0f, fpsMax );
	frameTimer->dtMin = 1.0f / frameTimer->fpsMax;
	frameTimer->dtMax = Max( frameTimer->dtMin, dtMax );
	memset( frameTimer->frameDurations, 0, sizeof( frameTimer->frameDurations ) );
	frameTimer->frameDurationSum = 0;
}

static void FrameTimer_ComputeNewFrameInfo( FrameTimer_s* frameTimer, FrameInfo_s* frameInfo )
{
	frameInfo->dt = 0.0f;
	frameInfo->frameID = frameTimer->frameID;

	const u64 frameTick = GetTickCount();
	u64 frameUpdatedTick = frameTick;
	u64 frameDelta;
	for (;;)
	{
		frameDelta = frameUpdatedTick - frameTimer->frameTickLast;
		frameInfo->dt = Min( ConvertTicksToSeconds( frameDelta ), frameTimer->dtMax );
//#if !V6_USE_HMD
		if ( frameInfo->dt + 0.0001f >= frameTimer->dtMin )
			break;
		SwitchToThread();
		frameUpdatedTick = GetTickCount();
//#endif // #if !V6_USE_HMD
	}
	frameTimer->frameTickLast = frameUpdatedTick;
	
	const u32 frameDurationRank = frameTimer->frameID % 32;
	frameTimer->frameDurationSum -= frameTimer->frameDurations[frameDurationRank];
	frameTimer->frameDurationSum += frameDelta;
	frameTimer->frameDurations[frameDurationRank] = frameDelta;
	
	const float avgFrameDuration = ConvertTicksToSeconds( frameTimer->frameDurationSum ) / 32.0f;
	frameInfo->averageFPS = avgFrameDuration < FLT_EPSILON ? 0.0f : 1.0f / avgFrameDuration; 

	++frameTimer->frameID;
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
	Camera_Create( &player->camera, &Vec3_Zero(), ZNEAR, DegToRad( 90.0f ), (float)player->mainRenderTargetSet.width / player->mainRenderTargetSet.height, nullptr );
	
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

	FontContext_Create( &player->fontContext );
}

static void PlayerDevice_Release( Player_s* player )
{
	FontContext_Release( &player->fontContext );

	GPURenderTargetSet_Release( &player->mainRenderTargetSet );

	GPUDevice_Release();
}

//----------------------------------------------------------------------------------------------------

static bool PlayerStream_Create( Player_s* player, const char* streamFilename )
{
	if ( !VideoStream_Load( &player->stream, streamFilename, player->heap, player->stack ) )
		return false;
	
	TraceDesc_s traceDesc = {};
	traceDesc.screenWidth = player->mainRenderTargetSet.width;
	traceDesc.screenHeight = player->mainRenderTargetSet.height;
	traceDesc.stereo = false;

	TraceContext_Create( &player->traceContext, &traceDesc, &player->stream );

	player->curFrameID = 0.0f;
	player->targetFrameID = 0;

	return true;
}

static void PlayerStream_Release( Player_s* player )
{
	TraceContext_Release( &player->traceContext );
	VideoStream_Release( &player->stream, player->heap );
	player->stream.buffer = nullptr;
}

static bool PlayerStream_IsValid( Player_s* player )
{
	return player->stream.buffer != nullptr;
}

//----------------------------------------------------------------------------------------------------

static void PlayerCommandBuffer_MakeFromCommandLine( CommandBuffer_s* commandBuffer, const char* commandLine )
{
	switch ( commandLine[0] )
	{
	case 'B':
		if ( strcmp( commandLine, "BUCKET ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_BUCKET;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "BUCKET OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_BUCKET;
			commandBuffer->arg[0] = 0;
			return;
		}

		break;

	case 'L':
		if ( strcmp( commandLine, "LOG" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_LOG;
			return;
		}

		break;

	case 'M':
		if ( strcmp( commandLine, "MIP ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_MIP;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "MIP OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_MIP;
			commandBuffer->arg[0] = 0;
			return;
		}

		break;

	case 'O':
		if ( strcmp( commandLine, "OVERDRAW ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_OVERDRAW;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "OVERDRAW OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_OVERDRAW;
			commandBuffer->arg[0] = 0;
			return;
		}

		break;

	case 'T':
		if ( strcmp( commandLine, "TSAA ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_TSAA;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "TSAA OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_TSAA;
			commandBuffer->arg[0] = 0;
			return;
		}

		break;

	case 'U':
		if ( strcmp( commandLine, "UI ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_UI;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "UI OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_UI;
			commandBuffer->arg[0] = 0;
			return;
		}

		break;
	}

	V6_WARNING( "Unknown command %s\n", commandLine );
	commandBuffer->action = COMMAND_ACTION_NONE;
}

static void PlayerCommandBuffer_Process( Player_s* player )
{
	CommandBuffer_s commandBuffer = player->commandBuffer;
	if ( commandBuffer.action == COMMAND_ACTION_COMMAND_LINE )
		PlayerCommandBuffer_MakeFromCommandLine( &commandBuffer, player->commandBuffer.arg );

	switch ( commandBuffer.action )
	{
	case COMMAND_ACTION_COMMAND_LINE:
		V6_ASSERT_NOT_SUPPORTED();
		break;
	case COMMAND_ACTION_LOAD_STREAM:
		{
			if ( PlayerStream_IsValid( player ) )
				PlayerStream_Release( player );
			if ( !PlayerStream_Create( player, commandBuffer.arg ) )
			{
				V6_ERROR( "Unable to load stream %s\n", commandBuffer.arg );
			}
			else
			{
				player->camera.pos = player->stream.desc.sequenceCount > 0 ? player->stream.sequences[0].frameDescArray[0].gridOrigin : Vec3_Zero();
				player->camera.yaw = 0.0f;
				player->camera.pitch = 0.0f;
				V6_MSG( "Loaded stream %s\n", commandBuffer.arg );
			}
		}
		break;
	case COMMAND_ACTION_UNLOAD_STREAM:
		{
			if ( PlayerStream_IsValid( player ) )
				PlayerStream_Release( player );
		}
		break;
	case COMMAND_ACTION_PLAY:
		if ( PlayerStream_IsValid( player ) )
		{
			player->targetFrameID = (u32)-1;
			V6_MSG( "Target frame loop\n" );
		}
		break;
	case COMMAND_ACTION_BEGIN_FRAME:
		if ( PlayerStream_IsValid( player ) && player->targetFrameID > 0 )
		{
			player->targetFrameID = 0;
			V6_MSG( "Target frame %d\n", player->targetFrameID );
		}
		break;
	case COMMAND_ACTION_END_FRAME:
		if ( PlayerStream_IsValid( player ) && (player->targetFrameID < player->stream.desc.frameCount-1 || player->targetFrameID == (u32)-1) )
		{
			player->targetFrameID = player->stream.desc.frameCount-1;
			V6_MSG( "Target frame %d\n", player->targetFrameID );
		}
		break;
	case COMMAND_ACTION_PREV_FRAME:
		if ( PlayerStream_IsValid( player ) )
		{
			if ( player->targetFrameID == (u32)-1 )
			{
				player->targetFrameID = (u32)player->curFrameID;
				V6_MSG( "Target frame %d\n", player->targetFrameID );
			}
			else if ( player->targetFrameID > 0 )
			{
				--player->targetFrameID;
				V6_MSG( "Target frame %d\n", player->targetFrameID );
			}
		}
		break;
	case COMMAND_ACTION_NEXT_FRAME:
		if ( PlayerStream_IsValid( player ) )
		{
			if ( player->targetFrameID == (u32)-1 )
			{
				player->targetFrameID = (u32)player->curFrameID;
				V6_MSG( "Target frame %d\n", player->targetFrameID );
			}
			else if ( player->targetFrameID < player->stream.desc.frameCount-1 )
			{
				++player->targetFrameID;
				V6_MSG( "Target frame %d\n", player->targetFrameID );
			}
		}
		break;
	case COMMAND_ACTION_TRACE_OPTION_BUCKET:
		player->traceOptions.showBucket = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 1) : !player->traceOptions.showBucket;
		break;
	case COMMAND_ACTION_TRACE_OPTION_LOG:
		player->traceOptions.logReadBack = true;
		break;
	case COMMAND_ACTION_TRACE_OPTION_MIP:
		player->traceOptions.showMip = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 1) : !player->traceOptions.showMip;
		break;
	case COMMAND_ACTION_TRACE_OPTION_OVERDRAW:
		player->traceOptions.showOverdraw = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 1) : !player->traceOptions.showOverdraw;
		break;
	case COMMAND_ACTION_TRACE_OPTION_TSAA:
		player->traceOptions.noTSAA = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->traceOptions.noTSAA;
		break;
	case COMMAND_ACTION_UI:
		player->hideUI = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->hideUI;
		break;
	}

	player->commandBuffer.action = COMMAND_ACTION_NONE;
}

//----------------------------------------------------------------------------------------------------

static void Player_OnKeyEvent( const KeyEvent_s* keyEvent )
{
	Player_s* player = (Player_s*)keyEvent->win->owner;

	if ( player->commandLineSize != (u32)-1 )
	{
		if ( !keyEvent->pressed )
			return ;

		if ( keyEvent->key == 0x0D )
		{
			if ( player->commandLineSize == 0 )
			{
				V6_MSG( "\r~<NULL>\n" );
			}
			else
			{
				V6_MSG( "\r~%s\n", player->commandLine );
				player->commandBuffer.action = COMMAND_ACTION_COMMAND_LINE;
				strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), player->commandLine );
			}
			player->commandLineSize = (u32)-1;
			
		}
		else if ( keyEvent->key == 0xC0 )
		{
			V6_MSG( "\r~<NULL>\n" );
			player->commandLineSize = (u32)-1;
		}
		else if ( keyEvent->key >= ' ' && player->commandLineSize < sizeof( player->commandLine ) )
		{
			player->commandLine[player->commandLineSize] = keyEvent->key;
			++player->commandLineSize;
			player->commandLine[player->commandLineSize] = 0;
			V6_MSG( "\r~%s", player->commandLine );
		}

		return;
	}

	switch( keyEvent->key )
	{
	case 'A': player->keyLeftPressed = keyEvent->pressed; break;
	case 'D': player->keyRightPressed = keyEvent->pressed; break;
	case 'S': player->keyDownPressed = keyEvent->pressed; break;
	case 'W': player->keyUpPressed = keyEvent->pressed; break;
	}

	if ( !keyEvent->pressed )
		return ;

	switch( keyEvent->key )
	{
	case 0x1B:
		Win_Release( &player->win );
		break;
	case 0x21:
		player->commandBuffer.action = COMMAND_ACTION_NEXT_FRAME;
		break;
	case 0x22:
		player->commandBuffer.action = COMMAND_ACTION_PREV_FRAME;
		break;
	case 0x23:
		player->commandBuffer.action = COMMAND_ACTION_END_FRAME;
		break;
	case 0x24:
		player->commandBuffer.action = COMMAND_ACTION_BEGIN_FRAME;
		break;
	case 0xC0:
		player->commandLineSize = 0;
		V6_MSG( "~" );
		break;
	case 'C':
		player->commandBuffer.action = COMMAND_ACTION_UNLOAD_STREAM;
		break;
	case 'I':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_LOG;
		break;
	case 'J':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_TSAA;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'L':
		{
			player->commandBuffer.action = COMMAND_ACTION_LOAD_STREAM;
			//strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), "D:/media/obj/crytek-sponza/sponza.v6" );
			strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), "D:/tmp/v6/ue.v6" );
		}
		break;
	case 'N':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_BUCKET;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'M':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_MIP;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'O':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_OVERDRAW;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'P':
		player->commandBuffer.action = COMMAND_ACTION_PLAY;
		break;
	case 'U':
		player->commandBuffer.action = COMMAND_ACTION_UI;
		player->commandBuffer.arg[0] = 2;
		break;
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
}

static void Player_DrawUI( Player_s* player, float averageFPS, const GPUEventDuration_s* eventDurations, u32 eventCount )
{
	const u32 lineHeight = FontContext_GetLineHeight( &player->fontContext );
	u32 cursorY = lineHeight / 2;

	FontContext_AddText( &player->fontContext, 8, cursorY, Color_White(), String_Format( "FPS: %3.1f", averageFPS ) );
	cursorY += lineHeight;

	if ( PlayerStream_IsValid( player ) )
		FontContext_AddText( &player->fontContext, 8, cursorY, Color_White(), String_Format( "Stream: %s", player->stream.name ) );
	else
		FontContext_AddText( &player->fontContext, 8, cursorY, Color_White(), "Stream: <none>" );
	cursorY += lineHeight;

	for ( u32 eventRank = 0; eventRank < eventCount; ++eventRank )
	{
		const GPUEventDuration_s* eventDuration = &eventDurations[eventRank];
		const char* txt = String_Format( "%*s%s: %dus", eventDuration->depth * 2, "", eventDuration->name, eventDuration->durationUS );
		FontContext_AddText( &player->fontContext, 8, cursorY, Color_White(), txt );
		cursorY += lineHeight;
	}

	FontContext_Draw( &player->fontContext, &player->mainRenderTargetSet );
}

static void Player_ProcessFrame( Player_s* player, u32 frameID, float dt, float averageFPS )
{
	String_ResetInternalBuffer();
	
	PlayerCommandBuffer_Process( player );

	Player_ProcessInputs( player, dt );

	GPUEvent_BeginFrame( frameID );

	{
		GPUEvent_Begin( s_gpuEventDraw );

		if ( PlayerStream_IsValid( player ) )
		{
			if ( player->targetFrameID < player->curFrameID )
				player->curFrameID = (float)player->targetFrameID;
		
			const u32 frameID = (u32)(player->curFrameID + FLT_EPSILON);
		
			TraceContext_UpdateFrame( &player->traceContext, frameID, player->stack );

			Vec3 right, up, forward;
			TraceContext_GetFrameBasis( &player->traceContext, &right, &up, &forward );
		
			const Mat4x4 lookAt = Mat4x4_Basis( &right, &up, &forward );
			Camera_UpdateBasis( &player->camera, &lookAt );

			View_s view;
			Camera_MakeView( &player->camera, &view );

			TraceContext_DrawFrame( &player->traceContext, &player->mainRenderTargetSet, &view, &player->traceOptions, player->stack );

			if ( frameID < player->targetFrameID )
				player->curFrameID = fmodf( player->curFrameID + (float)player->stream.desc.frameRate / PLAY_RATE, (float)player->stream.desc.frameCount );
		}
		else
		{
			Camera_UpdateBasis( &player->camera, nullptr );

			View_s view;
			Camera_MakeView( &player->camera, &view );

			PlayerScene_Draw( player, &view );
		}

		GPUEvent_End();
	}

	GPUEventDuration_s* eventDurations;
	const u32 eventCount = GPUEvent_UpdateDurations( &eventDurations );

	if ( !player->hideUI )
		Player_DrawUI( player, averageFPS, eventDurations, eventCount );

	{
		GPUEvent_Begin( s_gpuEventCopy );
		
		GPUColorRenderTarget_Copy( &GPUSurfaceContext_Get()->surface, &player->mainRenderTargetSet.colorBuffers[0] );

		GPUEvent_End();
	}

	{
		GPUEvent_Begin( s_gpuEventPresent );

		GPUSurfaceContext_Present();

		GPUEvent_End();
	}

	GPUEvent_EndFrame();

	player->traceOptions.logReadBack = false;
}

static bool Player_Create( Player_s* player, u32 width, u32 height, IAllocator* heap, IStack* stack )
{
	memset( player, 0, sizeof( *player ) );

	player->commandLineSize = (u32)-1;

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
	if ( PlayerStream_IsValid( player ) )
		PlayerStream_Release( player );
	PlayerScene_Release( player );
	PlayerDevice_Release( player );
}

END_V6_NAMESPACE

//----------------------------------------------------------------------------------------------------

int main( int argc, char** argv )
{
	v6::CHeap heap;
	v6::Stack stack( &heap, 100 * 1024 * 1024 );

	v6::Player_s* player = stack.newInstance< v6::Player_s >();

	const v6::u32 width = 512;
	const v6::u32 height = 512;

	if ( !v6::Player_Create( player, width, height, &heap, &stack ) )
		return -1;

	if ( argc >= 2 )
	{
		player->commandBuffer.action = v6::COMMAND_ACTION_LOAD_STREAM;
		strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), argv[1] );
	}

	v6::Win_Show( &player->win, true );
	
	v6::FrameTimer_s frameTimer;
	v6::FrameTimer_Init( &frameTimer, (float)v6::PLAY_RATE, 0.1f );
	
	while ( !v6::Win_ProcessMessagesAndShouldQuit( &player->win ) )
	{
		v6::FrameInfo_s frameInfo;
		v6::FrameTimer_ComputeNewFrameInfo( &frameTimer, &frameInfo );

		v6::Player_ProcessFrame( player, frameInfo.frameID, frameInfo.dt, frameInfo.averageFPS );
	}

	v6::Player_Release( player );

	return 0;
}