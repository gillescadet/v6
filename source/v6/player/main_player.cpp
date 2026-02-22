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
#include <v6/core/ini.h>
#include <v6/core/memory.h>
#include <v6/core/platform.h>
#include <v6/core/random.h>
#include <v6/core/stream.h>
#include <v6/core/string.h>
#include <v6/core/thread.h>
#include <v6/core/time.h>
#include <v6/core/win.h>
#include <v6/graphic/font.h>
#include <v6/graphic/gpu.h>
#include <v6/graphic/hmd.h>
#include <v6/graphic/scene.h>
#include <v6/graphic/trace.h>
#include <v6/graphic/view.h>
#include <v6/player/grid_tile_512_bc1.h>
#include <v6/player/missing_stream_256_bc1.h>
#include <v6/player/player_shaders.h>
#include <v6/player/player_shared.h>
#include <v6/player/stream_overlay_image_64_bc1.h>
#include <v6/player/stream_overlay_movie_64_bc1.h>

#define V6_VERSION_MAJOR		1
#define V6_VERSION_MINOR		0

#define V6_DEV					1
#define V6_D3D_DEBUG			0
#define V6_STEREO				1
#define V6_ENABLE_HMD			0
#define V6_USE_HMD				(V6_ENABLE_HMD == 1 && V6_STEREO == 1)
#define V6_DUMP_GAMEPAD			0
#define V6_LIMIT_FRAME_RATE		1
#define V6_DEBUG_HMD_UI			0

BEGIN_V6_NAMESPACE

static const CPUEventID_t s_cpuEventFrame		= CPUEvent_Register( "Frame", true );
static const CPUEventID_t s_cpuEventGetSequence	= CPUEvent_Register( "GetSequence", true );
static const CPUEventID_t s_cpuEventStream		= CPUEvent_Register( "Stream", true );
static const CPUEventID_t s_cpuEventUpdate		= CPUEvent_Register( "Update", true );
static const CPUEventID_t s_cpuEventDraw		= CPUEvent_Register( "Draw", true );
static const GPUEventID_t s_cpuEventHMD			= CPUEvent_Register( "HMD", true );
static const CPUEventID_t s_cpuEventPresent		= CPUEvent_Register( "Present", true );

static const GPUEventID_t s_gpuEventStream		= GPUEvent_Register( "Stream", true );
static const GPUEventID_t s_gpuEventUpdate		= GPUEvent_Register( "Update", true );
static const GPUEventID_t s_gpuEventDraw		= GPUEvent_Register( "Draw", true );
static const GPUEventID_t s_gpuEventMetrics		= GPUEvent_Register( "Metrics", true );
static const GPUEventID_t s_gpuEventHMD			= GPUEvent_Register( "HMD", true );
static const GPUEventID_t s_gpuEventCopy		= GPUEvent_Register( "Copy", true );
static const GPUEventID_t s_gpuEventList		= GPUEvent_Register( "List", true );
static const GPUEventID_t s_gpuEventPresent		= GPUEvent_Register( "Present", true );

extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_deviceContext;

static const char*	COMPANY_NAME					= "SupraWings";
static const char*	PRODUCT_NAME					= "Dragonfly Player";
static const char*	INI_FILE						= "player.ini";
static const char*	LOG_FILE						= "player.log";
static const char*	PLATFORM_APP_ID					= (V6_DEV == 1) ? "" : "1276880609039522";

static const u32	OVERLAY_WIDTH					= 64; 

static const u32	PARTICLE_COUNT					= 1024; 

static const u32	RENDER_TARGET_HEIGHT_AT_90_LOW	= 1200;
static const u32	RENDER_TARGET_HEIGHT_AT_90_HIGH	= 1400;
static const float	ZNEAR_DEFAULT					= 10.0f;
static const float	ZFAR_DEFAULT					= 5000.0f;
static const float	MOUSE_ROTATION_SPEED			= 0.5f;
static const float	KEY_TRANSLATION_SPEED			= 200.0f;
static const float	LISTVIEW_SCROLL_SPEED			= 3000.0f;
static const float	LISTVIEW_ROTATE_SPEED			= 2.0f;
static const float	USER_HEIGHT						= 100.0f;
#if V6_STEREO == 1
static const u32 EYE_COUNT							= 2;
static const float IPD								= 6.5f;
#else
static const u32 EYE_COUNT							= 1;
static const float IPD								= 0.0f;
#endif
static const char* PLAYER_STREAM_ITEM_FOLDER		= "media";
static const float SEQUENCE_PREFTECH_DURATION		= 4.0f;

static const Color_s V6_TRANSPARENT			= Color_Make(   0,   0,   0, 0   );
static const Color_s V6_BLACK				= Color_Make(   0,   0,   0, 255 );
static const Color_s V6_WHITE				= Color_Make( 255, 255, 255, 255 );
static const Color_s V6_GRID_COLOR			= Color_Make(  10,  10,  10, 255 );
static const Color_s V6_DARK_GRAY			= Color_Make(  41,  41,  41, 255 );
static const Color_s V6_LIGHT_GRAY			= Color_Make( 204, 204, 204, 255 );
static const Color_s V6_ORANGE				= Color_Make( 226,  73,  27, 255 );
static const Color_s V6_HMD_FONT_COLOR		= Color_Make( 150, 150, 150, 255 );

static const u32 s_ouputMessageBufferCount = 3;
static char s_ouputMessageBuffers[s_ouputMessageBufferCount][4096] = {};
static u32 s_ouputMessageCount = 0;

enum
{
	CONSTANT_BUFFER_ARROW,
	CONSTANT_BUFFER_PARTICLE,
	CONSTANT_BUFFER_LIST_ITEM,
	CONSTANT_BUFFER_ENV,
	CONSTANT_BUFFER_COMPOSE,
	CONSTANT_BUFFER_FRAMEMETRICS,

	CONSTANT_BUFFER_COUNT
};

enum
{
	BUFFER_FRAMEMETRICSEVENT,
	BUFFER_FRAMEMETRICSTIME,

	BUFFER_COUNT
};

enum
{
	SHADER_ARROW,
	SHADER_PARTICLE,
	SHADER_LIST,
	SHADER_ENV,

	SHADER_COUNT
};

enum
{
	COMPUTE_COMPOSESURFACE,
	COMPUTE_FRAMEMETRICS,

	COMPUTE_COUNT
};

enum
{
	FRAME_PROFILE_GPU_UPDATE_AND_DRAW,
	FRAME_PROFILE_GPU_PRESENT,

	FRAME_PROFILE_CPU_DECODE_SEQUENCE,
	FRAME_PROFILE_CPU_GET_SEQUENCE,
	FRAME_PROFILE_CPU_STREAM,
	FRAME_PROFILE_CPU_UPDATE,

	FRAME_PROFILE_COUNT
};

enum FrameMetricsEvent_e
{
	FRAME_METRICS_EVENT_LOADING,
	FRAME_METRICS_EVENT_STREAMING0,
	FRAME_METRICS_EVENT_STREAMING1,
	FRAME_METRICS_EVENT_3,

	FRAME_METRICS_EVENT_COUNT
};

enum ListViewLayout_e
{
	LISTVIEW_LAYOUT_DESKTOP,
	LISTVIEW_LAYOUT_HMD,

	LISTVIEW_LAYOUT_COUNT,
};

struct StreamItem_s
{
	char			filename[256];
	char			title[256];
	StreamItem_s*	next;
	u8*				iconTextureData;
	u32				iconTextureSize;
	float			duration;
	u32				textureID;
	struct
	{
		u32			entityID;
		u32			row;
		u32			col;
	}				layouts[LISTVIEW_LAYOUT_COUNT];
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
	static const u32			WIDTH = HLSL_FRAME_METRICS_WIDTH;
	u32							frameID;
	u32							pendingEvents;
	struct
	{
		const char*					name;
		void*						dataTimeBuffer;
		hlsl::FrameMetricsTime_s*	dataTime;
		u32							drawMaxTimeUS;
		float						verticalMax;
		float						verticalUnit;
	}							profiles[FRAME_PROFILE_COUNT];
	void*						dataEventBuffer;
	hlsl::FrameMetricsEvent_s*	dataEvent;
};

enum CommandAction_e
{
	COMMAND_ACTION_NONE,
	
	COMMAND_ACTION_EXIT,

	COMMAND_ACTION_COMMAND_LINE,

	COMMAND_ACTION_LOAD_STREAM,
	COMMAND_ACTION_UNLOAD_STREAM,
	COMMAND_ACTION_RELOAD_STREAMS,
	
	COMMAND_ACTION_MOVE_SELECTION_TO_CURSOR,
	COMMAND_ACTION_MOVE_SELECTION_TO_LEFT,
	COMMAND_ACTION_MOVE_SELECTION_TO_RIGHT,
	COMMAND_ACTION_MOVE_SELECTION_TO_BOTTOM,
	COMMAND_ACTION_MOVE_SELECTION_TO_TOP,

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
	COMMAND_ACTION_PLAYER_OPTION_HIDE_HUD,
	COMMAND_ACTION_PLAYER_OPTION_LOCK_HMD,
	COMMAND_ACTION_PLAYER_OPTION_MIRROR_HMD,
	COMMAND_ACTION_PLAYER_OPTION_SHOW_HMD_PERF_HUD,
};

enum
{
	PLAYER_INVALID_SEQUENCE_ID = 0xFFFFFFFF
};

enum PlayerState_e
{
	PLAYER_STATE_LIST,
	PLAYER_STATE_STREAM
};

enum PlayerSequenceStatus_e
{
	SEQUENCE_STATUS_FAILED,
	SEQUENCE_STATUS_LOADING,
	SEQUENCE_STATUS_STREAMING,
	SEQUENCE_STATUS_READY,
	SEQUENCE_STATUS_NONE
};

enum PlayerController_e
{
	PLAYER_CONTROLLER_NONE,
	PLAYER_CONTROLLER_REMOTE,
	PLAYER_CONTROLLER_XBOX,

	PLAYER_CONTROLLER_COUNT
};

struct CommandBuffer_s
{
	CommandAction_e			action;
	union
	{
		char				arg[256];
		int					integerArgs[64];
	};
};

struct PlayerOptions_s
{
	u32						showMetricsProfile;
	bool					showUI;
	bool					hideHUD;
	bool					lockHMD;
	bool					mirrorHMD;
	u32						showHMDPerfHUD;
};

struct Player_s;

struct ListViewScene_s : public Scene_s
{
	static const u32		OBJECT_MAX_COUNT = 256;
	Player_s*				player;
	FontObject_s			fontObjects[OBJECT_MAX_COUNT];
	StreamItem_s*			firstStreamItem;
	StreamItem_s*			selectedStreamItem;
	u32						streamItemCount;
	u32						particleEntityID;
	u32						arrowEntityIDs[2];
	struct
	{
		u32					colCount;
		u32					rowCount;
		float				scrollOffset;
		float				scrollSpeed;
	}						layouts[LISTVIEW_LAYOUT_COUNT];
};

struct Player_s
{
	IAllocator*						heap;
	IStack*							stack;
	BlockAllocator_s				streamItemAllocator;
	CommandBuffer_s					commandBuffer;
	Win_s							win;
	Gamepad_s						gamepad;
	Ini_s							ini;
	GPURenderTargetSet_s			winRenderTargetSet;
	GPURenderTargetSet_s			createdRenderTargetSet;
	GPURenderTargetSet_s			mainRenderTargetSet;
	Camera_s						camera;
	ListViewScene_s					sceneListView;
	VideoStream_s					stream;
	VideoStreamPrefetcher_s			prefetcher;
	CFileReader						fileReader;
	FontContext_s					fontContext;
	TraceResource_s					traceRes;
	TraceContext_s					traceContext;
	TraceOptions_s					traceOptions;
	PlayerOptions_s					playerOptions;
	FrameMetrics_s					frameMetrics;
	char							appDataPath[256];
	Vec3							frameOrigin;
	float							frameYaw;
	float							curFrameID;
	u32								targetFrameID;
	PlayerState_e					playerState;
	u32								sequenceID;
	PlayerSequenceStatus_e			sequenceStatus[2];
	b32								sequenceFillBuffer;
	u32								presentRate;
#if V6_USE_HMD == 1
	u32								hmdTackingState;
	HmdStatus_s						hmdStatus;
#endif // #if V6_USE_HMD ==		1
	PlayerController_e				lastUsedController;
	float							time;

	// inputs
	bool							mouseRightPressed;
	float							mouseDeltaX;
	float							mouseDeltaY;
	int								keyLeftPressed;
	int								keyRightPressed;
	int								keyUpPressed;
	int								keyDownPressed;
	int								keyPlusPressed;
	int								keyMinusPressed;
	char							commandLine[256];
	u32								commandLineSize;
	bool							recentered;
	bool							navigated;
};

//----------------------------------------------------------------------------------------------------

static void FrameTimer_Init( FrameTimer_s* frameTimer, float fpsMax, float dtMax )
{
	frameTimer->frameID = 0;
	frameTimer->frameTickLast = Tick_GetCount();
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

	const u64 frameTick = Tick_GetCount();
	u64 frameUpdatedTick = frameTick;
	u64 frameDelta;
	for (;;)
	{
		frameDelta = frameUpdatedTick - frameTimer->frameTickLast;
		frameInfo->dt = Min( Tick_ConvertToSeconds( frameDelta ), frameTimer->dtMax );
#if V6_LIMIT_FRAME_RATE == 1
		if ( frameInfo->dt + 0.001f >= frameTimer->dtMin )
#endif
			break;
		Thread_Sleep( 1 );
		frameUpdatedTick = Tick_GetCount();
	}
	frameTimer->frameTickLast = frameUpdatedTick;
	
	const u32 frameDurationRank = frameTimer->frameID % 32;
	frameTimer->frameDurationSum -= frameTimer->frameDurations[frameDurationRank];
	frameTimer->frameDurationSum += frameDelta;
	frameTimer->frameDurations[frameDurationRank] = frameDelta;
	
	const float avgFrameDuration = Tick_ConvertToSeconds( frameTimer->frameDurationSum ) / 32.0f;
	frameInfo->averageFPS = avgFrameDuration < FLT_EPSILON ? 0.0f : 1.0f / avgFrameDuration; 

	++frameTimer->frameID;
}

//----------------------------------------------------------------------------------------------------

static bool Player_IsUsingHMD( const Player_s* player )
{
#if V6_USE_HMD == 1
	return player->hmdStatus.hmdMounted && player->hmdStatus.isVisible;
#else
	return false;
#endif
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

static void PlayerMaterial_DrawParticle( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 layoutType )
{
	if ( layoutType != LISTVIEW_LAYOUT_HMD )
		return;

	Player_s* player = ((ListViewScene_s*)scene)->player;

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	Mat4x4 objectToWorldMatrix = Mat4x4_RotationY( entity->yaw );
	Mat4x4_PreScale( &objectToWorldMatrix, entity->scale );
	Mat4x4_SetTranslation( &objectToWorldMatrix, entity->pos );

	Mat4x4 objectToViewMatrix;
	Mat4x4_Mul( &objectToViewMatrix, view->viewMatrix, objectToWorldMatrix );
	
	Mat4x4 objectToProjMatrix;
	Mat4x4_Mul( &objectToProjMatrix, view->projMatrix, objectToViewMatrix );

	const Color_s color = V6_DARK_GRAY;
	const float inv255 = 1.0f / 255.0f;

	hlsl::CBParticle* cbParticle = (hlsl::CBParticle*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_PARTICLE] );

	cbParticle->c_particleMatRow0 = objectToProjMatrix.m_rows[0];
	cbParticle->c_particleMatRow1 = objectToProjMatrix.m_rows[1];
	cbParticle->c_particleMatRow2 = objectToProjMatrix.m_rows[2];
	cbParticle->c_particleMatRow3 = objectToProjMatrix.m_rows[3];

	cbParticle->c_particleColor = Vec4_Make( color.r * inv255, color.g * inv255, color.b * inv255, 1.0f );
	
	cbParticle->c_particleTime = player->time;
	cbParticle->c_particleHoverW = (float)player->mainRenderTargetSet.height / player->mainRenderTargetSet.width;

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_PARTICLE] );
	
	static const void* nulls[8] = {};

	g_deviceContext->VSSetConstantBuffers( hlsl::CBParticleSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_PARTICLE].buf );
	g_deviceContext->PSSetConstantBuffers( hlsl::CBParticleSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_PARTICLE].buf );

	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &shaderContext->shaders[SHADER_PARTICLE];
	GPUMesh_Draw( mesh, PARTICLE_COUNT, shader );
}

static void PlayerMaterial_DrawArrow( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 layoutType )
{
	if ( layoutType != LISTVIEW_LAYOUT_HMD )
		return;

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	Mat4x4 objectToWorldMatrix = Mat4x4_RotationY( entity->yaw );
	Mat4x4_PreScale( &objectToWorldMatrix, entity->scale );
	Mat4x4_SetTranslation( &objectToWorldMatrix, entity->pos );

	Mat4x4 objectToViewMatrix;
	Mat4x4_Mul( &objectToViewMatrix, view->viewMatrix, objectToWorldMatrix );
	
	Mat4x4 objectToProjMatrix;
	Mat4x4_Mul( &objectToProjMatrix, view->projMatrix, objectToViewMatrix );

	hlsl::CBArrow* cbArrow = (hlsl::CBArrow*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_ARROW] );

	const Color_s color = V6_DARK_GRAY;
	const float inv255 = 1.0f / 255.0f;

	cbArrow->c_arrowMatRow0 = objectToProjMatrix.m_rows[0];
	cbArrow->c_arrowMatRow1 = objectToProjMatrix.m_rows[1];
	cbArrow->c_arrowMatRow2 = objectToProjMatrix.m_rows[2];
	cbArrow->c_arrowMatRow3 = objectToProjMatrix.m_rows[3];
	cbArrow->c_arrowColor = Vec4_Make( color.r * inv255, color.g * inv255, color.b * inv255, color.a * inv255 );

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_ARROW] );
	
	static const void* nulls[8] = {};

	g_deviceContext->VSSetConstantBuffers( hlsl::CBArrowSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_ARROW].buf );
	g_deviceContext->PSSetConstantBuffers( hlsl::CBArrowSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_ARROW].buf );

	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &shaderContext->shaders[SHADER_ARROW];
	GPUMesh_Draw( mesh, 1, shader );
}

static void PlayerMaterial_DrawList( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 layoutType )
{
	Player_s* player = ((ListViewScene_s*)scene)->player;

	const StreamItem_s* entityItem = (StreamItem_s*)entity->owner;
	const Entity_s* layoutEntity = &player->sceneListView.entities[entityItem->layouts[layoutType].entityID];
	if ( layoutEntity != entity )
		return;
	
	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	Mat4x4 objectToWorldMatrix = Mat4x4_RotationY( entity->yaw );
	Mat4x4_PreScale( &objectToWorldMatrix, entity->scale );
	Mat4x4_SetTranslation( &objectToWorldMatrix, entity->pos );

	Mat4x4 objectToViewMatrix;
	Mat4x4_Mul( &objectToViewMatrix, view->viewMatrix, objectToWorldMatrix );
	
	Mat4x4 objectToProjMatrix;
	Mat4x4_Mul( &objectToProjMatrix, view->projMatrix, objectToViewMatrix );

	hlsl::CBList* cbList = (hlsl::CBList*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM] );

	const Color_s color = (entityItem == player->sceneListView.selectedStreamItem) ? V6_ORANGE : V6_LIGHT_GRAY;
	const float inv255 = 1.0f / 255.0f;

	cbList->c_listMatRow0 = objectToProjMatrix.m_rows[0];
	cbList->c_listMatRow1 = objectToProjMatrix.m_rows[1];
	cbList->c_listMatRow2 = objectToProjMatrix.m_rows[2];
	cbList->c_listMatRow3 = objectToProjMatrix.m_rows[3];
	cbList->c_listColor = Vec4_Make( color.r * inv255, color.g * inv255, color.b * inv255, color.a * inv255 );

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM] );
	
	static const void* nulls[8] = {};

	g_deviceContext->VSSetConstantBuffers( hlsl::CBListSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM].buf );
	g_deviceContext->PSSetConstantBuffers( hlsl::CBListSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM].buf );

	g_deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &shaderContext->trilinearSamplerState );

	if ( material->textureIDs[0] != Material_s::TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[0]];
		g_deviceContext->PSSetShaderResources( HLSL_ALBEDO_SLOT, 1, &texture->srv );
	}
	else
	{
		g_deviceContext->PSSetShaderResources( HLSL_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	}

	if ( material->textureIDs[1] != Material_s::TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[1]];
		g_deviceContext->PSSetShaderResources( HLSL_OVERLAY_SLOT, 1, &texture->srv );
	}
	else
	{
		g_deviceContext->PSSetShaderResources( HLSL_OVERLAY_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	}
	
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &shaderContext->shaders[SHADER_LIST];
	GPUMesh_Draw( mesh, 1, shader );

	g_deviceContext->PSSetShaderResources( HLSL_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->PSSetShaderResources( HLSL_OVERLAY_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
}

static void PlayerMaterial_DrawEnv( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags )
{
	Player_s* player = ((ListViewScene_s*)scene)->player;

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	Mat4x4 objectToWorldMatrix = Mat4x4_RotationY( entity->yaw );
	Mat4x4_PreScale( &objectToWorldMatrix, entity->scale );
	Mat4x4_SetTranslation( &objectToWorldMatrix, entity->pos );

	Mat4x4 objectToViewMatrix;
	Mat4x4_Mul( &objectToViewMatrix, view->viewMatrix, objectToWorldMatrix );
	
	Mat4x4 objectToProjMatrix;
	Mat4x4_Mul( &objectToProjMatrix, view->projMatrix, objectToViewMatrix );

	hlsl::CBEnv* cbEnv = (hlsl::CBEnv*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_ENV] );

	cbEnv->c_envMatRow0 = objectToProjMatrix.m_rows[0];
	cbEnv->c_envMatRow1 = objectToProjMatrix.m_rows[1];
	cbEnv->c_envMatRow2 = objectToProjMatrix.m_rows[2];
	cbEnv->c_envMatRow3 = objectToProjMatrix.m_rows[3];

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_ENV] );
	
	static const void* nulls[8] = {};

	g_deviceContext->VSSetConstantBuffers( hlsl::CBEnvSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_ENV].buf );
	g_deviceContext->PSSetConstantBuffers( hlsl::CBEnvSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_ENV].buf );

	g_deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &shaderContext->trilinearSamplerState );

	if ( material->textureIDs[0] != Material_s::TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[0]];
		g_deviceContext->PSSetShaderResources( HLSL_ALBEDO_SLOT, 1, &texture->srv );
	}
	else
	{
		g_deviceContext->PSSetShaderResources( HLSL_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	}
	
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &shaderContext->shaders[SHADER_ENV];
	GPUMesh_Draw( mesh, 1, shader );

	g_deviceContext->PSSetShaderResources( HLSL_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
}

//----------------------------------------------------------------------------------------------------

static void PlayerScene_CreateParticleMesh( GPUMesh_s* mesh, u32 particleCount, float minRadius, float maxRadius, float speedMin, float speedMax, float sizeMin, float sizeMax, IStack* stack )
{
	ScopedStack scopedStack( stack );

	struct ParticleVertex_s { Vec3 pos; Vec3 params; };
	
	ParticleVertex_s* vertices = stack->newArray< ParticleVertex_s >( particleCount, "PlayerParticleVertex" );

	for ( u32 vertexID = 0; vertexID < particleCount; ++vertexID )
	{
		ParticleVertex_s* vertex = &vertices[vertexID];
		
		const float radius = Lerp( minRadius, maxRadius, RandFloat() );
		vertex->pos = RandSphere() * radius;
		vertex->params.x = RandFloat() * V6_PI;
		vertex->params.y = Lerp( speedMin, speedMax, RandFloat() ) * V6_PI;
		vertex->params.z = Lerp( sizeMin, sizeMax, RandFloat() );
	}

	u16 indices[4] = { 0, 1, 2, 3 };

	GPUMesh_Create( mesh, vertices, particleCount, sizeof( ParticleVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_INSTANCED, indices, V6_ARRAY_COUNT( indices ), sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
}

static void PlayerScene_CreateItemMesh( GPUMesh_s* mesh, float margin )
{
	struct ItemVertex_s { Vec3 pos; Vec3 uv; } vertices[16];

	{
		auto EmitVertex = [ &vertices ]( u32 vertexID, float u, float v, float textureSlot, float x = -2.0f, float y = -2.0f )
		{
			vertices[vertexID].pos.x = x == -2.0f ? u * 2.0f - 1.0f : x;
			vertices[vertexID].pos.y = y == -2.0f ? v * 2.0f - 1.0f : y;
			vertices[vertexID].pos.z = 0.0f;
			vertices[vertexID].uv.x = u;
			vertices[vertexID].uv.y = v;
			vertices[vertexID].uv.z = textureSlot;
		};

		const float low = margin;
		const float high = 1.0f - margin;

		EmitVertex(	 0, 0.0f, 0.0f, -1 );
		EmitVertex(	 1, 1.0f, 0.0f, -1 );
		EmitVertex(	 2, 0.0f, 1.0f, -1 );
		EmitVertex(	 3, 1.0f, 1.0f, -1 );
		EmitVertex(	 4,  low,  low, -1 );
		EmitVertex(	 5, high,  low, -1 );
		EmitVertex(	 6,  low, high, -1 );
		EmitVertex(	 7, high, high, -1 );

		EmitVertex(	 8,  low,  low, 0 );
		EmitVertex(	 9, high,  low, 0 );
		EmitVertex( 10,  low, high, 0 );
		EmitVertex( 11, high, high, 0 );

		const float lowOverlay = 0.65f;
		const float highOverlay = 0.90f;

		EmitVertex(	12, 0, 1, 1,  lowOverlay, -lowOverlay );
		EmitVertex(	13, 1, 1, 1, highOverlay, -lowOverlay );
		EmitVertex( 14, 0, 0, 1,  lowOverlay, -highOverlay );
		EmitVertex( 15, 1, 0, 1, highOverlay, -highOverlay );
	}
	
	u16 indices[6 * 6];

	{
		auto EmitQuad = [ &indices ]( u16 quadID, u32 v0, u32 v1, u32 v2, u32 v3 )
		{
			u16 indexID = quadID * 6;
			indices[indexID+0] = v0;
			indices[indexID+1] = v1;
			indices[indexID+2] = v2;
			indices[indexID+3] = v2;
			indices[indexID+4] = v1;
			indices[indexID+5] = v3;
		};

		EmitQuad( 0, 0, 1, 4, 5 );
		EmitQuad( 1, 1, 3, 5, 7 );
		EmitQuad( 2, 3, 2, 7, 6 );
		EmitQuad( 3, 2, 0, 6, 4 );
		
		EmitQuad( 4, 8, 9, 10, 11 );
		
		EmitQuad( 5, 12, 13, 14, 15 );
	}

	GPUMesh_Create( mesh, vertices, V6_ARRAY_COUNT( vertices ), sizeof( ItemVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3, indices, V6_ARRAY_COUNT( indices ), sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void PlayerScene_CreateGroundMesh( GPUMesh_s* mesh, u32 segmentCount, float uvScale, IStack* stack )
{
	ScopedStack scopedStack( stack );

	const u32 vertexCount = 1 + segmentCount;
	struct GroundVertex_s { Vec3 pos; Vec2 uv; } * vertices = stack->newArray< GroundVertex_s >( vertexCount, "PlayerGroundVertex" );

	const u16 indexCount = segmentCount * 3;
	u16* indices = stack->newArray< u16 >( indexCount, "PlayerGroundIndex" );

	vertices[0].pos = Vec3_Make( 0.0f, 0.0f, 0.0f );
	vertices[0].uv = Vec2_Make( 0.0f, 0.0f );
	float angle = 0.0f;
	float angleStep = V6_PI * 2.0f / segmentCount;
	for ( u32 vertexID = 1; vertexID < vertexCount; ++vertexID, angle += angleStep )
	{
		float s, c;
		SinCos( angle, &s, &c );
		vertices[vertexID].pos = Vec3_Make( c, 0.f, s );
		vertices[vertexID].uv = Vec2_Make( c * uvScale, s * uvScale );
	}

	indices[0] = 0;
	indices[1] = vertexCount-1;
	indices[2] = 1;
	u32 indexID = 3;
	for ( u32 vertexID = 1; vertexID < vertexCount-1; ++vertexID, indexID += 3 )
	{
		indices[indexID+0] = 0;
		indices[indexID+1] = vertexID;
		indices[indexID+2] = vertexID+1;
	}
		
	GPUMesh_Create( mesh, vertices, vertexCount, sizeof( GroundVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2, indices, indexCount, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void PlayerScene_CreateArrowMesh( GPUMesh_s* mesh, u32 side )
{
	struct ArrowVertex_s { Vec3 pos; } vertices[3];

	if ( side == 0 )
	{
		vertices[0].pos = Vec3_Make( 1.0f, -1.0f, 0.0f );
		vertices[1].pos = Vec3_Make( 0.5f,  0.0f, 0.0f );
		vertices[2].pos = Vec3_Make( 1.0f,  1.0f, 0.0f );
	}
	else
	{
		vertices[0].pos = Vec3_Make( -1.0f, -1.0f, 0.0f );
		vertices[1].pos = Vec3_Make( -0.5f,  0.0f, 0.0f );
		vertices[2].pos = Vec3_Make( -1.0f,  1.0f, 0.0f );
	}

	GPUMesh_Create( mesh, vertices, 3, sizeof( ArrowVertex_s ), VERTEX_FORMAT_POSITION, nullptr, 0, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void PlayerScene_Create( Player_s* player )
{
	Scene_Create( &player->sceneListView );
	player->sceneListView.player = player;

	{
		const u32 meshID = Scene_GetNewMeshID( &player->sceneListView );
		PlayerScene_CreateParticleMesh( &player->sceneListView.meshes[meshID], PARTICLE_COUNT, 3000.0f, 10000.0f, 0.25f, 1.0f, 5.0f, 20.0f, player->stack );

		const u32 materialID = Scene_GetNewMaterialID( &player->sceneListView );
		Material_Create( &player->sceneListView.materials[materialID], PlayerMaterial_DrawParticle );

		{
			player->sceneListView.particleEntityID = Scene_GetNewEntityID( &player->sceneListView );
			Entity_s* entity = &player->sceneListView.entities[player->sceneListView.particleEntityID];
			Entity_Create( entity, materialID, meshID, Vec3_Zero(), 1.0f );
			entity->pos = Vec3_Make( 0.0f, -USER_HEIGHT, 0.0f );
		}
	}

	{
		const float radius = 2000.0f;

		const u32 meshID = Scene_GetNewMeshID( &player->sceneListView );
		PlayerScene_CreateGroundMesh( &player->sceneListView.meshes[meshID], 32, 5.0f, player->stack  );

		const u32 gridTextureID = Scene_GetNewTextureID( &player->sceneListView );
		GPUTexture2D_CreateCompressed( &player->sceneListView.textures[gridTextureID], g_gridTileTextureSize.x, g_gridTileTextureSize.y, (void*)g_gridTileTextureData, true, "grid_tile" );

		const u32 materialID = Scene_GetNewMaterialID( &player->sceneListView );
		Material_Create( &player->sceneListView.materials[materialID], PlayerMaterial_DrawEnv );
		Material_SetTexture( &player->sceneListView.materials[materialID], gridTextureID, 0 );

		{
			const u32 entityID = Scene_GetNewEntityID( &player->sceneListView );
			Entity_s* entity = &player->sceneListView.entities[entityID];
			Entity_Create( entity, materialID, meshID, Vec3_Zero(), radius );
			entity->pos = Vec3_Make( 0.0f, -USER_HEIGHT, 0.0f );
		}
	}

	const u32 meshQuadID = Scene_GetNewMeshID( &player->sceneListView );
	PlayerScene_CreateItemMesh( &player->sceneListView.meshes[meshQuadID], 2.0f / CODEC_ICON_WIDTH );

	const u32 missingTextureID = Scene_GetNewTextureID( &player->sceneListView );
	GPUTexture2D_CreateCompressed( &player->sceneListView.textures[missingTextureID], CODEC_ICON_WIDTH, CODEC_ICON_WIDTH, (void*)g_missingStreamTextureData, true, "missing_icon" );
		
	const u32 overlayImageTextureID = Scene_GetNewTextureID( &player->sceneListView );
	GPUTexture2D_CreateCompressed( &player->sceneListView.textures[overlayImageTextureID], OVERLAY_WIDTH, OVERLAY_WIDTH, (void*)g_streamOverlayImage, true, "overlay_image" );

	const u32 overlayMovieTextureID = Scene_GetNewTextureID( &player->sceneListView );
	GPUTexture2D_CreateCompressed( &player->sceneListView.textures[overlayMovieTextureID], OVERLAY_WIDTH, OVERLAY_WIDTH, (void*)g_streamOverlayMovie, true, "overlay_movie" );

	for ( StreamItem_s* item = player->sceneListView.firstStreamItem; item; item = item->next )
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

		if ( item->duration > 0.0f )
			Material_SetTexture( &player->sceneListView.materials[materialID], overlayMovieTextureID, 1 );
		else
			Material_SetTexture( &player->sceneListView.materials[materialID], overlayImageTextureID, 1 );

		for ( u32 layoutType = LISTVIEW_LAYOUT_DESKTOP; layoutType < LISTVIEW_LAYOUT_COUNT; ++layoutType )
		{
			const u32 entityID = Scene_GetNewEntityID( &player->sceneListView );
			Entity_s* entity = &player->sceneListView.entities[entityID];
			Entity_Create( entity, materialID, meshQuadID, Vec3_Zero(), CODEC_ICON_WIDTH * 0.5f );
			entity->owner = item;
			item->layouts[layoutType].entityID = entityID;
		}
	}

	for ( u32 fontObjectID = 0; fontObjectID < ListViewScene_s::OBJECT_MAX_COUNT; ++fontObjectID )
		FontObject_Create( &player->sceneListView.fontObjects[fontObjectID], 256 );

	const u32 arrowMaterialID = Scene_GetNewMaterialID( &player->sceneListView );
	Material_Create( &player->sceneListView.materials[arrowMaterialID], PlayerMaterial_DrawArrow );

	for ( u32 arrowID = 0; arrowID < 2; ++arrowID )
	{
		const u32 meshID = Scene_GetNewMeshID( &player->sceneListView );
		PlayerScene_CreateArrowMesh( &player->sceneListView.meshes[meshID], arrowID );

		{
			player->sceneListView.arrowEntityIDs[arrowID] = Scene_GetNewEntityID( &player->sceneListView );
			Entity_s* entity = &player->sceneListView.entities[player->sceneListView.arrowEntityIDs[arrowID]];
			Entity_Create( entity, arrowMaterialID, meshID, Vec3_Zero(), CODEC_ICON_WIDTH * 0.5f );
		}
	}

	V6_MSG( "Player scene created\n" );
}

static void PlayerScene_Release( Player_s* player )
{
	for ( u32 fontObjectID = 0; fontObjectID < ListViewScene_s::OBJECT_MAX_COUNT; ++fontObjectID )
		FontObject_Release( &player->sceneListView.fontObjects[fontObjectID] );
	Scene_Release( &player->sceneListView );
}

static void PlayerScene_DrawListForDesktop( Player_s* player, float dt )
{
	if ( player->sceneListView.streamItemCount == 0 || player->win.size.x == 0 )
		return;

	const float margin = 60.0f;
	player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].rowCount = 0;
	player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].colCount = (u32)((player->win.size.x - margin) / (CODEC_ICON_WIDTH + margin));
	const float listWidth = player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].colCount * (CODEC_ICON_WIDTH + margin) + margin;
	const float listMargin = margin + (player->win.size.x - listWidth) * 0.5f;
	const float leftBase = (listMargin < 0.0f ? 0.0f : listMargin) + CODEC_ICON_WIDTH * 0.5f;
	const float verticalMargin  = margin + CODEC_ICON_WIDTH * 0.5f;
	const float topBase = player->win.size.y - verticalMargin;
	float x = leftBase;
	float y = topBase;

	u32 col = 0;
	u32 row = 0;
	for ( StreamItem_s* item = player->sceneListView.firstStreamItem; item; item = item->next )
	{
		if ( col == player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].colCount )
		{
			col = 0;
			x = leftBase;

			++row;
			y -= CODEC_ICON_WIDTH + margin;
		}
		
		Entity_s* entity = &player->sceneListView.entities[item->layouts[LISTVIEW_LAYOUT_DESKTOP].entityID];
		entity->pos = Vec3_Make( x, player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].scrollOffset + y, 0.0f );
		entity->yaw = 0.0f;
		item->layouts[LISTVIEW_LAYOUT_DESKTOP].row = row;
		item->layouts[LISTVIEW_LAYOUT_DESKTOP].col = col;
		
		++col;
		x += CODEC_ICON_WIDTH + margin;
	}
	player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].rowCount = row + 1;

	player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].scrollSpeed = 0.0f;
	if ( player->sceneListView.selectedStreamItem )
	{
		const float yPos = player->sceneListView.entities[player->sceneListView.selectedStreamItem->layouts[LISTVIEW_LAYOUT_DESKTOP].entityID].pos.y;
		
		if ( yPos - verticalMargin < 0.0f )
		{
			const float scrollDiff = verticalMargin - yPos;
			player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].scrollSpeed = Min( scrollDiff * 8.0f, LISTVIEW_SCROLL_SPEED );
		}
		else if ( yPos + verticalMargin > (float)player->win.size.y )
		{
			const float scrollDiff = yPos + verticalMargin - (float)player->win.size.y;
			player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].scrollSpeed = -Min( scrollDiff * 8.0f, LISTVIEW_SCROLL_SPEED );
		}
	}
	player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].scrollOffset = Max( 0.0f, player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].scrollOffset + player->sceneListView.layouts[LISTVIEW_LAYOUT_DESKTOP].scrollSpeed * dt );


	{
		GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
		renderTargetSetBindingDesc.clearColor = V6_DARK_GRAY;
		renderTargetSetBindingDesc.clear = true;
		renderTargetSetBindingDesc.useMSAA = true;
		renderTargetSetBindingDesc.noZ = true;
		renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_ADDITIF;

		GPURenderTargetSet_Bind( &player->winRenderTargetSet, &renderTargetSetBindingDesc, 0 );

		View_s orthoView = {};
		orthoView.viewMatrix = Mat4x4_Identity();
		orthoView.projMatrix = Mat4x4_Orthographic( (float)player->win.size.x, (float)player->win.size.y );

		Scene_Draw( &player->sceneListView, &orthoView, LISTVIEW_LAYOUT_DESKTOP );

		GPURenderTargetSet_Unbind( &player->winRenderTargetSet );
	}

	{
		const float textMargin = 8.0f;

		GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
		renderTargetSetBindingDesc.useMSAA = true;
		renderTargetSetBindingDesc.noZ = true;
		renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_ADDITIF;

		GPURenderTargetSet_Bind( &player->winRenderTargetSet, &renderTargetSetBindingDesc, 0 );

		const Mat4x4 ortho = Mat4x4_Orthographic( (float)player->win.size.x, (float)player->win.size.y );

		u32 fontObjectID = 0;
		for ( const StreamItem_s* item = player->sceneListView.firstStreamItem; item; item = item->next )
		{
			{
				V6_ASSERT( fontObjectID < ListViewScene_s::OBJECT_MAX_COUNT );
				FontObject_s* fontObject = &player->sceneListView.fontObjects[fontObjectID];
				++fontObjectID;

				Vec3 pos = player->sceneListView.entities[item->layouts[LISTVIEW_LAYOUT_DESKTOP].entityID].pos;
				pos.x -= FontObject_GetTextWidth( fontObject, item->title ) / 2;
				pos.y -= (CODEC_ICON_WIDTH / 2) + textMargin + FontObject_GetTextHeight( fontObject, item->title );

				Mat4x4 world = Mat4x4_Identity();
				Mat4x4_SetTranslation( &world, pos );

				Mat4x4 objectToProjMatrix;
				Mat4x4_Mul( &objectToProjMatrix, ortho, world );
				
				const Color_s color = player->sceneListView.selectedStreamItem == item ? V6_ORANGE : V6_LIGHT_GRAY;
				FontObject_Draw( fontObject, &objectToProjMatrix, 0, 0, color, item->title );
			}

			if ( item->duration > 0.0f )
			{
				V6_ASSERT( fontObjectID < ListViewScene_s::OBJECT_MAX_COUNT );
				FontObject_s* fontObject = &player->sceneListView.fontObjects[fontObjectID];
				++fontObjectID;

				const u32 seconds = (u32)item->duration;
				const char* durationStr = String_Format( "%02d'%02d\"", seconds / 60, seconds % 60 );

				Vec3 pos = player->sceneListView.entities[item->layouts[LISTVIEW_LAYOUT_DESKTOP].entityID].pos;
				pos.x = pos.x - (CODEC_ICON_WIDTH / 2) + textMargin;
				pos.y = pos.y - (CODEC_ICON_WIDTH / 2) + FontObject_GetTextHeight( fontObject, durationStr ) * 0.5f;

				Mat4x4 world = Mat4x4_Identity();
				Mat4x4_SetTranslation( &world, pos );

				Mat4x4 objectToProjMatrix;
				Mat4x4_Mul( &objectToProjMatrix, ortho, world );
				
				FontObject_Draw( fontObject, &objectToProjMatrix, 0, 0, V6_LIGHT_GRAY, durationStr );
			}
		}

		GPURenderTargetSet_Unbind( &player->winRenderTargetSet );
	}
}

static void PlayerScene_DrawParticleForHMD( Player_s* player, View_s* views )
{
	for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
	{
		GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
		renderTargetSetBindingDesc.clearColor = V6_BLACK;
		renderTargetSetBindingDesc.clear = true;
		renderTargetSetBindingDesc.useMSAA = true;
		renderTargetSetBindingDesc.noZ = true;
		renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_ADDITIF;

		GPURenderTargetSet_Bind( &player->mainRenderTargetSet, &renderTargetSetBindingDesc, eye );

		Entity_Draw( &player->sceneListView.entities[player->sceneListView.particleEntityID], &player->sceneListView, &views[eye], LISTVIEW_LAYOUT_HMD );

		GPURenderTargetSet_Unbind( &player->mainRenderTargetSet );
	}
}

static void PlayerScene_DrawListForHMD( Player_s* player, View_s* views, float dt )
{
	if ( player->sceneListView.streamItemCount == 0 )
		return;

	player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].rowCount = player->sceneListView.streamItemCount > 0 ? 1 : 0;
	player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].colCount = player->sceneListView.streamItemCount;

	const float radius = 1000.0f;
	const float invRadius = 1.0f / radius;
	const float fov = DegToRad( 95.0f );
	const float iconAngle = Tan( CODEC_ICON_WIDTH * 0.5f * invRadius ) * 2.0f;
	const float margin = 20.0f;
	const float marginAngle = Tan( margin * 0.5f * invRadius ) * 2.0f;
	const float visibilityAngle = iconAngle * 0.5f + marginAngle;

	const float phiMin = (V6_PI - fov) * 0.5f;
	const float phiMax = V6_PI - phiMin;
	const float topBase = CODEC_ICON_WIDTH * 0.5f - USER_HEIGHT + 50.0f;
	float phi = phiMax;
	float y = topBase;

	float scrollOffset = V6_PI * 0.5f - phiMax;
	for ( StreamItem_s* item = player->sceneListView.firstStreamItem; item; item = item->next )
	{	
		if ( item == player->sceneListView.selectedStreamItem )
			break;
		scrollOffset += iconAngle + marginAngle;
	}

	const float scrollDiff = Abs( scrollOffset - player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].scrollOffset );
	const float scrollSpeed = Min( scrollDiff * 8.0f, LISTVIEW_ROTATE_SPEED );
	if ( scrollOffset < player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].scrollOffset )
		player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].scrollSpeed = -scrollSpeed;
	else
		player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].scrollSpeed = scrollSpeed;

	player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].scrollOffset += player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].scrollSpeed * dt;

	u32 col = 0;
	u32 row = 0;
	Entity_s* lastHiddenEntityLeft = nullptr;
	Entity_s* firstHiddenEntityRight = nullptr;
	for ( StreamItem_s* item = player->sceneListView.firstStreamItem; item; item = item->next )
	{
		const float phiItem = player->sceneListView.layouts[LISTVIEW_LAYOUT_HMD].scrollOffset + phi;
		
		float s, c;
		SinCos( phiItem, &s, &c );
		const float x = c * radius;
		const float z = -s * radius;

		const bool hiddenLeft = phiItem - visibilityAngle > phiMax;
		const bool hiddenRight = phiItem + visibilityAngle < phiMin;

		Entity_s* entity = &player->sceneListView.entities[item->layouts[LISTVIEW_LAYOUT_HMD].entityID];
		entity->pos = Vec3_Make( x, y, z );
		entity->yaw = phiItem - V6_PI * 0.5f;
		entity->visible = !hiddenLeft && !hiddenRight;

		if ( hiddenLeft )
			lastHiddenEntityLeft = entity;
		else if ( hiddenRight && !firstHiddenEntityRight )
			firstHiddenEntityRight = entity;

		item->layouts[LISTVIEW_LAYOUT_HMD].row = row;
		item->layouts[LISTVIEW_LAYOUT_HMD].col = col;

		++col;
		phi -= iconAngle + marginAngle;
	}

	{
		Entity_s* entity = &player->sceneListView.entities[player->sceneListView.arrowEntityIDs[0]];
		if ( lastHiddenEntityLeft )
		{
			entity->pos = lastHiddenEntityLeft->pos;
			entity->yaw = lastHiddenEntityLeft->yaw;
		}
		entity->visible = lastHiddenEntityLeft != nullptr;
	}
	
	{
		Entity_s* entity = &player->sceneListView.entities[player->sceneListView.arrowEntityIDs[1]];
		if ( firstHiddenEntityRight )
		{
			entity->pos = firstHiddenEntityRight->pos;
			entity->yaw = firstHiddenEntityRight->yaw;
		}
		entity->visible = firstHiddenEntityRight != nullptr;
	}

	for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
	{
		{
			GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
			renderTargetSetBindingDesc.clearColor = V6_BLACK;
			renderTargetSetBindingDesc.clear = true;
			renderTargetSetBindingDesc.useMSAA = true;
			renderTargetSetBindingDesc.noZ = true;
			renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_ADDITIF;

			GPURenderTargetSet_Bind( &player->mainRenderTargetSet, &renderTargetSetBindingDesc, eye );

			Scene_Draw( &player->sceneListView, &views[eye], LISTVIEW_LAYOUT_HMD );

			GPURenderTargetSet_Unbind( &player->mainRenderTargetSet );
		}
		
		{
			const float fontScale = 1.0f;
			const float textMargin = 8.0f * fontScale;

			GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
			renderTargetSetBindingDesc.useMSAA = true;
			renderTargetSetBindingDesc.noZ = true;
			renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_ADDITIF;

			GPURenderTargetSet_Bind( &player->mainRenderTargetSet, &renderTargetSetBindingDesc, eye );

			u32 fontObjectID = 0;
			for ( const StreamItem_s* item = player->sceneListView.firstStreamItem; item; item = item->next )
			{
				const Entity_s* entity = &player->sceneListView.entities[item->layouts[LISTVIEW_LAYOUT_HMD].entityID];
				if ( !entity->visible )
					continue;

				{
					V6_ASSERT( fontObjectID < ListViewScene_s::OBJECT_MAX_COUNT );
					FontObject_s* fontObject = &player->sceneListView.fontObjects[fontObjectID];
					++fontObjectID;

					Mat4x4 objectToWorldMatrix = Mat4x4_RotationY( entity->yaw );

					Vec3 pos = entity->pos;
					pos -= objectToWorldMatrix.GetXAxis() * (FontObject_GetTextWidth( fontObject, item->title ) * fontScale / 2.0f);
					pos -= objectToWorldMatrix.GetYAxis() * ((CODEC_ICON_WIDTH / 2) + textMargin + FontObject_GetTextHeight( fontObject, item->title ) * fontScale);

					Mat4x4_PreScale( &objectToWorldMatrix, fontScale );
					Mat4x4_SetTranslation( &objectToWorldMatrix, pos );

					Mat4x4 objectToViewMatrix;
					Mat4x4_Mul( &objectToViewMatrix, views[eye].viewMatrix, objectToWorldMatrix );
					
					Mat4x4 objectToProjMatrix;
					Mat4x4_Mul( &objectToProjMatrix, views[eye].projMatrix, objectToViewMatrix );
					
					const Color_s color = player->sceneListView.selectedStreamItem == item ? V6_ORANGE : V6_LIGHT_GRAY;
					FontObject_Draw( fontObject, &objectToProjMatrix, 0, 0, color, item->title );
				}

				if ( item->duration > 0.0f )
				{
					V6_ASSERT( fontObjectID < ListViewScene_s::OBJECT_MAX_COUNT );
					FontObject_s* fontObject = &player->sceneListView.fontObjects[fontObjectID];
					++fontObjectID;

					Mat4x4 objectToWorldMatrix = Mat4x4_RotationY( entity->yaw );

					const u32 seconds = (u32)item->duration;
					const char* durationStr = String_Format( "%02d'%02d\"", seconds / 60, seconds % 60 );

					Vec3 pos = entity->pos;
					pos -= objectToWorldMatrix.GetXAxis() * ((CODEC_ICON_WIDTH / 2) - textMargin);
					pos -= objectToWorldMatrix.GetYAxis() * ((CODEC_ICON_WIDTH / 2) - FontObject_GetTextHeight( fontObject, durationStr ) * fontScale * 0.5f);

					Mat4x4_PreScale( &objectToWorldMatrix, fontScale );
					Mat4x4_SetTranslation( &objectToWorldMatrix, pos );

					Mat4x4 objectToViewMatrix;
					Mat4x4_Mul( &objectToViewMatrix, views[eye].viewMatrix, objectToWorldMatrix );
					
					Mat4x4 objectToProjMatrix;
					Mat4x4_Mul( &objectToProjMatrix, views[eye].projMatrix, objectToViewMatrix );
					
					FontObject_Draw( fontObject, &objectToProjMatrix, 0, 0, V6_LIGHT_GRAY, durationStr );
				}
			}

			GPURenderTargetSet_Unbind( &player->mainRenderTargetSet );
		}
	}
}

//----------------------------------------------------------------------------------------------------

static bool PlayerDevice_Create( Player_s* player, u32 width, u32 height )
{
	bool debugDevice = false;
#if V6_D3D_DEBUG == 1
#pragma message( "### D3D11 DEBUG ENABLED ###" )
	debugDevice = true;
#endif
	if ( !GPUDevice_CreateWithSurfaceContext( player->win.size.x, player->win.size.y, player->win.hWnd, debugDevice ) )
		return false;
	
	GPUInfo_s gpuInfo = {};
	GPUDevice_GetInfo( &gpuInfo );

	V6_MSG( "GPU: %s (%s-0x%04X-0x%04X)\n", gpuInfo.deviceName, GPUDevice_GetVendorName( gpuInfo.vendor ), gpuInfo.vendorID, gpuInfo.deviceID );

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
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_ARROW], sizeof( hlsl::CBParticle ), "arrow" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_PARTICLE], sizeof( hlsl::CBParticle ), "particle" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_LIST_ITEM], sizeof( hlsl::CBList ), "list" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_ENV], sizeof( hlsl::CBEnv ), "env" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE], sizeof( hlsl::CBCompose ), "compose" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_FRAMEMETRICS], sizeof( hlsl::CBFrameMetrics ), "frameMetrics" );

	static_assert( BUFFER_COUNT <= GPUShaderContext_s::BUFFER_MAX_COUNT, "Out of buffer" );
	GPUBuffer_CreateStructured( &shaderContext->buffers[BUFFER_FRAMEMETRICSTIME], sizeof( hlsl::FrameMetricsTime_s ), FrameMetrics_s::WIDTH, GPUBUFFER_CREATION_FLAG_MAP_DISCARD, "frameMetricsTime" );
	GPUBuffer_CreateStructured( &shaderContext->buffers[BUFFER_FRAMEMETRICSEVENT], sizeof( hlsl::FrameMetricsEvent_s ), FrameMetrics_s::WIDTH, GPUBUFFER_CREATION_FLAG_MAP_DISCARD, "frameMetricsEvent" );

	{
		ScopedStack scopedStack( player->stack );
		
		static_assert( SHADER_COUNT <= GPUShaderContext_s::SHADER_MAX_COUNT, "Out of shader" );
		GPUShader_CreateFromSource( &shaderContext->shaders[SHADER_ARROW], hlsl::g_main_player_arrow_vs, sizeof( hlsl::g_main_player_arrow_vs ), hlsl::g_main_player_arrow_ps, sizeof( hlsl::g_main_player_arrow_ps ), VERTEX_FORMAT_POSITION );
		GPUShader_CreateFromSource( &shaderContext->shaders[SHADER_PARTICLE], hlsl::g_main_player_particle_vs, sizeof( hlsl::g_main_player_particle_vs ), hlsl::g_main_player_particle_ps, sizeof( hlsl::g_main_player_particle_ps ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_INSTANCED );
		GPUShader_CreateFromSource( &shaderContext->shaders[SHADER_LIST], hlsl::g_main_player_list_vs, sizeof( hlsl::g_main_player_list_vs ), hlsl::g_main_player_list_ps, sizeof( hlsl::g_main_player_list_ps ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 );
		GPUShader_CreateFromSource( &shaderContext->shaders[SHADER_ENV], hlsl::g_main_player_env_vs, sizeof( hlsl::g_main_player_env_vs ), hlsl::g_main_player_env_ps, sizeof( hlsl::g_main_player_env_ps ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2 );

		static_assert( COMPUTE_COUNT <= GPUShaderContext_s::COMPUTE_MAX_COUNT, "Out of compute" );
		GPUCompute_CreateFromSource( &shaderContext->computes[COMPUTE_COMPOSESURFACE], hlsl::g_main_player_surface_compose_cs, sizeof( hlsl::g_main_player_surface_compose_cs ) );
		GPUCompute_CreateFromSource( &shaderContext->computes[COMPUTE_FRAMEMETRICS], hlsl::g_main_player_frame_metrics_cs, sizeof( hlsl::g_main_player_frame_metrics_cs ) );
	}

	{
		TraceDesc_s traceDesc = {};
		traceDesc.screenWidth = width;
		traceDesc.screenHeight = height;
		traceDesc.stereo = V6_STEREO;

		TraceResource_Create( &player->traceRes, &traceDesc );
	}

	FontSystem_Create();
	FontContext_Create( &player->fontContext );

	for ( u32 profileID = 0; profileID < FRAME_PROFILE_COUNT; ++profileID )
	{
		player->frameMetrics.profiles[profileID].dataTime = (hlsl::FrameMetricsTime_s*)player->heap->alloc_aligned< 16 >( &player->frameMetrics.profiles[profileID].dataTimeBuffer, player->frameMetrics.WIDTH * sizeof( hlsl::FrameMetricsTime_s ), "PlayerDeviceFrameMetricsTime" );
		memset( player->frameMetrics.profiles[profileID].dataTime, 0, player->frameMetrics.WIDTH * sizeof( hlsl::FrameMetricsTime_s ) );
	}
	player->frameMetrics.dataEvent = (hlsl::FrameMetricsEvent_s*)player->heap->alloc_aligned< 16 >( &player->frameMetrics.dataEventBuffer, player->frameMetrics.WIDTH * sizeof( hlsl::FrameMetricsEvent_s ), "PlayerDeviceFrameMetricsEvent" );
	memset( player->frameMetrics.dataEvent, 0, player->frameMetrics.WIDTH * sizeof( hlsl::FrameMetricsEvent_s ) );

	player->frameMetrics.profiles[FRAME_PROFILE_GPU_UPDATE_AND_DRAW].name = "[GPU] Update And Draw";
	player->frameMetrics.profiles[FRAME_PROFILE_GPU_UPDATE_AND_DRAW].verticalMax = 8000.0f;
	player->frameMetrics.profiles[FRAME_PROFILE_GPU_UPDATE_AND_DRAW].verticalUnit = 1000.0f;

	player->frameMetrics.profiles[FRAME_PROFILE_GPU_PRESENT].name = "[GPU] Present";
	player->frameMetrics.profiles[FRAME_PROFILE_GPU_PRESENT].verticalMax = 15000.0f;
	player->frameMetrics.profiles[FRAME_PROFILE_GPU_PRESENT].verticalUnit = 1000.0f;

	player->frameMetrics.profiles[FRAME_PROFILE_CPU_DECODE_SEQUENCE].name = "[CPU] Decode Streamed Sequence";
	player->frameMetrics.profiles[FRAME_PROFILE_CPU_DECODE_SEQUENCE].verticalMax = 15000.0f;
	player->frameMetrics.profiles[FRAME_PROFILE_CPU_DECODE_SEQUENCE].verticalUnit = 1000.0f;

	player->frameMetrics.profiles[FRAME_PROFILE_CPU_GET_SEQUENCE].name = "[CPU] Get Streamed Sequence";
	player->frameMetrics.profiles[FRAME_PROFILE_CPU_GET_SEQUENCE].verticalMax = 4000.0f;
	player->frameMetrics.profiles[FRAME_PROFILE_CPU_GET_SEQUENCE].verticalUnit = 1000.0f;

	player->frameMetrics.profiles[FRAME_PROFILE_CPU_STREAM].name = "[CPU] Stream to GPU";
	player->frameMetrics.profiles[FRAME_PROFILE_CPU_STREAM].verticalMax = 4000.0f;
	player->frameMetrics.profiles[FRAME_PROFILE_CPU_STREAM].verticalUnit = 1000.0f;

	player->frameMetrics.profiles[FRAME_PROFILE_CPU_UPDATE].name = "[CPU] Update GPU";
	player->frameMetrics.profiles[FRAME_PROFILE_CPU_UPDATE].verticalMax = 4000.0f;
	player->frameMetrics.profiles[FRAME_PROFILE_CPU_UPDATE].verticalUnit = 1000.0f;

	V6_MSG( "Player device created\n" );

	return true;
}

static void PlayerDevice_Release( Player_s* player )
{
	FontContext_Release( &player->fontContext );
	FontSystem_Release();

	TraceResource_Release( &player->traceRes );

	for ( u32 profileID = 0; profileID < FRAME_PROFILE_COUNT; ++profileID )
		player->heap->free( player->frameMetrics.profiles[profileID].dataTimeBuffer );
	player->heap->free( player->frameMetrics.dataEventBuffer );

	GPURenderTargetSet_Release( &player->createdRenderTargetSet );
	GPURenderTargetSet_Release( &player->winRenderTargetSet );

	GPUDevice_Release();
}

//----------------------------------------------------------------------------------------------------

static void PlayerStreamItem_Add( const char* pFileName, void* pCallbackData, const char* filter )
{
	Player_s* player = (Player_s*)pCallbackData;

	char path[256];
	FilePath_ExtractPath( path, sizeof( path ), filter );

	char streamFilename[256];
	sprintf_s( streamFilename, sizeof( streamFilename ), "%s/%s", path, pFileName );

	ScopedStack scopedStack( player->stack );

	CodecStreamDesc_s streamDesc;
	CodecStreamData_s streamData;

	if ( VideoStream_LoadDescAndData( streamFilename, &streamDesc, &streamData, player->stack ) == nullptr )
	{
		V6_ERROR( "Unable to read stream desc for %s\n", streamFilename );
		return;
	}

	u32 titleSize;
	const char* title = (const char*)VideoStream_GetKeyValue( &titleSize, &streamDesc, &streamData, CODEC_KEY_TITLE, player->stack );

	u32 iconSize;
	u8* iconData = VideoStream_GetKeyValue( &iconSize, &streamDesc, &streamData, "icon", player->stack );

	StreamItem_s* newItem = BlockAllocator_Add< StreamItem_s >( &player->streamItemAllocator, 1 );
	memset( newItem, 0, sizeof( StreamItem_s ) );

	strcpy_s( newItem->filename, sizeof( newItem->filename ), streamFilename );

	if ( title )
	{
		memcpy( newItem->title, title, Min( titleSize, (u32)sizeof( newItem->title )-1u ) );
	}
	else
	{
		char filename[256];
		FilePath_ExtractFilename( filename, sizeof( filename ), streamFilename );
		FilePath_TrimExtension( newItem->title, sizeof( newItem->title ), filename );
	}

	if ( iconData && iconSize > 4 && memcmp( iconData, CODEC_ICON_MAGIC, 4 ) == 0 )
	{
		newItem->iconTextureSize = iconSize-4;
		newItem->iconTextureData = (u8*)BlockAllocator_Alloc( &player->streamItemAllocator, newItem->iconTextureSize );
			
		memcpy( newItem->iconTextureData, iconData+4, newItem->iconTextureSize );
	}

	newItem->duration = (streamDesc.frameCount <= 1) ? 0.0f : ((float)streamDesc.frameCount / streamDesc.frameRate);

	StreamItem_s* prevItem = nullptr;
	for ( StreamItem_s* item = player->sceneListView.firstStreamItem; item; prevItem = item, item = item->next )
	{
		if ( strcmp( item->filename, newItem->title ) >= 0 )
			break;
	}

	if ( prevItem == nullptr )
	{
		newItem->next = player->sceneListView.firstStreamItem;
		player->sceneListView.firstStreamItem = newItem;
		player->sceneListView.selectedStreamItem = player->sceneListView.firstStreamItem;
	}
	else
	{
		newItem->next = prevItem->next;
		prevItem->next = newItem;
	}

	++player->sceneListView.streamItemCount;
}

static void PlayerStreamItem_Create( Player_s* player )
{
	BlockAllocator_Create( &player->streamItemAllocator, player->heap, MulMB( 1 ) );

	{
		char filter[256];
		sprintf_s( filter, sizeof( filter ), "%s/*.%s", PLAYER_STREAM_ITEM_FOLDER, CODEC_FILE_EXTENSION );
		FileSystem_GetFileList( filter, PlayerStreamItem_Add, player );
	}

	const char* userMediaFolder = Ini_ReadKey( &player->ini, "MEDIA", "folder", "" );

	if ( userMediaFolder && *userMediaFolder )
	{
		char filter[256];
		sprintf_s( filter, sizeof( filter ), "%s/*.%s", userMediaFolder, CODEC_FILE_EXTENSION );
		FileSystem_GetFileList( filter, PlayerStreamItem_Add, player );
	}

	V6_MSG( "Stream items added\n" );
}

static void PlayerStreamItem_Release( Player_s* player )
{
	player->sceneListView.selectedStreamItem = nullptr;
	BlockAllocator_Release( &player->streamItemAllocator );
	player->sceneListView.firstStreamItem = nullptr;
	player->sceneListView.streamItemCount = 0;
}

static void PlayerStreamItem_MoveSelectionToCursor( Player_s* player, int x, int y )
{
	const int flippedY = player->win.size.y - y;
	for ( StreamItem_s* item = player->sceneListView.firstStreamItem; item; item = item->next )
	{
		Entity_s* entity = &player->sceneListView.entities[item->layouts[LISTVIEW_LAYOUT_DESKTOP].entityID];
		const float dx = Abs( entity->pos.x - x );
		const float dy = Abs( entity->pos.y - flippedY );
		const float halfIconSize = CODEC_ICON_WIDTH * 0.5f;
		if ( dx <= halfIconSize && dy <= halfIconSize )
		{
			player->sceneListView.selectedStreamItem = item;
			return;
		}
	}
}

static void PlayerStreamItem_MoveSelectionToNeighbour( Player_s* player, int colDir, int rowDir, ListViewLayout_e layoutType )
{
	if ( player->sceneListView.selectedStreamItem == nullptr )
		return;

	const u32 newRow = (u32)Clamp( (int)player->sceneListView.selectedStreamItem->layouts[layoutType].row + rowDir, 0, (int)player->sceneListView.layouts[layoutType].rowCount-1 );
	u32 newCol = (u32)Clamp( (int)player->sceneListView.selectedStreamItem->layouts[layoutType].col + colDir, 0, (int)player->sceneListView.layouts[layoutType].colCount-1 );
	const u32 streamItemID = newRow * player->sceneListView.layouts[layoutType].colCount + newCol;
	if ( streamItemID >= player->sceneListView.streamItemCount )
		newCol -= streamItemID - player->sceneListView.streamItemCount + 1;

	for ( StreamItem_s* item = player->sceneListView.firstStreamItem; item; item = item->next )
	{
		if ( item->layouts[layoutType].row == newRow && item->layouts[layoutType].col == newCol )
		{
			player->sceneListView.selectedStreamItem = item;
			return;
		}
	}

	V6_ASSERT_ALWAYS( "Stream item not found" );
}

//----------------------------------------------------------------------------------------------------

static bool PlayerStream_Create( Player_s* player, const char* streamFilename )
{
	if ( !VideoStream_InitWithPrefetcher( &player->stream, &player->prefetcher, &player->fileReader, streamFilename, player->heap ) )
		return false;

	TraceContext_Create( &player->traceContext, &player->traceRes, &player->stream.desc, &player->stream.data, player->presentRate );

	player->curFrameID = 0.0f;
	player->targetFrameID = 0;

	return true;
}

static void PlayerStream_Release( Player_s* player )
{
	TraceContext_Release( &player->traceContext );
	for ( u32 sequenceOffset = 0; sequenceOffset < 2; ++sequenceOffset )
	{
		if ( player->sequenceStatus[sequenceOffset] == SEQUENCE_STATUS_STREAMING || player->sequenceStatus[sequenceOffset] == SEQUENCE_STATUS_READY )
			VideoStreamPrefetcher_ReleaseSequence( &player->prefetcher, player->sequenceID + sequenceOffset );
	}
	VideoStreamPrefetcher_CancelAllPendingSequences( &player->prefetcher );
	VideoStreamPrefetcher_ReleaseSequences( &player->prefetcher, player->heap );
	VideoStream_Release( &player->stream, player->heap );
	player->fileReader.Close();
}

static void PlayerStream_Init( Player_s* player, const char* streamFileName )
{
	if ( player->playerState == PLAYER_STATE_STREAM )
		PlayerStream_Release( player );

	player->playerState = PLAYER_STATE_LIST;

	if ( !PlayerStream_Create( player, streamFileName ) )
	{
		V6_ERROR( "Unable to create stream %s\n", streamFileName );
	}
	else
	{
		V6_MSG( "Loading stream %s\n", streamFileName );

		player->playerState = PLAYER_STATE_STREAM;
		player->sequenceID = PLAYER_INVALID_SEQUENCE_ID;
		player->sequenceStatus[0] = SEQUENCE_STATUS_NONE;
		player->sequenceStatus[1] = SEQUENCE_STATUS_NONE;
		player->sequenceFillBuffer = true;

		player->frameOrigin = Vec3_Zero();
		player->frameYaw = 0.0f;
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
		if ( strcmp( commandLine, "HIDE_HUD ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_HIDE_HUD;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "HIDE_HUD OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_HIDE_HUD;
			commandBuffer->arg[0] = 0;
			return;
		}

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

		if ( strcmp( commandLine, "HMD_MIRROR ON" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_MIRROR_HMD;
			commandBuffer->arg[0] = 1;
			return;
		}

		if ( strcmp( commandLine, "HMD_MIRROR OFF" ) == 0 )
		{
			commandBuffer->action = COMMAND_ACTION_PLAYER_OPTION_MIRROR_HMD;
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

static void Player_ReloadStreams( Player_s* player );

static void PlayerCommandBuffer_Process( Player_s* player )
{
	CommandBuffer_s commandBuffer = player->commandBuffer;
	if ( commandBuffer.action == COMMAND_ACTION_COMMAND_LINE )
		PlayerCommandBuffer_MakeFromCommandLine( &commandBuffer, player->commandBuffer.arg );

	if ( Player_IsUsingHMD( player ) )
	{
		if ( player->recentered )
		{
			switch ( commandBuffer.action )
			{
			case COMMAND_ACTION_MOVE_SELECTION_TO_CURSOR:
			case COMMAND_ACTION_MOVE_SELECTION_TO_LEFT:
			case COMMAND_ACTION_MOVE_SELECTION_TO_RIGHT:
			case COMMAND_ACTION_MOVE_SELECTION_TO_TOP:
			case COMMAND_ACTION_MOVE_SELECTION_TO_BOTTOM:
			case COMMAND_ACTION_PLAY_PAUSE:
				player->navigated = true;
				break;
			}
		}
		else
		{
			switch ( commandBuffer.action )
			{
			case COMMAND_ACTION_MOVE_SELECTION_TO_CURSOR:
			case COMMAND_ACTION_MOVE_SELECTION_TO_LEFT:
			case COMMAND_ACTION_MOVE_SELECTION_TO_RIGHT:
			case COMMAND_ACTION_MOVE_SELECTION_TO_TOP:
			case COMMAND_ACTION_MOVE_SELECTION_TO_BOTTOM:
			case COMMAND_ACTION_PLAY_PAUSE:
			case COMMAND_ACTION_BEGIN_FRAME:
			case COMMAND_ACTION_END_FRAME:
			case COMMAND_ACTION_PREV_FRAME:
			case COMMAND_ACTION_NEXT_FRAME:
				commandBuffer.action = COMMAND_ACTION_NONE;
				break;
			}
		}
	}

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
		PlayerStream_Init( player, commandBuffer.arg );
		break;
	case COMMAND_ACTION_UNLOAD_STREAM:
		PlayerStream_Unload( player );
		break;
	case COMMAND_ACTION_RELOAD_STREAMS:
		if ( player->playerState == PLAYER_STATE_LIST )
			Player_ReloadStreams( player );
		break;
	case COMMAND_ACTION_MOVE_SELECTION_TO_CURSOR:
		if ( player->playerState == PLAYER_STATE_LIST )
			PlayerStreamItem_MoveSelectionToCursor( player, commandBuffer.integerArgs[0], commandBuffer.integerArgs[1] );
		break;
	case COMMAND_ACTION_MOVE_SELECTION_TO_LEFT:
		if ( player->playerState == PLAYER_STATE_LIST )
			PlayerStreamItem_MoveSelectionToNeighbour( player, -1, 0, (ListViewLayout_e)commandBuffer.arg[0] );
		break;
	case COMMAND_ACTION_MOVE_SELECTION_TO_RIGHT:
		if ( player->playerState == PLAYER_STATE_LIST )
			PlayerStreamItem_MoveSelectionToNeighbour( player, +1, 0, (ListViewLayout_e)commandBuffer.arg[0] );
		break;
	case COMMAND_ACTION_MOVE_SELECTION_TO_TOP:
		if ( player->playerState == PLAYER_STATE_LIST )
			PlayerStreamItem_MoveSelectionToNeighbour( player, 0, -1, (ListViewLayout_e)commandBuffer.arg[0] );
		break;
	case COMMAND_ACTION_MOVE_SELECTION_TO_BOTTOM:
		if ( player->playerState == PLAYER_STATE_LIST )
			PlayerStreamItem_MoveSelectionToNeighbour( player, 0, +1, (ListViewLayout_e)commandBuffer.arg[0] );
		break;
	case COMMAND_ACTION_PLAY_PAUSE:
		if ( player->playerState == PLAYER_STATE_LIST )
		{
			if ( player->sceneListView.selectedStreamItem )
				PlayerStream_Init( player, player->sceneListView.selectedStreamItem->filename );
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
			V6_DEVMSG( "Target frame %d\n", player->targetFrameID );
		}
		break;
	case COMMAND_ACTION_BEGIN_FRAME:
		if ( player->playerState == PLAYER_STATE_STREAM && player->targetFrameID > 0 )
		{
			player->targetFrameID = 0;
			V6_DEVMSG( "Target frame %d\n", player->targetFrameID );
		}
		break;
	case COMMAND_ACTION_END_FRAME:
		if ( player->playerState == PLAYER_STATE_STREAM && (player->targetFrameID < player->stream.desc.frameCount-1 || player->targetFrameID == (u32)-1) )
		{
			player->targetFrameID = player->stream.desc.frameCount-1;
			V6_DEVMSG( "Target frame %d\n", player->targetFrameID );
		}
		break;
	case COMMAND_ACTION_PREV_FRAME:
		if ( player->playerState == PLAYER_STATE_STREAM )
		{
			if ( player->targetFrameID == (u32)-1 )
			{
				player->targetFrameID = (u32)player->curFrameID;
				V6_DEVMSG( "Target frame %d\n", player->targetFrameID );
			}
			else if ( player->targetFrameID > 0 )
			{
				--player->targetFrameID;
				V6_DEVMSG( "Target frame %d\n", player->targetFrameID );
			}
		}
		break;
	case COMMAND_ACTION_NEXT_FRAME:
		if ( player->playerState == PLAYER_STATE_STREAM )
		{
			if ( player->targetFrameID == (u32)-1 )
			{
				player->targetFrameID = (u32)player->curFrameID;
				V6_DEVMSG( "Target frame %d\n", player->targetFrameID );
			}
			else if ( player->targetFrameID < player->stream.desc.frameCount-1 )
			{
				++player->targetFrameID;
				V6_DEVMSG( "Target frame %d\n", player->targetFrameID );
			}
		}
		break;


	case COMMAND_ACTION_CAMERA_RECENTER:
		if ( player->recentered )
		{
			player->recentered = false;
		}
		else
		{
			PlayerCamera_Recenter( player );
#if V6_USE_HMD == 1
			Hmd_Recenter();
#endif
			player->recentered = true;
			V6_MSG( "Camera recentered.\n");
		}
		break;
	}

	if ( Platform_IsDevelopperMode()  )
	{
		switch ( commandBuffer.action )
		{
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
			player->playerOptions.showMetricsProfile = (commandBuffer.arg[0] < 2) ? commandBuffer.arg[0] : ((player->playerOptions.showMetricsProfile + 1) % (1+FRAME_PROFILE_COUNT));
			break;
		case COMMAND_ACTION_PLAYER_OPTION_UI:
			player->playerOptions.showUI = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->playerOptions.showUI;
			break;
		case COMMAND_ACTION_PLAYER_OPTION_HIDE_HUD:
			player->playerOptions.hideHUD = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->playerOptions.hideHUD;
			break;
		case COMMAND_ACTION_PLAYER_OPTION_LOCK_HMD:
			player->playerOptions.lockHMD = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->playerOptions.lockHMD;
			break;
		case COMMAND_ACTION_PLAYER_OPTION_MIRROR_HMD:
			player->playerOptions.mirrorHMD = (commandBuffer.arg[0] < 2) ? (commandBuffer.arg[0] == 0) : !player->playerOptions.mirrorHMD;
			if ( player->playerOptions.mirrorHMD )
				Hmd_CreateMirrorResources( GPUDevice_Get(), &Vec2i_Make( player->win.size.x, player->win.size.y ) );
			else
				Hmd_ReleaseMirrorResources();
			break;
		case COMMAND_ACTION_PLAYER_OPTION_SHOW_HMD_PERF_HUD:
			player->playerOptions.showHMDPerfHUD = commandBuffer.arg[0];
			break;
		}
	}

	player->commandBuffer.action = COMMAND_ACTION_NONE;
}

//----------------------------------------------------------------------------------------------------

const char* Player_GetStringForNavigateButtons( Player_s* player )
{
	if ( player->lastUsedController == PLAYER_CONTROLLER_XBOX )
		return "[D-pad]";
	
	return "[D-pad]";
}

const char* Player_GetStringForNavigateLeftButton( Player_s* player )
{
	if ( player->lastUsedController == PLAYER_CONTROLLER_XBOX )
		return "[D-pad Left]";
	
	return "[D-pad Left]";
}

const char* Player_GetStringForNavigateRightButton( Player_s* player )
{
	if ( player->lastUsedController == PLAYER_CONTROLLER_XBOX )
		return "[D-pad Right]";
	
	return "[D-pad Right]";
}

const char* Player_GetStringForBackButton( Player_s* player )
{
	if ( player->lastUsedController == PLAYER_CONTROLLER_XBOX )
		return "[B]";
	
	return "[Back]";
}

const char* Player_GetStringForBeginFrameButton( Player_s* player )
{
	if ( player->lastUsedController == PLAYER_CONTROLLER_XBOX )
		return "[D-pad Down]";
	
	return "[D-pad Down]";
}

const char* Player_GetStringForPlayButton( Player_s* player )
{
	if ( player->lastUsedController == PLAYER_CONTROLLER_XBOX )
		return "[A]";
	
	return "[Select]";
}

const char* Player_GetStringForRecenterButton( Player_s* player )
{
	if ( player->lastUsedController == PLAYER_CONTROLLER_XBOX )
		return "[D-pad Up]";
	
	return "[D-pad Up]";
}

//----------------------------------------------------------------------------------------------------

static void Player_OnKeyEvent( const KeyEvent_s* keyEvent )
{
	Player_s* player = (Player_s*)keyEvent->win->owner;

	if ( Platform_IsDevelopperMode() && player->commandLineSize != (u32)-1 )
	{
		if ( !keyEvent->pressed )
			return ;

		if ( keyEvent->key == 0x0D )
		{
			if ( player->commandLineSize == 0 )
			{
				V6_DEVMSG( "\r~<NULL>\n" );
			}
			else
			{
				V6_DEVMSG( "\r~%s\n", player->commandLine );
				player->commandBuffer.action = COMMAND_ACTION_COMMAND_LINE;
				strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), player->commandLine );
			}
			player->commandLineSize = (u32)-1;
			
		}
		else if ( keyEvent->key == 0xC0 )
		{
			V6_DEVMSG( "\r~<NULL>\n" );
			player->commandLineSize = (u32)-1;
		}
		else if ( keyEvent->key >= ' ' && player->commandLineSize < sizeof( player->commandLine ) )
		{
			player->commandLine[player->commandLineSize] = keyEvent->key;
			++player->commandLineSize;
			player->commandLine[player->commandLineSize] = 0;
			V6_DEVMSG( "\r~%s", player->commandLine );
		}

		return;
	}

	if ( Platform_IsDevelopperMode() )
	{
		switch( keyEvent->key )
		{
		case 'A': player->keyLeftPressed = keyEvent->pressed; break;
		case 'D': player->keyRightPressed = keyEvent->pressed; break;
		case 'S': player->keyDownPressed = keyEvent->pressed; break;
		case 'W': player->keyUpPressed = keyEvent->pressed; break;
		}
	}

	if ( !keyEvent->pressed )
		return ;

	if ( !Player_IsUsingHMD( player) )
	{
		switch( keyEvent->key )
		{
		case 0x1B:
			player->commandBuffer.action = COMMAND_ACTION_EXIT;
			break;
		case 0x74:
			player->commandBuffer.action = COMMAND_ACTION_RELOAD_STREAMS;
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
			player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_LEFT;
			player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_DESKTOP;
			break;
		case 0x26:
			player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_TOP;
			player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_DESKTOP;
			break;
		case 0x27:
			player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_RIGHT;
			player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_DESKTOP;
			break;
		case 0x28:
			player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_BOTTOM;
			player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_DESKTOP;
			break;
		case 0x0D:
		case 'P':
			player->commandBuffer.action = COMMAND_ACTION_PLAY_PAUSE;
			break;
		case 'R':
			player->commandBuffer.action = COMMAND_ACTION_CAMERA_RECENTER;
			break;
		}
	}

	if ( Platform_IsDevelopperMode() )
	{
		switch( keyEvent->key )
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
			player->commandBuffer.action = COMMAND_ACTION_PLAYER_OPTION_SHOW_HMD_PERF_HUD;
			player->commandBuffer.arg[0] = keyEvent->key - '0';
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
			player->commandBuffer.action = COMMAND_ACTION_PLAYER_OPTION_HIDE_HUD;
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
				if ( FileDialog_Open( filename, sizeof( filename ), CODEC_FILE_EXTENSION ) )
				{
					player->commandBuffer.action = COMMAND_ACTION_LOAD_STREAM;
					strcpy_s( player->commandBuffer.arg, sizeof( player->commandBuffer.arg ), filename );
				}
			}
			break;
		case 'M':
			player->commandBuffer.action = COMMAND_ACTION_PLAYER_OPTION_MIRROR_HMD;
			player->commandBuffer.arg[0] = 2;
			break;
		case 'O':
			player->commandBuffer.action = COMMAND_ACTION_TRACE_OPTION_OVERDRAW;
			player->commandBuffer.arg[0] = 2;
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
}

static void Player_OnMouseEvent( const MouseEvent_s* mouseEvent )
{
	Player_s* player = (Player_s*)mouseEvent->win->owner;

	if ( mouseEvent->leftButton == MOUSE_BUTTON_DOWN )
	{
		player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_CURSOR;
		player->commandBuffer.integerArgs[0] = mouseEvent->posX;
		player->commandBuffer.integerArgs[1] = mouseEvent->posY;
	}
	else if ( mouseEvent->leftButton == MOUSE_BUTTON_DOUBLE_CLICK )
	{
		player->commandBuffer.action = COMMAND_ACTION_PLAY_PAUSE;
	}

	if ( mouseEvent->deltaWheel != 0 )
	{
		player->commandBuffer.action = mouseEvent->deltaWheel > 0 ? COMMAND_ACTION_MOVE_SELECTION_TO_TOP : COMMAND_ACTION_MOVE_SELECTION_TO_BOTTOM;
		player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_DESKTOP;
	}

	if ( mouseEvent->rightButton == MOUSE_BUTTON_DOWN )
	{
		Win_CaptureMouse( &player->win );
		player->mouseRightPressed = true;
	}
	else if ( mouseEvent->rightButton == MOUSE_BUTTON_UP )
	{
		Win_ReleaseMouse( &player->win );
		player->mouseRightPressed = false;
	}

	if ( player->mouseRightPressed )
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

	if ( player->playerOptions.mirrorHMD )
	{
		Hmd_ReleaseMirrorResources();
		Hmd_CreateMirrorResources( GPUDevice_Get(), &Vec2i_Make( win->size.x, win->size.y ) );
	}
}

static void Player_OnGamepadButtonEvent( const Gamepad_s* gamepad, GamepadButtons_s leftButtonIsChanged, GamepadButtons_s rightButtonIsChanged )
{
	Player_s* player = (Player_s*)gamepad->owner;

	player->lastUsedController = PLAYER_CONTROLLER_XBOX;

	if ( !Player_IsUsingHMD( player ) )
		return;

	GamepadButtons_s leftButtonIsPressed;
	leftButtonIsPressed.bits = leftButtonIsChanged.bits & ~gamepad->leftButtonWasDown.bits;

	GamepadButtons_s rightButtonIsPressed;
	rightButtonIsPressed.bits = rightButtonIsChanged.bits & ~gamepad->rightButtonWasDown.bits;

	if ( player->playerState == PLAYER_STATE_STREAM )
	{
		if ( leftButtonIsPressed.L )
			player->commandBuffer.action = COMMAND_ACTION_PREV_FRAME;

		if ( leftButtonIsPressed.R )
			player->commandBuffer.action = COMMAND_ACTION_NEXT_FRAME;

		if ( leftButtonIsPressed.B )
			player->commandBuffer.action = COMMAND_ACTION_BEGIN_FRAME;
	}
	else
	{
		if ( leftButtonIsPressed.L )
		{
			player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_LEFT;
			player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_HMD;
		}

		if ( leftButtonIsPressed.R )
		{
			player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_RIGHT;
			player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_HMD;
		}
	}

	if ( rightButtonIsPressed.R )
		player->commandBuffer.action = COMMAND_ACTION_EXIT;

	if ( rightButtonIsPressed.B )
		player->commandBuffer.action = COMMAND_ACTION_PLAY_PAUSE;

	if ( leftButtonIsPressed.T )
		player->commandBuffer.action = COMMAND_ACTION_CAMERA_RECENTER;
}

static void Player_ProcessInputs( Player_s* player, float dt )
{
	Gamepad_UpdateState( &player->gamepad );

	if ( Player_IsUsingHMD( player ) )
	{
		HmdInputState_s hmdInputState;
		if ( Hmd_GetInputState( &hmdInputState ) )
		{
			player->lastUsedController = PLAYER_CONTROLLER_REMOTE;

			if ( player->playerState == PLAYER_STATE_STREAM )
			{
				if ( hmdInputState.left == HMD_INPUT_EVENT_PRESSED )
					player->commandBuffer.action = COMMAND_ACTION_PREV_FRAME;

				if ( hmdInputState.right == HMD_INPUT_EVENT_PRESSED )
					player->commandBuffer.action = COMMAND_ACTION_NEXT_FRAME;

				if ( hmdInputState.down == HMD_INPUT_EVENT_PRESSED )
					player->commandBuffer.action = COMMAND_ACTION_BEGIN_FRAME;
			}
			else
			{
				if ( hmdInputState.left == HMD_INPUT_EVENT_PRESSED )
				{
					player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_LEFT;
					player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_HMD;
				}

				if ( hmdInputState.right == HMD_INPUT_EVENT_PRESSED )
				{
					player->commandBuffer.action = COMMAND_ACTION_MOVE_SELECTION_TO_RIGHT;
					player->commandBuffer.arg[0] = LISTVIEW_LAYOUT_HMD;
				}
			}

			if ( hmdInputState.back == HMD_INPUT_EVENT_PRESSED )
				player->commandBuffer.action = COMMAND_ACTION_EXIT;

			if ( hmdInputState.enter == HMD_INPUT_EVENT_PRESSED )
				player->commandBuffer.action = COMMAND_ACTION_PLAY_PAUSE;

			if ( hmdInputState.up == HMD_INPUT_EVENT_PRESSED )
				player->commandBuffer.action = COMMAND_ACTION_CAMERA_RECENTER;

		}
	}

	{
		float mouseDeltaX = 0;
		float mouseDeltaY = 0;

		if ( player->mouseRightPressed )
		{		
			mouseDeltaX = player->mouseDeltaX;
			mouseDeltaY = player->mouseDeltaY;
			player->mouseDeltaX = 0;
			player->mouseDeltaY = 0;
		}

		if ( Platform_IsDevelopperMode() && !Player_IsUsingHMD( player ) )
		{
			player->camera.yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
			player->camera.pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;
		}
	}

	if ( !Player_IsUsingHMD( player ) )
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
	
		if ( Platform_IsDevelopperMode() && (keyDeltaX || keyDeltaY || keyDeltaZ) )
		{
			player->camera.pos += player->camera.right * (float)keyDeltaX * KEY_TRANSLATION_SPEED * dt;
			player->camera.pos += player->camera.up * (float)keyDeltaY * KEY_TRANSLATION_SPEED * dt;
			player->camera.pos += player->camera.forward * (float)keyDeltaZ * KEY_TRANSLATION_SPEED * dt;
		}
	}
}

static void Player_DrawHUD( Player_s* player, const View_s* views, float outOfSensorRange, float outOfBoxRange )
{
	const u32 lineHeight = FontContext_GetLineHeight( &player->fontContext );

	static const Color_s fontColor = V6_HMD_FONT_COLOR;

	// middle

	{
		const u32 cursorX = player->mainRenderTargetSet.width / 2;
		u32 cursorY = player->mainRenderTargetSet.height * 2 / 5;

		if ( player->playerState == PLAYER_STATE_STREAM && (player->sequenceStatus[0] != SEQUENCE_STATUS_READY || player->sequenceStatus[1] < SEQUENCE_STATUS_STREAMING) )
		{
			const u32 pacifierID = (u32)(player->time * 8) % 4;
			const char *pacifierStr[] = { "Loading.   ", "Loading..  ", "Loading... ", "Loading...." };
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, pacifierStr[pacifierID], FONT_ANCHOR_HORIZONTAL_CENTER );
			cursorY -= lineHeight;
		}
#if V6_USE_HMD == 1
		else if ( !player->hmdStatus.hmdMounted )
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, "Put your VR headset on", FONT_ANCHOR_HORIZONTAL_CENTER );
			cursorY -= lineHeight;
		}
		else if ( !player->hmdStatus.isVisible )
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, "Waiting for user", FONT_ANCHOR_HORIZONTAL_CENTER );
			cursorY -= lineHeight;
		}
#endif
		else
		{
			if ( !player->recentered )
			{
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, "Look forward in a relaxed position and", FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, String_Format( "press %s to recenter", Player_GetStringForRecenterButton( player ) ), FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
			}
#if 1
			else if ( player->playerState == PLAYER_STATE_LIST && !player->navigated )
			{
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, String_Format( "Press %s to change video or", Player_GetStringForNavigateButtons( player ) ), FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, String_Format( "press %s to open a video", Player_GetStringForPlayButton( player ) ), FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
			}
#endif
			else if ( outOfSensorRange > 0.0f )
			{
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_Make( 200, 127, 127, Clamp( (u32)(255 * outOfSensorRange), 0u, 255u ) ), "Out of sensor range", FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, "Get closer to the tracking sensor", FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
			}
			else if ( outOfBoxRange > 0.0f )
			{
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_Make( 200, 127, 127, Clamp( (u32)(255 * outOfBoxRange), 0u, 255u ) ), "Out of the volume of view", FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, String_Format( "Press %s if you want to recenter", Player_GetStringForRecenterButton( player ) ), FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
			}
			else if ( player->playerState == PLAYER_STATE_STREAM && player->stream.desc.frameCount > 1 && (u32)player->curFrameID == player->targetFrameID )
			{
				if ( player->targetFrameID == 0 )
					FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, String_Format( "Press %s to play", Player_GetStringForPlayButton( player ) ), FONT_ANCHOR_HORIZONTAL_CENTER );
				else if ( player->targetFrameID == player->stream.desc.frameCount-1 )
					FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, String_Format( "Press %s to re-play or %s to exit", Player_GetStringForPlayButton( player ), Player_GetStringForBackButton( player ) ), FONT_ANCHOR_HORIZONTAL_CENTER );
				else
					FontContext_AddLine( &player->fontContext, cursorX, cursorY, fontColor, String_Format( "Press %s to resume or %s to re-start", Player_GetStringForPlayButton( player ), Player_GetStringForBeginFrameButton( player ) ), FONT_ANCHOR_HORIZONTAL_CENTER );
				cursorY -= lineHeight;
			}
		}
	}

	const float widthWU = 200.0f;
	const float depthWU = 100.0f;
	const float norm = widthWU / player->mainRenderTargetSet.width;

	Mat4x4 fontScale;
	fontScale.m_row0 = Vec4_Make( norm, 0.0f, 0.0f, -(player->mainRenderTargetSet.width / 2.0f) * norm );
	fontScale.m_row1 = Vec4_Make( 0.0f, norm, 0.0f, -(player->mainRenderTargetSet.height / 2.0f) * norm );
	fontScale.m_row2 = Vec4_Make( 0.0f, 0.0f, 1.0f, -depthWU );
	fontScale.m_row3 = Vec4_Make( 0.0f, 0.0f, 0.0f, 1.0f );

	View_s fontViews[2];
	for ( u32 eye = 0; eye < 2; ++eye )
	{
		Mat4x4_Mul( &fontViews[eye].viewMatrix, views[eye].lockedViewMatrix, fontScale );
		fontViews[eye].projMatrix = views[eye].projMatrix;
	}
	FontContext_Draw( &player->fontContext, &player->mainRenderTargetSet, &fontViews[0], &fontViews[1], V6_DARK_GRAY );
}

static void Player_DrawUI( Player_s* player, float averageFPS, const CPUEventDuration_s* cpuEventDurations, u32 cpuEventCount, const GPUEventDuration_s* gpuEventDurations, u32 gpuEventCount )
{
	const Vec2i viewportSize = Vec2i_Make( player->winRenderTargetSet.width, player->winRenderTargetSet.height );

	const u32 lineHeight = FontContext_GetLineHeight( &player->fontContext );
	const u32 lineTop = (u32)(viewportSize.y - lineHeight * 1.5f);

	// top left

	{
		const u32 cursorX = 8;
		u32 cursorY = lineTop;

		FontContext_AddLine( &player->fontContext, 8, cursorY, Color_White(), String_Format( "FPS: %3.1f", averageFPS ), FONT_ANCHOR_HORIZONTAL_LEFT );
		cursorY -= lineHeight;

		for ( u32 eventRank = 0; eventRank < gpuEventCount; ++eventRank )
		{
			const GPUEventDuration_s* eventDuration = &gpuEventDurations[eventRank];
			const char* txt = String_Format( "%*s%-10s : %5d us", eventDuration->depth * 4, "", eventDuration->name, eventDuration->avgDurationUS );
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), txt, FONT_ANCHOR_HORIZONTAL_LEFT );
			cursorY -= lineHeight;
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
		const u32 messageCount = Min( s_ouputMessageCount, s_ouputMessageBufferCount );

		const u32 cursorX = 8;
		u32 cursorY = messageCount * lineHeight;
		
		for ( u32 messageOffset = 0; messageOffset < messageCount; ++messageOffset )
		{
			const u32 bufferID = (s_ouputMessageCount - messageOffset - 1) % s_ouputMessageBufferCount;
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), s_ouputMessageBuffers[bufferID], FONT_ANCHOR_HORIZONTAL_LEFT );
			cursorY -= lineHeight;
		}
	}

	// top middle

	{
		const u32 cursorX = viewportSize.x / 2;
		u32 cursorY = lineTop;

		if ( player->playerState == PLAYER_STATE_STREAM )
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "Stream: %s", player->stream.name ), FONT_ANCHOR_HORIZONTAL_LEFT );
			cursorY -= lineHeight;
            FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "Grid: %dx%dx%d", player->stream.desc.gridWidth, player->stream.desc.gridWidth, player->stream.desc.gridWidth), FONT_ANCHOR_HORIZONTAL_LEFT );
			cursorY -= lineHeight;
			const u32 streamFrameID = (u32)(player->curFrameID + FLT_EPSILON);
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "Frame: %5d/%5d @ %d Hz", streamFrameID+1, player->stream.desc.frameCount, player->stream.desc.frameRate ), FONT_ANCHOR_HORIZONTAL_LEFT );
			cursorY -= lineHeight;
		}
	}

	// center middle

	{
		const u32 cursorX = viewportSize.x / 2;
		u32 cursorY = 500;

		if ( player->playerOptions.showMetricsProfile > 0 )
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), player->frameMetrics.profiles[player->playerOptions.showMetricsProfile-1].name, FONT_ANCHOR_HORIZONTAL_CENTER );
			cursorY -= lineHeight;
		}
	}

	// top right

	{
		const u32 cursorX = viewportSize.x - 400;
		u32 cursorY = lineTop;

		for ( u32 eventRank = 0; eventRank < cpuEventCount; ++eventRank )
		{
			const CPUEventDuration_s* eventDuration = &cpuEventDurations[eventRank];
			const char* txt = String_Format( "%-12s : %5d us (%.1f calls)", eventDuration->name, eventDuration->avgDurationUS, eventDuration->avgCallCount );
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), txt, FONT_ANCHOR_HORIZONTAL_LEFT );
			cursorY -= lineHeight;
		}
	}

	// bottom right

#if V6_USE_HMD == 1
	{
		u32 cursorX = viewportSize.x - 300;
		u32 cursorY = 9 * lineHeight;
		
		FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "HMD: %spresent",			player->hmdStatus.hmdPresent ? "" : "not " ), FONT_ANCHOR_HORIZONTAL_LEFT );
		cursorY -= lineHeight;
		
		FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "HMD: %smounted",			player->hmdStatus.hmdMounted ? "" : "not " ), FONT_ANCHOR_HORIZONTAL_LEFT );
		cursorY -= lineHeight;

		FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "HMD: %svisible",			player->hmdStatus.isVisible ? "" : "not " ), FONT_ANCHOR_HORIZONTAL_LEFT );
		cursorY -= lineHeight;

		FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "HMD: display %slost",		player->hmdStatus.displayLost ? "" : "not " ), FONT_ANCHOR_HORIZONTAL_LEFT );
		cursorY -= lineHeight;

		FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "HMD: should %squit",		player->hmdStatus.shouldQuit ? "" : "not " ), FONT_ANCHOR_HORIZONTAL_LEFT );
		cursorY -= lineHeight;

		FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), String_Format( "HMD: should %srecenter",	player->hmdStatus.shouldRecenter ? "" : "not " ), FONT_ANCHOR_HORIZONTAL_LEFT );
		cursorY -= lineHeight;

		if ( player->hmdTackingState & HMD_TRACKING_STATE_ON )
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), "HMD: rotation tracked", FONT_ANCHOR_HORIZONTAL_LEFT );
			cursorY -= lineHeight;

			if ( player->hmdTackingState & HMD_TRACKING_STATE_POS )
			{
				FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), "HMD: position tracked", FONT_ANCHOR_HORIZONTAL_LEFT );
				cursorY -= lineHeight;
			}
		}
		else
		{
			FontContext_AddLine( &player->fontContext, cursorX, cursorY, Color_White(), "HMD: off", FONT_ANCHOR_HORIZONTAL_LEFT );
			cursorY -= lineHeight;
		}
	}
#endif // #if V6_USE_HMD == 1
	
	View_s orthoView = {};
	Mat4x4_Identity( &orthoView.viewMatrix );
	orthoView.projMatrix = Mat4x4_Orthographic( (float)viewportSize.x, (float)viewportSize.y );
	
	FontContext_Draw( &player->fontContext, &player->winRenderTargetSet, &orthoView, nullptr, V6_TRANSPARENT );
}

static void Player_SetMetricsEvent( Player_s* player, FrameMetricsEvent_e metricsEvent )
{
	player->frameMetrics.pendingEvents |= 1 << metricsEvent;
}

static void Player_UpdateMetrics( Player_s* player, u32 frameID, const CPUEventDuration_s* cpuEventDurations, u32 cpuEventCount, const GPUEventDuration_s* gpuEventDurations, u32 gpuEventCount )
{
	player->frameMetrics.frameID = frameID % FrameMetrics_s::WIDTH;

	{
		static const CPUEventID_t s_cpuEventDecode = CPUEvent_Find( "Decode" );

		u32 decodeSequenceTimeUS = 0;
		u32 getSequenceTimeUS = 0;
		u32 streamTimeUS = 0;
		u32 updateTimeUS = 0;
		for ( u32 eventRank = 0; eventRank < cpuEventCount; ++eventRank )
		{
			if ( cpuEventDurations[eventRank].id == s_cpuEventDecode )
				decodeSequenceTimeUS += cpuEventDurations[eventRank].curDurationUS;
			if ( cpuEventDurations[eventRank].id == s_cpuEventGetSequence )
				getSequenceTimeUS += cpuEventDurations[eventRank].curDurationUS;
			else if ( cpuEventDurations[eventRank].id == s_cpuEventStream )
				streamTimeUS += cpuEventDurations[eventRank].curDurationUS;
			else if ( cpuEventDurations[eventRank].id == s_cpuEventUpdate )
				updateTimeUS += cpuEventDurations[eventRank].curDurationUS;
		}

		player->frameMetrics.profiles[FRAME_PROFILE_CPU_DECODE_SEQUENCE].dataTime[player->frameMetrics.frameID].drawTimeUS = decodeSequenceTimeUS;
		player->frameMetrics.profiles[FRAME_PROFILE_CPU_GET_SEQUENCE].dataTime[player->frameMetrics.frameID].drawTimeUS = getSequenceTimeUS;
		player->frameMetrics.profiles[FRAME_PROFILE_CPU_STREAM].dataTime[player->frameMetrics.frameID].drawTimeUS = streamTimeUS;
		player->frameMetrics.profiles[FRAME_PROFILE_CPU_UPDATE].dataTime[player->frameMetrics.frameID].drawTimeUS = updateTimeUS;
	}

	{
		u32 updateAndDrawTimeUS = 0;
		u32 updatePresentTimeUS = 0;
		for ( u32 eventRank = 0; eventRank < gpuEventCount; ++eventRank )
		{
			if ( gpuEventDurations[eventRank].id == s_gpuEventStream || gpuEventDurations[eventRank].id == s_gpuEventUpdate || gpuEventDurations[eventRank].id == s_gpuEventDraw )
				updateAndDrawTimeUS += gpuEventDurations[eventRank].curDurationUS;
			else if ( gpuEventDurations[eventRank].id == s_gpuEventPresent )
				updatePresentTimeUS += gpuEventDurations[eventRank].curDurationUS;
		}

		player->frameMetrics.profiles[FRAME_PROFILE_GPU_UPDATE_AND_DRAW].dataTime[player->frameMetrics.frameID].drawTimeUS = updateAndDrawTimeUS;
		player->frameMetrics.profiles[FRAME_PROFILE_GPU_PRESENT].dataTime[player->frameMetrics.frameID].drawTimeUS = updatePresentTimeUS;
	}
	
	player->frameMetrics.dataEvent[player->frameMetrics.frameID].events = player->frameMetrics.pendingEvents;
	player->frameMetrics.pendingEvents = 0;
}

static void Player_DrawMetrics( Player_s* player, u32 profileID )
{
	V6_ASSERT( profileID < FRAME_PROFILE_COUNT );

	V6_GPU_EVENT_SCOPE( s_gpuEventMetrics );

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	// update buffer

	GPUBuffer_Update( &shaderContext->buffers[BUFFER_FRAMEMETRICSTIME], 0, player->frameMetrics.profiles[profileID].dataTime, FrameMetrics_s::WIDTH );
	GPUBuffer_Update( &shaderContext->buffers[BUFFER_FRAMEMETRICSEVENT], 0, player->frameMetrics.dataEvent, FrameMetrics_s::WIDTH );

	// set

	g_deviceContext->CSSetConstantBuffers( hlsl::CBFrameMetricsSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_FRAMEMETRICS].buf );

	g_deviceContext->CSSetShaderResources( HLSL_FRAME_METRICS_TIME_SLOT, 1, &shaderContext->buffers[BUFFER_FRAMEMETRICSTIME].srv );
	g_deviceContext->CSSetShaderResources( HLSL_FRAME_METRICS_EVENT_SLOT, 1, &shaderContext->buffers[BUFFER_FRAMEMETRICSEVENT].srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, &player->winRenderTargetSet.colorBuffers[0].uav, nullptr );

	g_deviceContext->CSSetShader( shaderContext->computes[COMPUTE_FRAMEMETRICS].m_computeShader, nullptr, 0 );

	const Vec2u frameMetricsRTSize = Vec2u_Make( player->winRenderTargetSet.width - 16, 240 );
	const Vec2u frameMetricsRTOffset = Vec2u_Make( 8, player->winRenderTargetSet.height - frameMetricsRTSize.y - 250 - 8 );

	const u32 eventHeight = frameMetricsRTSize.y / FRAME_METRICS_EVENT_COUNT;

	{
		hlsl::CBFrameMetrics* cbFrameMetrics = (hlsl::CBFrameMetrics*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_FRAMEMETRICS] );
		
		cbFrameMetrics->c_frameMetricsRTSize = frameMetricsRTSize;
		cbFrameMetrics->c_frameMetricsRTOffset = frameMetricsRTOffset;
	
		cbFrameMetrics->c_frameMetricsEnd = player->frameMetrics.frameID;
		cbFrameMetrics->c_frameMetricsFrameRate = player->presentRate;
		cbFrameMetrics->c_frameMetricsVerticalScale = frameMetricsRTSize.y / player->frameMetrics.profiles[profileID].verticalMax;
		cbFrameMetrics->c_frameMetricsVerticalUnit = player->frameMetrics.profiles[profileID].verticalUnit;

		cbFrameMetrics->c_frameMetricsEvent0Mask = 1 << FRAME_METRICS_EVENT_LOADING;
		cbFrameMetrics->c_frameMetricsEvent0MinY = (FRAME_METRICS_EVENT_LOADING + 0) * eventHeight;
		cbFrameMetrics->c_frameMetricsEvent0MaxY = (FRAME_METRICS_EVENT_LOADING + 1) * eventHeight;
		cbFrameMetrics->c_frameMetricsEvent0Color = Vec4_Make( 1.0f, 0.0f, 0.0f, 0.0f ); 

		cbFrameMetrics->c_frameMetricsEvent1Mask = 1 << FRAME_METRICS_EVENT_STREAMING0;
		cbFrameMetrics->c_frameMetricsEvent1MinY = (FRAME_METRICS_EVENT_STREAMING0 + 0) * eventHeight;
		cbFrameMetrics->c_frameMetricsEvent1MaxY = (FRAME_METRICS_EVENT_STREAMING0 + 1) * eventHeight;
		cbFrameMetrics->c_frameMetricsEvent1Color = Vec4_Make( 0.0f, 0.0f, 1.0f, 0.0f ); 

		cbFrameMetrics->c_frameMetricsEvent2Mask = 1 << FRAME_METRICS_EVENT_STREAMING1;
		cbFrameMetrics->c_frameMetricsEvent2MinY = (FRAME_METRICS_EVENT_STREAMING1 + 0) * eventHeight;
		cbFrameMetrics->c_frameMetricsEvent2MaxY = (FRAME_METRICS_EVENT_STREAMING1 + 1) * eventHeight;
		cbFrameMetrics->c_frameMetricsEvent2Color = Vec4_Make( 0.0f, 1.0f, 0.0f, 0.0f ); 

		cbFrameMetrics->c_frameMetricsEvent3Mask = 1 << FRAME_METRICS_EVENT_3;
		cbFrameMetrics->c_frameMetricsEvent3MinY = (FRAME_METRICS_EVENT_3 + 0) * eventHeight;
		cbFrameMetrics->c_frameMetricsEvent3MaxY = (FRAME_METRICS_EVENT_3 + 1) * eventHeight;
		cbFrameMetrics->c_frameMetricsEvent3Color = Vec4_Make( 1.0f, 1.0f, 0.0f, 0.0f ); 

		GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_FRAMEMETRICS] );
	}

	V6_ASSERT( (frameMetricsRTSize.x & 0x7) == 0 );
	V6_ASSERT( (frameMetricsRTSize.y & 0x7) == 0 );
	const u32 pixelGroupWidth = frameMetricsRTSize.x >> 3;
	const u32 pixelGroupHeight = frameMetricsRTSize.y >> 3;
	g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_FRAME_METRICS_TIME_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_FRAME_METRICS_EVENT_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
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
		
		const float surfaceWoH = (float)player->win.size.x / player->win.size.y;
		const float frameWoH = (float)player->mainRenderTargetSet.width / player->mainRenderTargetSet.height;

		cbCompose->c_composeSurfaceWidth = player->win.size.x;
		const float norm = player->mainRenderTargetSet.width < (u32)player->win.size.x ? 1.0f : ((float)player->mainRenderTargetSet.width / player->win.size.x);
		cbCompose->c_composeFrameUVBias = Vec2_Make( 0.5f * ( 1.0f - player->win.size.x * norm / player->mainRenderTargetSet.width ), 0.5f * ( 1.0f - player->win.size.y * norm / player->mainRenderTargetSet.height ) );
		cbCompose->c_composeFrameUVScale = Vec2_Make( norm / player->mainRenderTargetSet.width, norm / player->mainRenderTargetSet.height );

		const Color_s backColor = V6_DARK_GRAY;
		const float inv255 = 1.0f / 255.0f;

		cbCompose->c_composeBackColor = Vec4_Make( backColor.r * inv255, backColor.g * inv255, backColor.b * inv255, backColor.a * inv255 );

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
	V6_CPU_EVENT_SCOPE( s_cpuEventFrame );

	// CPU updates
	
	String_ResetInternalBuffer();

	Player_ProcessInputs( player, dt );

	PlayerCommandBuffer_Process( player );

	// GPU frame

	GPUEvent_BeginFrame( frameID );

	if ( player->playerState == PLAYER_STATE_STREAM )
	{
		// update stream GPU data

		if ( player->targetFrameID < player->curFrameID )
			player->curFrameID = (float)player->targetFrameID;
		
		const u32 streamFrameID = (u32)(player->curFrameID + FLT_EPSILON);
		
		u32 sequenceID;

		{
			V6_CPU_EVENT_SCOPE( s_cpuEventGetSequence );

			sequenceID = VideoStream_FindSequenceIDFromFrameID( &player->stream, streamFrameID );

			if ( player->sequenceID != sequenceID )
			{
				for ( u32 sequenceOffset = 0; sequenceOffset < 2; ++sequenceOffset )
				{
					if ( player->sequenceStatus[sequenceOffset] == SEQUENCE_STATUS_STREAMING || player->sequenceStatus[sequenceOffset] == SEQUENCE_STATUS_READY )
						VideoStreamPrefetcher_ReleaseSequence( &player->prefetcher, player->sequenceID + sequenceOffset );
				}

				player->sequenceID = sequenceID;
				player->sequenceStatus[0] = SEQUENCE_STATUS_LOADING;
				player->sequenceStatus[1] = sequenceID + 1 < player->stream.desc.sequenceCount ? SEQUENCE_STATUS_LOADING : SEQUENCE_STATUS_NONE;
			}

			for ( u32 sequenceOffset = 0; sequenceOffset < 2; ++sequenceOffset )
			{
				if ( player->sequenceStatus[sequenceOffset] == SEQUENCE_STATUS_LOADING )
				{
					const VideoStreamGetSequenceStatus_e streamStatus = VideoStreamPrefetcher_GetSequence( &player->prefetcher, sequenceID + sequenceOffset, player->sequenceFillBuffer != 0 );
					if ( streamStatus == VIDEO_STREAM_GET_SEQUENCE_FAILED )
					{
						player->sequenceStatus[sequenceOffset] = SEQUENCE_STATUS_FAILED;
						goto update_frame;
					}
					const bool isLoading = streamStatus == VIDEO_STREAM_GET_SEQUENCE_LOADING;
					player->sequenceStatus[sequenceOffset] = isLoading ? SEQUENCE_STATUS_LOADING : SEQUENCE_STATUS_STREAMING;
					player->sequenceFillBuffer = isLoading;
					
					if ( isLoading )
						Player_SetMetricsEvent( player, FRAME_METRICS_EVENT_LOADING );
				}
			}

			if ( player->sequenceStatus[0] > SEQUENCE_STATUS_LOADING && player->sequenceStatus[1] > SEQUENCE_STATUS_LOADING )
				VideoStreamPrefetcher_Process( &player->prefetcher );
		}
		
		{
			V6_CPU_EVENT_SCOPE( s_cpuEventStream );
			V6_GPU_EVENT_SCOPE( s_gpuEventStream );

			if ( player->sequenceStatus[0] == SEQUENCE_STATUS_STREAMING )
			{
				if ( TraceContext_StreamSequence( &player->traceContext, &player->stream.sequences[sequenceID + 0], sequenceID + 0, true ) )
					player->sequenceStatus[0] = SEQUENCE_STATUS_READY;
				else
					Player_SetMetricsEvent( player, FRAME_METRICS_EVENT_STREAMING0 );
			}
			else if ( player->sequenceStatus[0] == SEQUENCE_STATUS_READY && player->sequenceStatus[1] == SEQUENCE_STATUS_STREAMING )
			{
				if ( TraceContext_StreamSequence( &player->traceContext, &player->stream.sequences[sequenceID + 1], sequenceID + 1 ) )
					player->sequenceStatus[1] = SEQUENCE_STATUS_READY;
				else
					Player_SetMetricsEvent( player, FRAME_METRICS_EVENT_STREAMING1 );
			}
		}

update_frame:
		{
			V6_CPU_EVENT_SCOPE( s_cpuEventUpdate );
			V6_GPU_EVENT_SCOPE( s_gpuEventUpdate );

			if ( player->sequenceStatus[0] == SEQUENCE_STATUS_FAILED || player->sequenceStatus[1] == SEQUENCE_STATUS_FAILED )
			{
				PlayerStream_Unload( player );
			}
			else if ( player->sequenceStatus[0] == SEQUENCE_STATUS_READY && player->sequenceStatus[1] >= SEQUENCE_STATUS_STREAMING )
			{
				TraceContext_UpdateFrame( &player->traceContext, &player->stream.sequences[sequenceID], sequenceID, player->stream.frameOffsets[sequenceID], streamFrameID - player->stream.frameOffsets[sequenceID], player->stack );
				TraceContext_GetFrameBasis( &player->traceContext, &player->frameOrigin, &player->frameYaw );

				if ( streamFrameID < player->targetFrameID )
				{
					const float frameFraction = player->stream.desc.frameRate * dt;
					player->curFrameID = Min( player->curFrameID + frameFraction, player->stream.desc.frameCount - 1.0f );
				}
			}
		}

		Camera_SetPosOffset( &player->camera, &player->frameOrigin );
		Camera_SetYawOffset( &player->camera, player->frameYaw );
	}
	else
	{
		const Vec3 zero = Vec3_Zero();
		Camera_SetPosOffset( &player->camera, &zero );
		Camera_SetYawOffset( &player->camera, 0.0f );
	}

	// update view with inputs

	View_s views[EYE_COUNT];

	float outOfSensorRange = 0.0f;

#if V6_USE_HMD == 1
	Hmd_GetStatus( &player->hmdStatus );

	if ( player->hmdStatus.shouldRecenter )
	{
		PlayerCamera_Recenter( player );
		Hmd_Recenter();
		player->recentered = true;
	}

	if ( player->hmdStatus.shouldQuit )
		Win_Release( &player->win );

	Hmd_SetPerfHUdMode( player->playerOptions.showHMDPerfHUD );

	HmdRenderTarget_s hmdColorRenderTargets[2];
	HmdEyePose_s hmdEyePoses[2];
	player->hmdTackingState = HMD_TRACKING_STATE_OFF;

	if ( Player_IsUsingHMD( player ) )
	{
		PlayerCamera_Recenter( player );

		player->hmdTackingState = Hmd_BeginRendering( hmdColorRenderTargets, hmdEyePoses, player->camera.znear, player->camera.zfar );
		outOfSensorRange = (player->hmdTackingState & HMD_TRACKING_STATE_POS) == 0 ? 1.0f : 0.0f;

		if ( player->playerOptions.lockHMD )
		{
			Camera_SetStereoUsingIPD( &player->camera, IPD );
		}
		else
		{
			const Vec3 eyeOffsets[2] = { hmdEyePoses[0].eyeOffset, hmdEyePoses[1].eyeOffset };
			const Vec3 eyePos[2] = { hmdEyePoses[0].lookAt.GetTranslation(), hmdEyePoses[1].lookAt.GetTranslation() };
			Camera_SetStereoUsingOrientation( &player->camera, &hmdEyePoses[0].lookAt, eyeOffsets, eyePos );
		}
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
	}
	else
#endif // #if V6_USE_HMD == 1
	{
		Camera_SetStereoUsingIPD( &player->camera, IPD );
		Camera_UpdateBasis( &player->camera );
		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
			Camera_MakeView( &views[eye], &player->camera, eye, nullptr );
	}

	float outOfBoxRange = 0.0f;

	if ( player->playerState == PLAYER_STATE_STREAM )
	{
		Vec3 centerEye = Vec3_Zero();
		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
			centerEye += views[eye].org * (1.0f / EYE_COUNT);
		const Vec3 eyeDistanceToOrigin = Abs( centerEye - player->traceContext.frameState.origin );
		outOfBoxRange = Clamp( eyeDistanceToOrigin.Max() - (CODEC_HEAD_ROOM_SIZE - 5.0f), 0.0f, 5.0f ) / 5.0f;
	}

	if ( player->playerOptions.hideHUD )
	{
		outOfSensorRange = 0.0f;
		outOfBoxRange = 0.0f;
	}

	// draw

	{
		V6_CPU_EVENT_SCOPE( s_cpuEventDraw );
		V6_GPU_EVENT_SCOPE( s_gpuEventDraw );

		if ( !player->recentered )
			PlayerScene_DrawParticleForHMD( player, views );
		else if ( player->playerState == PLAYER_STATE_STREAM )
			TraceContext_DrawFrame( &player->traceContext, &player->mainRenderTargetSet, views, &player->traceOptions, Max( outOfSensorRange, outOfBoxRange ) );
		else
			PlayerScene_DrawListForHMD( player, views, dt );
	}

	if ( !player->playerOptions.hideHUD )
		Player_DrawHUD( player, views, outOfSensorRange, outOfBoxRange );

	// present draw

	bool mirrored = false;

#if V6_USE_HMD == 1
	if ( Player_IsUsingHMD( player ) )
	{
		V6_CPU_EVENT_SCOPE( s_cpuEventHMD );
		V6_GPU_EVENT_SCOPE( s_gpuEventHMD );
		Hmd_EndRendering();

		if ( player->playerOptions.mirrorHMD )
		{
			ID3D11Texture2D* tex = (ID3D11Texture2D*)Hmd_GetMirrorTexture();
			g_deviceContext->CopyResource( player->winRenderTargetSet.colorBuffers[0].tex, tex );
			Hmd_ReleaseMirrorTexture( tex );
			mirrored = true;
		}
	}
#endif // #if V6_USE_HMD == 1

	if ( !mirrored )
	{
		if ( player->playerState == PLAYER_STATE_STREAM || V6_DEBUG_HMD_UI )
		{
			V6_GPU_EVENT_SCOPE( s_gpuEventCopy );
			Player_CopyToSurface( player );
		}
		else
		{
			V6_GPU_EVENT_SCOPE( s_gpuEventList );
			PlayerScene_DrawListForDesktop( player, dt );
		}
	}

	// draw UI

	{
		CPUEventDuration_s* cpuEventDurations;
		const u32 cpuEventCount = CPUEvent_UpdateDurations( &cpuEventDurations );

		GPUEventDuration_s* gpuEventDurations;
		const u32 gpuEventCount = GPUEvent_UpdateDurations( &gpuEventDurations );

		if ( player->playerOptions.showUI )
			Player_DrawUI( player, averageFPS, cpuEventDurations, cpuEventCount, gpuEventDurations, gpuEventCount );

		Player_UpdateMetrics( player, frameID, cpuEventDurations, cpuEventCount, gpuEventDurations, gpuEventCount );
		if ( player->playerOptions.showMetricsProfile > 0 )
			Player_DrawMetrics( player, player->playerOptions.showMetricsProfile-1 );

		if ( player->traceOptions.logReadBack )
		{
			for ( u32 cpuEventRank = 0; cpuEventRank < cpuEventCount; ++cpuEventRank )
			{
				const CPUEventDuration_s* eventDuration = &cpuEventDurations[cpuEventRank];
				V6_DEVMSG( "%-10s : %5d us (%1.f calls)\n", eventDuration->name, eventDuration->avgDurationUS, eventDuration->avgCallCount );
			}
			for ( u32 gpuEventRank = 0; gpuEventRank < gpuEventCount; ++gpuEventRank )
			{
				const GPUEventDuration_s* eventDuration = &gpuEventDurations[gpuEventRank];
				V6_DEVMSG( "%*s%-10s : %5d us\n", eventDuration->depth * 4, "", eventDuration->name, eventDuration->avgDurationUS );
			}
		}
	}

	{
		V6_CPU_EVENT_SCOPE( s_cpuEventPresent );
		V6_GPU_EVENT_SCOPE( s_gpuEventPresent );
		GPUSurfaceContext_Present();
	}

	GPUEvent_EndFrame();

	player->traceOptions.logReadBack = false;

	player->time += dt;
}

static void Player_FrindOrCreateAppDataPath( char* appDataPath, u32 appDataPathMaxSize )
{	
	V6_ASSERT( appDataPathMaxSize > 0 );
	appDataPath[0] = 0;
	if ( FileSystem_GetLocalAppDataPath( appDataPath, appDataPathMaxSize ) )
	{
		sprintf_s( appDataPath, appDataPathMaxSize, "%s\\%s\\%s", appDataPath, COMPANY_NAME, PRODUCT_NAME );
		if ( !FileSystem_CreateDirectory( appDataPath ) )
			appDataPath[0] = 0;
	}
}

static void Player_InitStreams( Player_s* player )
{
	PlayerStreamItem_Create( player );
	PlayerScene_Create( player );
}

static void Player_ReleaseStreams( Player_s* player )
{
	if ( player->playerState == PLAYER_STATE_STREAM )
		PlayerStream_Release( player );
	PlayerScene_Release( player );
	PlayerStreamItem_Release( player );
}

static void Player_ReloadStreams( Player_s* player )
{
	Player_ReleaseStreams( player );
	Player_InitStreams( player );
}

static bool Player_Create( Player_s* player, const char* appDataPath, IAllocator* heap, IStack* stack )
{
#if V6_USE_HMD == 1
	if ( !Hmd_Init() )
	{
		V6_ERROR( "Call to Hmd_Init failed!\n" );
		return false;
	}

	const u32 presentRate = (u32)Hmd_GetDisplayRefreshRate();
	const Vec2 tanHalfFov = Hmd_GetRecommendedTanHalfFOV();
	const u32 renderTargetHeightAt90 = Hmd_GetResolution().y < 1200 ? RENDER_TARGET_HEIGHT_AT_90_LOW : RENDER_TARGET_HEIGHT_AT_90_HIGH;
#else
	const u32 presentRate = Codec_GetDefaultFrameRate();
	//const Vec2 tanHalfFov = Vec2_Make( Tan( 0.5f * DegToRad( 96.0f ) ), Tan( 0.5f * DegToRad( 115.0f ) ) );
    const Vec2 tanHalfFov = Vec2_Make( Tan( 0.5f * DegToRad( 90.0f ) ), Tan( 0.5f * DegToRad( 90.0f ) ) );
	const u32 renderTargetHeightAt90 = RENDER_TARGET_HEIGHT_AT_90_HIGH;
#endif
	
	const float ratioXoverY = tanHalfFov.x / tanHalfFov.y;
	
	const float renderTargetHeight = (float)renderTargetHeightAt90 * tanHalfFov.y;
	const float renderTargetWidth = renderTargetHeight * ratioXoverY; 
	const u32 width = (u32)(renderTargetWidth + 7) & ~7;
	const u32 height = (u32)(renderTargetHeight + 7) & ~7;
	const float tanHalfFovPerPixel = tanHalfFov.y / height;

	V6_MSG( "rt.resolution: %dx%d\n", width, height );

	const u32 windowWidth = 1280;
	const u32 windowHeight = 720;

	player->commandLineSize = (u32)-1;
	player->heap = heap;
	player->stack = stack;
	player->presentRate = presentRate;

	const char* title = String_Format( "%s %d.%d.%d%s", PRODUCT_NAME, V6_VERSION_MAJOR, V6_VERSION_MINOR, V6_VERSION_REV, Platform_IsDevelopperMode() ? " - DEV" : "" );
	if ( !Win_Create( &player->win, player, title, -1, -1, windowWidth, windowHeight, WIN_FLAG_IS_MAIN | WIN_FLAG_RESIZABLE ) )
		return false;

	Win_RegisterKeyEvent( &player->win, Player_OnKeyEvent );
	Win_RegisterMouseEvent( &player->win, Player_OnMouseEvent );
	Win_RegisterResizeEvent( &player->win, Player_OnResizeEvent );

	Gamepad_Init( &player->gamepad, 0, player );
	Gamepad_RegisterButtonEvent( &player->gamepad, Player_OnGamepadButtonEvent );

	strcpy_s( player->appDataPath, sizeof( player->appDataPath ), appDataPath );

	V6_MSG( "Application data path: %s\n", player->appDataPath );

	char iniPath[256];
	if ( player->appDataPath[0] )
		sprintf_s( iniPath, sizeof( iniPath ), "%s/%s", player->appDataPath, INI_FILE );
	else
		strcpy_s( iniPath, sizeof( iniPath ), INI_FILE );
	Ini_Init( &player->ini, iniPath );

	if ( !PlayerDevice_Create( player, width, height ) )
		return false;

#if V6_USE_HMD == 1
	if ( !Hmd_CreateResources( GPUDevice_Get(), &Vec2i_Make( width, height ) ) )
	{
		V6_ERROR( "Call to Hmd_CreateResources failed!\n" );
		PlayerDevice_Release( player );
		return false;
	}
#else
	player->recentered = true;
#endif // #if V6_USE_HMD == 1

	Camera_Create( &player->camera, ZNEAR_DEFAULT, ZFAR_DEFAULT, tanHalfFovPerPixel * player->mainRenderTargetSet.height, (float)player->mainRenderTargetSet.width / player->mainRenderTargetSet.height );
	
	Player_InitStreams( player );

	VideoStreamPrefetcher_Create( &player->prefetcher, SEQUENCE_PREFTECH_DURATION, player->heap );

	V6_MSG( "Player created\n" );

	return true;
}

static void Player_Release( Player_s* player )
{
	VideoStreamPrefetcher_Release( &player->prefetcher, player->heap );
	Player_ReleaseStreams( player );
	Ini_Release( &player->ini );
	Gamepad_Release( &player->gamepad );
#if V6_USE_HMD == 1
	Hmd_ReleaseResources();
	Hmd_Shutdown();
#endif // #if V6_USE_HMD == 1
	PlayerDevice_Release( player );
}

//----------------------------------------------------------------------------------------------------

static Player_s*	s_outputPlayer = nullptr;
static FILE*		s_outputLogFile = nullptr;
static Mutex_s		s_outputLogMutex;

void OutputMessage( u32 msgType, const char * format, ... )
{
#if 0
	const u32 bufferID = s_ouputMessageCount % s_ouputMessageBufferCount;
	Atomic_Inc( &s_ouputMessageCount );

	va_list args;
	va_start( args, format );
	vsprintf_s( s_ouputMessageBuffers[bufferID], sizeof( s_ouputMessageBuffers[bufferID] ), format, args );
	va_end( args );

	if ( !s_outputLogFile )
		return;

	Mutex_Lock( &s_outputLogMutex );

	switch( msgType )
	{
	case MSG_DEV:
		if ( Platform_IsDevelopperMode() )
			fprintf( s_outputLogFile, "[DEV] %s", s_ouputMessageBuffers[bufferID] );
		break;
	
	case MSG_LOG:
		fprintf( s_outputLogFile, "[LOG] %s", s_ouputMessageBuffers[bufferID] );
		break;
	
	case MSG_WARNING:
		fprintf( s_outputLogFile, "[WARNING] %s", s_ouputMessageBuffers[bufferID] );
		break;
	
	case MSG_ERROR:
		fprintf( s_outputLogFile, "[ERROR] %s", s_ouputMessageBuffers[bufferID] );
		break;
	
	case MSG_FATAL:
		fprintf( s_outputLogFile, "[FATAL] %s", s_ouputMessageBuffers[bufferID] );
		fflush( s_outputLogFile );
		
		if ( s_outputPlayer )
		{
			Win_ShowMessage( &s_outputPlayer->win, s_ouputMessageBuffers[bufferID], "Fatal Error" );
			Win_Terminate( &s_outputPlayer->win );
		}

		break;
	}

	Mutex_Unlock( &s_outputLogMutex );
#else
  va_list args;
  va_start( args, format );
  vprintf_s( format, args );
  va_end( args );
#endif
}

static void Player_OpenLogFile( const char* appDataPath )
{
	char logPath[256];
	if ( appDataPath[0] )
		sprintf_s( logPath, sizeof( logPath ), "%s/%s", appDataPath, LOG_FILE );
	else
		strcpy_s( logPath, sizeof( logPath ), LOG_FILE );

	if ( fopen_s( &s_outputLogFile, logPath, "wt" ) == 0 && s_outputLogFile )
	{
		Mutex_Create( &s_outputLogMutex );
	}
	else
	{
		V6_WARNING( "Unable to open log file %s\n", logPath );
	}
}

static void Player_CloseLogFile()
{
	if ( s_outputLogFile )
	{
		Mutex_Lock( &s_outputLogMutex );
		FILE* const logFile = s_outputLogFile;
		s_outputLogFile = nullptr;
		Mutex_Unlock( &s_outputLogMutex );

		Mutex_Release( &s_outputLogMutex );
		fclose( logFile );
	}
}

int Player_Main( const char* appDataPath, const char* platformName, IAllocator* heap, IStack* stack )
{
	V6_MSG( "%s %d.%d.%d\n", PRODUCT_NAME, V6_VERSION_MAJOR, V6_VERSION_MINOR, V6_VERSION_REV );

	if ( !Platform_Init( platformName, PLATFORM_APP_ID ) )
		return 1;

	Player_s* player = stack->newInstanceAndClear< Player_s >( "Player" );
	s_outputPlayer = player;

	if ( !Player_Create( player, appDataPath, heap, stack ) )
		return 1;

	Win_Show( &player->win, true );

	V6_MSG( "Process loop started\n" );

	FrameTimer_s frameTimer;
	FrameTimer_Init( &frameTimer, (float)player->presentRate, 0.1f );

	while ( !Win_ProcessMessagesAndShouldQuit( &player->win ) )
	{
		FrameInfo_s frameInfo;
		FrameTimer_ComputeNewFrameInfo( &frameTimer, &frameInfo );

		if ( Platform_ProcessMessages() )
			Player_ProcessFrame( player, frameInfo.frameID, frameInfo.dt, frameInfo.averageFPS );
		else
			Win_Release( &player->win );
	}

	Player_Release( player );
	stack->deleteInstance( player );

	Platform_Shutdown();

	return 0;
}

END_V6_NAMESPACE

#if 0
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow )
{
	const char* const platform = pCmdLine;
#else
int main( int argc, const char** argv )
{
	const char* const platform = argc > 1 ? argv[1] : "";
#endif
	v6::CHeap heap;
	v6::Stack stack( &heap, 400 * 1024 * 1024 );

	char appDataPath[256] = {};
	v6::Player_FrindOrCreateAppDataPath( appDataPath, sizeof( appDataPath ) );
	v6::Player_OpenLogFile( appDataPath );

	const int exitCode = v6::Player_Main( appDataPath, platform, &heap, &stack );

	v6::Player_CloseLogFile();

	return exitCode;
}
