/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <Windowsx.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/codec/codec.h>
#include <v6/codec/decoder.h>
#include <v6/codec/encoder.h>
#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/memory.h>
#include <v6/core/obj_reader.h>
#include <v6/core/random.h>
#include <v6/core/stream.h>
#include <v6/core/string.h>
#include <v6/core/time.h>
#include <v6/core/thread.h>
#include <v6/core/vec2.h>
#include <v6/core/vec2i.h>
#include <v6/core/vec3.h>
#include <v6/core/vec3i.h>
#include <v6/core/win.h>

#include <v6/graphic/capture.h>
#include <v6/graphic/gpu.h>
#include <v6/graphic/scene.h>

#include <v6/viewer/scene_info.h>
#include <v6/viewer/viewer_shared.h>

#pragma comment( lib, "d3d11.lib" )

#define V6_D3D_DEBUG			0
#define V6_LOAD_EXTERNAL		1
#define V6_SIMPLE_SCENE			0
#define V6_USE_ALPHA_COVERAGE	1
#define V6_ENABLE_HMD			1
#define V6_USE_HMD				(V6_ENABLE_HMD == 1 && HLSL_STEREO == 1)
#define V6_USE_CACHE			0

#if V6_USE_HMD == 1
#include <v6/graphic/hmd.h>
#endif // #if HLSL_STEREO == 1

BEGIN_V6_NAMESPACE

extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_deviceContext;

static const u32 CUBE_SIZE						= HLSL_GRID_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH;
static const float GRID_MAX_SCALE				= 2000.0f;
static const float GRID_MIN_SCALE				= 50.0f;

static const u32 ANY_EYE						= 0;
#if HLSL_STEREO == 1
static const u32 LEFT_EYE						= 0;
static const u32 RIGHT_EYE						= 1;
static const float IPD							= 6.5f;
#else
static const u32 LEFT_EYE						= 0;
static const u32 RIGHT_EYE						= 0;
static const float IPD							= 0.0f;
#endif
static const float ZNEAR						= GRID_MIN_SCALE * 0.5f;
static const float ZFAR							= 10000.0f;
#if V6_SIMPLE_SCENE == 1
static const float FOV							= DegToRad( 90.0f );
#else
static const float FOV							= DegToRad( 90.0f );
#endif
static const u32 GRID_COUNT						= Codec_GetMipCount( GRID_MIN_SCALE, GRID_MAX_SCALE );
static const int SAMPLE_MAX_COUNT				= 1;
static const float FREE_SCALE					= 50.0f;
static const u32 RANDOM_CUBE_COUNT				= 100;

static const u32 HMD_FPS						= 75;
static const u32 VIDEO_FRAME_MAX_COUNT			= 10;
static const float VIDEO_FRAME_RATIO			= 25.0f / 75.0f;

static const u32 DEBUG_BLOCK_MAX_COUNT			= HLSL_BLOCK_THREAD_GROUP_SIZE * 10;
static const u32 DEBUG_TRACE_MAX_COUNT			= HLSL_BLOCK_THREAD_GROUP_SIZE * 10;

static Win_s									s_win;
#if V6_USE_HMD
v6::u32		s_hmdState							= v6::HMD_TRACKING_STATE_OFF;
#else
v6::u32		s_hmdState							= 0;
#endif // #if V6_USE_HMD

static POINT		s_mouseCursorPos			= {};

enum DrawMode_e
{
	DRAW_MODE_DEFAULT,	
	DRAW_MODE_BLOCK,
	
	DRAW_MODE_COUNT
};

enum
{
	CONSTANT_BUFFER_BASIC		=	hlsl::CBBasicSlot,
	CONSTANT_BUFFER_GENERIC		=	hlsl::CBGenericSlot,
	CONSTANT_BUFFER_CULL		=	hlsl::CBCullSlot,
	CONSTANT_BUFFER_BLOCK		=	hlsl::CBBlockSlot,
	CONSTANT_BUFFER_PIXEL		=	hlsl::CBPixelSlot,
	CONSTANT_BUFFER_COMPOSE		=	hlsl::CBComposeSlot,

	CONSTANT_BUFFER_COUNT
};

enum
{
	COMPUTE_BLOCK_CULL4,
	COMPUTE_BLOCK_CULL8,
	COMPUTE_BLOCK_CULL16,
	COMPUTE_BLOCK_CULL32,
	COMPUTE_BLOCK_CULL64,
	COMPUTE_BLOCK_CULL_STATS4,
	COMPUTE_BLOCK_CULL_STATS8,
	COMPUTE_BLOCK_CULL_STATS16,
	COMPUTE_BLOCK_CULL_STATS32,
	COMPUTE_BLOCK_CULL_STATS64,
	COMPUTE_BLOCK_TRACE_INIT,
	COMPUTE_BLOCK_TRACE4,
	COMPUTE_BLOCK_TRACE8,
	COMPUTE_BLOCK_TRACE16,
	COMPUTE_BLOCK_TRACE32,
	COMPUTE_BLOCK_TRACE64,
	COMPUTE_BLOCK_TRACE_DEBUG4,
	COMPUTE_BLOCK_TRACE_DEBUG8,
	COMPUTE_BLOCK_TRACE_DEBUG16,
	COMPUTE_BLOCK_TRACE_DEBUG32,
	COMPUTE_BLOCK_TRACE_DEBUG64,
	COMPUTE_BLENDPIXEL,
	COMPUTE_BLENDPIXEL_OVERDRAW,
	COMPUTE_COMPOSESURFACE,

	COMPUTE_COUNT
};

enum
{
	SHADER_BASIC,
	SHADER_FAKE_CUBE,
	SHADER_GENERIC,
	SHADER_GENERIC_ALPHA_TEST,

	SHADER_COUNT
};

enum
{
	QUERY_FREQUENCY,
	QUERY_FRAME_BEGIN,
	QUERY_T0,
	QUERY_T1,
	QUERY_T2,
	QUERY_T3,
	QUERY_T4,
	QUERY_FRAME_END,

	QUERY_COUNT,

	QUERY_TIME_COUNT = QUERY_FRAME_END - QUERY_T0 - 1
};

enum
{
	MESH_TRIANGLE,
	MESH_BOX_WIREFRAME,
	MESH_BOX_RED,
	MESH_BOX_BLUE,
	MESH_BOX_GREEN,
	MESH_QUAD_RED0,
	MESH_QUAD_RED1,
	MESH_QUAD_RED2,
	MESH_QUAD_BLUE,
	MESH_QUAD_GREEN,
	MESH_VIRTUAL_QUAD,
	MESH_FAKE_CUBE,
	MESH_POINT,
	MESH_LINE,
	MESH_VIRTUAL_BOX,

	MESH_COUNT
};

enum
{
	MATERIAL_DEFAULT_BASIC,
	MATERIAL_DEFAULT_FAKE_CUBE,

	MATERIAL_DEFAULT_COUNT
};

enum
{
	TEXTURE_GENERIC_DIFFUSE,
	TEXTURE_GENERIC_ALPHA,
	TEXTURE_GENERIC_NORMAL,

	TEXTURE_GENERIC_COUNT
};

enum CubeAxis_e
{
	CUBE_AXIS_POSITIVE_X,
	CUBE_AXIS_NEGATIVE_X,
	CUBE_AXIS_POSITIVE_Y,
	CUBE_AXIS_NEGATIVE_Y,
	CUBE_AXIS_POSITIVE_Z,
	CUBE_AXIS_NEGATIVE_Z,

	CUBE_AXIS_COUNT
};

enum
{
	PATH_CAMERA,
	PATH_OBJECT1,

	PATH_COUNT
};

enum
{
	RENDER_FLAGS_IS_CAPTURING			= 1 << 0,
	RENDER_FLAGS_USE_ALPHA_COVERAGE		= 1 << 1,
};

struct Path_s
{
	static const u32	MAX_POINT_COUNT = 128;
	Vec3				positions[MAX_POINT_COUNT];
	int					keyCount;
	int					activeKey;
	u32					entityID;
	float				speed;
	bool				dirty;
};

struct PathPlayer_s
{
	Path_s*				paths;
	u32					pathCount;
	float				times[PATH_COUNT][Path_s::MAX_POINT_COUNT];
	float				durations[PATH_COUNT];
	float				durationMax;
	float				pathTime;
	u32					pathFrame;
	bool				isPlaying;
	bool				isPaused;
};

struct BasicVertex_s
{
	Vec3 position;
	Color_s color;
};

struct GenericVertex_s
{
	Vec3		position;
	Vec3		normal;
	Vec2		uv;
};

struct CubeVertex_s
{
	Vec3 pos;
	Vec2 uv;
};

struct CubeFaceContext_s
{
	GPUColorRenderTarget_s		color;
	GPUDepthRenderTarget_s		depth;
};

struct RenderingView_s
{
	Vec3							org;
	Vec3							forward;
	Vec3							right;
	Vec3							up;

	View_s							view;
	
	float							tanHalfFOVLeft;
	float							tanHalfFOVRight;
	float							tanHalfFOVUp;
	float							tanHalfFOVDown;
};

struct SceneViewer_s : Scene_s
{
	char			filename[256];
	SceneInfo_s		info;
};

struct SceneDebug_s : SceneViewer_s
{
	u32				meshLineID;
	u32				meshGridID;
	u32				meshCellIDs[HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH][2];
	u32				meshBlockIDs[DEBUG_BLOCK_MAX_COUNT][2];
	u32				entityLineID;
	u32				entityGridID;
	u32				entityCellIDs[HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH][2];
	u32				entityBlockIDs[DEBUG_BLOCK_MAX_COUNT][2];
	u32				entityTraceIDs[DEBUG_BLOCK_MAX_COUNT];
};

struct ScenePathGeo_s : SceneViewer_s
{
	u32				meshLineIDs[Path_s::MAX_POINT_COUNT-1];
	u32				meshBoxID;
	u32				meshSelectedBoxID;
	u32				entityLineIDs[Path_s::MAX_POINT_COUNT];
	u32				entityBoxIDs[Path_s::MAX_POINT_COUNT];
	u32				entitySelectedBoxID;
};

struct SceneContext_s
{
	char			filename[256];
	v6::IStack*		stack;
	ObjScene_s		objScene;
	Scene_s*		scene;
	v6::Signal_s	deviceReady;
	v6::Signal_s	loadDone;
};

struct SequenceContext_s
{	
	static const u32	GROUP_MAX_COUNT = 65536;

	CodecRange_s*		rangeDefs[CODEC_BUCKET_COUNT];
	hlsl::BlockRange	blockRanges[CODEC_BUCKET_COUNT][CODEC_RANGE_MAX_COUNT];
	u32					frameBlockPosOffsets[CODEC_FRAME_MAX_COUNT];
	u32					frameBlockDataOffsets[CODEC_FRAME_MAX_COUNT];
	u32					blockMaxCount;
	GPUBuffer_s			blockPos;
	GPUBuffer_s			blockData;
	GPUBuffer_s			ranges[2];
	GPUBuffer_s			groups[2];
};

struct TraceContext_s
{
	GPUBuffer_s		traceCell;
	GPUBuffer_s		traceIndirectArgs;
	
	GPUBuffer_s		cellItems;
	GPUBuffer_s		cellItemCounters;

	GPUTexture2D_s	colors;

	GPUBuffer_s		cullStats;
	GPUBuffer_s		traceStats;

	u32				passedBlockCount;
	u32				cellItemCount;
};

struct TraceData_s
{
	float	tIn;
	float	tOut;
	Vec3i	hitFoundCoords;
	u32		hitFailBits;
};

static float g_translation_speed	= 200.0f;
static bool g_mousePressed			= false;
static int g_mouseDeltaX			= 0;
static int g_mouseDeltaY			= 0;
static int g_mousePickPosX			= 0;
static int g_mousePickPosY			= 0;
static bool g_mousePicked			= false;
static u32 g_pickedPackedID			= (u32)-1;
static int g_keyLeftPressed			= false;
static int g_keyRightPressed		= false;
static int g_keyUpPressed			= false;
static int g_keyDownPressed			= false;

static DrawMode_e g_drawMode		= DRAW_MODE_DEFAULT;

static int g_sample					= 0;

static bool g_keyPath				= false;
static bool g_showPath				= false;
static int g_limit					= false; 
static bool g_showMip				= false;
static bool g_showBucket			= false; 
static bool g_showOverdraw			= false;
static bool g_showHistory			= false;
static int g_pixelMode				= 0;
static bool g_randomBackground		= false;
static bool g_traceGrid				= false;
#if V6_SIMPLE_SCENE == 1
static bool g_useMSAA				= false;
#else
static bool g_useMSAA				= true;
#endif
static bool g_showObjects			= false;
static bool g_debugBlocks			= false;
static bool g_transparentDebug		= false;
static bool g_reloadShaders			= false;

static float s_yaw					= 0.0f;
static float s_pitch				= 0.0f;
static Vec3 s_headOffset			= Vec3_Zero();
static Vec3 s_buildOrigin			= Vec3_Zero();

static bool s_logReadBack			= false;

static SceneViewer_s* s_activeScene	= nullptr;

static Vec3 s_gridCenter = {};
static float s_gridScale = 0;
static u32 s_gridOccupancy = 0;
static Vec3 s_rayOrg = {};
static Vec3 s_rayEnd = {};

static u32				s_activePath = PATH_CAMERA;
static Path_s			s_paths[PATH_COUNT];
static const float		s_defaultPathSpeed = 100.0f;
static PathPlayer_s		s_pathPlayer;
static GPUQuery_s*		s_pendingQueries = nullptr;

static void Path_Init( Path_s* path )
{
	memset( path, 0, sizeof( *path ) );
	path->entityID = (u32)-1;
	path->speed = s_defaultPathSpeed;
	path->dirty = true;
}

static void Path_Release( Path_s* path )
{
}

static void Path_SelectKey( Path_s* path, int key )
{
	path->activeKey = Clamp( key, 0, path->keyCount-1 );
	path->dirty = true;
}

static void Path_DeleteKey( Path_s* path, u32 key )
{
	V6_ASSERT( path->keyCount > 0 );
	V6_ASSERT( key < (u32)path->keyCount );
	for ( u32 keyNext = key+1; keyNext < (u32)path->keyCount; ++keyNext )
		path->positions[keyNext-1] = path->positions[keyNext];
	--path->keyCount;

	path->activeKey = Min( (int)key, path->keyCount );
	path->dirty = true;
}

static void Path_InsertKey( Path_s* path, const Vec3* position, u32 key )
{
	V6_ASSERT( path->keyCount < Path_s::MAX_POINT_COUNT );
	if ( key != path->keyCount )
	{
		++key;
		for ( u32 keyNext = path->keyCount; keyNext > key; --keyNext )
			path->positions[keyNext] = path->positions[keyNext-1];
	}
	V6_ASSERT( key <= (u32)path->keyCount );
	path->positions[key] = *position;
	++path->keyCount;

	path->activeKey = key;
	path->dirty = true;
}

static void Path_Load( Path_s* paths, u32 maxPathCount, const SceneViewer_s* scene )
{
	V6_STATIC_ASSERT( SceneInfo_s::MAX_POSITION_COUNT == Path_s::MAX_POINT_COUNT );
	V6_ASSERT( maxPathCount <= SceneInfo_s::MAX_PATH_COUNT );
	const SceneInfo_s* sceneInfo = &scene->info;
	for ( u32 pathID = 0; pathID < maxPathCount; ++pathID )
	{
		for ( u32 positionID = 0; positionID < sceneInfo->paths[pathID].positionCount; ++positionID )
			paths[pathID].positions[positionID] = sceneInfo->paths[pathID].positions[positionID];
		paths[pathID].keyCount = sceneInfo->paths[pathID].positionCount;
		paths[pathID].entityID = Scene_FindEntityByName( scene, sceneInfo->paths[pathID].entityName );
		paths[pathID].speed = sceneInfo->paths[pathID].speed != 0.0f ? sceneInfo->paths[pathID].speed : s_defaultPathSpeed;
		paths[pathID].dirty = true;
	}
}

static void Path_Save( const Path_s* paths, u32 pathCount, SceneViewer_s* scene )
{
	V6_ASSERT( pathCount <= SceneInfo_s::MAX_PATH_COUNT );
	SceneInfo_s* sceneInfo = &scene->info;
	for ( u32 pathID = 0; pathID < pathCount; ++pathID )
	{
		for ( u32 positionID = 0; positionID < (u32)paths[pathID].keyCount; ++positionID )
			sceneInfo->paths[pathID].positions[positionID] = paths[pathID].positions[positionID];
		sceneInfo->paths[pathID].positionCount = paths[pathID].keyCount;
		if ( paths[pathID].entityID != (u32)-1 )
			strcpy_s( sceneInfo->paths[pathID].entityName, sizeof( sceneInfo->paths[pathID].entityName ), scene->entities[paths[pathID].entityID].name );
		else
			sceneInfo->paths[pathID].entityName[0] = 0;
		sceneInfo->paths[pathID].speed = paths[pathID].speed;
		sceneInfo->dirty = true;
	}
}

static void PathPlayer_Init( PathPlayer_s* pathlayer )
{
	memset( pathlayer, 0, sizeof( * pathlayer ) );
}

static void PathPlayer_Release( PathPlayer_s* pathPlayer )
{
}

static void PathPlayer_Compute( PathPlayer_s* pathPlayer, Path_s* paths, u32 pathCount )
{
	pathPlayer->paths = paths;
	pathPlayer->pathCount = pathCount;

	for ( u32 pathID = 0; pathID < pathCount; ++pathID )
	{
		Path_s* path = &paths[pathID];

		if ( !path->dirty )
			continue;

		if ( path->keyCount == 0 )
		{
			for ( u32 key = 0; key < (u32)path->keyCount; ++key )
			{
				pathPlayer->times[pathID][key] = 0.0f;
				pathPlayer->durations[pathID] = 0.0f;
			}
		}
		else
		{
			V6_ASSERT( path->speed > 0.0f );
			const float invSpeed = 1.0f / path->speed;
			pathPlayer->times[pathID][0] = 0.0f;
			pathPlayer->durations[pathID] = 0.0f;
			for ( u32 key = 1; key < (u32)path->keyCount; ++key )
			{
				const Vec3 delta = path->positions[key] - path->positions[key-1];
				const float interval = delta.Length() * invSpeed;
				V6_ASSERT( interval > 0.00001f );
				pathPlayer->times[pathID][key] = pathPlayer->times[pathID][key-1] + interval;
				pathPlayer->durations[pathID] += interval;
			}
		}

		path->dirty = false;
	}

	pathPlayer->durationMax = 0.0f;
	for ( u32 pathID = 0; pathID < pathCount; ++pathID )
		pathPlayer->durationMax = Max( pathPlayer->durationMax, pathPlayer->durations[pathID] );
}

static void PathPlayer_Play( PathPlayer_s* pathPlayer, bool play )
{
	pathPlayer->isPlaying = play;
	pathPlayer->isPaused = false;
	pathPlayer->pathTime = 0.0f;
	pathPlayer->pathFrame = 0;
}

static void PathPlayer_Pause( PathPlayer_s* pathPlayer, int relativeFrameID )
{
	pathPlayer->isPaused = true;
	
	if ( !pathPlayer->isPlaying )
	{
		pathPlayer->isPlaying = true;
		pathPlayer->pathTime = 0.0f;
		pathPlayer->pathFrame = 0;
		return;
	}

	if ( relativeFrameID == 0 )
		return;

	const float dt = 1.0f / HMD_FPS;
	pathPlayer->pathFrame = Clamp( (int)pathPlayer->pathFrame + relativeFrameID, 0, (int)VIDEO_FRAME_MAX_COUNT-1 );
	pathPlayer->pathTime = pathPlayer->pathFrame * dt;
}

static u32 PathPlayer_GetFrame( PathPlayer_s* pathPlayer )
{
	if ( pathPlayer->isPlaying )
		return pathPlayer->pathFrame;
	return 0;
}

static bool PathPlayer_GetPosition( Vec3* position, const PathPlayer_s* pathPlayer, u32 pathID )
{
	V6_ASSERT( pathPlayer->paths != nullptr);
	V6_ASSERT( pathID < pathPlayer->pathCount );

	const Path_s* path = &pathPlayer->paths[pathID];

	V6_ASSERT( !path->dirty );

	if ( path->keyCount == 0 )
		return false;

	V6_ASSERT( pathPlayer->pathTime >= 0.0f );
	for ( u32 key = 1; key < (u32)path->keyCount; ++key )
	{
		if ( pathPlayer->times[pathID][key] >= pathPlayer->pathTime )
		{
			const float alpha = (pathPlayer->pathTime - pathPlayer->times[pathID][key-1]) / (pathPlayer->times[pathID][key] - pathPlayer->times[pathID][key-1]);
			*position = (1.0f - alpha) * path->positions[key-1] + alpha * path->positions[key];
			return true;
		}
	}

	*position = path->positions[path->keyCount-1];
	return true;
}

static bool PathPlayer_AddTimeStep( PathPlayer_s* pathPlayer, float dt )
{
	if ( !pathPlayer->isPlaying )
		return false;

	if ( pathPlayer->isPaused )
		return true;

	if ( pathPlayer->pathTime + dt > pathPlayer->durationMax || pathPlayer->pathFrame + 1 >= VIDEO_FRAME_MAX_COUNT )
	{
		pathPlayer->isPlaying = false;
		return false;
	}

	pathPlayer->pathTime += dt;
	++pathPlayer->pathFrame;

	return true;
}

static void Viewer_OnKeyEvent( const KeyEvent_s* keyEvent )
{
	if ( g_keyPath )
	{
		if ( keyEvent->pressed )
		{
			switch( keyEvent->key )
			{
			case 'A':
				s_activePath = (s_activePath + 1) % PATH_COUNT;
				s_paths[s_activePath].dirty = true;
				V6_MSG( "Path: %s#%d\n", s_activePath == PATH_CAMERA ? "camera" : "object", s_activePath );
				break;
			case 'C':
				break;
			case 'P':
				PathPlayer_Play( &s_pathPlayer, s_pathPlayer.isPlaying ? false : s_paths[s_activePath].keyCount > 0 );
				V6_MSG( "Path: %s\n", s_pathPlayer.isPlaying ? "play" : "stop" );
				break;
			case 'R':
				if ( s_activePath == PATH_CAMERA && s_paths[PATH_CAMERA].keyCount > 0 )
				{
					PathPlayer_Play( &s_pathPlayer, true );
					g_drawMode = DRAW_MODE_BLOCK;
					g_sample = 0;
					V6_MSG( "Path: record\n" );
				}
				break;
			case 'S':
				g_showPath = !g_showPath;
				V6_MSG( "Path: %s\n", g_showPath ? "show" : "hide" );
				break;
			case 0x21:
				Path_SelectKey( &s_paths[s_activePath], s_paths[s_activePath].activeKey + 1 );
				V6_MSG( "Path: key #%03u/#%03u\n", s_paths[s_activePath].activeKey, s_paths[s_activePath].keyCount );
				break;
			case 0x22:
				Path_SelectKey( &s_paths[s_activePath], s_paths[s_activePath].activeKey - 1 );
				V6_MSG( "Path: key #%03u/#%03u\n", s_paths[s_activePath].activeKey, s_paths[s_activePath].keyCount );
				break;
			case 0x23:
				Path_SelectKey( &s_paths[s_activePath], s_paths[s_activePath].keyCount-1 );
				V6_MSG( "Path: key #%03u/#%03u\n", s_paths[s_activePath].activeKey, s_paths[s_activePath].keyCount );
				break;
			case 0x24:
				Path_SelectKey( &s_paths[s_activePath], 0 );
				V6_MSG( "Path: key #%03u/#%03u\n", s_paths[s_activePath].activeKey, s_paths[s_activePath].keyCount );
				break;
			case 0x6A:
				Path_Save( s_paths, PATH_COUNT, s_activeScene );
				V6_MSG( "Path: saved.\n" );
				break;
			case 0x6B:
				Path_InsertKey( &s_paths[s_activePath], &s_headOffset, s_paths[s_activePath].activeKey );
				V6_MSG( "Path: key #%03u/#%03u\n", s_paths[s_activePath].activeKey, s_paths[s_activePath].keyCount );
				break;
			case 0x6D:
				if ( s_paths[s_activePath].keyCount )
				{
					Path_DeleteKey( &s_paths[s_activePath], s_paths[s_activePath].activeKey );
					V6_MSG( "Path: key #%03u/#%03u\n", s_paths[s_activePath].activeKey, s_paths[s_activePath].keyCount );
				}
				break;
			case 0x28:
				PathPlayer_Pause( &s_pathPlayer, -1 );
				V6_MSG( "Camera pause: frame %d\n", s_pathPlayer.pathFrame );
				break;
			case 0x26:
				PathPlayer_Pause( &s_pathPlayer, +1 );
				V6_MSG( "Camera pause: frame %d\n", s_pathPlayer.pathFrame );
				break;
			default:
				V6_MSG( "Unknow path key: %04x\n", keyEvent->key );
			}
		}
		else if ( keyEvent->key == 'C' )
		{
			g_keyPath = false;
		}
	}
	else
	{
		switch( keyEvent->key )
		{			
		case 0x1B:
			Win_Release( &s_win );
			break;
		case 'A': g_keyLeftPressed = keyEvent->pressed; break;
		case 'B': g_drawMode = keyEvent->pressed ? (g_drawMode == DRAW_MODE_BLOCK ? DRAW_MODE_DEFAULT : DRAW_MODE_BLOCK) : g_drawMode; break;
		case 'C': g_keyPath = keyEvent->pressed; break;
		case 'D': g_keyRightPressed = keyEvent->pressed; break;
		case 'E': g_useMSAA = keyEvent->pressed ? !g_useMSAA : g_useMSAA; break;
		case 'F': g_showObjects = keyEvent->pressed ? !g_showObjects : g_showObjects; break;
		case 'G': if ( keyEvent->pressed ) { g_debugBlocks = true; } break;
		case 'H': g_showHistory = keyEvent->pressed ? !g_showHistory: g_showHistory; break;
		case 'I': if ( keyEvent->pressed ) { s_logReadBack = true; } break;
		case 'L': g_limit = keyEvent->pressed ? !g_limit : g_limit; break;
		case 'M': g_showMip = keyEvent->pressed ? !g_showMip : g_showMip; break;
		case 'N': g_showBucket = keyEvent->pressed ? !g_showBucket : g_showBucket; break;
		case 'O': g_showOverdraw = keyEvent->pressed ? !g_showOverdraw : g_showOverdraw; break;
		case 'P': g_pixelMode = keyEvent->pressed ? ((g_pixelMode+1)%6) : g_pixelMode; break;
		case 'Q': if ( keyEvent->pressed ) { g_traceGrid = true; } break;
		case 'R': if ( keyEvent->pressed ) { g_sample = 0; } break;
		case 'S': g_keyDownPressed = keyEvent->pressed; break;
		case 'U': if ( keyEvent->pressed ) { s_activeScene->info.dirty = false; }; break;
		case 'W': g_keyUpPressed = keyEvent->pressed; break;
		case 'X': g_randomBackground = keyEvent->pressed ? !g_randomBackground : g_randomBackground; break;
		case 'Z': if ( keyEvent->pressed ) { g_reloadShaders = true; }; break;
		case ' ':
			if ( keyEvent->pressed ) 
			{
				s_yaw = DegToRad( s_activeScene->info.cameraYaw );
				s_pitch = 0.0f;
			}
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if ( keyEvent->pressed ) 
			{
				const int key = Min( keyEvent->key - '0', s_paths[PATH_CAMERA].keyCount-1 );
				if ( s_paths[PATH_CAMERA].keyCount == 0 )
					s_headOffset = Vec3_Zero();
				else
					s_headOffset = s_paths[PATH_CAMERA].positions[key];
			}
			break;
		case 109:
			if ( keyEvent->pressed ) 
			{
				g_translation_speed *= 0.5f;
				V6_MSG( "Translation speed: %g\n", g_translation_speed );
			}
			break;
		case 107:
			if ( keyEvent->pressed ) 
			{
				g_translation_speed *= 2.0f;
				V6_MSG( "Translation speed: %g\n", g_translation_speed );
			}
			break;
		}
	}
}

static void Viewer_OnMouseEvent( const MouseEvent_s* mouseEvent )
{
	if ( mouseEvent->rightButton == MOUSE_BUTTON_DOWN )
	{
		Win_CaptureMouse( &s_win );
		g_mousePressed = true;
	}
	else if ( mouseEvent->rightButton == MOUSE_BUTTON_UP )
	{
		Win_ReleaseMouse( &s_win );
		g_mousePressed = false;
	}

	if ( g_mousePressed )
	{
		g_mouseDeltaX += mouseEvent->deltaX;
		g_mouseDeltaY += mouseEvent->deltaY;
	}
}

static const bool IsBakingMode( DrawMode_e drawMode )
{
	return drawMode == DRAW_MODE_BLOCK && g_sample < SAMPLE_MAX_COUNT;
}

static const char* ModeToString( DrawMode_e drawMode )
{
	switch ( drawMode )
	{
		case DRAW_MODE_DEFAULT: return "default";
		case DRAW_MODE_BLOCK: 
		{
			if ( g_sample < SAMPLE_MAX_COUNT )
			{
				static char buffer[256];
				sprintf_s( buffer, sizeof( buffer ), "baking sample %d @ frame %d", g_sample, PathPlayer_GetFrame( &s_pathPlayer ) );
				return buffer;
			}
			else
			{
				static char buffer[256];
				sprintf_s( buffer, sizeof( buffer ), "block @ frame %d", PathPlayer_GetFrame( &s_pathPlayer ) );
				return buffer;
			}
		}
	}
	return "unknown";
}

static void RenderingView_MakeForStereo( RenderingView_s* renderingView, const Vec3* org, const Vec3* forward, const Vec3* up, const Vec3* right, const u32 eye, float aspectRatio )
{
	const Vec3 eyeOffset = *right * 0.5f * IPD;
	renderingView->org = *org + (eye == 0 ? -eyeOffset : eyeOffset);
	renderingView->forward = *forward;
	renderingView->right = *right;
	renderingView->up = *up;
	renderingView->view.viewMatrix = Mat4x4_View( &renderingView->org, forward, up, right );
	renderingView->view.projMatrix = Mat4x4_Projection( ZNEAR, ZFAR, FOV, aspectRatio );
	renderingView->tanHalfFOVLeft = Tan( FOV * 0.5f );
	renderingView->tanHalfFOVRight = renderingView->tanHalfFOVLeft;
	renderingView->tanHalfFOVUp = renderingView->tanHalfFOVLeft;
	renderingView->tanHalfFOVDown = renderingView->tanHalfFOVLeft;
}

#if V6_USE_HMD

static void RenderingView_MakeForHMD( RenderingView_s* renderingView, const HmdEyePose_s* eyePose, const Vec3* orgOffset, float yawOffset, u32 eye )
{
	const Mat4x4 yawMatrix = Mat4x4_RotationY( s_yaw );
	Mat4x4_Mul( &renderingView->viewMatrix, yawMatrix, eyePose->lookAt );
	renderingView->org = *orgOffset + renderingView->viewMatrix.GetTranslation();
	Mat4x4_SetTranslation( &renderingView->viewMatrix, renderingView->org );
	Mat4x4_AffineInverse( &renderingView->viewMatrix );
	renderingView->forward = -renderingView->viewMatrix.GetZAxis()->Normalized();
	renderingView->right = renderingView->viewMatrix.GetXAxis()->Normalized();
	renderingView->up = renderingView->viewMatrix.GetYAxis()->Normalized();
	renderingView->projMatrix = eyePose->projection;
	renderingView->eye = eye;
	renderingView->tanHalfFOVLeft = eyePose->tanHalfFOVLeft;
	renderingView->tanHalfFOVRight = eyePose->tanHalfFOVRight;
	renderingView->tanHalfFOVUp = eyePose->tanHalfFOVUp;
	renderingView->tanHalfFOVDown = eyePose->tanHalfFOVDown;
}

#endif // #if V6_USE_HMD

static void Cube_GetLookAt( Vec3& lookAt, Vec3& up, CubeAxis_e axis )
{
	switch ( axis )
    {
        case CUBE_AXIS_POSITIVE_X:            
			lookAt  = Vec3_Make( 1.0f,  0.0f,  0.0f );
            up		= Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;								 	 	    
        case CUBE_AXIS_NEGATIVE_X:				 	 	    
            lookAt	= Vec3_Make( -1.0f , 0.0f, 0.0f );
            up		= Vec3_Make(  0.0f , 1.0f, 0.0f );
            break;								 	 	    
        case CUBE_AXIS_POSITIVE_Y:				 	 	    
            lookAt	= Vec3_Make( 0.0f,  1.0f,  0.0f );
            up		= Vec3_Make( 0.0f,  0.0f, -1.0f );
            break;						 		 	 	    
        case CUBE_AXIS_NEGATIVE_Y:		 		 	 	    
            lookAt	= Vec3_Make( 0.0f, -1.0f,  0.0f );
            up		= Vec3_Make( 0.0f,  0.0f,  1.0f );
            break;						 		 	 	    
        case CUBE_AXIS_POSITIVE_Z:		 		 	 	    
            lookAt	= Vec3_Make( 0.0f,  0.0f,  1.0f );
            up		= Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;						 		 	 	    
        case CUBE_AXIS_NEGATIVE_Z:		 		 	 	    
            lookAt	= Vec3_Make( 0.0f,  0.0f, -1.0f );
            up		= Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;
		default:
			V6_ASSERT_NOT_SUPPORTED();
    }
}

static void Cube_MakeViewMatrix( Mat4x4* matrix, const Vec3& center, CubeAxis_e axis )
{
	Vec3 lookAt;
	Vec3 up;
	Cube_GetLookAt( lookAt, up, axis );
	
	*matrix = Mat4x4_Rotation( lookAt, up );
	Mat4x4_SetTranslation( matrix, center );
	Mat4x4_AffineInverse( matrix );
}

static void CubeFace_Create( CubeFaceContext_s* cubeFaceContext, u32 gridMacroShift )
{
	const u32 gridWidth = 1 << (gridMacroShift + 2);
	const u32 renderTargetSize = gridWidth * HLSL_CELL_SUPER_SAMPLING_WIDTH;

	GPUColorRenderTarget_Create( &cubeFaceContext->color, renderTargetSize, renderTargetSize, 1, true, false, "cubeColor" );
	GPUDepthRenderTarget_Create( &cubeFaceContext->depth, renderTargetSize, renderTargetSize, 1, true, "cubeDepth" );
}

static void CubeFace_Release( CubeFaceContext_s* cubeFaceContext )
{
	GPUColorRenderTarget_Release( &cubeFaceContext->color );
	GPUDepthRenderTarget_Release( &cubeFaceContext->depth );
}

static void GPUContext_CreateShaders( CFileSystem* fileSystem, IStack* stack )
{
	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	static_assert( CONSTANT_BUFFER_COUNT <= GPUShaderContext_s::CONSTANT_BUFFER_MAX_COUNT, "Out of constant buffer" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC], sizeof( v6::hlsl::CBBasic ), "basic" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_GENERIC], sizeof( v6::hlsl::CBGeneric ), "generic" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_CULL], sizeof( v6::hlsl::CBCull ), "cull" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_BLOCK], sizeof( v6::hlsl::CBBlock ), "blockContext" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_PIXEL], sizeof( v6::hlsl::CBPixel), "pixel" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE], sizeof( v6::hlsl::CBCompose), "compose" );

	static_assert( COMPUTE_COUNT <= GPUShaderContext_s::COMPUTE_MAX_COUNT, "Out of compute" );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL4], "block_cull_x4_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL8], "block_cull_x8_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL16], "block_cull_x16_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL32], "block_cull_x32_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL64], "block_cull_x64_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL_STATS4], "block_cull_stats_x4_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL_STATS8], "block_cull_stats_x8_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL_STATS16], "block_cull_stats_x16_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL_STATS32], "block_cull_stats_x32_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_CULL_STATS64], "block_cull_stats_x64_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE_INIT], "block_trace_init_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE4], "block_trace_x4_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE8], "block_trace_x8_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE16], "block_trace_x16_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE32], "block_trace_x32_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE64], "block_trace_x64_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE_DEBUG4], "block_trace_debug_x4_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE_DEBUG8], "block_trace_debug_x8_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE_DEBUG16], "block_trace_debug_x16_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE_DEBUG32], "block_trace_debug_x32_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLOCK_TRACE_DEBUG64], "block_trace_debug_x64_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLENDPIXEL], "pixel_blend_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_BLENDPIXEL_OVERDRAW], "pixel_blend_overdraw_cs.cso", fileSystem, stack );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_COMPOSESURFACE], "surface_compose_cs.cso", fileSystem, stack );

	static_assert( SHADER_COUNT <= GPUShaderContext_s::SHADER_MAX_COUNT, "Out of shader" );
	GPUShader_Create( &shaderContext->shaders[SHADER_BASIC], "basic_vs.cso", "basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, fileSystem, stack );
	GPUShader_Create( &shaderContext->shaders[SHADER_FAKE_CUBE], "fake_cube_vs.cso", "fake_cube_ps.cso", 0, fileSystem, stack );
	GPUShader_Create( &shaderContext->shaders[SHADER_GENERIC], "generic_vs.cso", "generic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, fileSystem, stack );
	GPUShader_Create( &shaderContext->shaders[SHADER_GENERIC_ALPHA_TEST], "generic_vs.cso", "generic_alpha_test_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, fileSystem, stack );
}

static void GPUContext_Create( u32 width, u32 height, HWND hWnd, CFileSystem* fileSystem, IAllocator* heap, IStack* stack )
{
	bool debug = false;
#if V6_D3D_DEBUG == 1
	debug = true;
#endif
	GPUDevice_CreateWithSurfaceContext( width * HLSL_EYE_COUNT, height, hWnd, debug );
	GPURenderTargetContext_Create( width, height, true, HLSL_STEREO );
	GPUShaderContext_Create();
	GPUQueryContext_Create();

	GPUContext_CreateShaders( fileSystem, stack );

	GPUQueryContext_s* queryContext = GPUQueryContext_Get();

	for ( u32 bufferID = 0; bufferID < 2; ++bufferID )
	{
		for ( u32 queryID = 0; queryID < QUERY_COUNT; ++queryID )
		{
			if ( queryID == QUERY_FREQUENCY )
				GPUQuery_CreateTimeStampDisjoint( &queryContext->queries[bufferID][queryID] );
			else
				GPUQuery_CreateTimeStamp( &queryContext->queries[bufferID][queryID] );
		}
	}
}

static void SequenceContext_UpdateFrameData( SequenceContext_s* sequenceContext, u32 groupCounts[CODEC_BUCKET_COUNT], u32 frameID, const Sequence_s* sequence, IStack* stack )
{
	ScopedStack scopedStack( stack );

	Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
	float gridScale = sequence->desc.gridScaleMin;
	u32 gridMacroHalfWidth = 1 << (sequence->desc.gridMacroShift-1);
	for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
		macroGridCoords[mip] = Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameID].origin, gridScale, gridMacroHalfWidth ); // patched per frame

	const u16* rangeIDs = sequence->frameDataArray[frameID].rangeIDs;
	hlsl::BlockRange* const blockRangeBuffer = stack->newArray< hlsl::BlockRange >( CODEC_BUCKET_COUNT * CODEC_RANGE_MAX_COUNT );
	hlsl::BlockRange* blockRanges = blockRangeBuffer;
	u32 blockGroupBuffer[SequenceContext_s::GROUP_MAX_COUNT];
	u32* blockGroups = blockGroupBuffer;
	u32 framePosCount = 0;
	u32 frameDataCount = 0;
	u32 frameBlockRangeCount = 0;
	u32 frameGroupCount = 0;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);
		const u32 bucketBlockCount = sequence->frameDescArray[frameID].blockCounts[bucket];
		const u32 bucketBlockRangeCount = sequence->frameDescArray[frameID].blockRangeCounts[bucket];
		u32 firstThreadID = 0;
		u32 bucketGroupCount = 0;

		for ( u32 rangeRank = 0; rangeRank < bucketBlockRangeCount; ++rangeRank )
		{
			const u32 rangeID = rangeIDs[rangeRank];

			const CodecRange_s* codecRange = &sequenceContext->rangeDefs[bucket][rangeID];
			const u32 rangeFrameID = codecRange->frameID8_mip4_blockCount20 >> 24;
			const u32 rangeMip = (codecRange->frameID8_mip4_blockCount20 >> 20) & 0xF;
			const hlsl::BlockRange* srcBlockRange = &sequenceContext->blockRanges[bucket][rangeID];
			
			hlsl::BlockRange* dstBlockRange = &blockRanges[rangeRank];
			memcpy( dstBlockRange, srcBlockRange, sizeof( hlsl::BlockRange ) );
			dstBlockRange->macroGridOffset -= macroGridCoords[rangeMip];
			dstBlockRange->frameDistance = frameID - rangeFrameID;
			dstBlockRange->firstThreadID = firstThreadID;

			const u32 blockCount = srcBlockRange->blockCount;
			const u32 groupCount = GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );

			for ( u32 groupRank = 0; groupRank < groupCount; ++groupRank )
				blockGroups[bucketGroupCount + groupRank] = rangeRank;

			firstThreadID += groupCount * HLSL_BLOCK_THREAD_GROUP_SIZE;
			bucketGroupCount += groupCount;
		}

		groupCounts[bucket] = bucketGroupCount;
		rangeIDs += bucketBlockRangeCount;
		blockRanges += bucketBlockRangeCount;
		blockGroups += bucketGroupCount;
		framePosCount += bucketBlockCount;
		frameDataCount += bucketBlockCount * cellPerBucketCount;
		frameBlockRangeCount += bucketBlockRangeCount;
		frameGroupCount += bucketGroupCount;
	}

	u32 bufferID = frameID & 1;
	GPUBuffer_Update( &sequenceContext->ranges[bufferID], 0, blockRangeBuffer, frameBlockRangeCount );
	GPUBuffer_Update( &sequenceContext->groups[bufferID], 0, blockGroupBuffer, frameGroupCount );
	GPUBuffer_Update( &sequenceContext->blockPos, sequenceContext->frameBlockPosOffsets[frameID], sequence->frameDataArray[frameID].blockPos, framePosCount );
	GPUBuffer_Update( &sequenceContext->blockData, sequenceContext->frameBlockDataOffsets[frameID], sequence->frameDataArray[frameID].blockData, frameDataCount );
}

static void SequenceContext_CreateFromData( SequenceContext_s* sequenceContext, const Sequence_s* sequence )
{
	{
		u32 rangeDefOffset = 0;
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			sequenceContext->rangeDefs[bucket] = sequence->data.rangeDefs + rangeDefOffset;
			rangeDefOffset += sequence->desc.rangeDefCounts[bucket];
		}
	}

	u32 nextRangeIDs[CODEC_BUCKET_COUNT] = {};
	u32 blockPosCount = 0;
	u32 blockDataCount = 0;
	for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
	{
		if ( sequence->frameDescArray[frameID].flags & CODEC_FRAME_FLAG_MOTION )
			continue;

		sequenceContext->frameBlockPosOffsets[frameID] = blockPosCount;
		sequenceContext->frameBlockDataOffsets[frameID] = blockDataCount;

		Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
		float gridScale = sequence->desc.gridScaleMin;
		u32 gridMacroHalfWidth = 1 << (sequence->desc.gridMacroShift-1);
		for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
			macroGridCoords[mip] = Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameID].origin, gridScale, gridMacroHalfWidth ); // patched per frame
		
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; )
		{
			const u32 rangeID = nextRangeIDs[bucket];

			if ( rangeID == sequence->desc.rangeDefCounts[bucket] )
			{
				++bucket;
				continue;
			}
			
			const CodecRange_s* codecRange = &sequenceContext->rangeDefs[bucket][rangeID];
			u32 rangeFrameID = codecRange->frameID8_mip4_blockCount20 >> 24;
			if ( frameID != rangeFrameID )
			{
				++bucket;
				continue;
			}

			const u32 blockCount = codecRange->frameID8_mip4_blockCount20 & 0xFFFFF;
			const u32 mip = (codecRange->frameID8_mip4_blockCount20 >> 20) & 0xF;

			hlsl::BlockRange* blockRange = &sequenceContext->blockRanges[bucket][rangeID];
			
			blockRange->macroGridOffset = macroGridCoords[mip]; // patched per frame
			blockRange->firstThreadID = 0; // patched per frame
			blockRange->blockCount = blockCount;
			blockRange->blockPosOffset = blockPosCount;
			blockRange->blockDataOffset = blockDataCount;

			const u32 cellPerBucketCount = 1 << (2 + bucket);
			blockPosCount += blockCount;
			blockDataCount += blockCount * cellPerBucketCount;

			++nextRangeIDs[bucket];
		}
	}

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		V6_ASSERT( nextRangeIDs[bucket] == sequence->desc.rangeDefCounts[bucket] );

	u32 maxBlockRangeCount = 0;
	u32 maxBlockCount = 0;
	u32 maxBlockGroupCount = 0;

	for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
	{
		if ( sequence->frameDescArray[frameID].flags & CODEC_FRAME_FLAG_MOTION )
			continue;

		const u16* rangeIDs = sequence->frameDataArray[frameID].rangeIDs;

		u32 frameBlockRangeCount = 0;
		u32 frameBlockCount = 0;
		u32 frameBlockGroupCount = 0;
		
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			u32 bucketBlockRangeCount = sequence->frameDescArray[frameID].blockRangeCounts[bucket];

			for ( u32 rangeRank = 0; rangeRank < bucketBlockRangeCount; ++rangeRank )
			{
				const u32 rangeID = rangeIDs[rangeRank];
				const u32 blockCount = sequenceContext->rangeDefs[bucket][rangeID].frameID8_mip4_blockCount20 & 0xFFFFF;
				frameBlockCount += blockCount;
				frameBlockGroupCount += GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
			}

			rangeIDs += bucketBlockRangeCount;
			frameBlockRangeCount += bucketBlockRangeCount;
		}
		maxBlockRangeCount = Max( maxBlockRangeCount, frameBlockRangeCount );
		maxBlockCount = Max( maxBlockCount, frameBlockCount );
		maxBlockGroupCount = Max( maxBlockGroupCount, frameBlockGroupCount );
	}

	sequenceContext->blockMaxCount = maxBlockCount;

	blockPosCount = Max( 1u, blockPosCount );
	blockDataCount = Max( 1u, blockDataCount );
	maxBlockRangeCount = Max( 1u, maxBlockRangeCount );
	maxBlockGroupCount = Max( 1u, maxBlockGroupCount );

	V6_ASSERT( maxBlockGroupCount <= SequenceContext_s::GROUP_MAX_COUNT );

	GPUBuffer_CreateTyped( &sequenceContext->blockPos, DXGI_FORMAT_R32_UINT, blockPosCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockPositions" );
	GPUBuffer_CreateTyped( &sequenceContext->blockData, DXGI_FORMAT_R32_UINT, blockDataCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockData" );
	GPUBuffer_CreateStructured( &sequenceContext->ranges[0], sizeof( hlsl::BlockRange ), maxBlockRangeCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockRanges0" );
	GPUBuffer_CreateStructured( &sequenceContext->ranges[1], sizeof( hlsl::BlockRange ), maxBlockRangeCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockRanges1" );
	GPUBuffer_CreateTyped( &sequenceContext->groups[0], DXGI_FORMAT_R32_UINT, maxBlockGroupCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockGroups0" );
	GPUBuffer_CreateTyped( &sequenceContext->groups[1], DXGI_FORMAT_R32_UINT, maxBlockGroupCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockGroups1" );
}

static void SequenceContext_Release( SequenceContext_s* sequenceContext )
{
	GPUBuffer_Release( &sequenceContext->blockPos );
	GPUBuffer_Release( &sequenceContext->blockData );
	GPUBuffer_Release( &sequenceContext->ranges[0] );
	GPUBuffer_Release( &sequenceContext->ranges[1] );
	GPUBuffer_Release( &sequenceContext->groups[0] );
	GPUBuffer_Release( &sequenceContext->groups[1] );
}

static void TraceContext_Create( TraceContext_s* traceContext, u32 screenWidth, u32 screenHeight, u32 passedBlockCount, u32 cellItemCount )
{
	V6_ASSERT( screenWidth > 0 );
	V6_ASSERT( screenHeight > 0 );
	passedBlockCount = Max( 1u, passedBlockCount );
	cellItemCount = Max( 1u, cellItemCount );

	GPUBuffer_CreateTyped( &traceContext->traceCell, DXGI_FORMAT_R32_UINT, passedBlockCount * 2, 0, "traceCell" );
	traceContext->passedBlockCount = passedBlockCount;
	GPUBuffer_CreateIndirectArgs( &traceContext->traceIndirectArgs, trace_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "traceIndirectArgs" );

	GPUBuffer_CreateStructured( &traceContext->cellItems, sizeof( hlsl::BlockCellItem ), cellItemCount, 0, "blockCellItems" );
	traceContext->cellItemCount = cellItemCount;
	GPUBuffer_CreateTyped( &traceContext->cellItemCounters, DXGI_FORMAT_R32_UINT, screenWidth * HLSL_EYE_COUNT * screenHeight, 0, "blockCellItemCounters" );	
	
	GPUBuffer_CreateStructured( &traceContext->cullStats, sizeof( hlsl::BlockCullStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockCullStats" );
	GPUBuffer_CreateStructured( &traceContext->traceStats, sizeof( hlsl::BlockTraceStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockTraceStats" );

	GPUTexture2D_CreateRW( &traceContext->colors, screenWidth, screenHeight, "pixelColors" );
}

static void TraceContext_Release( TraceContext_s* traceContext )
{
	GPUBuffer_Release( &traceContext->traceCell );
	GPUBuffer_Release( &traceContext->traceIndirectArgs );

	GPUBuffer_Release( &traceContext->cellItems );
	GPUBuffer_Release( &traceContext->cellItemCounters );

	GPUBuffer_Release( &traceContext->cullStats );
	GPUBuffer_Release( &traceContext->traceStats );
	
	GPUTexture2D_Release( &traceContext->colors );
}

static void Mesh_CreateVirtualQuad( GPUMesh_s* mesh )
{
	GPUMesh_Create( mesh, nullptr, 4, 0, 0, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
}

static void Mesh_CreateVirtualBox( GPUMesh_s* mesh )
{
	const u16 indices[36] = { 
		0, 2, 3,
		0, 3, 1,
		1, 3, 7, 
		1, 7, 5, 
		5, 7, 6,
		5, 6, 4,
		4, 6, 2,
		4, 2, 0, 
		2, 6, 7, 
		2, 7, 3,
		1, 5, 4,
		1, 4, 0 };

	GPUMesh_Create( mesh, nullptr, 0, 0, 0, indices, 36, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );	
}

static void Mesh_CreateFakeCube( GPUMesh_s* mesh )
{
#if 0
	const u16 indices[36] = { 
			0, 2, 3,
			0, 3, 1,
			1, 3, 7, 
			1, 7, 5, 
			5, 7, 6,
			5, 6, 4,
			4, 6, 2,
			4, 2, 0, 
			2, 6, 7, 
			2, 7, 3,
			1, 5, 4,
			1, 4, 0 };

	GPUMesh_Create( mesh, nullptr, 0, 0, 0, indices, 36, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
#else
	const u16 indices[5] = { 0, 2, 3, 1, 0 };

	GPUMesh_Create( mesh, nullptr, 0, 0, 0, indices, 5, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP );
#endif
}

static void Material_DrawBasic( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags )
{
#if V6_SIMPLE_SCENE == 0
	if ( (flags & RENDER_FLAGS_IS_CAPTURING) != 0 )
		return;
#endif // #if V6_SIMPLE_SCENE == 0

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	v6::hlsl::CBBasic* cbBasic = (v6::hlsl::CBBasic*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC] );

	Mat4x4 worlMatrix;
	Mat4x4_Scale( &worlMatrix, entity->scale );
	Mat4x4_SetTranslation( &worlMatrix, entity->pos );

	// use this order because one matrix is "from" local space and the other is "to" local space
	Mat4x4 objectToViewMatrix;
	Mat4x4_Mul( &objectToViewMatrix, view->viewMatrix, worlMatrix );
	
	cbBasic->c_basicObjectToView = objectToViewMatrix;
	cbBasic->c_basicViewToProj = view->projMatrix;
	Mat4x4_Mul( &cbBasic->c_basicObjectToProj, objectToViewMatrix, view->projMatrix );

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC] );

	g_deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_BASIC, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC].buf );
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &shaderContext->shaders[SHADER_BASIC];
	GPUMesh_Draw( mesh, 1, shader );
}

static void Material_DrawFakeCube( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags )
{
	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	v6::hlsl::CBBasic* cbBasic = (v6::hlsl::CBBasic*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC] );

	Mat4x4 worlMatrix;
	Mat4x4_Scale( &worlMatrix, entity->scale );
	Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
	// use this order because one matrix is "from" local space and the other is "to" local space
	Mat4x4_Mul( &cbBasic->c_basicObjectToView, view->viewMatrix, worlMatrix );
	cbBasic->c_basicViewToProj = view->projMatrix;

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC] );

	g_deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_BASIC, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC].buf );
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &shaderContext->shaders[SHADER_FAKE_CUBE];
	GPUMesh_Draw( mesh, 1, shader );
}

static void Material_DrawGeneric( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags )
{
	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	v6::hlsl::CBGeneric* cbGeneric = (v6::hlsl::CBGeneric*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_GENERIC] );

	Mat4x4 worlMatrix;
	Mat4x4_Scale( &worlMatrix, entity->scale );
	Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
	// use this order because one matrix is "from" local space and the other is "to" local space
	const bool useAlbedo = material->textureIDs[TEXTURE_GENERIC_DIFFUSE] != Material_s::TEXTURE_INVALID;
	const bool useAlpha = material->textureIDs[TEXTURE_GENERIC_ALPHA] != Material_s::TEXTURE_INVALID;
	cbGeneric->c_genericObjectToWorld = worlMatrix;
	cbGeneric->c_genericWorldToView = view->viewMatrix;
	cbGeneric->c_genericViewToProj = view->projMatrix;
	if ( g_showObjects )
	{
		Color_s color;
		color.bits = HashPointer( entity );
		cbGeneric->c_genericDiffuse = Vec3_Make( color.r / 255.0f, color.g / 255.0f, color.b / 255.0f );
		cbGeneric->c_genericUseAlbedo = false;
	}
	else
	{
		cbGeneric->c_genericDiffuse = useAlbedo ? Vec3_Make( 1.0f, 1.0f, 1.0f ) : material->diffuse;
		cbGeneric->c_genericUseAlbedo = useAlbedo;
	}
	cbGeneric->c_genericUseAlpha = useAlpha;

	GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_GENERIC] );

	g_deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_GENERIC, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_GENERIC].buf );
	g_deviceContext->PSSetConstantBuffers( CONSTANT_BUFFER_GENERIC, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_GENERIC].buf );
	
	g_deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &shaderContext->samplerState );

	if ( material->textureIDs[TEXTURE_GENERIC_DIFFUSE] != Material_s::TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[TEXTURE_GENERIC_DIFFUSE]];
		if ( texture->mipmapState == GPUTEXTURE_MIPMAP_STATE_REQUIRED )
		{
			g_deviceContext->GenerateMips( texture->srv );
			texture->mipmapState = GPUTEXTURE_MIPMAP_STATE_GENERATED;
		}
		g_deviceContext->PSSetShaderResources( HLSL_GENERIC_ALBEDO_SLOT, 1, &texture->srv );
	}

	if ( material->textureIDs[TEXTURE_GENERIC_ALPHA] != Material_s::TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[TEXTURE_GENERIC_ALPHA]];
		if ( texture->mipmapState == GPUTEXTURE_MIPMAP_STATE_REQUIRED )
		{
			g_deviceContext->GenerateMips( texture->srv );
			texture->mipmapState = GPUTEXTURE_MIPMAP_STATE_GENERATED;
		}
		g_deviceContext->PSSetShaderResources( HLSL_GENERIC_ALPHA_SLOT, 1, &texture->srv );
	}

	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	const bool useAlphaCoverage = (flags & RENDER_FLAGS_USE_ALPHA_COVERAGE) != 0;
	GPUShader_s* shader = (useAlpha && !useAlphaCoverage) ? &shaderContext->shaders[SHADER_GENERIC_ALPHA_TEST] : &shaderContext->shaders[SHADER_GENERIC];
	GPUMesh_Draw( mesh, 1, shader );

	static const void* nulls[8] = {};
	if ( material->textureIDs[TEXTURE_GENERIC_DIFFUSE] )
		g_deviceContext->PSSetShaderResources( HLSL_GENERIC_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	if ( material->textureIDs[TEXTURE_GENERIC_ALPHA] )
		g_deviceContext->PSSetShaderResources( HLSL_GENERIC_ALPHA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
}

static void SceneViewer_SetFilename( SceneViewer_s* scene, const char* filename )
{
	strcpy_s( scene->filename, sizeof( scene->filename ), filename );
}

static void SceneViewer_SetInfo( SceneViewer_s* scene, const SceneInfo_s* sceneInfo )
{
	memcpy( &scene->info, sceneInfo, sizeof( scene->info ) );
}

static void SceneViewer_SaveInfo( SceneViewer_s* scene )
{
	if ( !scene->filename[0] )
		return;

	V6_ASSERT( !FilePath_HasExtension( scene->filename, "info" ) );
	char fileinfo[256];
	FilePath_ChangeExtension( fileinfo, sizeof( fileinfo ), scene->filename, "info" );

	SceneInfo_WriteToFile( &scene->info, fileinfo );
}

static void SceneViewer_MakeSequenceFilename( const SceneViewer_s* scene, char* path, u32 maxPathSize )
{
	V6_ASSERT( scene->filename[0] );

	FilePath_ChangeExtension( path, maxPathSize, scene->filename, "v6s" );
}

static void SceneViewer_MakeRawFrameFileTemplate( const SceneViewer_s* scene, char* path, u32 maxPathSize )
{
	V6_ASSERT( scene->filename[0] );

	char filepath[256];
	FilePath_ExtractPath( filepath, sizeof( filepath ), scene->filename );

	char filename[256];
	FilePath_ExtractFilename( filename, sizeof( filename ), scene->filename );

	char filenameWithoutExtension[256];
	FilePath_TrimExtension( filenameWithoutExtension, sizeof( filenameWithoutExtension ), filename );

	FilePath_Make( path, maxPathSize, filepath, filenameWithoutExtension );
	sprintf_s( path, maxPathSize, "%s_%s.v6f", path, "%06d" );
}

static void SceneViewer_MakeRawFrameFilename( const SceneViewer_s* scene, char* path, u32 maxPathSize, u32 frame  )
{
	V6_ASSERT( scene->filename[0] );

	char filepath[256];
	FilePath_ExtractPath( filepath, sizeof( filepath ), scene->filename );

	char filename[256];
	FilePath_ExtractFilename( filename, sizeof( filename ), scene->filename );

	char filenameWithoutExtension[256];
	FilePath_TrimExtension( filenameWithoutExtension, sizeof( filenameWithoutExtension ), filename );

	FilePath_Make( path, maxPathSize, filepath, filenameWithoutExtension );
	sprintf_s( path, maxPathSize, "%s_%06u.v6f", path, frame );
}

static void SceneContext_Create( SceneContext_s* sceneContext, v6::IStack* stack )
{
	stack->push();

	memset( sceneContext, 0, sizeof( SceneContext_s ) );
	sceneContext->stack = stack;
	Signal_Create( &sceneContext->deviceReady );
	Signal_Create( &sceneContext->loadDone );
}

static void SceneContext_SetFilename( SceneContext_s* sceneContext, const char* filename )
{
	strcpy_s( sceneContext->filename, sizeof( sceneContext->filename ), filename );
}

static void SceneContext_Release( SceneContext_s* sceneContext )
{
	if ( sceneContext->scene )
		Scene_Release( sceneContext->scene );
	Signal_Release( &sceneContext->deviceReady );
	Signal_Release( &sceneContext->loadDone );

	sceneContext->stack->pop();
}

static void SceneContext_SetDeviceReady( SceneContext_s* sceneContext )
{
	Signal_Emit( &sceneContext->deviceReady );
}

static void SceneContext_Load( SceneContext_s* sceneContext )
{
	V6_MSG( "Load scene\n" );

	V6_ASSERT( !FilePath_HasExtension( sceneContext->filename, "info" ) );
	char fileinfo[256];
	FilePath_ChangeExtension( fileinfo, sizeof( fileinfo ), sceneContext->filename, "info" );
	
	SceneInfo_s info;
	if ( !SceneInfo_ReadFromFile( &info, fileinfo ) )
		V6_WARNING( "Unable to read info file\n" );
	
	if ( !Obj_ReadObjectFile( &sceneContext->objScene, sceneContext->filename, sceneContext->stack ) )
	{
		sceneContext->objScene.meshCount = 0;
		V6_ERROR( "Unable to load %s\n", sceneContext->filename );
		Signal_Emit( &sceneContext->loadDone );
		return;
	}
	
	V6_MSG( "%d meshes loaded\n",  sceneContext->objScene.meshCount );
	
	Signal_Wait( &sceneContext->deviceReady );

	V6_MSG( "Init scene\n" );

	ObjScene_s* objScene = &sceneContext->objScene;
	SceneViewer_s* scene = sceneContext->stack->newInstance< SceneViewer_s >();
	Scene_Create( scene );
	SceneViewer_SetFilename( scene, sceneContext->filename );
	SceneViewer_SetInfo( scene, &info );

	for ( u32 materialID = 0; materialID < objScene->materialCount; ++materialID )
	{
		ObjMaterial_s* objMaterial = &objScene->materials[materialID];
		Material_s* material = &scene->materials[materialID];

		Material_Create( material, Material_DrawGeneric );
		++scene->materialCount;

		sceneContext->stack->push();

		material->diffuse = objMaterial->kd;
		
		const char* textureFilenames[TEXTURE_GENERIC_COUNT];
		textureFilenames[TEXTURE_GENERIC_DIFFUSE] = objMaterial->mapKd;
		textureFilenames[TEXTURE_GENERIC_ALPHA] = objMaterial->mapD;
		textureFilenames[TEXTURE_GENERIC_NORMAL] = objMaterial->mapBump;

		for ( u32 textureSlot = 0; textureSlot < TEXTURE_GENERIC_COUNT; ++textureSlot )
		{
			const char* textureFilename = textureFilenames[textureSlot];
			if ( !*textureFilename )
				continue;

			Image_s image = {};
			CFileReader fileReader;
			if ( FilePath_HasExtension( textureFilename, "tga" ) && fileReader.Open( textureFilename ) && Image_ReadTga( &image, &fileReader, sceneContext->stack ) )
			{
				static const char* textureNames[TEXTURE_GENERIC_COUNT] = { "diffuse", "alpha", "bump" };
				const u32 textureID = scene->textureCount;
				GPUTexture2D_Create( &scene->textures[scene->textureCount], image.width, image.height, image.pixels, true, textureNames[textureSlot] );
				++scene->textureCount;

				Material_SetTexture( material, textureID, textureSlot );
			}
			else
				V6_WARNING( "Unable to load %s for material %s\n", textureFilename, objMaterial->name );
		}

		sceneContext->stack->pop();
	}

	float maxDim = 0.0f;

	for ( u32 meshID = 0; meshID < objScene->meshCount; ++meshID )
	{
		V6_ASSERT( meshID < Scene_s::MESH_MAX_COUNT );
		V6_ASSERT( meshID < Scene_s::ENTITY_MAX_COUNT );

		sceneContext->stack->push();

		ObjMesh_s* mesh = &objScene->meshes[meshID];
		
		GenericVertex_s* vertices = sceneContext->stack->newArray< GenericVertex_s >( mesh->triangleCount * 3 );
		
		ObjTriangle_s* triangle = &objScene->triangles[mesh->firstTriangleID];
		
		Vec3 meshCenter = Vec3_Zero();
		for ( u32 triangleID = 0; triangleID < mesh->triangleCount; ++triangleID, ++triangle )
		{
			for ( u32 vertexID = 0; vertexID < 3; ++vertexID )
			{
				const Vec3 worldPos = objScene->positions[triangle->vertices[vertexID].posID] * info.worldUnitToCM;
				meshCenter += worldPos;
				maxDim = Max( maxDim, worldPos.x );
				maxDim = Max( maxDim, worldPos.y );
				maxDim = Max( maxDim, worldPos.z );
			}
		}
		if ( mesh->triangleCount )
			meshCenter *= 1.0f / (mesh->triangleCount * 3);

		triangle = &objScene->triangles[mesh->firstTriangleID];
		GenericVertex_s* vertex = vertices;
		for ( u32 triangleID = 0; triangleID < mesh->triangleCount; ++triangleID, ++triangle, vertex += 3 )
		{
			vertex[0].position = objScene->positions[triangle->vertices[0].posID] * info.worldUnitToCM - meshCenter;
			vertex[1].position = objScene->positions[triangle->vertices[1].posID] * info.worldUnitToCM - meshCenter;
			vertex[2].position = objScene->positions[triangle->vertices[2].posID] * info.worldUnitToCM - meshCenter;

			if ( objScene->normals && triangle->vertices[0].normalID != (u32)-1 )
			{
				vertex[0].normal = objScene->normals[triangle->vertices[0].normalID];
				vertex[1].normal = objScene->normals[triangle->vertices[1].normalID];
				vertex[2].normal = objScene->normals[triangle->vertices[2].normalID];
			}
			else
			{
				const Vec3 edge1 = vertex[1].position - vertex[0].position;
				const Vec3 edge2 = vertex[2].position - vertex[0].position;
				const Vec3 normal = Cross( edge1, edge2 ).Normalized();
				vertex[0].normal = normal;
				vertex[1].normal = normal;
				vertex[2].normal = normal;
			}

			if ( objScene->uvs && triangle->vertices[0].uvID != (u32)-1 )
			{
				vertex[0].uv = objScene->uvs[triangle->vertices[0].uvID];
				vertex[1].uv = objScene->uvs[triangle->vertices[1].uvID];
				vertex[2].uv = objScene->uvs[triangle->vertices[2].uvID];
			}
			else
			{
				vertex[0].uv = Vec2_Make( 0.0f, 0.0f );
				vertex[1].uv = Vec2_Make( 0.0f, 0.0f );
				vertex[2].uv = Vec2_Make( 0.0f, 0.0f );
			}
		}	

		GPUMesh_Create( &scene->meshes[meshID], vertices, mesh->triangleCount * 3, sizeof( GenericVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		++scene->meshCount;
		
		sceneContext->stack->pop();

		Entity_Create( &scene->entities[meshID], mesh->materialID, meshID, meshCenter, 1.0f );
		scene->entities[meshID].name = sceneContext->stack->copyString( mesh->name );
		++scene->entityCount;
	}

	const u32 materialID = scene->materialCount;
	Material_Create( &scene->materials[materialID], Material_DrawBasic );
	++scene->materialCount;

	for ( u32 boxID = 0; boxID < 3; ++boxID )
	{
		u32 meshID = scene->meshCount;
	
		V6_ASSERT( meshID < Scene_s::MESH_MAX_COUNT );
		V6_ASSERT( meshID < Scene_s::ENTITY_MAX_COUNT );

		switch ( boxID )
		{
		case 0:
			{
				GPUMesh_CreateBox( &scene->meshes[meshID], Color_Make( 127, 127, 127, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, Vec3_Make( 0.0f, 0.0f, 0.0f), maxDim );
				++scene->entityCount;
			}
			break;
		case 1:
			{
				GPUMesh_CreateBox( &scene->meshes[meshID], Color_Make( 255, 0, 0, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MIN_SCALE );
				++scene->entityCount;
			}
			break;
		case 2:
			{
				GPUMesh_CreateBox( &scene->meshes[meshID], Color_Make( 0, 0, 255, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MAX_SCALE );
				++scene->entityCount;
			}
			break;
		}
	}

	V6_MSG( "%d entities created\n", scene->entityCount );

	GPUResource_LogMemoryUsage();

	sceneContext->scene = scene;
	s_activeScene = scene;

	Path_Load( s_paths, PATH_COUNT, scene );
	s_yaw = DegToRad( info.cameraYaw );
	if ( s_paths[PATH_CAMERA].keyCount )
		s_headOffset = s_paths[PATH_CAMERA].positions[0];
	for ( u32 pathID = 0; pathID < PATH_COUNT; ++pathID )
	{
		if ( s_paths[pathID].keyCount && s_paths[pathID].entityID != (u32)-1 )
			scene->entities[s_paths[pathID].entityID].pos = s_paths[pathID].positions[0];
	}

	Signal_Emit( &sceneContext->loadDone );
}

void Scene_CreateDebug( SceneDebug_s* scene )
{
	Scene_Create( scene );

	scene->meshLineID = Scene_GetNewMeshID( scene );
	GPUMesh_CreateLine( &scene->meshes[scene->meshLineID], Color_Make( 255, 255, 255, 255 ) );
	
	scene->meshGridID = Scene_GetNewMeshID( scene );
	GPUMesh_CreateBox( &scene->meshes[scene->meshGridID], Color_Make( 255, 255, 255, 255 ), true );

	const u32 basicMaterialID = Scene_GetNewMaterialID( scene );
	Material_Create( &scene->materials[basicMaterialID], Material_DrawBasic );
	
	u32 cellID = 0;
	for ( int z = 0; z < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++z )
	{
		for ( int y = 0; y < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++y )
		{
			for ( int x = 0; x < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++x, ++cellID )
			{						
				for ( u32 cellType = 0; cellType < 2; ++cellType )
				{
					const u32 meshID = Scene_GetNewMeshID( scene );
					scene->meshCellIDs[cellID][cellType] = meshID;
					GPUMesh_CreateBox( &scene->meshes[meshID], Color_Make( 0, (cellType == 0) * 255, (cellType == 1) * 255, 255 ), cellType == 0 );

					const u32 entityID = Scene_GetNewEntityID( scene );
					scene->entityCellIDs[cellID][cellType] = entityID;
					Entity_Create( &scene->entities[entityID], basicMaterialID, meshID, Vec3_Zero(), 1.0f );
					scene->entities[entityID].visible = false;
				}
			}
		}
	}

	scene->entityGridID = Scene_GetNewEntityID( scene );
	Entity_Create( &scene->entities[scene->entityGridID], basicMaterialID, scene->meshGridID, Vec3_Zero(), 1.0f );
	scene->entities[scene->entityGridID].visible = false;

	scene->entityLineID = Scene_GetNewEntityID( scene );
	Entity_Create( &scene->entities[scene->entityLineID], basicMaterialID, scene->meshLineID, Vec3_Zero(), 1.0f );
	scene->entities[scene->entityLineID].visible = false;

	for ( int debugBlockID = 0; debugBlockID < DEBUG_BLOCK_MAX_COUNT; ++debugBlockID )
	{
		for ( u32 cellType = 0; cellType < 2; ++cellType )
		{
			u32 hashColor = 1 + (debugBlockID%7);
			const u32 meshID = Scene_GetNewMeshID( scene );
			scene->meshBlockIDs[debugBlockID][cellType] = meshID;
			GPUMesh_CreateBox( &scene->meshes[meshID], Color_Make( (hashColor & 1) ? 255 : 0, (hashColor & 2) ? 255 : 0, (hashColor & 4) ? 255 : 0, 255 ), cellType == 0 );

			const u32 entityID = Scene_GetNewEntityID( scene );
			scene->entityBlockIDs[debugBlockID][cellType] = entityID;
			Entity_Create( &scene->entities[entityID], basicMaterialID, meshID, Vec3_Zero(), 1.0f );
			scene->entities[entityID].visible = false;
		}

		{
			const u32 meshID = Scene_GetNewMeshID( scene );
			GPUMesh_CreateLine( &scene->meshes[meshID], Color_Make( 255, 255, 255, 255 ) );

			const u32 entityID = Scene_GetNewEntityID( scene );
			scene->entityTraceIDs[debugBlockID] = entityID;
			Entity_Create( &scene->entities[entityID], basicMaterialID, meshID, Vec3_Zero(), 1.0f );
			scene->entities[entityID].visible = false;
		}
	}
}

void Scene_CreatePathGeo( ScenePathGeo_s* scene )
{
	Scene_Create( scene );

	for ( u32 lineRank = 0; lineRank < Path_s::MAX_POINT_COUNT-1; ++lineRank )
	{
		scene->meshLineIDs[lineRank] = scene->meshCount;
		GPUMesh_CreateLine( &scene->meshes[scene->meshCount++], Color_Make( 128, 128, 128, 255 ) );
	}

	scene->meshBoxID = scene->meshCount;
	GPUMesh_CreateBox( &scene->meshes[scene->meshCount++], Color_Make( 255, 255, 255, 255 ), true );

	scene->meshSelectedBoxID = scene->meshCount;
	GPUMesh_CreateBox( &scene->meshes[scene->meshCount++], Color_Make( 255, 0, 0, 255 ), true );

	Material_Create( &scene->materials[scene->materialCount++], Material_DrawBasic );
}

void Scene_UpdatePathGeo( ScenePathGeo_s* scene, const Path_s* path )
{
	if ( !path->dirty )
		return;

	scene->entityCount = 0;
	
	for ( u32 key = 0; key < (u32)path->keyCount; ++key )
	{
		const bool isSelected = key == path->activeKey;
		Entity_Create( &scene->entities[scene->entityCount++], 0, isSelected ? scene->meshSelectedBoxID : scene->meshBoxID, path->positions[key], 5.0f );
	}

	int lineRank;
	for ( lineRank = 0; lineRank < path->keyCount-1; ++lineRank )
	{		
		const u32 meshLineID = scene->meshLineIDs[lineRank];
		const BasicVertex_s vertices[2] = 
		{
			{ path->positions[lineRank], Color_Make( 128, 128, 128, 255 ) },
			{ path->positions[lineRank+1], Color_Make( 128, 128, 128, 255 ) },
		};
		GPUMesh_UpdateVertices( &scene->meshes[meshLineID], vertices );
		Entity_Create( &scene->entities[scene->entityCount++], 0, meshLineID, Vec3_Zero(), 1.0f );
	}
}

#if V6_SIMPLE_SCENE == 1

void Scene_CreateDefault( Scene_s* scene )
{
	const char* filename = "D:/media/obj/default/default.obj";

	V6_ASSERT( !FilePath_HasExtension( filename, "info" ) );
	char fileinfo[256];
	FilePath_ChangeExtension( fileinfo, sizeof( fileinfo ), filename, "info" );

	SceneInfo_s info;
	if ( !SceneInfo_ReadFromFile( &info, fileinfo ) )
		V6_WARNING( "Unable to read info file\n" );

	Scene_Create( scene );
	SceneViewer_SetFilename( scene, filename );
	SceneViewer_SetInfo( scene, &info );

	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_RED], Color_Make( 255, 0, 0, 255 ), false );
	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_BLUE], Color_Make( 0, 0, 255, 255 ), false );

	Material_Create( &scene->materials[MATERIAL_DEFAULT_BASIC], Material_DrawBasic );
	
	const u32 screenWidth = HLSL_GRID_WIDTH >> 1;
	//const float depth = -99.0001f;
	const float depth = -100.0001f;
	const float pixelRadius = 0.5f * (200.0f / screenWidth);
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_RED, Vec3_Make( 0, 0, depth ), pixelRadius * 8 );
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_BLUE, Vec3_Make( -pixelRadius * 32, 0, 0.5f * depth ), pixelRadius * 16 );

	CameraPath_Load( &s_cameraPath, &info );
}

#else

void Scene_CreateDefault( Scene_s* scene )
{
	Scene_Create( scene );

	GPUMesh_CreateTriangle( &scene->meshes[MESH_TRIANGLE] );	
	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_WIREFRAME], Color_Make( 255, 255, 255, 255 ), true );
	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_RED], Color_Make( 255, 0, 0, 255 ), false );
	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_GREEN], Color_Make( 0, 255, 0, 255 ), false );
	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_BLUE], Color_Make( 0, 0, 255, 255 ), false );
	Mesh_CreateVirtualQuad( &scene->meshes[MESH_VIRTUAL_QUAD] );
	Mesh_CreateFakeCube( &scene->meshes[MESH_FAKE_CUBE] );
	GPUMesh_CreatePoint( &scene->meshes[MESH_POINT] );
	GPUMesh_CreateLine( &scene->meshes[MESH_LINE],  Color_Make( 255, 255, 255, 255 ) );
	Mesh_CreateVirtualBox( &scene->meshes[MESH_VIRTUAL_BOX] );

	Material_Create( &scene->materials[MATERIAL_DEFAULT_BASIC], Material_DrawBasic );
	Material_Create( &scene->materials[MATERIAL_DEFAULT_FAKE_CUBE], Material_DrawFakeCube );
		
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_WIREFRAME, Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MAX_SCALE );
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_TRIANGLE, Vec3_Make( 0.0f, 0.0f, -GRID_MAX_SCALE ), 5.0f );
	for ( u32 randomCubeID = 0; randomCubeID < RANDOM_CUBE_COUNT;  )
	{
		const Vec3 center = Vec3_Rand() * (GRID_MAX_SCALE - FREE_SCALE);
		const float size = 1.0f + 74.0f * rand() / RAND_MAX;
		if ( 
			center.x - size < FREE_SCALE && center.x + size > -FREE_SCALE &&
			center.y - size < FREE_SCALE && center.y + size > -FREE_SCALE &&
			center.z - size < FREE_SCALE && center.z + size > -FREE_SCALE ) continue;
		if ( fabsf( center.x ) + size > GRID_MAX_SCALE ) continue;
		if ( fabsf( center.y ) + size > GRID_MAX_SCALE ) continue;
		if ( fabsf( center.z ) + size > GRID_MAX_SCALE ) continue;

#if 0
		Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_RED + (randomCubeID % 3), center, size );
#else
		Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_FAKE_CUBE, MESH_FAKE_CUBE, center, size );
#endif
		Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_WIREFRAME, center, size );
		
		++randomCubeID;
	}	
}

#endif

u32 Grid_Trace( const Vec3* gridCenter, float gridScale, u32 gridOccupancy, const Vec3* rayOrg, const Vec3* rayDir, TraceData_s* traceData )
{
	const Vec3 invDir = rayDir->Rcp();
	const Vec3 alpha = (*gridCenter - *rayOrg) * invDir;
	const Vec3 beta = gridScale * invDir;	
	const Vec3 t0 = alpha + beta;
	const Vec3 t1 = alpha - beta;
	const Vec3 tMin = Min( t0, t1 );
	const Vec3 tMax = Max( t0, t1 );
	const float tIn = Max( Max( tMin.x, tMin.y ), tMin.z );
	const float tOut = Min( Min( tMax.x, tMax.y ), tMax.z );

	if ( traceData )
	{
		traceData->tIn = tIn;
		traceData->tOut = tOut;
		traceData->hitFoundCoords = Vec3i_Zero();
		traceData->hitFailBits = 0;
	}
		
	if ( tIn > tOut )
		return 0;

	const float cellSize = (gridScale * 2.0f) / HLSL_CELL_SUPER_SAMPLING_WIDTH;
	const float scale = 1.0f / cellSize;
	const float offset = HLSL_CELL_SUPER_SAMPLING_WIDTH * 0.5f;
	
	const Vec3 tDelta = cellSize * invDir.Abs();	

	const Vec3 pIn = *rayOrg + tIn * *rayDir;
	const Vec3 coordIn = (pIn - *gridCenter) * scale + offset;
		
	const int x = min( (int)coordIn.x, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	const int y = min( (int)coordIn.y, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	const int z = min( (int)coordIn.z, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	Vec3i coords = Vec3i_Make( x, y, z );
	const Vec3i step = Vec3i_Make( rayDir->x < 0.0f ? -1 : 1, rayDir->y < 0.0f ? -1 : 1, rayDir->z < 0.0f ? -1 : 1 );
	Vec3 tCur = tMin;
	for ( u32 pass = 0; pass < 2; ++pass )
	{
		const Vec3 tNext = tCur + tDelta;
		tCur.x = tNext.x < tIn ? tNext.x : tCur.x;
		tCur.y = tNext.y < tIn ? tNext.y : tCur.y;
		tCur.z = tNext.z < tIn ? tNext.z : tCur.z;
	}
	
	u32 hitFound = 0;

	for ( ;; )
	{
		const u32 cellID = coords.z * HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH + coords.y * HLSL_CELL_SUPER_SAMPLING_WIDTH + coords.x;
		const u32 occupancyBit = 1 << cellID;
		hitFound |= (gridOccupancy & occupancyBit);
		if ( hitFound )
		{
			if ( traceData)
				traceData->hitFoundCoords = coords;
			break;
		}

		if ( traceData)
			traceData->hitFailBits |= occupancyBit;

		const Vec3 tNext = tCur + tDelta;
		u32 nextAxis;
		if ( tNext.x < tNext.y )
			nextAxis = tNext.x < tNext.z ? 0 : 2;
		else
			nextAxis = tNext.y < tNext.z ? 1 : 2;
		
		tCur[nextAxis] = tNext[nextAxis];
		coords[nextAxis] += step[nextAxis];
		
		if ( coords[nextAxis] < 0 || coords[nextAxis] >= HLSL_CELL_SUPER_SAMPLING_WIDTH )
			break;
	}

	return hitFound;
}

void Block_TraceDisplay( SceneDebug_s* scene, const Vec3* gridCenter, float gridScale, u32 gridOccupancy, const Vec3* rayOrg, const Vec3* rayEnd )
{
	s_gridCenter = *gridCenter;
	s_gridScale = gridScale;
	s_gridOccupancy = gridOccupancy;
	s_rayOrg = *rayOrg;
	s_rayEnd = *rayEnd;

	scene->entities[scene->entityGridID].pos = *gridCenter;
	scene->entities[scene->entityGridID].scale = gridScale;
	scene->entities[scene->entityGridID].visible = true;

	const Vec3 rayDir = (*rayEnd - *rayOrg).Normalized();
	
	BasicVertex_s vertices[2];
	vertices[0].position = *rayOrg;
	vertices[0].color = Color_Make( 255, 0, 0, 255 );	
	vertices[1].position = *rayOrg + 1000000.0f * rayDir;
	vertices[1].color = Color_Make( 0, 255, 0, 255 );
	GPUMesh_UpdateVertices( &scene->meshes[scene->meshLineID], vertices );
	scene->entities[scene->entityLineID].visible = true;
	
	const float cellSize = (gridScale * 2.0f) / HLSL_CELL_SUPER_SAMPLING_WIDTH;
	const Vec3 cellOrg = *gridCenter - gridScale + cellSize * 0.5f;
	
	u32 cellID = 0;
	for ( int z = 0; z < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++z )
	{
		for ( int y = 0; y < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++y )
		{
			for ( int x = 0; x < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++x, ++cellID )
			{
				const Vec3 cellCenter = cellOrg + Vec3_Make( (float)x, (float)y, (float)z ) * cellSize;
				
				Entity_s* cellEntity = &scene->entities[scene->entityCellIDs[cellID][0]];
				cellEntity->visible = (gridOccupancy & (1 << cellID)) != 0;
				cellEntity->pos = cellCenter;
				cellEntity->scale = cellSize * 0.5f;
				
				cellEntity = &scene->entities[scene->entityCellIDs[cellID][1]];
				cellEntity->visible = false;
				cellEntity->pos = cellCenter;
				cellEntity->scale = cellSize * 0.5f;
			}
		}
	}
	
	const u32 hitFound = Grid_Trace( gridCenter, gridScale, gridOccupancy, rayOrg, &rayDir, nullptr );

	const u32 cellCount = HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH;
	for ( u32 cellID = 0; cellID < cellCount; ++cellID )
		scene->entities[scene->entityCellIDs[cellID][1]].visible = (hitFound & (1 << cellID)) != 0;
}

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
	void BlendPixel( const RenderingView_s* view, u32 eye );
	bool BuildBlock( u32 frameID );
	void Capture_Render( CubeFaceContext_s* cubeFaceContext, const Vec3* sampleOffset, u32 faceID );
	bool Create(int nWidth, int nHeight, CFileSystem* fileSystem, IAllocator* heap, IStack* stack );
	void CullBlock( const RenderingView_s* views, const Vec3* buildOrigin, float gridMinScale, u32 groupCounts[CODEC_BUCKET_COUNT], u32 frameID );
	void Draw( float dt );
	void DrawCameraPath( const RenderingView_s* view, u32 eye );
	void DrawDebug( const RenderingView_s* view, u32 eye );
	void DrawWorld( const RenderingView_s* view, u32 eye );
	bool HasValidRawFrameFile( u32 frameID );
	bool InitTraceMode( u32 frameCount );
	void Output( ID3D11ShaderResourceView* srvLeft, ID3D11ShaderResourceView* srvRight );
	void Release();
	void ReleaseTraceMode();
	void ResetDrawMode();
	void TraceBlock( const RenderingView_s* views, const Vec3* buildOrigin );
	bool WriteRawFrameFile( CaptureContext_s* captureContext, u32 frame );

	Vec3				m_sampleOffsets[SAMPLE_MAX_COUNT];
	
	CubeFaceContext_s	m_cubeFaceContext;
	CaptureContext_s	m_captureContext;
	SequenceContext_s*	m_sequenceContext;
	TraceContext_s*		m_traceContext;
	Sequence_s			m_sequence;
	int					m_bakedFrameCount;
	u32					m_lastUpdatedFrameID;

	SceneViewer_s*		m_defaultScene;
	SceneDebug_s*		m_debugScene;
	ScenePathGeo_s*		m_pathGeoScene;

	IAllocator*			m_heap;
	IStack*				m_stack;

	u32					m_width;
	u32					m_height;
	float				m_aspectRatio;
};

CRenderingDevice::CRenderingDevice()
{
	memset( this, 0, sizeof( CRenderingDevice ) );
}

CRenderingDevice::~CRenderingDevice()
{
}

bool CRenderingDevice::Create( int nWidth, int nHeight, CFileSystem* fileSystem, IAllocator* heap, IStack* stack )
{
	m_heap = heap;
	m_stack = stack;
	ScopedStack scopedStack( stack );

	for ( u32 pathID = 0; pathID < PATH_COUNT; ++pathID )
		Path_Init( &s_paths[pathID] );
	PathPlayer_Init( &s_pathPlayer );

	m_width = nWidth;
	m_height = nHeight;
	m_aspectRatio = (float)nWidth / nHeight;

	GPUContext_Create( nWidth, nHeight, (HWND)s_win.hWnd, fileSystem, heap, stack );

	m_defaultScene = heap->newInstance< SceneViewer_s >();
	Scene_CreateDefault( m_defaultScene );
	s_activeScene = m_defaultScene;

	m_debugScene = heap->newInstance< SceneDebug_s >();
	Scene_CreateDebug( m_debugScene );

	m_pathGeoScene = heap->newInstance< ScenePathGeo_s >();
	Scene_CreatePathGeo( m_pathGeoScene );

	g_sample = 0;
	m_sampleOffsets[0] = Vec3_Make( 0.0f, 0.0f, 0.0f );
	for ( u32 sample = 1; sample < SAMPLE_MAX_COUNT; ++sample )
	{
		if ( sample <= 8 )
		{
			u32 vertexID = sample-1;
			m_sampleOffsets[sample].x = (vertexID&1) == 0 ? -FREE_SCALE : FREE_SCALE;
			m_sampleOffsets[sample].y = (vertexID&2) == 0 ? -FREE_SCALE : FREE_SCALE;
			m_sampleOffsets[sample].z = (vertexID&4) == 0 ? -FREE_SCALE : FREE_SCALE;
		}
		else
		{
			m_sampleOffsets[sample] = Vec3_Rand() * FREE_SCALE;
		}
	}

	GPUResource_LogMemoryUsage();

	return true;
}

void CRenderingDevice::DrawWorld( const RenderingView_s* view, u32 eye )
{
	u32 flags = 0;

	GPURenderTargetContextDesc_s renderTargetContextDesc = {};
	renderTargetContextDesc.clear = true;
	renderTargetContextDesc.useMSAA = g_useMSAA;
#if V6_USE_ALPHA_COVERAGE == 1
	renderTargetContextDesc.useAlphaCoverage = true;
	flags = RENDER_FLAGS_USE_ALPHA_COVERAGE;
#endif

	GPURenderTargetContext_Begin( &renderTargetContextDesc, eye );

	Scene_Draw( s_activeScene, &view->view, flags );

	GPURenderTargetContext_End();
}

void CRenderingDevice::DrawCameraPath( const RenderingView_s* view, u32 eye )
{
	const GPURenderTargetContextDesc_s renderTargetContextDesc = {};

	GPURenderTargetContext_Begin( &renderTargetContextDesc, eye );

	Scene_Draw( m_pathGeoScene, &view->view, 0 );

	GPURenderTargetContext_End();
}

void CRenderingDevice::DrawDebug( const RenderingView_s* view, u32 eye )
{	
	if ( g_traceGrid )
	{		
		Block_TraceDisplay( m_debugScene, &s_gridCenter, s_gridScale, s_gridOccupancy, &s_rayOrg, &s_rayEnd );
		g_traceGrid = false;
	}

	GPURenderTargetContextDesc_s renderTargetContextDesc = {};
	renderTargetContextDesc.disableZ = g_transparentDebug;

	GPURenderTargetContext_Begin( &renderTargetContextDesc, eye );

	Scene_Draw( m_debugScene, &view->view, 0 );

	GPURenderTargetContext_End();
}

void CRenderingDevice::Capture_Render( CubeFaceContext_s* cubeFaceContext, const Vec3* samplePos, u32 faceID )
{
	GPURenderTargetContext_s* renderTargetContext = GPURenderTargetContext_Get();

	// Rasterization state
	g_deviceContext->OMSetDepthStencilState( renderTargetContext->depthStencilStateZRW, 0 );
	g_deviceContext->OMSetBlendState( renderTargetContext->blendStateOpaque, nullptr, 0XFFFFFFFF );

	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)CUBE_SIZE;
		viewport.Height = (float)CUBE_SIZE;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		g_deviceContext->RSSetViewports( 1, &viewport );
		g_deviceContext->RSSetState( renderTargetContext->rasterState );
	}

	GPUEvent_Begin( "Capture" );
		
	// RT
	g_deviceContext->OMSetRenderTargets( 1, &cubeFaceContext->color.rtv, cubeFaceContext->depth.dsv );

	// Clear
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	g_deviceContext->ClearRenderTargetView( cubeFaceContext->color.rtv, pRGBA );
	g_deviceContext->ClearDepthStencilView( cubeFaceContext->depth.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

	// View
	View_s view;
	Cube_MakeViewMatrix( &view.viewMatrix, *samplePos, (CubeAxis_e)faceID );
	view.projMatrix = Mat4x4_Projection( ZNEAR, ZFAR, DegToRad( 90.0f ), 1.0f );

	Scene_Draw( s_activeScene, &view, RENDER_FLAGS_IS_CAPTURING );

	// un RT
	g_deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );

	GPUEvent_End();
}

void CRenderingDevice::CullBlock( const RenderingView_s* views, const Vec3* buildOrigin, float gridMinScale, u32 groupCounts[CODEC_BUCKET_COUNT], u32 frameID )
{
	static const void* nulls[8] = {};

	GPUEvent_Begin( "Cull Blocks");

	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( m_traceContext->traceIndirectArgs.uav, values );
	if ( s_logReadBack )
		g_deviceContext->ClearUnorderedAccessViewUint( m_traceContext->cullStats.uav, values );

	v6::hlsl::CBCull cbCullData = {};
	{
		float gridScale = gridMinScale;
		for ( u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID, gridScale *= 2.0f )
		{
			const float cellScale = gridScale * HLSL_GRID_INV_WIDTH;
			cbCullData.c_cullGridScales[gridID] = Vec4_Make( gridScale, 0.0f, 0.0f, 0.0f );
			const Vec3 center = Codec_ComputeGridCenter( buildOrigin, gridScale, HLSL_GRID_MACRO_HALF_WIDTH );
			cbCullData.c_cullCenters[gridID] = Vec4_Make( &center, 0.0f );
		}

		V6_ASSERT( views[LEFT_EYE].forward == views[RIGHT_EYE].forward );
		V6_ASSERT( views[LEFT_EYE].right == views[RIGHT_EYE].right );
		V6_ASSERT( views[LEFT_EYE].up == views[RIGHT_EYE].up );

		const float tanHalfFOVLeft = Max( views[LEFT_EYE].tanHalfFOVLeft, views[RIGHT_EYE].tanHalfFOVLeft );
		const float tanHalfFOVRight = Max( views[LEFT_EYE].tanHalfFOVRight, views[RIGHT_EYE].tanHalfFOVRight );
		const float tanHalfFOVUp = Max( views[LEFT_EYE].tanHalfFOVUp, views[RIGHT_EYE].tanHalfFOVUp );
		const float tanHalfFOVDown = Max( views[LEFT_EYE].tanHalfFOVDown, views[RIGHT_EYE].tanHalfFOVDown );

		Vec3 leftPlane = (views[ANY_EYE].forward * tanHalfFOVLeft + views[ANY_EYE].right).Normalized();
		Vec3 rightPlane = (views[ANY_EYE].forward * tanHalfFOVRight - views[ANY_EYE].right).Normalized();
		Vec3 upPlane = (views[ANY_EYE].forward * tanHalfFOVUp - views[ANY_EYE].up).Normalized();
		Vec3 bottomPlane = (views[ANY_EYE].forward * tanHalfFOVDown + views[ANY_EYE].up).Normalized();
		
		cbCullData.c_cullFrustumPlanes[0] = Vec4_Make( &leftPlane, -Dot( leftPlane, views[LEFT_EYE].org ) );
		cbCullData.c_cullFrustumPlanes[1] = Vec4_Make( &rightPlane, -Dot( rightPlane, views[RIGHT_EYE].org ) );
		cbCullData.c_cullFrustumPlanes[2] = Vec4_Make( &upPlane, -Dot( upPlane, views[ANY_EYE].org ) );
		cbCullData.c_cullFrustumPlanes[3] = Vec4_Make( &bottomPlane, -Dot( bottomPlane, views[ANY_EYE].org ) );
	}

	// set

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	u32 bufferID = frameID & 1;
	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBCullSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_CULL].buf );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_GROUP_SLOT, 1, &m_sequenceContext->groups[bufferID].srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_RANGE_SLOT, 1, &m_sequenceContext->ranges[bufferID].srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, &m_sequenceContext->blockPos.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELLS_SLOT, 1, &m_traceContext->traceCell.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_traceContext->traceIndirectArgs.uav, nullptr );
	if ( s_logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, &m_traceContext->cullStats.uav, nullptr );

	for ( u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		GPUEvent_Begin( "Cull Bucket");

		// update
		{
			v6::hlsl::CBCull* cbCull = (v6::hlsl::CBCull*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_CULL] );
			memcpy( cbCull, &cbCullData, sizeof( cbCullData ) );
			GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_CULL] );

			cbCullData.c_cullBlockGroupOffset += groupCounts[bucket];
			cbCullData.c_cullBlockRangeOffset += m_sequence.frameDescArray[frameID].blockRangeCounts[bucket];
		}

		// dispach
		if ( s_logReadBack || g_showHistory )
			GPUCompute_Dispatch( &shaderContext->computes[COMPUTE_BLOCK_CULL_STATS4+bucket], groupCounts[bucket], 1, 1 );
		else
			GPUCompute_Dispatch( &shaderContext->computes[COMPUTE_BLOCK_CULL4+bucket], groupCounts[bucket], 1, 1 );

		GPUEvent_End();
	}
	
	// Unset
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_GROUP_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_RANGE_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELLS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( s_logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();

	if ( s_logReadBack )
	{
		V6_MSG( "\n" );

		{
			const hlsl::BlockCullStats* blockCullStats = (hlsl::BlockCullStats*)GPUBuffer_MapReadBack( &m_traceContext->cullStats );

			ReadBack_Log( "blockCull", blockCullStats->blockInputCount, "blockInputCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockProcessedCount, "blockProcessedCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockPassedCount, "blockPassedCount" );
			V6_ASSERT( blockCullStats->blockPassedCount <= m_traceContext->passedBlockCount );
			ReadBack_Log( "blockCull", blockCullStats->cellOutputCount, "cellOutputCount" );

			GPUBuffer_UnmapReadBack( &m_traceContext->cullStats );
		}
	}
}

void CRenderingDevice::ResetDrawMode()
{
	g_drawMode = DRAW_MODE_DEFAULT;
	g_sample = 0;
	PathPlayer_Play( &s_pathPlayer, false );
	m_bakedFrameCount = 0;
}

bool CRenderingDevice::InitTraceMode( u32 frameCount )
{
	char templateFilename[256];
	char sequenceFilename[256];

	SceneViewer_MakeRawFrameFileTemplate( s_activeScene, templateFilename, sizeof( templateFilename ) );
	SceneViewer_MakeSequenceFilename( s_activeScene, sequenceFilename, sizeof( sequenceFilename ) );

#if V6_USE_CACHE == 1
	CodecSequenceDesc_s sequenceDesc;
	if ( !Sequence_LoadDesc( sequenceFilename, &sequenceDesc, m_stack ) ||
		sequenceDesc.frameCount != frameCount ||
		sequenceDesc.sampleCount != SAMPLE_MAX_COUNT ||
		sequenceDesc.gridMacroShift != HLSL_GRID_MACRO_SHIFT ||
		sequenceDesc.gridScaleMin != GRID_MIN_SCALE ||
		sequenceDesc.gridScaleMax != GRID_MAX_SCALE )
#endif // #if V6_USE_CACHE == 1
	{
		if ( !Sequence_Encode( templateFilename, frameCount, sequenceFilename, VIDEO_FRAME_RATIO, m_heap ) )
			return false;
	}
		
	if ( !Sequence_Load( sequenceFilename, &m_sequence, m_heap, m_stack ) )
		return false;
	
	m_sequenceContext = m_heap->newInstance< SequenceContext_s >();
	m_traceContext = m_heap->newInstance< TraceContext_s >();

	SequenceContext_CreateFromData( m_sequenceContext, &m_sequence );
	const u32 passedBlockCount = m_sequenceContext->blockMaxCount / 3;
	const u32 cellItemCount = (m_width * HLSL_EYE_COUNT * m_height) * HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT; 
	TraceContext_Create( m_traceContext, m_width, m_height, passedBlockCount, cellItemCount );

	m_lastUpdatedFrameID = (u32)-1;

	return true;
}

void CRenderingDevice::ReleaseTraceMode()
{
	Sequence_Release( &m_sequence, m_heap );
	SequenceContext_Release( m_sequenceContext );
	TraceContext_Release( m_traceContext );

	m_heap->deleteInstance( m_sequenceContext );
	m_heap->deleteInstance( m_traceContext );
}

void CRenderingDevice::TraceBlock( const RenderingView_s* views, const Vec3* buildOrigin )
{		
	static const void* nulls[8] = {};
	
	GPUEvent_Begin( "Trace Blocks");

	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( m_traceContext->cellItemCounters.uav, values );
	if ( s_logReadBack )
		g_deviceContext->ClearUnorderedAccessViewUint( m_traceContext->traceStats.uav, values );

	float gridScale = GRID_MIN_SCALE;

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	{
		v6::hlsl::CBBlock* cbBlock = (v6::hlsl::CBBlock*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BLOCK] );

		float gridScale = GRID_MIN_SCALE;
		for ( u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID, gridScale *= 2.0f )
		{
			const float cellScale = gridScale * HLSL_GRID_INV_WIDTH;
			cbBlock->c_blockGridScales[gridID] = Vec4_Make( gridScale, cellScale, ((1 << 21) - 1) / gridScale, 0.0f );
			const Vec3 center = Codec_ComputeGridCenter( buildOrigin, gridScale, HLSL_GRID_MACRO_HALF_WIDTH );
			cbBlock->c_blockGridCenters[gridID] = Vec4_Make( &center, 0.0f );
		}

		const Vec2 frameSize = Vec2_Make( (float)m_width, (float)m_height );
		cbBlock->c_blockFrameSize = frameSize;
				
		cbBlock->c_blockFrameSize.x = (float)m_width;
		cbBlock->c_blockFrameSize.y = (float)m_height;

		for ( u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
		{	
			hlsl::BlockPerEye blockPerEye;
			blockPerEye.objectToView = views[eye].view.viewMatrix;
			blockPerEye.viewToProj = views[eye].view.projMatrix;

			const float scaleRight = (views[eye].tanHalfFOVLeft + views[eye].tanHalfFOVRight) / m_width;
			const float scaleUp = (views[eye].tanHalfFOVUp + views[eye].tanHalfFOVDown) / m_height;

			const Vec3 forward = views[eye].forward;
			const Vec3 right = views[eye].right * scaleRight;
			const Vec3 up = views[eye].up * scaleUp;
				
			blockPerEye.org = views[eye].org;

			blockPerEye.rayDirBase = forward - views[eye].up * views[eye].tanHalfFOVDown + 0.5f * up - views[eye].right * views[eye].tanHalfFOVLeft + 0.5f * right;
			blockPerEye.rayDirUp = up;
			blockPerEye.rayDirRight = right;

			cbBlock->c_blockEyes[eye] = blockPerEye;

			cbBlock->c_blockGetStats = s_logReadBack;
			cbBlock->c_blockShowFlag = (g_showMip ? HLSL_BLOCK_SHOW_FLAG_MIPS : 0) | (g_showBucket ? HLSL_BLOCK_SHOW_FLAG_BUCKETS : 0) | (g_showHistory ? HLSL_BLOCK_SHOW_FLAG_HISTORY : 0);
		}

		GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BLOCK] );
	}

	// set

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBBlockSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_BLOCK].buf );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, &m_sequenceContext->blockData.srv );
	g_deviceContext->CSSetShaderResources( HLSL_TRACE_CELLS_SLOT, 1, &m_traceContext->traceCell.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_SLOT, 1, &m_traceContext->cellItems.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, &m_traceContext->cellItemCounters.uav, nullptr );
	if ( s_logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, &m_traceContext->traceStats.uav, nullptr );
	
	{
		GPUEvent_Begin( "Init Trace");

		// set
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_traceContext->traceIndirectArgs.uav, nullptr );
				
		// dispatch
		GPUCompute_Dispatch( &shaderContext->computes[COMPUTE_BLOCK_TRACE_INIT], 1, 1, 1 );

		// unset
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

		GPUEvent_End();
	}

	for ( u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		GPUEvent_Begin( "Trace Bucket");

		// set

		g_deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_traceContext->traceIndirectArgs.srv );

		// dispach
		if ( s_logReadBack || g_showMip || g_showBucket || g_showHistory )
			GPUCompute_DispatchIndirect( &shaderContext->computes[COMPUTE_BLOCK_TRACE_DEBUG4+bucket], &m_traceContext->traceIndirectArgs, trace_cellGroupCountX_offset( bucket ) * sizeof( u32 ) );
		else
			GPUCompute_DispatchIndirect( &shaderContext->computes[COMPUTE_BLOCK_TRACE4+bucket], &m_traceContext->traceIndirectArgs, trace_cellGroupCountX_offset( bucket ) * sizeof( u32 ) );

		// unset		
		g_deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );

		GPUEvent_End();
	}

	// Unset
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_TRACE_CELLS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( s_logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	
	GPUEvent_End();

	if ( s_logReadBack )
	{			
		V6_MSG( "\n" );

		{
			const hlsl::BlockTraceStats* blockTraceStats = (hlsl::BlockTraceStats*)GPUBuffer_MapReadBack( &m_traceContext->traceStats );

			ReadBack_Log( "blockTrace", blockTraceStats->cellInputCount, "cellInputCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellProcessedCount, "cellProcessedCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelSampleCount, "pixelSampleCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellItemCount, "cellItemCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellItemMaxCountPerPixel, "cellItemMaxCountPerPixel" );
			V6_ASSERT( blockTraceStats->cellItemCount < m_traceContext->cellItemCount );

			GPUBuffer_UnmapReadBack( &m_traceContext->traceStats );
		}
	}
}

void CRenderingDevice::BlendPixel( const RenderingView_s* view, u32 eye )
{
	// Render

	GPUEvent_Begin( "Blend Pixels");
	
	// Set

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();
	GPURenderTargetContext_s* renderTargetContext = GPURenderTargetContext_Get();

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBPixelSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_PIXEL].buf );

	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_SLOT, 1, &m_traceContext->cellItems.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, &m_traceContext->cellItemCounters.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_LCOLOR_SLOT, 1, &renderTargetContext->colorBuffers[eye].uav, nullptr );
	if ( g_showOverdraw	)
		g_deviceContext->CSSetShader( shaderContext->computes[COMPUTE_BLENDPIXEL_OVERDRAW].m_computeShader, nullptr, 0 );
	else
		g_deviceContext->CSSetShader( shaderContext->computes[COMPUTE_BLENDPIXEL].m_computeShader, nullptr, 0 );

	{
		v6::hlsl::CBPixel* cbPixel = (v6::hlsl::CBPixel*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_PIXEL] );

		cbPixel->c_pixelFrameSize.x = m_width;
		cbPixel->c_pixelFrameSize.y = m_height;

		if ( g_randomBackground )
		{
			const float r = RandFloat();
			cbPixel->c_pixelBackColor.x = 1.0f; 
			cbPixel->c_pixelBackColor.y = r;
			cbPixel->c_pixelBackColor.z = r;
		}
		else
		{
			cbPixel->c_pixelBackColor.x = 0.0f; 
			cbPixel->c_pixelBackColor.y = 0.0f; 
			cbPixel->c_pixelBackColor.z = 0.0f; 
		}

		cbPixel->c_eye = eye;

		GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_PIXEL]  );
	}		

	V6_ASSERT( (m_width & 0x7) == 0 );
	V6_ASSERT( (m_height & 0x7) == 0 );
	const u32 pixelGroupWidth = m_width >> 3;
	const u32 pixelGroupHeight = m_height >> 3;
	g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_LCOLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();	
}

void CRenderingDevice::Output( ID3D11ShaderResourceView* srvLeft, ID3D11ShaderResourceView* srvRight )
{
	GPUSurfaceContext_s* surfaceContext = GPUSurfaceContext_Get();

#if HLSL_STEREO == 1
	// Render

	GPUEvent_Begin( "Compose Surface" );

	// set

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBComposeSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE].buf );

	g_deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, &srvLeft );
	g_deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, &srvRight );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, &surfaceContext->surface.uav, nullptr );
		
	g_deviceContext->CSSetShader( shaderContext->computes[COMPUTE_COMPOSESURFACE].m_computeShader, nullptr, 0 );

	{
		v6::hlsl::CBCompose* cbCompose = ConstantBuffer_MapWrite< v6::hlsl::CBCompose >( g_deviceContext, &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE] );
		
		cbCompose->c_composeFrameWidth = m_config.screenWidth;

		ConstantBuffer_UnmapWrite( g_deviceContext, &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE]  );
	}		

	V6_ASSERT( (m_width & 0x7) == 0 );
	V6_ASSERT( (m_height & 0x7) == 0 );
	const u32 pixelGroupWidth = (m_width >> 3) * HLSL_EYE_COUNT;
	const u32 pixelGroupHeight = m_height >> 3;
	g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();
#else	
	ID3D11Resource* colorBuffer;
	srvLeft->GetResource( &colorBuffer );
	g_deviceContext->CopyResource( surfaceContext->surface.tex, colorBuffer );
#endif
}

bool CRenderingDevice::HasValidRawFrameFile( u32 frameID )
{
	if ( s_activeScene->filename[0] == 0 )
	{
		V6_ERROR( "Null scene file.\n" );
		return false;
	}

	char path[256];
	SceneViewer_MakeRawFrameFilename( s_activeScene, path, sizeof( path ), frameID );

	CFileReader fileReader;
	if ( !fileReader.Open( path ) )
	{
		V6_ERROR( "Unable to open file %s.\n", path );
		return false;
	}

	CodecRawFrameDesc_s frameDesc;

	if ( !Codec_ReadRawFrameHeader( &fileReader, &frameDesc ) )
	{
		V6_ERROR( "Unable to read file %s.\n", path );
		return false;
	}
			
	if ( frameDesc.origin != s_buildOrigin )
	{
		V6_ERROR( "Stream origin is not compatible.\n" );
		return false;
	}
	
	if ( frameDesc.frameID != frameID )
	{
		V6_ERROR( "Stream frame is not compatible.\n" );
		return false;
	}
	
	if ( frameDesc.sampleCount != SAMPLE_MAX_COUNT )
	{
		V6_ERROR( "Stream sampleCount is not compatible.\n" );
		return false;
	}
	
	if ( frameDesc.gridMacroShift != HLSL_GRID_MACRO_SHIFT )
	{
		V6_ERROR( "Stream gridMacroShift is not compatible.\n" );
		return false;
	}
	
	if ( frameDesc.gridScaleMin != GRID_MIN_SCALE )
	{
		V6_ERROR( "Stream gridScaleMin is not compatible.\n" );
		return false;
	}
	
	if ( frameDesc.gridScaleMax != GRID_MAX_SCALE )
	{
		V6_ERROR( "Stream gridScaleMax is not compatible.\n" );
		return false;
	}

	return true;
}

bool CRenderingDevice::WriteRawFrameFile( CaptureContext_s* captureContext, u32 frameID )
{
	if ( s_activeScene->filename[0] == 0 )
	{
		V6_ERROR( "Null scene file.\n" );
		return false;
	}

	bool hasError = false;

	{
		char path[256];
		SceneViewer_MakeRawFrameFilename( s_activeScene, path, sizeof( path ), frameID );

		CFileWriter fileWriter;
		if ( fileWriter.Open( path ) )
		{
			CodecRawFrameDesc_s frameDesc = {};
			frameDesc.origin = s_buildOrigin;
			frameDesc.frameID = frameID;
			frameDesc.sampleCount = SAMPLE_MAX_COUNT;
			frameDesc.gridMacroShift = HLSL_GRID_MACRO_SHIFT;
			frameDesc.gridScaleMin = GRID_MIN_SCALE;
			frameDesc.gridScaleMax = GRID_MAX_SCALE;
			
			CodecRawFrameData_s frameData = {};

			{
				Capture_MapBlocksForRead( captureContext, frameDesc.blockCounts, &frameData.blockPos, &frameData.blockData );
				Codec_WriteRawFrame( &fileWriter, &frameDesc, &frameData );
				Capture_UnmapBlocksForRead( captureContext );
			}

			V6_MSG( "Stream saved to file %s.\n", path );
		}
		else
		{
			V6_ERROR( "Unable to create file %s.\n", path );
			hasError = true;
		}
	}

	return !hasError;
}

bool CRenderingDevice::BuildBlock( u32 frameID )
{
	static u32 lastSumLeafCount = 0;

	if ( g_sample == 0 )
	{
		s_buildOrigin = s_headOffset;

#if V6_USE_CACHE == 1
		if ( HasValidRawFrameFile( frameID ) )
		{
			g_sample = SAMPLE_MAX_COUNT;
			return true;
		}
#endif // #if V6_USE_CACHE == 1

		CaptureDesc_s captureDesc;
		captureDesc.gridMacroShift = HLSL_GRID_MACRO_SHIFT;
		captureDesc.gridScaleMin = GRID_MIN_SCALE;
		captureDesc.gridScaleMax = GRID_MAX_SCALE;
		captureDesc.depthLinearScale = -1.0f / ZNEAR;
		captureDesc.depthLinearBias = 1.0f / ZNEAR;
		captureDesc.logReadBack = s_logReadBack;

		CubeFace_Create( &m_cubeFaceContext, HLSL_GRID_MACRO_SHIFT );
		Capture_Create( &m_captureContext, &captureDesc );

		lastSumLeafCount = 0;
		Capture_Begin( &m_captureContext );
	}

	V6_MSG( "Capturing sample #%03d...", g_sample );

	const Vec3 samplePos = s_buildOrigin + m_sampleOffsets[g_sample];

	u32 sumLeafCount = 0;

	for ( u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		Capture_Render( &m_cubeFaceContext, &samplePos, faceID );
		sumLeafCount += Capture_AddSamplesFromCubeFace( &m_captureContext, &s_buildOrigin, &samplePos, faceID, m_cubeFaceContext.color.srv, m_cubeFaceContext.depth.srv );
	}

	const u32 newLeafCount = sumLeafCount - lastSumLeafCount;
	lastSumLeafCount = sumLeafCount;

	V6_MSG( "\r" );
	V6_MSG( "Captured  sample #%03d: %13s cells added\n", g_sample, String_FormatInteger_Unsafe( newLeafCount ) );

	++g_sample;

	v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T1] );

	if ( g_sample == SAMPLE_MAX_COUNT )
	{
		V6_MSG( "Packing all samples..." );

		Capture_End( &m_captureContext );

		const bool written = WriteRawFrameFile( &m_captureContext, frameID );

		Capture_Release( &m_captureContext );
		CubeFace_Release( &m_cubeFaceContext );

		if ( !written )
			return false;

		V6_MSG( "\r" );
		V6_MSG( "Packed  all samples: %13s cells added @ frame %d\n", String_FormatInteger_Unsafe( sumLeafCount ), frameID );
		s_logReadBack = false;
#if 0
		g_sample = 0;
#endif
	}

	return true;
}

void CRenderingDevice::Draw( float dt )
{
	static int lastKeyPosX = -1;
	static int lastKeyPosZ = -1;

	const u32 frameID = PathPlayer_GetFrame( &s_pathPlayer );

	int mouseDeltaX = 0;
	int mouseDeltaY = 0;
	int keyDeltaX = 0;
	int keyDeltaZ = 0;

	if ( g_mousePressed )
	{		
		mouseDeltaX = g_mouseDeltaX;
		mouseDeltaY = g_mouseDeltaY;
		g_mouseDeltaX = 0;
		g_mouseDeltaY = 0;
	}
	else
	{
		mouseDeltaX = 0;
		mouseDeltaY = 0;
	}

	if ( g_keyLeftPressed != g_keyRightPressed )
		keyDeltaX = g_keyLeftPressed ? -1 : 1;

	if ( g_keyDownPressed != g_keyUpPressed )
		keyDeltaZ = g_keyDownPressed ? -1 : 1;
		
	const static float MOUSE_ROTATION_SPEED = 0.5f;
	
	Mat4x4 orientationMatrix;
#if V6_USE_HMD
	HmdRenderTarget_s renderTargets[2];
	HmdEyePose_s eyePoses[2];
	s_hmdState = v6::Hmd_BeginRendering( renderTargets, eyePoses, ZNEAR, ZFAR );
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		const Mat4x4 yawMatrix = Mat4x4_RotationY( s_yaw );
		Mat4x4_Mul3x3( &orientationMatrix, yawMatrix, eyePoses[0].lookAt );
		Mat4x4_Transpose( &orientationMatrix );
	}
	else
#endif // #if V6_USE_HMD
	{
		s_yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
		s_pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

		const Mat4x4 yawMatrix = Mat4x4_RotationY( s_yaw );
		const Mat4x4 pitchMatrix = Mat4x4_RotationX( s_pitch );
		Mat4x4_Mul( &orientationMatrix, yawMatrix, pitchMatrix );
		Mat4x4_Transpose( &orientationMatrix );
	}

	const Vec3 forward = -orientationMatrix.GetZAxis()->Normalized();
	const Vec3 right = orientationMatrix.GetXAxis()->Normalized();
	const Vec3 up = orientationMatrix.GetYAxis()->Normalized();

	if ( keyDeltaX || keyDeltaZ )
	{
		s_headOffset += right * (float)keyDeltaX * g_translation_speed * dt;
		s_headOffset += forward * (float)keyDeltaZ * g_translation_speed * dt;
	}
	
	if ( g_limit )
	{
		Vec3 distanceToCenter = s_headOffset - s_buildOrigin;
		distanceToCenter.x = Clamp( distanceToCenter.x, -FREE_SCALE, FREE_SCALE );
		distanceToCenter.y = Clamp( distanceToCenter.y, -FREE_SCALE, FREE_SCALE );
		distanceToCenter.z = Clamp( distanceToCenter.z, -FREE_SCALE, FREE_SCALE );
		s_headOffset = s_buildOrigin + distanceToCenter;
	}

	if ( g_showPath || s_pathPlayer.isPlaying )
	{
		Scene_UpdatePathGeo( m_pathGeoScene, &s_paths[s_activePath] );
		PathPlayer_Compute( &s_pathPlayer, s_paths, PATH_COUNT );
	}

	if ( s_pathPlayer.isPlaying )
	{
		if ( s_activePath == PATH_CAMERA )
			PathPlayer_GetPosition( &s_headOffset, &s_pathPlayer, PATH_CAMERA );
		for ( u32 pathID = PATH_CAMERA+1; pathID < PATH_COUNT; ++pathID )
		{
			Vec3 entityPosition;
			const u32 entityID = s_paths[pathID].entityID;
			if ( entityID != (u32)-1 && PathPlayer_GetPosition( &entityPosition, &s_pathPlayer, pathID ) )
				s_activeScene->entities[entityID].pos = entityPosition;
		}
	}

	GPUSurfaceContext_s* surfaceContext = GPUSurfaceContext_Get();

	RenderingView_s views[HLSL_EYE_COUNT];
#if V6_USE_HMD
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		for ( u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
		{
			RenderingView_MakeForHMD( &views[eye], &eyePoses[eye], &s_headOffset, s_yaw, eye );
			views[eye].texture2D = (ID3D11Texture2D*)renderTargets[eye].texture2D;
			views[eye].rtv = (ID3D11RenderTargetView*)renderTargets[eye].rtv;
			views[eye].uav = (ID3D11UnorderedAccessView*)renderTargets[eye].uav;
		}
	}
	else
#endif // #if V6_USE_HMD
	{
		for ( u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
			RenderingView_MakeForStereo( &views[eye], &s_headOffset, &forward, &up, &right, eye, m_aspectRatio );
}

	if ( g_drawMode == DRAW_MODE_DEFAULT )
	{
		// draw mode

		v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T0] );

		for ( u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
			DrawWorld( &views[eye], eye );
		
		v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T1] );
		v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T2] );
		v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T3] );
		v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T4] );

		PathPlayer_AddTimeStep( &s_pathPlayer, dt );
	}	
	else if ( g_drawMode == DRAW_MODE_BLOCK )
	{
		if ( g_sample < SAMPLE_MAX_COUNT )
		{
			// bake mode

			if ( m_bakedFrameCount == -1 )
			{
				ReleaseTraceMode();
				m_bakedFrameCount = 0;
			}

			V6_ASSERT( m_bakedFrameCount == frameID );

			v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T0] );

			if ( BuildBlock( frameID ) )
			{
				if ( g_sample == SAMPLE_MAX_COUNT )
				{
					++m_bakedFrameCount;

					if ( PathPlayer_AddTimeStep( &s_pathPlayer, 1.0f / HMD_FPS ) )
						g_sample = 0;
				}
			}
			else
			{
				ResetDrawMode();
			}

			v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T2] );
			v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T3] );
			v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T4] );
		}
		else
		{
			// trace mode

			if ( m_bakedFrameCount > 0 )
			{
				if ( InitTraceMode( m_bakedFrameCount ) )
					m_bakedFrameCount = -1;
				else
					ResetDrawMode();
			}

			if ( m_bakedFrameCount == -1 )
			{
				const CodecFrameDesc_s* frameDesc = &m_sequence.frameDescArray[frameID];
				if ( frameDesc->flags & CODEC_FRAME_FLAG_MOTION )
					frameDesc = &m_sequence.frameDescArray[frameDesc->frameID];

				static u32 groupCounts[CODEC_BUCKET_COUNT] = {};
				v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T0] );
				if ( m_lastUpdatedFrameID != frameDesc->frameID )
				{
					SequenceContext_UpdateFrameData( m_sequenceContext, groupCounts, frameDesc->frameID, &m_sequence, m_stack );
					m_lastUpdatedFrameID = frameDesc->frameID;
				}
				v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T1] );
				{
					CullBlock( views, &frameDesc->origin, m_sequence.desc.gridScaleMin, groupCounts, frameDesc->frameID );
				}
				v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T2] );
				{
					TraceBlock( views, &frameDesc->origin );
				}
				v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T3] );
				{
					for ( u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
						BlendPixel( &views[eye], eye );
				}
				v6::GPUQuery_WriteTimeStamp( &s_pendingQueries[v6::QUERY_T4] );

				s_logReadBack = false;
				g_mousePicked = false;

				PathPlayer_AddTimeStep( &s_pathPlayer, 1.0f / HMD_FPS );
			}
		}
	}	
	
#if V6_USE_HMD
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		HmdOuput_s output;
		if ( Hmd_EndRendering( &output ) )
			g_deviceContext->CopyResource( surfaceContext->surface.buf, (ID3D11Texture2D*)output.texture2D );
	}
	else
#endif // #if V6_USE_HMD
	{
		if ( g_showPath )
		{
			for ( u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
				DrawCameraPath( &views[eye], eye );
		}

		for ( u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
			DrawDebug( &views[eye], eye );

		GPURenderTargetContext_s* renderTargetContext = GPURenderTargetContext_Get();
		Output( renderTargetContext->colorBuffers[LEFT_EYE].srv, renderTargetContext->colorBuffers[RIGHT_EYE].srv );
	}
}

void CRenderingDevice::Release()
{
	if ( m_bakedFrameCount == -1 )
		ReleaseTraceMode();

	Scene_Release( m_defaultScene );
	m_heap->deleteInstance( m_defaultScene );

	Scene_Release( m_debugScene );
	m_heap->deleteInstance( m_debugScene );

	Scene_Release( m_pathGeoScene );
	m_heap->deleteInstance( m_pathGeoScene );

	for ( u32 pathID = 0; pathID < PATH_COUNT; ++pathID )
		Path_Release( &s_paths[pathID] );

	GPUDevice_Release();
}

END_V6_NAMESPACE

int main()
{
	V6_MSG( "Viewer 0.1\n" );

	v6::CHeap heap;
	v6::Stack stack( &heap, 100 * 1024 * 1024 );
	v6::CFileSystem filesystem;
		
#if V6_LOAD_EXTERNAL == 1
	v6::Stack stackScene( &heap, 400 * 1024 * 1024 );

	const char* filename = "D:/media/obj/crytek-sponza/sponza.obj";
	//const char* filename = "D:/media/obj/dragon/dragon.obj";
	//const char* filename = "D:/media/obj/buddha/buddha.obj";
	//const char* filename = "D:/media/obj/head/head.obj";
	//const char* filename = "D:/media/obj/hairball/hairball.obj"; // 100.0f
	//const char* filename = "D:/media/obj/hairball/hairball_simple.obj"; // 100.0f
	//const char* filename = "D:/media/obj/hairball/hairball_simple2.obj"; // 100.0f
	//const char* filename = "D:/media/obj/sibenik/sibenik.obj"; // 100.0f
	//const char* filename = "D:/media/obj/conference/conference.obj";

	v6::SceneContext_s sceneContext;
	SceneContext_Create( &sceneContext, &stackScene );
	SceneContext_SetFilename( &sceneContext, filename );

	v6::Job_Launch( v6::SceneContext_Load, &sceneContext );
#endif	

	const char* const title = "V6";

#if V6_USE_HMD
	if ( !v6::Hmd_Init() )
	{
		V6_ERROR( "Call to Hmd_Init failed!\n" );
		return -1;
	}

	v6::Vec2i renterTargerSize = v6::Hmd_GetRecommendedRenderTargetSize();
	v6::u32 maxRenderTargetSize = HLSL_GRID_WIDTH >> 1;
	if ( renterTargerSize.x > (int)maxRenderTargetSize )
	{
		renterTargerSize.y = (renterTargerSize.y * maxRenderTargetSize) / renterTargerSize.x;
		renterTargerSize.x = maxRenderTargetSize;
		
	}
	if ( renterTargerSize.y > (int)maxRenderTargetSize)
	{
		renterTargerSize.x = (renterTargerSize.x  * maxRenderTargetSize) / renterTargerSize.y;
		renterTargerSize.y = maxRenderTargetSize;
	}
	renterTargerSize.x = (renterTargerSize.x + 7) & ~7;
	renterTargerSize.y = (renterTargerSize.y + 7) & ~7;
#else
	v6::Vec2i renterTargerSize = v6::Vec2i_Make( HLSL_GRID_WIDTH >> 1, HLSL_GRID_WIDTH >> 1 );
#endif // #if V6_USE_HMD

	if ( !v6::Win_Create( &v6::s_win, title, 1920 - renterTargerSize.x, 48, renterTargerSize.x * HLSL_EYE_COUNT, renterTargerSize.y, true ) )
		return 1;

	v6::CRenderingDevice oRenderingDevice;
	if ( !oRenderingDevice.Create( renterTargerSize.x, renterTargerSize.y, &filesystem, &heap, &stack ) )
	{
		V6_ERROR( "Call to CRenderingDevice::Create failed!\n" );
		return -1;
	}

#if V6_USE_HMD
	if ( !v6::Hmd_CreateResources( v6::g_device, &renterTargerSize ) )
	{
		V6_ERROR( "Call to Hmd_CreateResources failed!\n" );
		return -1;
	}
#endif // #if V6_USE_HMD

#if V6_LOAD_EXTERNAL == 1
	SceneContext_SetDeviceReady( &sceneContext );
#endif

	v6::Win_Show( &v6::s_win, true );
	v6::Win_RegisterKeyEvent( &v6::s_win, v6::Viewer_OnKeyEvent );
	v6::Win_RegisterMouseEvent( &v6::s_win, v6::Viewer_OnMouseEvent );

	__int64 frameTickLast = GetTickCount(); 
	__int64 frameId = 0;
	while ( !Win_ProcessMessagesAndShouldQuit( &v6::s_win ) )
	{
		const v6::u32 bufferID = frameId & 1;

		v6::GPUQuery_s* pendingQueries = v6::GPUQueryContext_Get()->queries[bufferID];
		static const int tMaxCount = 64;
		static float dts[tMaxCount] = {};
		static float tfTimes[tMaxCount] = {};
		static float tbTimes[v6::QUERY_TIME_COUNT][tMaxCount] = {};
		static int tID = 0;
		
		if ( v6::GPUQuery_ReadTimeStampDisjoint( &pendingQueries[v6::QUERY_FREQUENCY] ) )
		{
			for ( v6::u32 queryID = v6::QUERY_FRAME_BEGIN; queryID < v6::QUERY_COUNT; ++queryID )
				v6::GPUQuery_ReadTimeStamp( &pendingQueries[queryID] );
			
			tfTimes[tID] = v6::GPUQuery_GetElpasedTime( &pendingQueries[v6::QUERY_FRAME_BEGIN], &pendingQueries[v6::QUERY_FRAME_END], &pendingQueries[v6::QUERY_FREQUENCY] );
			for ( v6::u32 timeID = 0; timeID < v6::QUERY_TIME_COUNT; ++timeID )
				tbTimes[timeID][tID] = v6::GPUQuery_GetElpasedTime( &pendingQueries[v6::QUERY_T0+timeID], &pendingQueries[v6::QUERY_T1+timeID], &pendingQueries[v6::QUERY_FREQUENCY] );
			tID = (tID + 1) & (tMaxCount-1);
		}
		
		v6::s_pendingQueries = pendingQueries;
		
		const __int64 frameTick = v6::GetTickCount();
		
		__int64 frameUpdatedTick = frameTick;
		float dt = 0.0f;
		for (;;)
		{
			const __int64 frameDelta = frameUpdatedTick - frameTickLast;
			dt = v6::Min( v6::ConvertTicksToSeconds( frameDelta ), 0.1f );
#if !V6_USE_HMD
			if ( dt + 0.0001f >= 1.0f / v6::HMD_FPS )
				break;
			SwitchToThread();
			frameUpdatedTick = v6::GetTickCount();
#endif // #if !V6_USE_HMD
		}
		dts[frameId & (tMaxCount-1)] = dt;

		frameTickLast = frameTick;

		if ( (frameId % 30) == 0 || v6::IsBakingMode( v6::g_drawMode ) ) 
		{
			float ifps = 0;
			float tfTime = 0;
			float tbTime[v6::QUERY_TIME_COUNT] = {};
				
			for ( int t = 0; t < tMaxCount; ++t )
			{
				ifps += dts[t];
				tfTime += tfTimes[t];
				for ( v6::u32 timeID = 0; timeID < v6::QUERY_TIME_COUNT; ++timeID )
					tbTime[timeID] += tbTimes[timeID][t];
			}

			ifps *= 1.0f / tMaxCount;
			tfTime *= 1.0f / tMaxCount;
			for ( v6::u32 timeID = 0; timeID < v6::QUERY_TIME_COUNT; ++timeID )
				tbTime[timeID] *= 1.0f / tMaxCount;

			char text[1024];
			sprintf_s( text, sizeof( text ), "%s | fps: %3u | tf: %4u | t0: %4u | t1: %4u | t2: %4u | t3: %4u | %s | Hmd %d %s", 
				title, 
				(int)(1.0f / ifps), 
				(int)(tfTime * 1000000.0f),
				(int)(tbTime[0] * 1000000.0f),
				(int)(tbTime[1] * 1000000.0f),
				(int)(tbTime[2] * 1000000.0f),
				(int)(tbTime[3] * 1000000.0f),
				v6::ModeToString( v6::g_drawMode ),
				v6::s_hmdState,
				v6::s_activeScene->info.dirty ? "| *" : "" );
			Win_SetTitle( &v6::s_win, text );
		}
		
		if ( v6::g_reloadShaders )
		{
			v6::GPUShaderContext_Release();
			v6::GPUShaderContext_Create();
			v6::GPUContext_CreateShaders( &filesystem, &stack );
			v6::g_reloadShaders = false;
		}

		v6::GPUQuery_BeginTimeStampDisjoint( &pendingQueries[v6::QUERY_FREQUENCY] );
		v6::GPUQuery_WriteTimeStamp( &pendingQueries[v6::QUERY_FRAME_BEGIN] );

		oRenderingDevice.Draw( dt );

		v6::GPUQuery_WriteTimeStamp( &pendingQueries[v6::QUERY_FRAME_END] );
		v6::GPUQuery_EndTimeStampDisjoint( &pendingQueries[v6::QUERY_FREQUENCY] );

		v6::GPUSurfaceContext_Present();

		++frameId;
	}

#if V6_USE_HMD
	v6::Hmd_ReleaseResources();

	v6::Hmd_Shutdown();
#endif // #if V6_USE_HMD

#if V6_LOAD_EXTERNAL == 1
	v6::Signal_Wait( &sceneContext.loadDone );
	SceneViewer_SaveInfo( v6::s_activeScene );
	SceneContext_Release( &sceneContext );
#else
	SceneViewer_SaveInfo( v6::s_activeScene );
#endif
	oRenderingDevice.Release();
}
