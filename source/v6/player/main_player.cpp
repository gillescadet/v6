/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <Windowsx.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/codec/decoder.h>
#include <v6/core/filesystem.h>
#include <v6/core/gamepad.h>
#include <v6/core/memory.h>
#include <v6/core/string.h>
#include <v6/core/time.h>
#include <v6/core/win.h>
#include <v6/graphic/font.h>
#include <v6/graphic/gpu.h>
#include <v6/graphic/hmd.h>
#include <v6/graphic/scene.h>
#include <v6/graphic/trace.h>
#include <v6/graphic/view.h>
#include <v6/player/missing_stream_256_bc1.h>
#include <v6/player/player_shared.h>

#pragma comment( lib, "d3d11.lib" )

#define V6_D3D_DEBUG			0
#define V6_STEREO				1
#define V6_ENABLE_HMD			0
#define V6_ENABLE_MIRRORING		1
#define V6_USE_HMD				(V6_ENABLE_HMD == 1 && V6_STEREO == 1)
#define V6_DUMP_GAMEPAD			0
#define V6_LIMIT_FRAME_RATE		1

BEGIN_V6_NAMESPACE

static const GPUEventID_t s_gpuEventUpdate		= GPUEvent_Register( "Update", true );
static const GPUEventID_t s_gpuEventDraw		= GPUEvent_Register( "Draw", true );
static const GPUEventID_t s_gpuEventMetrics		= GPUEvent_Register( "Metrics", true );
static const GPUEventID_t s_gpuEventHMD			= GPUEvent_Register( "HMD", true );
static const GPUEventID_t s_gpuEventCopy		= GPUEvent_Register( "Copy", true );
static const GPUEventID_t s_gpuEventList		= GPUEvent_Register( "List", true );
static const GPUEventID_t s_gpuEventPresent		= GPUEvent_Register( "Present", true );

extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_deviceContext;

static const u32	PLAY_RATE				= 75; 
static const float	ZNEAR_DEFAULT			= 10.0f;
static const float	ZFAR_DEFAULT			= 5000.0f;
static const float	MOUSE_ROTATION_SPEED	= 0.5f;
static const float	KEY_TRANSLATION_SPEED	= 200.0f;
#if V6_STEREO == 1
static const u32 EYE_COUNT					= 2;
static const float IPD						= 6.5f;
#else
static const u32 EYE_COUNT					= 1;
static const float IPD						= 0.0f;
#endif
static const char* PLAYER_STREAM_ITEM_FOLDER = "media";

static const Color_s V6_DARK_GRAY			= Color_Make(  41,  41,  41, 0 );
static const Color_s V6_LIGHT_GRAY			= Color_Make( 204, 204, 204, 0 );
static const Color_s V6_ORANGE				= Color_Make( 226,  73,  27, 0 );

static const u32 s_ouputMessageBufferCount = 3;
static char s_ouputMessageBuffers[s_ouputMessageBufferCount][4096] = {};
static u32 s_ouputMessageCount = 0;

enum
{
	CONSTANT_BUFFER_BASIC,
	CONSTANT_BUFFER_LIST_ITEM,
	CONSTANT_BUFFER_COMPOSE,
	CONSTANT_BUFFER_FRAMEMETRICS,

	CONSTANT_BUFFER_COUNT
};

enum
{
	BUFFER_FRAMEMETRICS,

	BUFFER_COUNT
};

enum
{
	SHADER_BASIC,
	SHADER_LIST,

	SHADER_COUNT
};

enum
{
	COMPUTE_COMPOSESURFACE,
	COMPUTE_FRAMEMETRICS,

	COMPUTE_COUNT
};

struct StreamItem_s
{
	char			filename[256];
	char			title[256];
	StreamItem_s*	prev;
	StreamItem_s*	next;
	u8*				iconTextureData;
	u32				iconTextureSize;
	float			duration;
	u32				entityID;
	u32				textureID;
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

struct FrameMetrics_s
{
	static const u32		WIDTH = HLSL_FRAME_METRICS_WIDTH;
	u32						frameID;
	void*					dataBuffer;
	hlsl::FrameMetrics_s*	data;
};

enum CommandAction_e
{
	COMMAND_ACTION_NONE,
	
	COMMAND_ACTION_EXIT,

	COMMAND_ACTION_COMMAND_LINE,

	COMMAND_ACTION_LOAD_STREAM,
	COMMAND_ACTION_UNLOAD_STREAM,
	
	COMMAND_ACTION_PREV_ITEM,
	COMMAND_ACTION_NEXT_ITEM,

	COMMAND_ACTION_PLAY_PAUSE,
	COMMAND_ACTION_BEGIN_FRAME,
	COMMAND_ACTION_END_FRAME,
	COMMAND_ACTION_PREV_FRAME,
	COMMAND_ACTION_NEXT_FRAME,
	
	COMMAND_ACTION_CAMERA_RECENTER,

	COMMAND_ACTION_TRACE_OPTION_BLOCK,
	COMMAND_ACTION_TRACE_OPTION_LOG,
	COMMAND_ACTION_TRACE_OPTION_OVERDRAW,
	COMMAND_ACTION_TRACE_OPTION_GRID,
	COMMAND_ACTION_TRACE_OPTION_HISTORY,
	COMMAND_ACTION_TRACE_OPTION_TSAA,
	COMMAND_ACTION_TRACE_OPTION_SHARPEN_FILTER,

	COMMAND_ACTION_PLAYER_OPTION_METRICS,
	COMMAND_ACTION_PLAYER_OPTION_UI,
	COMMAND_ACTION_PLAYER_OPTION_DISABLE_FADE_TO_BLACK,
	COMMAND_ACTION_PLAYER_OPTION_LOCK_HMD,
	COMMAND_ACTION_PLAYER_OPTION_SHOW_HMD_PERF_HUD,
};

enum PlayerState_e
{
	PLAYER_STATE_LIST,
	PLAYER_STATE_STREAM
};

struct CommandBuffer_s
{
	CommandAction_e			action;
	char					arg[256];
};

struct PlayerOptions_s
{
	bool					showMetrics;
	bool					showUI;
	bool					disableFadeToBlack;
	bool					lockHMD;
	u32						showHMDPerfHUD;
};

struct Player_s;

struct PlayerScene_s : public Scene_s
{
	Player_s*				player;
};

struct Player_s
{
	IAllocator*				heap;
	IStack*					stack;
	StreamItem_s*			firstStreamItem;
	StreamItem_s*			selectedStreamItem;
	BlockAllocator_s		streamItemAllocator;
	CommandBuffer_s			commandBuffer;
	Win_s					win;
	Gamepad_s				gamepad;
	GPURenderTargetSet_s	winRenderTargetSet;
	GPURenderTargetSet_s	createdRenderTargetSet;
	GPURenderTargetSet_s	mainRenderTargetSet;
	Camera_s				camera;
	PlayerScene_s			sceneGrid;
	PlayerScene_s			sceneListView;
	VideoStream_s			stream;
	FontContext_s			fontContext;
	TraceContext_s			traceContext;
	TraceOptions_s			traceOptions;
	PlayerOptions_s			playerOptions;
	FrameMetrics_s			frameMetrics;
	float					curFrameID;
	u32						targetFrameID;
	PlayerState_e			playerState;
#if V6_USE_HMD == 1
	u32						hmdState;
#endif // #if V6_USE_HMD == 1

	// inputs
	bool					mousePressed;
	float					mouseDeltaX;
	float					mouseDeltaY;
	int						keyLeftPressed;
	int						keyRightPressed;
	int						keyUpPressed;
	int						keyDownPressed;
	int						keyPlusPressed;
	int						keyMinusPressed;
	char					commandLine[256];
	u32						commandLineSize;
};

//----------------------------------------------------------------------------------------------------

void OutputMessage( const char * format, ... )
{
	const u32 bufferID = s_ouputMessageCount % s_ouputMessageBufferCount;
	va_list args;
	va_start( args, format );
	vsprintf_s( s_ouputMessageBuffers[bufferID], sizeof( s_ouputMessageBuffers[bufferID] ), format, args );
	va_end( args );

	fputs( s_ouputMessageBuffers[bufferID], stdout );

	++s_ouputMessageCount;
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
#if V6_LIMIT_FRAME_RATE == 1 && V6_USE_HMD == 0
		if ( frameInfo->dt + 0.0001f >= frameTimer->dtMin )
#endif // #if V6_USE_HMD == 0
			break;
		SwitchToThread();
		frameUpdatedTick = GetTickCount();
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

static void PlayerCamera_Recenter( Player_s* player )
{
	player->camera.pos = Vec3_Zero();
	player->camera.yaw = 0.0f;
	player->camera.pitch = 0.0f;
	player->camera.znear = ZNEAR_DEFAULT;
	player->camera.zfar = ZFAR_DEFAULT;
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

static void PlayerMaterial_DrawList( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags )
{
	Player_s* player = ((PlayerScene_s*)scene)->player;
	
	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	hlsl::CBList* cbList = (hlsl::CBList*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM] );

	const Color_s color = (player->selectedStreamItem && &player->sceneListView.entities[player->selectedStreamItem->entityID] == entity) ? V6_ORANGE : V6_LIGHT_GRAY;
	const float inv255 = 1.0f / 255.0f;

	cbList->c_listScreenInvSize = Vec2_Make( 1.0f / player->win.size.x, 1.0f / player->win.size.y );
	cbList->c_listPosAndScale = Vec4_Make( &entity->pos, entity->scale );
	cbList->c_listColor = Vec4_Make( color.r * inv255, color.g * inv255, color.b * inv255, color.a * inv255 );

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM] );
	
	static const void* nulls[8] = {};

	g_deviceContext->VSSetConstantBuffers( hlsl::CBListSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM].buf );
	g_deviceContext->PSSetConstantBuffers( hlsl::CBListSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM].buf );

	g_deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &shaderContext->trilinearSamplerState );

	if ( material->textureIDs[0] != Material_s::TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[0]];
		g_deviceContext->PSSetShaderResources( HLSL_LIST_ALBEDO_SLOT, 1, &texture->srv );
	}
	else
	{
		g_deviceContext->PSSetShaderResources( HLSL_LIST_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	}
	
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &shaderContext->shaders[SHADER_LIST];
	GPUMesh_Draw( mesh, 1, shader );

	g_deviceContext->PSSetShaderResources( HLSL_LIST_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
}

//----------------------------------------------------------------------------------------------------

static void PlayerScene_Create( Player_s* player, float tanHalfFovPerPixel )
{
	Camera_Create( &player->camera, ZNEAR_DEFAULT, ZFAR_DEFAULT, tanHalfFovPerPixel * player->mainRenderTargetSet.height, (float)player->mainRenderTargetSet.width / player->mainRenderTargetSet.height );
	
	{
		Scene_Create( &player->sceneGrid );
		player->sceneGrid.player = player;

		const u32 meshWireFrameBoxID = Scene_GetNewMeshID( &player->sceneGrid );
		GPUMesh_CreateBox( &player->sceneGrid.meshes[meshWireFrameBoxID], Color_Make( 255, 255, 255, 255 ), true );

		const u32 materialBasicID = Scene_GetNewMaterialID( &player->sceneGrid );
		Material_Create( &player->sceneGrid.materials[materialBasicID], PlayerMaterial_DrawBasic );
			
		const u32 mainBoxID = Scene_GetNewEntityID( &player->sceneGrid );
		Entity_Create( &player->sceneGrid.entities[mainBoxID], materialBasicID, meshWireFrameBoxID, Vec3_Make( 0.0f, 0.0f, 0.0f), ZNEAR_DEFAULT * 2.0f );
	}

	{
		Scene_Create( &player->sceneListView );
		player->sceneListView.player = player;

		const u32 meshQuadID = Scene_GetNewMeshID( &player->sceneListView );
	
		{
			const struct ItemVertex_s { Vec3 pos; Vec2 uv; } vertices[4] = 
			{
				{ Vec3_Make( -1.0f, -1.0f, 0.0f ), Vec2_Make( -0.01f,  1.01f ) },
				{ Vec3_Make(  1.0f, -1.0f, 0.0f ), Vec2_Make(  1.01f,  1.01f ) },
				{ Vec3_Make( -1.0f,  1.0f, 0.0f ), Vec2_Make( -0.01f, -0.01f ) },
				{ Vec3_Make(  1.0f,  1.0f, 0.0f ), Vec2_Make(  1.01f, -0.01f ) },
			};

			const u16 indices[4] = { 0, 2, 1, 3 };

			GPUMesh_Create( &player->sceneListView.meshes[meshQuadID], vertices, 4, sizeof( ItemVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2, indices, 4, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
		}

		const u32 missingTextureID = Scene_GetNewTextureID( &player->sceneListView );
		GPUTexture2D_CreateCompressed( &player->sceneListView.textures[missingTextureID], CODEC_ICON_WIDTH, CODEC_ICON_WIDTH, (void*)g_missingStreamTextureData, true, "missing_icon" );

		for ( StreamItem_s* item = player->firstStreamItem; item; item = item->next )
		{
			u32 textureID = missingTextureID;
			if ( item->iconTextureData )
			{
				textureID = Scene_GetNewTextureID( &player->sceneListView );
				GPUTexture2D_CreateCompressed( &player->sceneListView.textures[textureID], CODEC_ICON_WIDTH, CODEC_ICON_WIDTH, item->iconTextureData, true, "icon" );
			}

			const u32 materialID = Scene_GetNewMaterialID( &player->sceneListView );
			Material_Create( &player->sceneListView.materials[materialID], PlayerMaterial_DrawList );
			Material_SetTexture( &player->sceneListView.materials[materialID], textureID, 0 );

			const u32 entityID = Scene_GetNewEntityID( &player->sceneListView );
			Entity_Create( &player->sceneListView.entities[entityID], materialID, meshQuadID, Vec3_Zero(), CODEC_ICON_WIDTH * 0.5f );
			item->entityID = entityID;
		}
	}
}

static void PlayerScene_Release( Player_s* player )
{
	Scene_Release( &player->sceneGrid );

	Scene_Release( &player->sceneListView );
}

static void PlayerScene_DrawGrid( Player_s* player, View_s* views )
{
	GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
	renderTargetSetBindingDesc.clear = true;
	renderTargetSetBindingDesc.useMSAA = true;

	for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
	{
		GPURenderTargetSet_Bind( &player->mainRenderTargetSet, &renderTargetSetBindingDesc, eye );

		Scene_Draw( &player->sceneGrid, &views[eye], eye );

		GPURenderTargetSet_Unbind( &player->mainRenderTargetSet );
	}
}

static void PlayerScene_DrawList( Player_s* player )
{
	const float margin = 60.0f;
	const float base = margin + CODEC_ICON_WIDTH * 0.5f;
	float x = base;
	float y = base;

	for ( StreamItem_s* item = player->firstStreamItem; item; item = item->next )
	{
		if ( x + CODEC_ICON_WIDTH + margin > player->win.size.x )
		{
			x = base;
			y += CODEC_ICON_WIDTH + margin;
		}
		player->sceneListView.entities[item->entityID].pos = Vec3_Make( x, y, 0.0f );
		x += CODEC_ICON_WIDTH + margin;
	}

	GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
	renderTargetSetBindingDesc.clearColor = Color_Make( 41, 41, 41, 0 );
	renderTargetSetBindingDesc.clear = true;
	renderTargetSetBindingDesc.useMSAA = true;

	GPURenderTargetSet_Bind( &player->winRenderTargetSet, &renderTargetSetBindingDesc, 0 );

	Scene_Draw( &player->sceneListView, nullptr, 0 );

	GPURenderTargetSet_Unbind( &player->winRenderTargetSet );
}


//----------------------------------------------------------------------------------------------------

static void PlayerDevice_Create( Player_s* player, u32 width, u32 height )
{
	bool debugDevice = false;
#if V6_D3D_DEBUG == 1
	debugDevice = true;
#endif
	GPUDevice_CreateWithSurfaceContext( player->win.size.x, player->win.size.y, player->win.hWnd, debugDevice );
	
	{
		GPURenderTargetSetCreationDesc_s renderTargetSetCreationDesc = {};
		renderTargetSetCreationDesc.reuseColorRenderTargets[0] = GPUSurfaceContext_Get()->surface;
		renderTargetSetCreationDesc.name = "win";
		renderTargetSetCreationDesc.width = player->win.size.x;
		renderTargetSetCreationDesc.height = player->win.size.y;
		renderTargetSetCreationDesc.supportMSAA = true;
		renderTargetSetCreationDesc.bindable = true;
		renderTargetSetCreationDesc.writable = true;
		renderTargetSetCreationDesc.stereo = false;

		GPURenderTargetSet_Create( &player->winRenderTargetSet, &renderTargetSetCreationDesc );
	}

	{
		GPURenderTargetSetCreationDesc_s renderTargetSetCreationDesc = {};
		renderTargetSetCreationDesc.name = "main";
		renderTargetSetCreationDesc.width = width;
		renderTargetSetCreationDesc.height = height;
		renderTargetSetCreationDesc.supportMSAA = true;
		renderTargetSetCreationDesc.bindable = true;
		renderTargetSetCreationDesc.writable = true;
		renderTargetSetCreationDesc.stereo = V6_STEREO;

		GPURenderTargetSet_Create( &player->createdRenderTargetSet, &renderTargetSetCreationDesc );
		player->mainRenderTargetSet = player->createdRenderTargetSet;
	}

	GPUShaderContext_CreateEmpty();

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	static_assert( CONSTANT_BUFFER_COUNT <= GPUShaderContext_s::CONSTANT_BUFFER_MAX_COUNT, "Out of constant buffer" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC], sizeof( hlsl::CBBasic ), "basic" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM], sizeof( hlsl::CBList ), "list" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE], sizeof( hlsl::CBCompose ), "compose" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_FRAMEMETRICS], sizeof( hlsl::CBFrameMetrics ), "frameMetrics" );

	static_assert( BUFFER_COUNT <= GPUShaderContext_s::BUFFER_MAX_COUNT, "Out of buffer" );
	GPUBuffer_CreateStructured( &shaderContext->buffers[BUFFER_FRAMEMETRICS], sizeof( hlsl::FrameMetrics_s ), FrameMetrics_s::WIDTH, GPUBUFFER_CREATION_FLAG_MAP_DISCARD, "frameMetrics" );

	{
		ScopedStack scopedStack( player->stack );
		
		static_assert( SHADER_COUNT <= GPUShaderContext_s::SHADER_MAX_COUNT, "Out of shader" );
		GPUShader_Create( &shaderContext->shaders[SHADER_BASIC], "player_basic_vs.cso", "player_basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, player->stack );
		GPUShader_Create( &shaderContext->shaders[SHADER_LIST], "player_list_vs.cso", "player_list_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2, player->stack );

		static_assert( COMPUTE_COUNT <= GPUShaderContext_s::COMPUTE_MAX_COUNT, "Out of compute" );
		GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_COMPOSESURFACE], "surface_compose_cs.cso", player->stack );
		GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_FRAMEMETRICS], "frame_metrics_cs.cso", player->stack );
	}

	FontSystem_Create();
	FontContext_Create( &player->fontContext );

	player->frameMetrics.data = (hlsl::FrameMetrics_s*)player->heap->alloc_aligned< 16 >( &player->frameMetrics.dataBuffer, player->frameMetrics.WIDTH * sizeof( hlsl::FrameMetrics_s ) );
	memset( player->frameMetrics.data, 0, player->frameMetrics.WIDTH * sizeof( hlsl::FrameMetrics_s ) );
}

static void PlayerDevice_Release( Player_s* player )
{
	FontContext_Release( &player->fontContext );
	FontSystem_Release();

	player->heap->free( player->frameMetrics.dataBuffer );

	GPURenderTargetSet_Release( &player->createdRenderTargetSet );
	GPURenderTargetSet_Release( &player->winRenderTargetSet );

	GPUDevice_Release();
}

//----------------------------------------------------------------------------------------------------

static void PlayerStreamItem_Add( const char* pFileName, void* pCallbackData )
{
	Player_s* player = (Player_s*)pCallbackData;

	char streamFilename[256];
	sprintf_s( streamFilename, sizeof( streamFilename ), "%s/%s", PLAYER_STREAM_ITEM_FOLDER, pFileName );

	ScopedStack scopedStack( player->stack );

	CodecStreamDesc_s streamDesc;
	CodecStreamData_s streamData;

	if ( VideoStream_LoadDescAndData( streamFilename, &streamDesc, &streamData, player->stack ) == nullptr )
	{
		V6_ERROR( "Unable to read stream desc for %s\n", streamFilename );
		return;
	}

	u32 titleSize;
	const char* title = (const char*)VideoStream_GetKeyValue( &titleSize, &streamDesc, &streamData, "title", player->stack );

	u32 iconSize;
	u8* iconData = VideoStream_GetKeyValue( &iconSize, &streamDesc, &streamData, "icon", player->stack );

	StreamItem_s* newItem = BlockAllocator_Add< StreamItem_s >( &player->streamItemAllocator, 1 );
	memset( newItem, 0, sizeof( StreamItem_s ) );

	strcpy_s( newItem->filename, sizeof( newItem->filename ), streamFilename );

	if ( title )
		memcpy( newItem->title, title, Min( titleSize, (u32)sizeof( newItem->title )-1u ) );
	else
		strcpy_s( newItem->title, sizeof( newItem->title ), "unknown" );

	if ( iconData && iconSize > 4 && memcmp( iconData, CODEC_ICON_MAGIC, 4 ) == 0 )
	{
		newItem->iconTextureSize = iconSize-4;
		newItem->iconTextureData = (u8*)BlockAllocator_Alloc( &player->streamItemAllocator, newItem->iconTextureSize );
			
		memcpy( newItem->iconTextureData, iconData+4, newItem->iconTextureSize );
	}

	StreamItem_s* prevItem = nullptr;
	for ( StreamItem_s* item = player->firstStreamItem; item; prevItem = item, item = item->next )
	{
		if ( strcmp( item->filename, newItem->title ) <= 0 )
			break;
	}

	if ( prevItem == nullptr )
	{
		newItem->next = player->firstStreamItem;
		if ( player->firstStreamItem )
			player->firstStreamItem->prev = newItem;
		player->firstStreamItem = newItem;
		player->selectedStreamItem = player->firstStreamItem;
	}
	else
	{
		newItem->prev = prevItem;
		if ( prevItem->next )
			prevItem->next->prev = newItem;
		newItem->next = prevItem->next;
		prevItem->next = newItem;
	}
}

static void PlayerStreamItem_Create( Player_s* player )
{
	BlockAllocator_Create( &player->streamItemAllocator, player->heap, MulMB( 1 ) );

	char filter[256];
	sprintf_s( filter, sizeof( filter ), "%s/*.v6", PLAYER_STREAM_ITEM_FOLDER );
	FileSystem_GetFileList( filter, PlayerStreamItem_Add, player );
}

static void PlayerStreamItem_Release( Player_s* player )
{
	player->selectedStreamItem = nullptr;
	BlockAllocator_Release( &player->streamItemAllocator );
}

//----------------------------------------------------------------------------------------------------

static bool PlayerStream_Create( Player_s* player, const char* streamFilename )
{
	if ( !VideoStream_Load( &player->stream, streamFilename, player->heap, player->stack ) )
		return false;
	
	TraceDesc_s traceDesc = {};
	traceDesc.screenWidth = player->mainRenderTargetSet.width;
	traceDesc.screenHeight = player->mainRenderTargetSet.height;
	traceDesc.stereo = V6_STEREO;

	TraceContext_Create( &player->traceContext, &traceDesc, &player->stream );

	player->curFrameID = 0.0f;
	player->targetFrameID = 0;

	return true;
}

static void PlayerStream_Release( Player_s* player )
{
	TraceContext_Release( &player->traceContext );
	VideoStream_Release( &player->stream, player->heap );
}

static void PlayerStream_Load( Player_s* player, const char* streamFileName )
{
	if ( player->playerState == PLAYER_STATE_STREAM )
		PlayerStream_Release( player );

	player->playerState = PLAYER_STATE_LIST;

	if ( !PlayerStream_Create( player, streamFileName ) )
	{
		V6_ERROR( "Unable to load stream %s\n", streamFileName );
	}
	else
	{
		PlayerCamera_Recenter( player );
#if V6_ENABLE_HMD == 1
		Hmd_Recenter();
#endif // #if V6_ENABLE_HMD == 1
		V6_MSG( "Loaded stream %s\n", streamFileName );

		player->playerState = PLAYER_STATE_STREAM;
	}
}

static void PlayerStream_Unload( Player_s* player )
{
	if ( player->playerState == PLAYER_STATE_STREAM )
		PlayerStream_Release( player );

	PlayerCamera_Recenter( player );

	player->playerState = PLAYER_STATE_LIST;
}

//----------------------------------------------------------------------------------------------------

static void PlayerCommandBuffer_MakeFromCommandLine( CommandBuffer_s* commandBuffer, const char* commandLine )
{
	switch ( commandLine[0] )
	{
	case 'B':
		if ( strcmp( commandLine, "BLOCK ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_BLOCK;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "BLOCK OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_BLOCK;
			commandBuffer->arg[0] = 0;
			return;
		}

		break;

	case 'C':
		if ( strcmp( commandLine, "CAMERA RECENTER" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_CAMERA_RECENTER;
			return;
		}

		break;

	case 'E':
		if ( strcmp( commandLine, "EXIT" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_EXIT;
			return;
		}

		break;

	case 'F':
		if ( strcmp( commandLine, "FADE_TO_BLACK ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_DISABLE_FADE_TO_BLACK;
			commandBuffer->arg[0] = 0;
			return;
		}

		if ( strcmp( commandLine, "FADE_TO_BLACK OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_DISABLE_FADE_TO_BLACK;
			commandBuffer->arg[0] = 1;
			return;
		}

		break;

	case 'G':
		if ( strcmp( commandLine, "GRID ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_GRID;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "GRID OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_GRID;
			commandBuffer->arg[0] = 0;
			return;
		}

		break;

	case 'H':
		if ( strcmp( commandLine, "HISTORY ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_HISTORY;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "HISTORY OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_HISTORY;
			commandBuffer->arg[0] = 0;
			return;
		}

		if ( strcmp( commandLine, "HMD_LOCK ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_LOCK_HMD;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "HMD_LOCK OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_LOCK_HMD;
			commandBuffer->arg[0] = 0;
			return;
		}

		if ( strncmp( commandLine, "HMD_SHOW_PERF_HUF", strlen( "HMD_SHOW_PERF_HUF" ) ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_SHOW_HMD_PERF_HUD;
			commandBuffer->arg[0] = atoi( commandLine + strlen( "HMD_SHOW_PERF_HUF" ) );
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
		if ( strcmp( commandLine, "METRICS ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_METRICS;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "METRICS OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_METRICS;
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

	case 'S':
		if ( strcmp( commandLine, "SHARPEN_FILTER ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_SHARPEN_FILTER;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "SHARPEN_FILTER OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_TRACE_OPTION_SHARPEN_FILTER;
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
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_UI;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "UI OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_UI;
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
	case COMMAND_ACTION_EXIT:
		if ( player->playerState == PLAYER_STATE_STREAM )
			PlayerStream_Unload( player );
		else
			Win_Release( &player->win );
		break;
	case COMMAND_ACTION_COMMAND_LINE:
		V6_ASSERT_NOT_SUPPORTED();
		break;
	case COMMAND_ACTION_LOAD_STREAM:
		PlayerStream_Load( player, commandBuffer.arg );
		break;
	case COMMAND_ACTION_UNLOAD_STREAM:
		{
			if ( player->playerState == PLAYER_STATE_STREAM )
				PlayerStream_Release( player );

			PlayerCamera_Recenter( player );

			player->playerState = PLAYER_STATE_LIST;
		}
		break;
	case COMMAND_ACTION_PREV_ITEM:
		if ( player->playerState == PLAYER_STATE_LIST )
		{
			if ( player->selectedStreamItem )
			{
				player->selectedStreamItem = player->selectedStreamItem->prev;
				if ( player->selectedStreamItem == nullptr )
				{
					for ( StreamItem_s* item = player->firstStreamItem; item; item = item->next )
						player->selectedStreamItem = item;
				}
			}
		}
		break;
	case COMMAND_ACTION_NEXT_ITEM:
		if ( player->playerState == PLAYER_STATE_LIST )
		{
			if ( player->selectedStreamItem )
			{
				player->selectedStreamItem = player->selectedStreamItem->next;
				if ( player->selectedStreamItem == nullptr )
					player->selectedStreamItem = player->firstStreamItem;
			}
		}
		break;
	case COMMAND_ACTION_PLAY_PAUSE:
		if ( player->playerState == PLAYER_STATE_LIST )
		{
			if ( player->selectedStreamItem )
				PlayerStream_Load( player, player->selectedStreamItem->filename );
		}
		else if ( player->playerState == PLAYER_STATE_STREAM )
		{
			if ( player->targetFrameID == (u32)-1 || (u32)player->curFrameID < player->targetFrameID )
			{
				player->targetFrameID = (u32)player->curFrameID;
			}
			else 
			{
				if ( player->targetFrameID == player->stream.desc.frameCount-1 )
					player->curFrameID = 0;
				player->targetFrameID = player->stream.desc.frameCount-1;
			}
			V6_MSG( "Target frame %d\n", player->targetFrameID );
		}
		break;
	case COMMAND_ACTION_BEGIN_FRAME:
		if ( player->playerState == PLAYER_STATE_STREAM && player->targetFrameID > 0 )
		{
			player->targetFrameID = 0;
			V6_MSG( "Target frame %d\n", player->targetFrameID );
		}
		break;
	case COMMAND_ACTION_END_FRAME:
		if ( player->playerState == PLAYER_STATE_STREAM && (player->targetFrameID < player->stream.desc.frameCount-1 || player->targetFrameID == (u32)-1) )
		{
			player->targetFrameID = player->stream.desc.frameCount-1;
			V6_MSG( "Target frame %d\n", player->targetFrameID );
		}
		break;
	case COMMAND_ACTION_PREV_FRAME:
		if ( player->playerState == PLAYER_STATE_STREAM )
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
		if ( player->playerState == PLAYER_STATE_STREAM )
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


	case COMMAND_ACTION_CAMERA_RECENTER:
		PlayerCamera_Recenter( player );
#if V6_USE_HMD == 1
		Hmd_Recenter();
#endif
		V6_MSG( "Camera recentered.\n");
		break;

	case COMMAND_ACTION_TRACE_OPTION_BLOCK:
		player->traceOptions.showBlock = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 1) : !player->traceOptions.showBlock;
		break;
	case COMMAND_ACTION_TRACE_OPTION_LOG:
		player->traceOptions.logReadBack = true;
		break;
	case COMMAND_ACTION_TRACE_OPTION_GRID:
		player->traceOptions.showGrid = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 1) : !player->traceOptions.showGrid;
		break;
	case COMMAND_ACTION_TRACE_OPTION_HISTORY:
		player->traceOptions.showHistory = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 1) : !player->traceOptions.showHistory;
		break;
	case COMMAND_ACTION_TRACE_OPTION_OVERDRAW:
		player->traceOptions.showOverdraw = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 1) : !player->traceOptions.showOverdraw;
		break;
	case COMMAND_ACTION_TRACE_OPTION_TSAA:
		player->traceOptions.noTSAA = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->traceOptions.noTSAA;
		break;
	case COMMAND_ACTION_TRACE_OPTION_SHARPEN_FILTER:
		player->traceOptions.noSharpenFilter = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->traceOptions.noSharpenFilter;
		break;

	case COMMAND_ACTION_PLAYER_OPTION_METRICS:
		player->playerOptions.showMetrics = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->playerOptions.showMetrics;
		break;
	case COMMAND_ACTION_PLAYER_OPTION_UI:
		player->playerOptions.showUI = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->playerOptions.showUI;
		break;
	case COMMAND_ACTION_PLAYER_OPTION_DISABLE_FADE_TO_BLACK:
		player->playerOptions.disableFadeToBlack = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->playerOptions.disableFadeToBlack;
		break;
	case COMMAND_ACTION_PLAYER_OPTION_LOCK_HMD:
		player->playerOptions.lockHMD = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->playerOptions.lockHMD;
		break;
	case COMMAND_ACTION_PLAYER_OPTION_SHOW_HMD_PERF_HUD:
		player->playerOptions.showHMDPerfHUD = commandBuffer.arg[0];
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
	case 0x26: player->keyPlusPressed = keyEvent->pressed; break;
	case 0x28: player->keyMinusPressed = keyEvent->pressed; break;
	}

	if ( !keyEvent->pressed )
		return ;

	switch( keyEvent->key )
	{
	case 0x1B:
		player->commandBuffer.action = COMMAND_ACTION_EXIT;
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
		player->commandBuffer.action = COMMAND_ACTION_PLAYER_OPTION_SHOW_HMD_PERF_HUD;
		player->commandBuffer.arg[0] = keyEvent->key - '0';
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
	case 0x25:
		player->commandBuffer.action = COMMAND_ACTION_PREV_ITEM;
		break;
	case 0x27:
		player->commandBuffer.action = COMMAND_ACTION_NEXT_ITEM;
		break;
	case 0xC0:
		player->commandLineSize = 0;
		V6_MSG( "~" );
		break;
	case 'B':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_BLOCK;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'C':
		player->commandBuffer.action = COMMAND_ACTION_UNLOAD_STREAM;
		break;
	case 'E': 
		player->commandBuffer.action = COMMAND_ACTION_PLAYER_OPTION_DISABLE_FADE_TO_BLACK;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'F':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_SHARPEN_FILTER;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'G':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_GRID;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'H':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_HISTORY;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'I':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_LOG;
		break;
	case 'J':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_TSAA;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'K':
		player->commandBuffer.action = COMMAND_ACTION_PLAYER_OPTION_LOCK_HMD;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'L':
		{
			char filename[256];
			if ( FileDialog_Open( filename, sizeof( filename ), "v6" ) )
			{
				player->commandBuffer.action = COMMAND_ACTION_LOAD_STREAM;
				strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), filename );
			}
		}
		break;
	case 'O':
		player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_OVERDRAW;
		player->commandBuffer.arg[0] = 2;
		break;
	case 0x0D:
	case 'P':
		player->commandBuffer.action = COMMAND_ACTION_PLAY_PAUSE;
		break;
	case 'R':
		player->commandBuffer.action = COMMAND_ACTION_CAMERA_RECENTER;
		break;
	case 'T':
		player->commandBuffer.action = COMMAND_ACTION_PLAYER_OPTION_METRICS;
		player->commandBuffer.arg[0] = 2;
		break;
	case 'U':
		player->commandBuffer.action = COMMAND_ACTION_PLAYER_OPTION_UI;
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

static void Player_OnResizeEvent( Win_s* win )
{
	Player_s* player = (Player_s*)win->owner;

	GPURenderTargetSet_Release( &player->winRenderTargetSet );
	
	GPUSurfaceContext_Resize( win->size.x, win->size.y );

	GPURenderTargetSetCreationDesc_s renderTargetSetCreationDesc = {};
	renderTargetSetCreationDesc.reuseColorRenderTargets[0] = GPUSurfaceContext_Get()->surface;
	renderTargetSetCreationDesc.name = "win";
	renderTargetSetCreationDesc.width = player->win.size.x;
	renderTargetSetCreationDesc.height = player->win.size.y;
	renderTargetSetCreationDesc.supportMSAA = true;
	renderTargetSetCreationDesc.bindable = true;
	renderTargetSetCreationDesc.writable = true;
	renderTargetSetCreationDesc.stereo = false;

	GPURenderTargetSet_Create( &player->winRenderTargetSet, &renderTargetSetCreationDesc );
}

static void Player_OnGamepadButtonEvent( const Gamepad_s* gamepad, GamepadButtons_s leftButtonIsChanged, GamepadButtons_s rightButtonIsChanged )
{
	Player_s* player = (Player_s*)gamepad->owner;

	GamepadButtons_s leftButtonIsPressed;
	leftButtonIsPressed.bits = leftButtonIsChanged.bits & ~gamepad->leftButtonWasDown.bits;

	GamepadButtons_s rightButtonIsPressed;
	rightButtonIsPressed.bits = rightButtonIsChanged.bits & ~gamepad->rightButtonWasDown.bits;

	if ( leftButtonIsPressed.O )
		player->commandBuffer.action = COMMAND_ACTION_EXIT;

	if ( leftButtonIsPressed.L )
		player->commandBuffer.action = COMMAND_ACTION_PREV_ITEM;

	if ( leftButtonIsPressed.R )
		player->commandBuffer.action = COMMAND_ACTION_NEXT_ITEM;

	if ( rightButtonIsPressed.L )
		player->commandBuffer.action = COMMAND_ACTION_PREV_FRAME;

	if ( rightButtonIsPressed.R )
		player->commandBuffer.action = COMMAND_ACTION_NEXT_FRAME;

	if ( rightButtonIsPressed.B )
		player->commandBuffer.action = COMMAND_ACTION_PLAY_PAUSE;

	if ( rightButtonIsPressed.T )
		player->commandBuffer.action = COMMAND_ACTION_BEGIN_FRAME;
	
	if ( rightButtonIsPressed.O )
		player->commandBuffer.action = COMMAND_ACTION_CAMERA_RECENTER;
}

static void Player_ProcessInputs( Player_s* player, float dt )
{
	Gamepad_UpdateState( &player->gamepad );

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
		int keyDeltaY = 0;
		int keyDeltaZ = 0;

		if ( player->keyLeftPressed != player->keyRightPressed )
			keyDeltaX = player->keyLeftPressed ? -1 : 1;

		if ( player->keyMinusPressed != player->keyPlusPressed )
			keyDeltaY = player->keyMinusPressed ? -1 : 1;

		if ( player->keyDownPressed != player->keyUpPressed )
			keyDeltaZ = player->keyDownPressed ? -1 : 1;
	
		if ( keyDeltaX || keyDeltaY || keyDeltaZ )
		{
			player->camera.pos += player->camera.right * (float)keyDeltaX * KEY_TRANSLATION_SPEED * dt;
			player->camera.pos += player->camera.up * (float)keyDeltaY * KEY_TRANSLATION_SPEED * dt;
			player->camera.pos += player->camera.forward * (float)keyDeltaZ * KEY_TRANSLATION_SPEED * dt;
		}
	}
}

static void Player_DrawHUD( Player_s* player, float fadeToBlack )
{
	const u32 lineHeight = FontContext_GetLineHeight( &player->fontContext );

	// middle

	{
		const u32 cursorX = player->mainRenderTargetSet.width / 2;
		u32 cursorY = player->mainRenderTargetSet.height / 2;

		if ( fadeToBlack > 0.0f )
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_Make( 255, 255, 255, Clamp( (u32)(255 * fadeToBlack), 0u, 255u ) ), "Out of range" );
			cursorY += lineHeight;
		}

		if ( player->playerState == PLAYER_STATE_STREAM && player->stream.desc.frameCount > 1 && (u32)player->curFrameID == player->targetFrameID )
		{
			if ( player->targetFrameID == 0 )
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_Make( 255, 255, 255, 255 ), "Press A to play" );
			else if ( player->targetFrameID == player->stream.desc.frameCount-1 )
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_Make( 255, 255, 255, 255 ), "Press A to re-play" );
			else
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_Make( 255, 255, 255, 255 ), "Press A to resume" );
			cursorY += lineHeight;
		}
	}

	FontContext_Draw( &player->fontContext, &player->mainRenderTargetSet, true, true );
}

static void Player_DrawUI( Player_s* player, float averageFPS, const GPUEventDuration_s* eventDurations, u32 eventCount )
{
	const u32 lineHeight = FontContext_GetLineHeight( &player->fontContext );

	// top left

	{
		const u32 cursorX = 8;
		u32 cursorY = lineHeight / 2;

		FontContext_AddLine( &player->fontContext, 8, cursorY, Color_White(), String_Format( "FPS: %3.1f", averageFPS ) );
		cursorY += lineHeight;

		for ( u32 eventRank = 0; eventRank < eventCount; ++eventRank )
		{
			const GPUEventDuration_s* eventDuration = &eventDurations[eventRank];
			const char* txt = String_Format( "%*s%-10s : %5d us", eventDuration->depth * 4, "", eventDuration->name, eventDuration->avgDurationUS );
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), txt );
			cursorY += lineHeight;
		}

#if V6_DUMP_GAMEPAD == 1
		{
			const char* txt = Gamepad_DumpState( &player->gamepad );
			cursorY = FontContext_AddText( &player->fontContext, cursorX, cursorY, lineHeight, Color_White(), txt );
		}
#endif // #if V6_DUMP_GAMEPAD == 1

	}

	// bottom left

	{
		const u32 cursorX = 8;
		u32 cursorY = player->mainRenderTargetSet.height - lineHeight * 2;

		for ( u32 messageOffset = 0; messageOffset < Min( s_ouputMessageCount, s_ouputMessageBufferCount ); ++messageOffset )
		{
			const u32 bufferID = (s_ouputMessageCount - messageOffset - 1) % s_ouputMessageBufferCount;
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), s_ouputMessageBuffers[bufferID] );
			cursorY -= lineHeight;
		}
	}

	// top middle

	{
		const u32 cursorX = player->mainRenderTargetSet.width / 2;
		u32 cursorY = lineHeight / 2;

		if ( player->playerState == PLAYER_STATE_STREAM )
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "Stream: %s", player->stream.name ) );
			cursorY += lineHeight;
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "Frame: %g/%d", player->curFrameID, player->stream.desc.frameCount ) );
			cursorY += lineHeight;
		}
	}

	// top right

#if V6_USE_HMD == 1
	{
		u32 cursorX = player->mainRenderTargetSet.width - 160;
		u32 cursorY = lineHeight / 2;
		
		if ( player->hmdState & HMD_TRACKING_STATE_ON )
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), "HMD: rotation tracked" );
			cursorY += lineHeight;

			if ( player->hmdState & HMD_TRACKING_STATE_POS )
			{
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), "HMD: position tracked" );
				cursorY += lineHeight;
			}
		}
		else
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), "HMD: off" );
			cursorY += lineHeight;
		}
	}
#endif // #if V6_USE_HMD == 1

	FontContext_Draw( &player->fontContext, &player->mainRenderTargetSet, true, false );
}

static void Player_UpdateMetrics( Player_s* player, u32 frameID, const GPUEventDuration_s* eventDurations, u32 eventCount )
{
	const GPUEventDuration_s* eventDraw = nullptr;
	for ( u32 eventRank = 0; eventRank < eventCount; ++eventRank )
	{
		if ( eventDurations[eventRank].id == s_gpuEventDraw )
		{
			eventDraw = &eventDurations[eventRank];
			break;
		}
	}

	if ( !eventDraw )
		return;

	player->frameMetrics.frameID = frameID % FrameMetrics_s::WIDTH;
	player->frameMetrics.data[player->frameMetrics.frameID].drawTimeUS = eventDraw->curDurationUS;
}

static void Player_DrawMetrics( Player_s* player )
{
	V6_GPU_EVENT_SCOPE( s_gpuEventMetrics );

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	// update buffer

	GPUBuffer_Update( &shaderContext->buffers[BUFFER_FRAMEMETRICS], 0, player->frameMetrics.data, FrameMetrics_s::WIDTH );

	// set

	g_deviceContext->CSSetConstantBuffers( hlsl::CBFrameMetricsSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_FRAMEMETRICS].buf );

	g_deviceContext->CSSetShaderResources( HLSL_FRAME_METRICS_SLOT, 1, &shaderContext->buffers[BUFFER_FRAMEMETRICS].srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, &player->mainRenderTargetSet.colorBuffers[0].uav, nullptr );

	g_deviceContext->CSSetShader( shaderContext->computes[COMPUTE_FRAMEMETRICS].m_computeShader, nullptr, 0 );

	const Vec2u frameMetricsRTSize = Vec2u_Make( player->mainRenderTargetSet.width - 16, 200 );
	const Vec2u frameMetricsRTOffset = Vec2u_Make( 8, player->mainRenderTargetSet.height - frameMetricsRTSize.y - 100 - 8 );

	{
		hlsl::CBFrameMetrics* cbFrameMetrics = (hlsl::CBFrameMetrics*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_FRAMEMETRICS] );
		
		cbFrameMetrics->c_frameMetricsRTSize = frameMetricsRTSize;
		cbFrameMetrics->c_frameMetricsRTOffset = frameMetricsRTOffset;
	
		cbFrameMetrics->c_frameMetricsEnd = player->frameMetrics.frameID;
		cbFrameMetrics->c_frameMetricsScale = 25.0f * 0.001f;
		cbFrameMetrics->c_frameMetricsBias = 0.0f;

		cbFrameMetrics->c_frameMetricsMarkerMin = 2000.0f;
		cbFrameMetrics->c_frameMetricsMarkerMid = 4000.0f;
		cbFrameMetrics->c_frameMetricsMarkerMax = 6000.0f;

		GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_FRAMEMETRICS] );
	}

	V6_ASSERT( (frameMetricsRTSize.x & 0x7) == 0 );
	V6_ASSERT( (frameMetricsRTSize.y & 0x7) == 0 );
	const u32 pixelGroupWidth = frameMetricsRTSize.x >> 3;
	const u32 pixelGroupHeight = frameMetricsRTSize.y >> 3;
	g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_FRAME_METRICS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
}

static void Player_CopyToSurface( Player_s* player )
{
	GPUSurfaceContext_s* surfaceContext = GPUSurfaceContext_Get();

	// set

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	g_deviceContext->CSSetConstantBuffers( hlsl::CBComposeSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE].buf );

	g_deviceContext->PSSetSamplers( HLSL_BILINEAR_SLOT, 1, &shaderContext->bilinearSamplerState );

	g_deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, &player->mainRenderTargetSet.colorBuffers[0].srv );
	g_deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, &player->mainRenderTargetSet.colorBuffers[1].srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, &surfaceContext->surface.uav, nullptr );

	g_deviceContext->CSSetShader( shaderContext->computes[COMPUTE_COMPOSESURFACE].m_computeShader, nullptr, 0 );

	{
		hlsl::CBCompose* cbCompose = (hlsl::CBCompose*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE] );
		
		cbCompose->c_composeSurfaceWidth = player->win.size.x;
		const float surfaceWoH = (float)player->win.size.x / player->win.size.y;
		const float frameWoH = (float)player->mainRenderTargetSet.width / player->mainRenderTargetSet.height;
		if ( frameWoH > surfaceWoH )
		{
			const float scale = (float)player->win.size.x / player->mainRenderTargetSet.width;
			const float norm = 1.0f / (scale * player->mainRenderTargetSet.height);
			const float bias = player->win.size.y * norm * 0.5f - 0.5f;
			cbCompose->c_composeFrameUVBias = Vec2_Make( 0.0f, -bias );
			cbCompose->c_composeFrameUVScale = Vec2_Make( 1.0f / player->win.size.x, norm );
		}
		else
		{
			const float scale = (float)player->win.size.y / player->mainRenderTargetSet.height;
			const float norm = 1.0f / (scale * player->mainRenderTargetSet.width);
			const float bias = player->win.size.x * norm * 0.5f - 0.5f;
			cbCompose->c_composeFrameUVBias = Vec2_Make( -bias, 0.0f );
			cbCompose->c_composeFrameUVScale = Vec2_Make( norm, 1.0f / player->win.size.y );
		}

		const Color_s backColor = V6_DARK_GRAY;
		const Color_s borderColor = V6_ORANGE;
		const float inv255 = 1.0f / 255.0f;

		cbCompose->c_composeBackColor = Vec4_Make( backColor.r * inv255, backColor.g * inv255, backColor.b * inv255, backColor.a * inv255 );
		cbCompose->c_composeBorderColor = Vec4_Make( borderColor.r * inv255, borderColor.g * inv255, borderColor.b * inv255, borderColor.a * inv255 );

		GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE] );
	}

	const u32 pixelGroupWidth = HLSL_GROUP_COUNT( player->win.size.x, 8 ) * EYE_COUNT;
	const u32 pixelGroupHeight = HLSL_GROUP_COUNT( player->win.size.y, 8 );
	g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
}

static void Player_ProcessFrame( Player_s* player, u32 frameID, float dt, float averageFPS )
{
	// CPU updates

	String_ResetInternalBuffer();
	
	Player_ProcessInputs( player, dt );

	PlayerCommandBuffer_Process( player );

	// GPU frame

	GPUEvent_BeginFrame( frameID );

	Vec3 frameOrigin = Vec3_Zero();
	float frameYaw = 0.0f;
	if ( player->playerState == PLAYER_STATE_STREAM )
	{
		// update stream GPU data

		if ( player->targetFrameID < player->curFrameID )
			player->curFrameID = (float)player->targetFrameID;
		
		const u32 streamFrameID = (u32)(player->curFrameID + FLT_EPSILON);
		
		{
			V6_GPU_EVENT_SCOPE( s_gpuEventUpdate );
			TraceContext_UpdateFrame( &player->traceContext, streamFrameID, player->stack );
		}

		TraceContext_GetFrameBasis( &player->traceContext, &frameOrigin, &frameYaw );

		if ( streamFrameID < player->targetFrameID )
			player->curFrameID = fmodf( player->curFrameID + (float)player->stream.desc.frameRate / PLAY_RATE, (float)player->stream.desc.frameCount );
	}

	Camera_SetPosOffset( &player->camera, &frameOrigin );
	Camera_SetYawOffset( &player->camera, frameYaw );

	// update view with inputs

	View_s views[EYE_COUNT];

	float fadeToBlack = 0.0f;

#if V6_USE_HMD == 1
	fadeToBlack = 1.0f;

	Hmd_SetPerfHUdMode( player->playerOptions.showHMDPerfHUD );

	HmdRenderTarget_s hmdColorRenderTargets[2];
	HmdEyePose_s hmdEyePoses[2];

	player->hmdState = Hmd_BeginRendering( hmdColorRenderTargets, hmdEyePoses, player->camera.znear, player->camera.zfar );
	if ( player->hmdState & HMD_TRACKING_STATE_ON )
	{
		Vec3 eyePos[2];
		eyePos[0] = hmdEyePoses[0].lookAt.GetTranslation();
		eyePos[1] = hmdEyePoses[1].lookAt.GetTranslation();
		if ( player->playerOptions.lockHMD )
			Camera_SetStereoUsingIPD( &player->camera, IPD );
		else
			Camera_SetStereoUsingOrientation( &player->camera, &hmdEyePoses[0].lookAt, eyePos );
		Camera_UpdateBasis( &player->camera );

		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
		{
			player->mainRenderTargetSet.colorBuffers[eye].tex = (ID3D11Texture2D*)hmdColorRenderTargets[eye].tex;
			player->mainRenderTargetSet.colorBuffers[eye].rtv = (ID3D11RenderTargetView*)hmdColorRenderTargets[eye].rtv;
			player->mainRenderTargetSet.colorBuffers[eye].srv = (ID3D11ShaderResourceView*)hmdColorRenderTargets[eye].srv;
			player->mainRenderTargetSet.colorBuffers[eye].uav = (ID3D11UnorderedAccessView*)hmdColorRenderTargets[eye].uav;

			ViewProjection_s viewProjection;
			viewProjection.projMatrix = hmdEyePoses[eye].projection;
			viewProjection.tanHalfFOVLeft = hmdEyePoses[eye].tanHalfFOVLeft;
			viewProjection.tanHalfFOVRight = hmdEyePoses[eye].tanHalfFOVRight;
			viewProjection.tanHalfFOVUp = hmdEyePoses[eye].tanHalfFOVUp;
			viewProjection.tanHalfFOVDown = hmdEyePoses[eye].tanHalfFOVDown;

			Camera_MakeView( &views[eye], &player->camera, eye, &viewProjection );
		}

		if ( player->hmdState & HMD_TRACKING_STATE_POS )
			fadeToBlack = 0.0f;
	}
	else
#endif // #if V6_USE_HMD == 1
	{
		Camera_SetStereoUsingIPD( &player->camera, IPD );
		Camera_UpdateBasis( &player->camera );
		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
			Camera_MakeView( &views[eye], &player->camera, eye, nullptr );
	}

	if ( player->playerState == PLAYER_STATE_STREAM )
	{
		Vec3 centerEye = Vec3_Zero();
		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
			centerEye += views[eye].org * (1.0f / EYE_COUNT);
		const Vec3 eyeDistanceToOrigin = Abs( centerEye - player->traceContext.frameState.origin );
		fadeToBlack = Max( fadeToBlack, Clamp( eyeDistanceToOrigin.Max() - (CODEC_HEAD_ROOM_SIZE - 5.0f), 0.0f, 5.0f ) / 5.0f );
	}

	if ( player->playerOptions.disableFadeToBlack )
		fadeToBlack = 0.0f;

	// draw

	{
		V6_GPU_EVENT_SCOPE( s_gpuEventDraw );

		if ( player->playerState == PLAYER_STATE_STREAM )
			TraceContext_DrawFrame( &player->traceContext, &player->mainRenderTargetSet, views, &player->traceOptions, fadeToBlack );
		else
			PlayerScene_DrawGrid( player, views );
	}

	// draw UI

	{
		GPUEventDuration_s* eventDurations;
		const u32 eventCount = GPUEvent_UpdateDurations( &eventDurations );

		Player_DrawHUD( player, fadeToBlack );

		if ( player->playerOptions.showUI )
			Player_DrawUI( player, averageFPS, eventDurations, eventCount );

		Player_UpdateMetrics( player, frameID, eventDurations, eventCount );
		if ( player->playerOptions.showMetrics )
			Player_DrawMetrics( player );

		if ( player->traceOptions.logReadBack )
		{
			for ( u32 eventRank = 0; eventRank < eventCount; ++eventRank )
			{
				const GPUEventDuration_s* eventDuration = &eventDurations[eventRank];
				V6_MSG( "%*s%-10s : %5d us\n", eventDuration->depth * 4, "", eventDuration->name, eventDuration->avgDurationUS );
			}
		}
	}

	// present draw

#if V6_USE_HMD == 1
	if ( player->hmdState & HMD_TRACKING_STATE_ON )
	{
		V6_GPU_EVENT_SCOPE( s_gpuEventHMD );
		Hmd_EndRendering();
	}
#endif // #if V6_USE_HMD == 

#if V6_ENABLE_MIRRORING != 0
	if ( player->playerState == PLAYER_STATE_STREAM )
	{
		V6_GPU_EVENT_SCOPE( s_gpuEventCopy );
		Player_CopyToSurface( player );
	}
#endif // #if V6_ENABLE_MIRRORING != 0
	else
	{
		V6_GPU_EVENT_SCOPE( s_gpuEventList );
		PlayerScene_DrawList( player );
	}

	{
		V6_GPU_EVENT_SCOPE( s_gpuEventPresent );
		GPUSurfaceContext_Present();
	}

	GPUEvent_EndFrame();

	player->traceOptions.logReadBack = false;
}

static bool Player_Create( Player_s* player, u32 defaultWidth, u32 defaultHeight, float defaultTanHalfFovPerPixel, IAllocator* heap, IStack* stack )
{
	memset( player, 0, sizeof( *player ) );

#if V6_USE_HMD == 1
	if ( !v6::Hmd_Init() )
	{
		V6_ERROR( "Call to Hmd_Init failed!\n" );
		return false;
	}

	v6::Vec2 recommendedFOV = v6::Hmd_GetRecommendedFOV();
	v6::u32 renderTargetHalfSize = defaultHeight >> 1;
	u32 width = (u32)(renderTargetHalfSize * recommendedFOV.x);
	u32 height = (u32)(renderTargetHalfSize * recommendedFOV.y);
	width = (width + 7) & ~7;
	height = (height + 7) & ~7;
#else
	const u32 width = defaultWidth;
	const u32 height = defaultHeight;
	V6_MSG( "rt.resolution: %dx%d\n", width, height );
#endif // #if V6_USE_HMD == 1

	u32 windowWidth = 1280;
#if V6_ENABLE_MIRRORING == 2
	windowWidth *= EYE_COUNT;
#endif
	const u32 windowHeight = 720;

	player->commandLineSize = (u32)-1;
	player->heap = heap;
	player->stack = stack;

	if ( !Win_Create( &player->win, player, "V6 Player (pre-alpha rev211)", 40, 40, windowWidth, windowHeight, WIN_FLAG_IS_MAIN | WIN_FLAG_RESIZABLE ) )
		return false;

	Win_RegisterKeyEvent( &player->win, Player_OnKeyEvent );
	Win_RegisterMouseEvent( &player->win, Player_OnMouseEvent );
	Win_RegisterResizeEvent( &player->win, Player_OnResizeEvent );

	Gamepad_Init( &player->gamepad, 0, player );
	Gamepad_RegisterButtonEvent( &player->gamepad, Player_OnGamepadButtonEvent );

	PlayerDevice_Create( player, width, height );

#if V6_USE_HMD == 1
	if ( !v6::Hmd_CreateResources( GPUDevice_Get(), &Vec2i_Make( width, height ), false ) )
	{
		V6_ERROR( "Call to Hmd_CreateResources failed!\n" );
		PlayerDevice_Release( player );
		return false;
	}
#endif // #if V6_USE_HMD == 1

	PlayerStreamItem_Create( player );

	PlayerScene_Create( player, defaultTanHalfFovPerPixel );

	return true;
}

static void Player_Release( Player_s* player )
{
	if ( player->playerState == PLAYER_STATE_STREAM )
		PlayerStream_Release( player );
	PlayerScene_Release( player );
	PlayerStreamItem_Release( player );
	Gamepad_Release( &player->gamepad );
#if V6_USE_HMD == 1
	Hmd_ReleaseResources();
	Hmd_Shutdown();
#endif // #if V6_USE_HMD == 1
	PlayerDevice_Release( player );
}

END_V6_NAMESPACE

//----------------------------------------------------------------------------------------------------

int main( int argc, char** argv )
{
	v6::CHeap heap;
	v6::Stack stack( &heap, 400 * 1024 * 1024 );

	v6::Player_s* player = stack.newInstance< v6::Player_s >();

#if V6_ENABLE_HMD == 1
	const v6::u32 defaultWidth = 1024;
	const v6::u32 defaultHeight = 1024;
#else

#if 1
	// DK2
	const v6::u32 defaultWidth = 1104;
	const v6::u32 defaultHeight = 1368;
#elif 0
	// DK2 / 2
	const v6::u32 defaultWidth = 1104 / 2;
	const v6::u32 defaultHeight = 680;
#else
	// DK2 / 4
	const v6::u32 defaultWidth = 272;
	const v6::u32 defaultHeight = 336;
#endif

#endif

#if V6_ENABLE_HMD == 1
	V6_STATIC_ASSERT( defaultHeight <= 1024 );
#endif // #if V6_ENABLE_HMD == 1

	const float defaultTanHalfFovPerPixel = v6::Tan( v6::DegToRad( 45.0f ) ) / 1024;
	// const float defaultTanHalfFovPerPixel = v6::Tan( v6::DegToRad( 45.0f ) ) / 512;
	// const float defaultTanHalfFovPerPixel = v6::Tan( v6::DegToRad( 45.0f ) ) / 256;

	if ( !v6::Player_Create( player, defaultWidth, defaultHeight, defaultTanHalfFovPerPixel, &heap, &stack ) )
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
