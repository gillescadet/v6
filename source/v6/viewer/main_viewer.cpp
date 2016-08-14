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
#include <v6/graphic/trace.h>
#include <v6/graphic/view.h>

#include <v6/viewer/scene_info.h>
#include <v6/viewer/viewer_shared.h>

#pragma comment( lib, "d3d11.lib" )

#define V6_D3D_DEBUG			0
#define V6_LOAD_EXTERNAL		1
#define V6_SIMPLE_SCENE			0
#define V6_USE_ALPHA_COVERAGE	1
#define V6_STEREO				0
#define V6_ENABLE_HMD			0
#define V6_USE_HMD				(V6_ENABLE_HMD == 1 && V6_STEREO == 1)
#define V6_USE_CACHE			0
#define V6_BENCH_CAPTURE		0

#if V6_USE_HMD == 1
#include <v6/graphic/hmd.h>
#endif // #if V6_USE_HMD == 1

BEGIN_V6_NAMESPACE

extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_deviceContext;

static const u32 GRID_MACRO_SHIFT				= 9;
static const u32 GRID_WIDTH						= 1 << (GRID_MACRO_SHIFT + 2);
static const u32 CUBE_SIZE						= GRID_WIDTH;
static const float GRID_MIN_SCALE				= 50.0f;
static const float GRID_MAX_SCALE				= 2000.0f;

static const u32 ANY_EYE						= 0;
#if V6_STEREO == 1
static const u32 LEFT_EYE						= 0;
static const u32 RIGHT_EYE						= 1;
static const u32 EYE_COUNT						= 2;
static const float IPD							= 6.5f;
#else
static const u32 LEFT_EYE						= 0;
static const u32 RIGHT_EYE						= 0;
static const u32 EYE_COUNT						= 1;
static const float IPD							= 0.0f;
#endif
static const float ZNEAR						= GRID_MIN_SCALE * 0.5f;
static const float ZFAR							= GRID_MAX_SCALE * 2.0f;
#if V6_SIMPLE_SCENE == 1
static const float FOV							= DegToRad( 90.0f );
#else
static const float FOV							= DegToRad( 90.0f );
#endif
static const u32 GRID_COUNT						= Codec_GetMipCount( GRID_MIN_SCALE, GRID_MAX_SCALE );
static const int SAMPLE_MAX_COUNT				= 17;
static const float FREE_SCALE					= 50.0f;
static const u32 RANDOM_CUBE_COUNT				= 100;

static const u32 HMD_FPS						= 75;
static const u32 VIDEO_FRAME_MAX_COUNT			= 10;
static const u32 VIDEO_FPS						= 75;

static const u32 DEBUG_BLOCK_MAX_COUNT			= HLSL_BLOCK_THREAD_GROUP_SIZE * 10;
static const u32 DEBUG_TRACE_MAX_COUNT			= HLSL_BLOCK_THREAD_GROUP_SIZE * 10;

static const u32 s_gpuEventCapture				= GPUEvent_Register( "Capture", false );
static const u32 s_gpuEventComposeSurface		= GPUEvent_Register( "Compose Surface", true );

static Win_s									s_win;
#if V6_USE_HMD
u32		s_hmdState								= HMD_TRACKING_STATE_OFF;
#else
u32		s_hmdState								= 0;
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
	CONSTANT_BUFFER_COMPOSE		=	hlsl::CBComposeSlot,

	CONSTANT_BUFFER_COUNT
};

enum
{
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
	MESH_SPHERE_RED,

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

struct SceneViewer_s : Scene_s
{
	char			filename[256];
	SceneInfo_s		info;
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
	IStack*			stack;
	ObjScene_s		objScene;
	Scene_s*		scene;
	Signal_s		loadDone;
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
static bool g_noTSAA				= false;
static bool g_showMip				= false;
static bool g_showBucket			= false; 
static bool g_showOverdraw			= false;
static bool g_showHistory			= false;
static int g_pixelMode				= 0;
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

static u32						s_activePath = PATH_CAMERA;
static Path_s					s_paths[PATH_COUNT];
static const float				s_defaultPathSpeed = 100.0f;
static PathPlayer_s				s_pathPlayer;

static GPURenderTargetSet_s		s_mainRenderTargetSet;

//----------------------------------------------------------------------------------------------------

void OutputMessage( const char * format, ... )
{
	char buffer[4096];
	va_list args;
	va_start( args, format );
	vsprintf_s( buffer, sizeof( buffer ), format, args);
	va_end( args );

	fputs( buffer, stdout );
}

//----------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------

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
		case 'J': g_noTSAA = keyEvent->pressed ? !g_noTSAA : g_noTSAA; break;
		case 'L': g_limit = keyEvent->pressed ? !g_limit : g_limit; break;
		case 'M': g_showMip = keyEvent->pressed ? !g_showMip : g_showMip; break;
		case 'N': g_showBucket = keyEvent->pressed ? !g_showBucket : g_showBucket; break;
		case 'O': g_showOverdraw = keyEvent->pressed ? !g_showOverdraw : g_showOverdraw; break;
		case 'P': g_pixelMode = keyEvent->pressed ? ((g_pixelMode+1)%6) : g_pixelMode; break;
		case 'R': if ( keyEvent->pressed ) { g_sample = 0; } break;
		case 'S': g_keyDownPressed = keyEvent->pressed; break;
		case 'U': if ( keyEvent->pressed ) { s_activeScene->info.dirty = false; }; break;
		case 'W': g_keyUpPressed = keyEvent->pressed; break;
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

//----------------------------------------------------------------------------------------------------

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

static void RenderingView_MakeForStereo( View_s* renderingView, const Vec3* org, const Vec3* forward, const Vec3* up, const Vec3* right, const u32 eye, float aspectRatio )
{
	const float tanHalfFOV = Tan( FOV * 0.5f );
	const Vec3 eyeOffset = *right * 0.5f * IPD;
	renderingView->org = *org + (eye == 0 ? -eyeOffset : eyeOffset);
	renderingView->forward = *forward;
	renderingView->right = *right;
	renderingView->up = *up;
	renderingView->viewMatrix = Mat4x4_View( &renderingView->org, right, up, forward );
	renderingView->projMatrix = Mat4x4_Projection( ZNEAR, tanHalfFOV, aspectRatio );
	renderingView->tanHalfFOVLeft = tanHalfFOV;
	renderingView->tanHalfFOVRight = renderingView->tanHalfFOVLeft;
	renderingView->tanHalfFOVUp = renderingView->tanHalfFOVLeft;
	renderingView->tanHalfFOVDown = renderingView->tanHalfFOVLeft;
}

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

static void Cube_MakeViewMatrix( Mat4x4* matrix, const Vec3& center, const Vec3 basis[3] )
{
	*matrix = Mat4x4_Rotation( basis[2], basis[1] );
	Mat4x4_SetTranslation( matrix, center );
	Mat4x4_AffineInverse( matrix );
}

static void GPUContext_CreateShaders( IStack* stack )
{
	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	static_assert( CONSTANT_BUFFER_COUNT <= GPUShaderContext_s::CONSTANT_BUFFER_MAX_COUNT, "Out of constant buffer" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC], sizeof( hlsl::CBBasic ), "basic" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_GENERIC], sizeof( hlsl::CBGeneric ), "generic" );
	GPUConstantBuffer_Create( &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE], sizeof( hlsl::CBCompose), "compose" );

	static_assert( COMPUTE_COUNT <= GPUShaderContext_s::COMPUTE_MAX_COUNT, "Out of compute" );
	GPUCompute_CreateFromFile( &shaderContext->computes[COMPUTE_COMPOSESURFACE], "surface_compose_cs.cso", stack );

	static_assert( SHADER_COUNT <= GPUShaderContext_s::SHADER_MAX_COUNT, "Out of shader" );
	GPUShader_Create( &shaderContext->shaders[SHADER_BASIC], "viewer_basic_vs.cso", "viewer_basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, stack );
	GPUShader_Create( &shaderContext->shaders[SHADER_FAKE_CUBE], "fake_cube_vs.cso", "fake_cube_ps.cso", 0, stack );
	GPUShader_Create( &shaderContext->shaders[SHADER_GENERIC], "generic_vs.cso", "generic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, stack );
	GPUShader_Create( &shaderContext->shaders[SHADER_GENERIC_ALPHA_TEST], "generic_vs.cso", "generic_alpha_test_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, stack );
}

static void GPUContext_Create( u32 width, u32 height, HWND hWnd, IAllocator* heap, IStack* stack )
{
	bool debug = false;
#if V6_D3D_DEBUG == 1
	debug = true;
#endif
	GPUDevice_CreateWithSurfaceContext( width * EYE_COUNT, height, hWnd, debug );
	
	GPURenderTargetSetCreationDesc_s renderTargetSetCreationDesc = {};
	renderTargetSetCreationDesc.name = "main";
	renderTargetSetCreationDesc.width = width;
	renderTargetSetCreationDesc.height = height;
	renderTargetSetCreationDesc.supportMSAA = true;
	renderTargetSetCreationDesc.bindable = true;
	renderTargetSetCreationDesc.writable = true;
	renderTargetSetCreationDesc.stereo = V6_STEREO;

	GPURenderTargetSet_Create( &s_mainRenderTargetSet, &renderTargetSetCreationDesc );
	
	GPUShaderContext_CreateEmpty();

	GPUContext_CreateShaders( stack );
}

static void GPUContext_Release()
{
	GPURenderTargetSet_Release( &s_mainRenderTargetSet );
	GPUDevice_Release();
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

	hlsl::CBBasic* cbBasic = (hlsl::CBBasic*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC] );

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

	hlsl::CBBasic* cbBasic = (hlsl::CBBasic*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_BASIC] );

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

	hlsl::CBGeneric* cbGeneric = (hlsl::CBGeneric*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_GENERIC] );

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
	
	g_deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &shaderContext->trilinearSamplerState );

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

static void SceneViewer_MakeStreamFilename( const SceneViewer_s* scene, char* path, u32 maxPathSize )
{
	V6_ASSERT( scene->filename[0] );

	FilePath_ChangeExtension( path, maxPathSize, scene->filename, "v6" );
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

static void SceneContext_Create( SceneContext_s* sceneContext, IStack* stack )
{
	stack->push();

	memset( sceneContext, 0, sizeof( SceneContext_s ) );
	sceneContext->stack = stack;
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
	Signal_Release( &sceneContext->loadDone );

	sceneContext->stack->pop();
}

static void SceneContext_Load( SceneContext_s* sceneContext, u32 arg0, u32 arg1 )
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

void Scene_CreateDefault( SceneViewer_s* scene, IStack* stack )
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
	
	GPUMesh_CreateSphere( &scene->meshes[MESH_SPHERE_RED], Color_Make( 255, 0, 0, 255 ), 16, stack );
	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_RED], Color_Make( 255, 0, 0, 255 ), false );
	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_GREEN], Color_Make( 0, 255, 0, 255 ), false );
	GPUMesh_CreateBox( &scene->meshes[MESH_BOX_BLUE], Color_Make( 0, 0, 255, 255 ), false );

	Material_Create( &scene->materials[MATERIAL_DEFAULT_BASIC], Material_DrawBasic );
	
	const u32 screenWidth = GRID_WIDTH >> 1;
	// const float depth = -99.0001f;
	const float depth = -100.0001f;
	const float pixelRadius = 0.5f * (200.0f / screenWidth);

#if 0
	for ( float y = -500.0f; y < 500.0f; y += 100.0f )
	{
		for ( float x = -500.0f; x < 500.0f; x += 100.0f )
			Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_SPHERE_RED, Vec3_Make( x * pixelRadius, y * pixelRadius, depth * 2.0f ), pixelRadius * 16 );
	}
#else
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_SPHERE_RED, Vec3_Make( 0.0f, 0.0f, depth ), pixelRadius * 16 );
#endif

	Path_Load( s_paths, PATH_COUNT, scene );
	s_yaw = DegToRad( info.cameraYaw );
	if ( s_paths[PATH_CAMERA].keyCount )
		s_headOffset = s_paths[PATH_CAMERA].positions[0];
}

#else

void Scene_CreateDefault( SceneViewer_s* scene, IStack* stack )
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

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
	bool BuildBlock( u32 frameID );
	void Capture_Render( GPURenderTargetSet_s* cubeFaceRenderTargetSet, const Vec3* sampleOffset, const Vec3 basis[3] );
	bool Create(int nWidth, int nHeight, IAllocator* heap, IStack* stack );
	void Draw( float dt );
	void DrawCameraPath( const View_s* view, GPURenderTargetSet_s* renderTargetSet, u32 eye );
	void DrawWorld( const View_s* view, GPURenderTargetSet_s* renderTargetSet, u32 eye );
	bool HasValidRawFrameFile( u32 frameID );
	bool InitTraceMode( u32 frameCount );
	void Output( GPURenderTargetSet_s* renderTargetSet );
	void Release();
	void ReleaseTraceMode();
	void ResetDrawMode();
	bool WriteRawFrameFile( CaptureContext_s* captureContext, u32 frame );

	CaptureContext_s	m_captureContext;
	TraceContext_s*		m_traceContext;
	VideoStream_s		m_stream;
	int					m_bakedFrameCount;

	SceneViewer_s*		m_defaultScene;
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

bool CRenderingDevice::Create( int nWidth, int nHeight, IAllocator* heap, IStack* stack )
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

	GPUContext_Create( nWidth, nHeight, (HWND)s_win.hWnd, heap, stack );

	m_defaultScene = heap->newInstance< SceneViewer_s >();
	Scene_CreateDefault( m_defaultScene, stack );
	s_activeScene = m_defaultScene;

	m_pathGeoScene = heap->newInstance< ScenePathGeo_s >();
	Scene_CreatePathGeo( m_pathGeoScene );

	g_sample = 0;

	GPUResource_LogMemoryUsage();

	return true;
}

void CRenderingDevice::DrawWorld( const View_s* view, GPURenderTargetSet_s* renderTargetSet, u32 eye )
{
	u32 flags = 0;

	GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
	renderTargetSetBindingDesc.clear = true;
	renderTargetSetBindingDesc.useMSAA = g_useMSAA;
#if V6_USE_ALPHA_COVERAGE == 1
	renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_ALPHA_COVERAGE;
	flags = RENDER_FLAGS_USE_ALPHA_COVERAGE;
#else
	renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_OPAQUE;
#endif

	GPURenderTargetSet_Bind( renderTargetSet, &renderTargetSetBindingDesc, eye );

	Scene_Draw( s_activeScene, view, flags );

	GPURenderTargetSet_Unbind( renderTargetSet );
}

void CRenderingDevice::DrawCameraPath( const View_s* view, GPURenderTargetSet_s* renderTargetSet, u32 eye )
{
	const GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};

	GPURenderTargetSet_Bind( renderTargetSet, &renderTargetSetBindingDesc, eye );

	Scene_Draw( m_pathGeoScene, view, 0 );

	GPURenderTargetSet_Unbind( renderTargetSet );
}

void CRenderingDevice::Capture_Render( GPURenderTargetSet_s* cubeFaceRenderTargetSet, const Vec3* samplePos, const Vec3 basis[3] )
{
	GPUEvent_Begin( s_gpuEventCapture );
		
	GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
	renderTargetSetBindingDesc.clear = true;

	GPURenderTargetSet_Bind( cubeFaceRenderTargetSet, &renderTargetSetBindingDesc, 0 );

	// View
	View_s view;
	Cube_MakeViewMatrix( &view.viewMatrix, *samplePos, basis );
	view.projMatrix = Mat4x4_Projection( ZNEAR, Tan( DegToRad( 45.0f ) ), 1.0f );

	Scene_Draw( s_activeScene, &view, RENDER_FLAGS_IS_CAPTURING );

	GPURenderTargetSet_Unbind( cubeFaceRenderTargetSet );

	GPUEvent_End();
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
	char streamFilename[256];

	SceneViewer_MakeRawFrameFileTemplate( s_activeScene, templateFilename, sizeof( templateFilename ) );
	SceneViewer_MakeStreamFilename( s_activeScene, streamFilename, sizeof( streamFilename ) );

#if V6_USE_CACHE == 1
	CodecStreamDesc_s codecDesc;
	if ( !VideoStream_LoadDesc( streamFilename, &codecDesc, m_stack ) ||
		codecDesc.sequenceCount != 1 ||
		codecDesc.frameCount != frameCount ||
		codecDesc.frameRate != HMD_FPS ||
		codecDesc.playRate != VIDEO_FPS ||
		codecDesc.sampleCount != SAMPLE_MAX_COUNT ||
		codecDesc.gridMacroShift != GRID_MACRO_SHIFT ||
		codecDesc.gridScaleMin != GRID_MIN_SCALE ||
		codecDesc.gridScaleMax != GRID_MAX_SCALE )
#endif // #if V6_USE_CACHE == 1
	{
		if ( !VideoStream_Encode( streamFilename, templateFilename, 0, frameCount, VIDEO_FPS, m_heap ) )
			return false;
	}
		
	if ( !VideoStream_Load( &m_stream, streamFilename, m_heap, m_stack ) )
		return false;
	
	m_traceContext = m_heap->newInstance< TraceContext_s >();

	TraceDesc_s traceDesc = {};
	traceDesc.screenWidth = m_width;
	traceDesc.screenHeight = m_height;
	traceDesc.stereo = V6_STEREO;

	TraceContext_Create( m_traceContext, &traceDesc, &m_stream );

	return true;
}

void CRenderingDevice::ReleaseTraceMode()
{
	TraceContext_Release( m_traceContext );
	VideoStream_Release( &m_stream, m_heap );

	m_heap->deleteInstance( m_traceContext );
}

void CRenderingDevice::Output( GPURenderTargetSet_s* renderTargetSet )
{
	GPUSurfaceContext_s* surfaceContext = GPUSurfaceContext_Get();

#if V6_STEREO == 1
	// Render

	GPUEvent_Begin( s_gpuEventComposeSurface );

	// set

	GPUShaderContext_s* shaderContext = GPUShaderContext_Get();

	g_deviceContext->CSSetConstantBuffers( hlsl::CBComposeSlot, 1, &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE].buf );

	g_deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, &renderTargetSet->colorBuffers[0].srv );
	g_deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, &renderTargetSet->colorBuffers[1].srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, &surfaceContext->surface.uav, nullptr );
		
	g_deviceContext->CSSetShader( shaderContext->computes[COMPUTE_COMPOSESURFACE].m_computeShader, nullptr, 0 );

	{
		hlsl::CBCompose* cbCompose = (hlsl::CBCompose*)GPUConstantBuffer_MapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE] );
		
		cbCompose->c_composeFrameWidth = m_width;

		GPUConstantBuffer_UnmapWrite( &shaderContext->constantBuffers[CONSTANT_BUFFER_COMPOSE]  );
	}		

	V6_ASSERT( (m_width & 0x7) == 0 );
	V6_ASSERT( (m_height & 0x7) == 0 );
	const u32 pixelGroupWidth = (m_width >> 3) * 2;
	const u32 pixelGroupHeight = m_height >> 3;
	g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();
#else	
	g_deviceContext->CopyResource( surfaceContext->surface.tex, renderTargetSet->colorBuffers[0].tex );
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

	if ( frameDesc.gridOrigin != s_buildOrigin )
	{
		V6_ERROR( "Stream origin is not compatible.\n" );
		return false;
	}

	if ( frameDesc.gridYaw != 0.0f )
	{
		V6_ERROR( "Stream yaw is not compatible.\n" );
		return false;
	}

	if ( frameDesc.frameID != frameID )
	{
		V6_ERROR( "Stream frame ID is not compatible.\n" );
		return false;
	}

	if ( frameDesc.frameRate != VIDEO_FPS )
	{
		V6_ERROR( "Stream frame rate is not compatible.\n" );
		return false;
	}

	if ( frameDesc.sampleCount != SAMPLE_MAX_COUNT )
	{
		V6_ERROR( "Stream sampleCount is not compatible.\n" );
		return false;
	}
	
	if ( frameDesc.gridMacroShift != GRID_MACRO_SHIFT )
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
			frameDesc.gridOrigin = s_buildOrigin;
			frameDesc.gridYaw = 0.0f;
			frameDesc.frameID = frameID;
			frameDesc.frameRate = VIDEO_FPS;
			frameDesc.sampleCount = SAMPLE_MAX_COUNT;
			frameDesc.gridMacroShift = GRID_MACRO_SHIFT;
			frameDesc.gridScaleMin = GRID_MIN_SCALE;
			frameDesc.gridScaleMax = GRID_MAX_SCALE;
			
			CodecRawFrameData_s frameData = {};

			{
				CaptureContext_MapBlocksForRead( captureContext, frameDesc.blockCounts, &frameData.blockPos, &frameData.blockData );
				Codec_WriteRawFrame( &fileWriter, &frameDesc, &frameData );
				CaptureContext_UnmapBlocksForRead( captureContext );
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
	static GPURenderTargetSet_s s_cubeFaceRenderTargetSet;

	static u32 lastSumLeafCount = 0;
	static u64 s_captureStartTickCount = 0;

	if ( g_sample == 0 )
	{
		s_captureStartTickCount = GetTickCount();

		s_buildOrigin = s_headOffset;

#if V6_USE_CACHE == 1
		if ( HasValidRawFrameFile( frameID ) )
		{
			g_sample = SAMPLE_MAX_COUNT;
			return true;
		}
#endif // #if V6_USE_CACHE == 1

		CaptureDesc_s captureDesc;
		captureDesc.sampleCount = SAMPLE_MAX_COUNT;
		captureDesc.gridMacroShift = GRID_MACRO_SHIFT;
		captureDesc.gridScaleMin = GRID_MIN_SCALE;
		captureDesc.gridScaleMax = GRID_MAX_SCALE;
		captureDesc.depthLinearScale = -1.0f / ZNEAR;
		captureDesc.depthLinearBias = 1.0f / ZNEAR;
		captureDesc.logReadBack = s_logReadBack;

		const u32 renderTargetSize = GRID_WIDTH;

		GPURenderTargetSetCreationDesc_s renderTargetSetCreationDesc = {};
		renderTargetSetCreationDesc.name = "cubeFace";
		renderTargetSetCreationDesc.width = renderTargetSize;
		renderTargetSetCreationDesc.height = renderTargetSize;
		renderTargetSetCreationDesc.supportMSAA = false;
		renderTargetSetCreationDesc.bindable = true;
		renderTargetSetCreationDesc.writable = false;
		
		GPURenderTargetSet_Create( &s_cubeFaceRenderTargetSet, &renderTargetSetCreationDesc );

		CaptureContext_Create( &m_captureContext, &captureDesc );

		lastSumLeafCount = 0;
		CaptureContext_Begin( &m_captureContext, &s_buildOrigin );
	}

	V6_MSG( "Capturing sample #%03d...", g_sample );

	const Vec3 samplePos = s_buildOrigin + CaptureContext_GetSampleOffset( &m_captureContext, g_sample );

	u32 sumLeafCount = 0;

	for ( u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		Vec3 basis[3];
		Cube_GetLookAt( basis[2], basis[1], (CubeAxis_e)faceID );
		basis[0] = Cross( basis[2], basis[1] );

		Capture_Render( &s_cubeFaceRenderTargetSet, &samplePos, basis );
		sumLeafCount += CaptureContext_AddSamplesFromCubeFace( &m_captureContext, &samplePos, basis, s_cubeFaceRenderTargetSet.colorBuffers[0].srv, s_cubeFaceRenderTargetSet.depthBuffer.srv );
	}

	const u32 newLeafCount = sumLeafCount - lastSumLeafCount;
	lastSumLeafCount = sumLeafCount;

	V6_MSG( "\r" );
	V6_MSG( "Captured  sample #%03d: %13s cells added\n", g_sample, String_FormatInteger( newLeafCount ) );

	++g_sample;

	if ( g_sample == SAMPLE_MAX_COUNT )
	{
		const u64 captureStopTickCount = GetTickCount();

		V6_MSG( "Packing all samples..." );

		CaptureContext_End( &m_captureContext );

		const u64 packingStopTickCount = GetTickCount();

		const bool written = WriteRawFrameFile( &m_captureContext, frameID );

		const u64 writeStopTickCount = GetTickCount();

		CaptureContext_Release( &m_captureContext );
		GPURenderTargetSet_Release( &s_cubeFaceRenderTargetSet );

		if ( !written )
			return false;

		V6_MSG( "\r" );
		V6_MSG( "Packed  all samples: %13s cells added @ frame %d\n", String_FormatInteger( sumLeafCount ), frameID );
		V6_MSG( "Durations: %d captures in %.1fs, packing in %.1fs, writing in %.1fs => total of %.1fs\n",
			SAMPLE_MAX_COUNT,
			ConvertTicksToSeconds( captureStopTickCount - s_captureStartTickCount ),
			ConvertTicksToSeconds( packingStopTickCount - captureStopTickCount ),
			ConvertTicksToSeconds( writeStopTickCount - packingStopTickCount ),
			ConvertTicksToSeconds( writeStopTickCount - s_captureStartTickCount ) );
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
	GPURenderTargetSet_s* renderTargetSet = nullptr;
#if V6_USE_HMD
	HmdRenderTarget_s hmdColorRenderTargets[2];
	HmdEyePose_s eyePoses[2];
	s_hmdState = Hmd_BeginRendering( hmdColorRenderTargets, eyePoses, ZNEAR, ZFAR );
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		const Mat4x4 yawMatrix = Mat4x4_RotationY( s_yaw );
		Mat4x4_Mul3x3( &orientationMatrix, yawMatrix, eyePoses[0].lookAt );
		
		static GPURenderTargetSet_s	s_hmdRenderTargetSet;
		s_hmdRenderTargetSet = s_mainRenderTargetSet;
		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
		{
			s_hmdRenderTargetSet.colorBuffers[eye].tex = (ID3D11Texture2D*)hmdColorRenderTargets[eye].tex;
			s_hmdRenderTargetSet.colorBuffers[eye].rtv = (ID3D11RenderTargetView*)hmdColorRenderTargets[eye].rtv;
			s_hmdRenderTargetSet.colorBuffers[eye].srv = (ID3D11ShaderResourceView*)hmdColorRenderTargets[eye].srv;
			s_hmdRenderTargetSet.colorBuffers[eye].uav = (ID3D11UnorderedAccessView*)hmdColorRenderTargets[eye].uav;
		}
		
		renderTargetSet = &s_hmdRenderTargetSet;
	}
	else
#endif // #if V6_USE_HMD
	{
		s_yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
		s_pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

		const Mat4x4 yawMatrix = Mat4x4_RotationY( s_yaw );
		const Mat4x4 pitchMatrix = Mat4x4_RotationX( s_pitch );
		Mat4x4_Mul( &orientationMatrix, yawMatrix, pitchMatrix );

		renderTargetSet = &s_mainRenderTargetSet;
	}

	Vec3 forward, right, up;
	orientationMatrix.GetZAxis( &forward );
	forward = -forward;
	orientationMatrix.GetXAxis( &right );
	orientationMatrix.GetYAxis( &up );

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

	View_s views[EYE_COUNT];
#if V6_USE_HMD
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
			Hmd_MakeView( &views[eye], &eyePoses[eye], &s_headOffset, s_yaw, eye );
	}
	else
#endif // #if V6_USE_HMD
	{
		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
			RenderingView_MakeForStereo( &views[eye], &s_headOffset, &forward, &up, &right, eye, m_aspectRatio );
	}

	if ( g_drawMode == DRAW_MODE_DEFAULT )
	{
		// draw mode

		for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
			DrawWorld( &views[eye], renderTargetSet, eye );
		
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
		}
		else
		{
			// trace mode

			if ( m_bakedFrameCount > 0 )
			{
#if V6_BENCH_CAPTURE == 0
				if ( InitTraceMode( m_bakedFrameCount ) )
					m_bakedFrameCount = -1;
				else
#endif // #if V6_BENCH_CAPTURE == 0
					ResetDrawMode();
			}

			if ( m_bakedFrameCount == -1 )
			{
				TraceContext_UpdateFrame( m_traceContext, frameID, m_stack );
				
				{
					TraceOptions_s options = {};
					options.logReadBack = s_logReadBack;
					options.showHistory = g_showHistory;
					options.showMip = g_showMip;
					options.showOverdraw = g_showOverdraw;
					options.noTSAA = g_noTSAA;
					TraceContext_DrawFrame( m_traceContext, renderTargetSet, views, &options );
				}

				s_logReadBack = false;
				g_mousePicked = false;

				PathPlayer_AddTimeStep( &s_pathPlayer, 1.0f / HMD_FPS );
			}
		}
	}	
	
#if V6_USE_HMD
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		Hmd_EndRendering();
	}
	else
#endif // #if V6_USE_HMD
	{
		if ( g_showPath )
		{
			for ( u32 eye = 0; eye < EYE_COUNT; ++eye )
				DrawCameraPath( &views[eye], renderTargetSet, eye );
		}
	}
	
	Output( renderTargetSet );
}

void CRenderingDevice::Release()
{
	if ( m_bakedFrameCount == -1 )
		ReleaseTraceMode();

	Scene_Release( m_defaultScene );
	m_heap->deleteInstance( m_defaultScene );

	Scene_Release( m_pathGeoScene );
	m_heap->deleteInstance( m_pathGeoScene );

	for ( u32 pathID = 0; pathID < PATH_COUNT; ++pathID )
		Path_Release( &s_paths[pathID] );

	GPUContext_Release();
}

END_V6_NAMESPACE

int main()
{
	V6_MSG( "Viewer 0.1\n" );

	v6::CHeap heap;
	v6::Stack stack( &heap, 200 * 1024 * 1024 );

#if V6_USE_HMD
	if ( !v6::Hmd_Init() )
	{
		V6_ERROR( "Call to Hmd_Init failed!\n" );
		return -1;
	}

	v6::Vec2 recommendedFOV = v6::Hmd_GetRecommendedFOV();
	v6::u32 renderTargetHalfSize = v6::GRID_WIDTH >> 2;
	v6::Vec2i renterTargerSize = v6::Vec2i_Make( (int)(renderTargetHalfSize * recommendedFOV.x), (int)(renderTargetHalfSize * recommendedFOV.y) );
	renterTargerSize.x = (renterTargerSize.x + 7) & ~7;
	renterTargerSize.y = (renterTargerSize.y + 7) & ~7;
#else
	const v6::Vec2i renterTargerSize = v6::Vec2i_Make( v6::GRID_WIDTH >> 1, v6::GRID_WIDTH >> 1 );
#endif // #if V6_USE_HMD

	const char* const title = "V6";

	if ( !v6::Win_Create( &v6::s_win, nullptr, title, 1920 - renterTargerSize.x * v6::EYE_COUNT, 48, renterTargerSize.x * v6::EYE_COUNT, renterTargerSize.y, true ) )
		return 1;

	v6::CRenderingDevice oRenderingDevice;
	if ( !oRenderingDevice.Create( renterTargerSize.x, renterTargerSize.y, &heap, &stack ) )
	{
		V6_ERROR( "Call to CRenderingDevice::Create failed!\n" );
		return -1;
	}

#if V6_USE_HMD
	if ( !v6::Hmd_CreateResources( v6::g_device, &renterTargerSize, false ) )
	{
		V6_ERROR( "Call to Hmd_CreateResources failed!\n" );
		return -1;
	}
#endif // #if V6_USE_HMD

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

	v6::Job_Launch( v6::SceneContext_Load, &sceneContext, 0, 0 );
#endif

	v6::Win_Show( &v6::s_win, true );
	v6::Win_RegisterKeyEvent( &v6::s_win, v6::Viewer_OnKeyEvent );
	v6::Win_RegisterMouseEvent( &v6::s_win, v6::Viewer_OnMouseEvent );

	__int64 frameTickLast = GetTickCount(); 
	__int64 frameId = 0;
	while ( !Win_ProcessMessagesAndShouldQuit( &v6::s_win ) )
	{
		v6::String_ResetInternalBuffer();

		static const int tMaxCount = 64;
		static float dts[tMaxCount] = {};
		
		const __int64 frameTick = v6::GetTickCount();
		
		__int64 frameUpdatedTick = frameTick;
		float dt = 0.0f;
		for (;;)
		{
			const __int64 frameDelta = frameUpdatedTick - frameTickLast;
			dt = v6::Min( v6::ConvertTicksToSeconds( frameDelta ), 0.1f );
#if !V6_USE_HMD
			if ( dt + 0.0001f >= 1.0f / v6::HMD_FPS )
#endif // #if !V6_USE_HMD
			{
				break;
			}
			SwitchToThread();
			frameUpdatedTick = v6::GetTickCount();
		}
		dts[frameId & (tMaxCount-1)] = dt;

		frameTickLast = frameTick;

		if ( (frameId % 30) == 0 || v6::IsBakingMode( v6::g_drawMode ) ) 
		{
			float ifps = 0;
				
			for ( int t = 0; t < tMaxCount; ++t )
				ifps += dts[t];

			ifps *= 1.0f / tMaxCount;

			char text[1024];
			sprintf_s( text, sizeof( text ), "%s | fps: %3u | %s | Hmd %d %s",
				title, 
				(int)(1.0f / ifps), 
				v6::ModeToString( v6::g_drawMode ),
				v6::s_hmdState,
				v6::s_activeScene->info.dirty ? "| *" : "" );
			Win_SetTitle( &v6::s_win, text );
		}
		
		if ( v6::g_reloadShaders )
		{
			v6::GPUShaderContext_Release();
			v6::GPUShaderContext_CreateEmpty();
			v6::GPUContext_CreateShaders( &stack );
			v6::g_reloadShaders = false;
		}

		v6::GPUEvent_BeginFrame( (v6::u32)frameId );

		oRenderingDevice.Draw( dt );

		v6::GPUEvent_EndFrame();

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
