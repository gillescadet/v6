/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#include <Windowsx.h>
#include <d3d11_1.h>
#pragma warning( pop )

#include <v6/viewer/common.h>
#include <v6/viewer/common_shared.h>

#include <v6/core/codec.h>
#include <v6/core/color.h>
#include <v6/core/decoder.h>
#include <v6/core/encoder.h>
#include <v6/core/filesystem.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/memory.h>
#include <v6/core/random.h>
#include <v6/core/stream.h>
#include <v6/core/time.h>
#include <v6/core/thread.h>
#include <v6/core/vec2.h>
#include <v6/core/vec2i.h>
#include <v6/core/vec3.h>
#include <v6/core/vec3i.h>

#include <v6/viewer/obj_reader.h>
#include <v6/viewer/scene_info.h>

#pragma comment( lib, "d3d11.lib" )

#define V6_GPU_PROFILING		1
#define V6_D3D_DEBUG			0
#define V6_LOAD_EXTERNAL		1
#define V6_SIMPLE_SCENE			0
#define V6_USE_ALPHA_COVERAGE	1
#define V6_ENABLE_HMD			1
#define V6_USE_HMD				(V6_ENABLE_HMD == 1 && HLSL_STEREO == 1)

#if V6_USE_HMD == 1
#include <v6/viewer/hmd.h>
#endif // #if HLSL_STEREO == 1

#define V6_ASSERT_D3D11( EXP )  { HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }
#define V6_RELEASE_D3D11( EXP )  { V6_ASSERT( EXP ); EXP->Release(); EXP = nullptr; }

BEGIN_V6_VIEWER_NAMESPACE

static const float AVERAGE_LAYER_COUNT			= 4.0f; // temp for low res
//static const float AVERAGE_LAYER_COUNT			= 2.0f;
#if HLSL_CELL_SUPER_SAMPLING_WIDTH > 1
static const float AVERAGE_SAMPLE_PER_PIXEL		= 0.25f * HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH;
#else
static const float AVERAGE_SAMPLE_PER_PIXEL		= 1.0f;
#endif
static const core::u32 CUBE_SIZE				= HLSL_GRID_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH;
static const float GRID_MAX_SCALE				= 2000.0f;
static const float GRID_MIN_SCALE				= 50.0f;

static const core::u32 ANY_EYE					= 0;
#if HLSL_STEREO == 1
static const core::u32 LEFT_EYE					= 0;
static const core::u32 RIGHT_EYE				= 1;
static const float IPD							= 6.5f;
#else
static const core::u32 LEFT_EYE					= 0;
static const core::u32 RIGHT_EYE				= 0;
static const float IPD							= 0.0f;
#endif
static const float ZNEAR						= GRID_MIN_SCALE * 0.5f;
static const float ZFAR							= 10000.0f;
#if V6_SIMPLE_SCENE == 1
static const float FOV							= core::DegToRad( 90.0f );
#else
static const float FOV							= core::DegToRad( 90.0f );
#endif
static const core::u32 GRID_COUNT				= core::Codec_GetMipCount( GRID_MIN_SCALE, GRID_MAX_SCALE );
static const int SAMPLE_MAX_COUNT				= 1;
static const float FREE_SCALE					= 50.0f;
static const core::u32 RANDOM_CUBE_COUNT		= 100;

static const core::u32 HMD_FPS					= 75;
static const core::u32 VIDEO_FRAME_MAX_COUNT	= 75 * 5;

static const core::u32 VERTEX_INPUT_MAX_COUNT	= 6;
static const core::u32 MESH_MAX_COUNT			= 16384;
static const core::u32 TEXTURE_MAX_COUNT		= 1024;
static const core::u32 MATERIAL_MAX_COUNT		= 256;
static const core::u32 ENTITY_MAX_COUNT			= 16384;

static const core::u32 ENTITY_TEXTURE_MAX_COUNT	= 4;
static const core::u32 ENTITY_TEXTURE_INVALID	= (core::u32)-1;

static const core::u32 DEBUG_BLOCK_MAX_COUNT	= HLSL_BLOCK_THREAD_GROUP_SIZE * 80;
static const core::u32 DEBUG_TRACE_MAX_COUNT	= HLSL_BLOCK_THREAD_GROUP_SIZE * 80;

#if V6_USE_HMD
v6::core::u32		s_hmdState				= v6::viewer::HMD_TRACKING_STATE_OFF;
#else
v6::core::u32		s_hmdState				= 0;
#endif // #if V6_USE_HMD

static POINT		s_mouseCursorPos		= {};

enum DrawMode_e
{
	DRAW_MODE_DEFAULT,	
	DRAW_MODE_BLOCK,
	
	DRAW_MODE_COUNT
};

enum
{
	VERTEX_FORMAT_POSITION		= 1 << 0,
	
	VERTEX_FORMAT_COLOR			= 1 << 1,
		
	VERTEX_FORMAT_USER0_SHIFT	= 2,
	VERTEX_FORMAT_USER0_MASK	= 7 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F1		= 1 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F2		= 2 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F3		= 3 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F4		= 4 << VERTEX_FORMAT_USER0_SHIFT,
	
	
	VERTEX_FORMAT_USER1_SHIFT	= 5,
	VERTEX_FORMAT_USER1_MASK	= 7 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F1		= 1 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F2		= 2 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F3		= 3 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F4		= 4 << VERTEX_FORMAT_USER1_SHIFT,

	VERTEX_FORMAT_USER2_SHIFT	= 8,
	VERTEX_FORMAT_USER2_MASK	= 7 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F1		= 1 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F2		= 2 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F3		= 3 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F4		= 4 << VERTEX_FORMAT_USER2_SHIFT,

	VERTEX_FORMAT_USER3_SHIFT	= 11,
	VERTEX_FORMAT_USER3_MASK	= 7 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F1		= 1 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F2		= 2 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F3		= 3 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F4		= 4 << VERTEX_FORMAT_USER3_SHIFT,
};

enum
{
	CONSTANT_BUFFER_BASIC		=	hlsl::CBBasicSlot,
	CONSTANT_BUFFER_GENERIC		=	hlsl::CBGenericSlot,
	CONSTANT_BUFFER_SAMPLE		=	hlsl::CBSampleSlot,
	CONSTANT_BUFFER_OCTREE		=	hlsl::CBOctreeSlot,
	CONSTANT_BUFFER_CULL		=	hlsl::CBCullSlot,
	CONSTANT_BUFFER_BLOCK		=	hlsl::CBBlockSlot,
	CONSTANT_BUFFER_PIXEL		=	hlsl::CBPixelSlot,
	CONSTANT_BUFFER_COMPOSE		=	hlsl::CBComposeSlot,

	CONSTANT_BUFFER_COUNT
};

enum
{
	COMPUTE_SAMPLECOLLECT,
	COMPUTE_BUILDINNER,
	COMPUTE_BUILDLEAF,
	COMPUTE_FILLLEAF,
	COMPUTE_PACKCOLOR,
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

struct Path_s
{
	static const core::u32	MAX_POINT_COUNT = 128;
	core::Vec3				positions[MAX_POINT_COUNT];
	int						keyCount;
	int						activeKey;
	core::u32				entityID;
	float					speed;
	bool					dirty;
};

struct PathPlayer_s
{
	Path_s*					paths;
	core::u32				pathCount;
	float					times[PATH_COUNT][Path_s::MAX_POINT_COUNT];
	float					durations[PATH_COUNT];
	float					durationMax;
	float					pathTime;
	core::u32				pathFrame;
	bool					isPlaying;
	bool					isPaused;
};

struct BasicVertex_s
{
	core::Vec3 position;
	core::Color_s color;
};

struct GenericVertex_s
{
	core::Vec3		position;
	core::Vec3		normal;
	core::Vec2		uv;
};

struct CubeVertex_s
{
	core::Vec3 pos;
	core::Vec2 uv;
};

enum GPUBufferCreationFlag_e
{
	GPUBUFFER_CREATION_FLAG_READ_BACK	= 1 << 0,
	GPUBUFFER_CREATION_FLAG_DYNAMIC		= 1 << 1,
};

struct GPUBuffer_s
{
	ID3D11Buffer*					buf;
	ID3D11Buffer*					staging;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
};

enum GPUTextureMipMapState_e
{
	GPUTEXTURE_MIPMAP_STATE_NONE,
	GPUTEXTURE_MIPMAP_STATE_REQUIRED,
	GPUTEXTURE_MIPMAP_STATE_GENERATED,
};

struct GPUTexture2D_s
{
	ID3D11Texture2D*				tex;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
	GPUTextureMipMapState_e			mipmapState;
};

struct GPUConstantBuffer_s
{
	ID3D11Buffer*					buf;
};

struct GPUCompute_s
{
	ID3D11ComputeShader* m_computeShader;
};

struct GPUShader_s
{
	ID3D11VertexShader* m_vertexShader;
	ID3D11PixelShader*	m_pixelShader;

	ID3D11InputLayout*	m_inputLayout;

	core::u32			m_vertexFormat;
};

struct GPUMesh_s
{
	ID3D11Buffer*		m_vertexBuffer;
	ID3D11Buffer*		m_indexBuffer;	
	core::u32			m_vertexCount;
	core::u32			m_vertexSize;
	core::u32			m_vertexFormat;
	core::u32			m_indexCount;
	core::u32			m_indexSize;
	D3D11_PRIMITIVE_TOPOLOGY m_topology;	
};

struct GPUQuery_s 
{
#if V6_GPU_PROFILING == 1
	ID3D11Query*	query;
#endif
	core::u64		data;
};

struct GPUContext_s
{
	IDXGISwapChain*				swapChain;
	ID3D11Device*				device;
	D3D_FEATURE_LEVEL			featureLevel;
	ID3D11DeviceContext*		deviceContext;
	ID3DUserDefinedAnnotation*	userDefinedAnnotation;
	ID3D11Texture2D*			surfaceBuffer;
	ID3D11RenderTargetView*		surfaceView;
	ID3D11UnorderedAccessView*	surfaceUAV;
	ID3D11Texture2D*			colorBuffers[HLSL_EYE_COUNT];
	ID3D11RenderTargetView*		colorViews[HLSL_EYE_COUNT];
	ID3D11ShaderResourceView*	colorSRVs[HLSL_EYE_COUNT];
	ID3D11UnorderedAccessView*	colorUAVs[HLSL_EYE_COUNT];
	ID3D11Texture2D*			depthStencilBuffer;
	ID3D11DepthStencilView*		depthStencilView;
	ID3D11Texture2D*			colorBufferMSAA;
	ID3D11RenderTargetView*		colorViewMSAA;
	ID3D11Texture2D*			depthStencilBufferMSAA;
	ID3D11DepthStencilView*		depthStencilViewMSAA;
	ID3D11DepthStencilState*	depthStencilStateNoZ;
	ID3D11DepthStencilState*	depthStencilStateZRO;
	ID3D11DepthStencilState*	depthStencilStateZRW;
	ID3D11BlendState*			blendStateNoColor;
	ID3D11BlendState*			blendStateOpaque;
	ID3D11BlendState*			blendStateAlphaCoverage;
	ID3D11BlendState*			blendStateAdditif;
	ID3D11RasterizerState*		rasterState;	
	ID3D11SamplerState*			samplerState;

	GPUConstantBuffer_s			constantBuffers[CONSTANT_BUFFER_COUNT];
	GPUCompute_s				computes[COMPUTE_COUNT];
	GPUShader_s					shaders[SHADER_COUNT];
	GPUQuery_s					queries[2][QUERY_COUNT];
	GPUQuery_s*					pendingQueries;
};

struct RenderingView_s
{
	ID3D11Texture2D*			texture2D;
	ID3D11RenderTargetView*		rtv;
	ID3D11UnorderedAccessView*	uav;
	core::Vec3					org;
	core::Vec3					forward;
	core::Vec3					right;
	core::Vec3					up;
	core::Mat4x4				viewMatrix;
	core::Mat4x4				projMatrix;
	core::u32					eye; // neeeded?
	float						tanHalfFOVLeft;
	float						tanHalfFOVRight;
	float						tanHalfFOVUp;
	float						tanHalfFOVDown;
};

struct RenderingSettings_s
{
	bool			isCapturing;
	bool			useAlphaCoverage;
};

struct Material_s;
struct Entity_s;
struct Scene_s;
typedef void (*MaterialDraw_f)( Material_s* material, Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings );

struct Material_s
{
	MaterialDraw_f	drawFunction;
	core::u32		textureIDs[ENTITY_TEXTURE_MAX_COUNT];
	core::Vec3		diffuse;
};

struct Entity_s
{
	char			name[64];
	core::u32		materialID;
	core::u32		meshID;
	core::Vec3		pos;
	float			scale;
	bool			visible;
};

struct Scene_s
{
	char			filename[256];
	SceneInfo_s		info;
	GPUMesh_s		meshes[MESH_MAX_COUNT];	
	GPUTexture2D_s	textures[TEXTURE_MAX_COUNT];
	Material_s		materials[MATERIAL_MAX_COUNT];
	Entity_s		entities[ENTITY_MAX_COUNT];
	core::u32		meshCount;
	core::u32		textureCount;
	core::u32		materialCount;
	core::u32		entityCount;
};

struct SceneDebug_s : Scene_s
{
	core::u32 meshLineID;
	core::u32 meshGridID;
	core::u32 meshCellIDs[HLSL_CELL_SUPER_SAMPLING_WIDTH_CUBE][2];
	core::u32 meshBlockIDs[DEBUG_BLOCK_MAX_COUNT][2];
	core::u32 entityLineID;
	core::u32 entityGridID;
	core::u32 entityCellIDs[HLSL_CELL_SUPER_SAMPLING_WIDTH_CUBE][2];	
	core::u32 entityBlockIDs[DEBUG_BLOCK_MAX_COUNT][2];
	core::u32 entityTraceIDs[DEBUG_BLOCK_MAX_COUNT];
};

struct ScenePathGeo_s : Scene_s
{
	core::u32 meshLineIDs[Path_s::MAX_POINT_COUNT-1];
	core::u32 meshBoxID;
	core::u32 meshSelectedBoxID;
	core::u32 entityLineIDs[Path_s::MAX_POINT_COUNT];
	core::u32 entityBoxIDs[Path_s::MAX_POINT_COUNT];
	core::u32 entitySelectedBoxID;
};

struct SceneContext_s
{
	char				filename[256];
	v6::core::IStack*	stack;
	ObjScene_s			objScene;
	Scene_s*			scene;
	ID3D11Device*		device;
	v6::core::Signal_s	deviceReady;
	v6::core::Signal_s	loadDone;
};

struct Config_s
{
	core::u32 screenWidth;
	core::u32 screenHeight;
	core::u32 sampleCount;
	core::u32 leafCount;
	core::u32 nodeCount;	
	core::u32 cellCount;
	core::u32 blockCount;
	core::u32 culledBlockCount;
	core::u32 cellItemCount;
};

struct CubeContext_s
{	
	ID3D11Texture2D* colorBuffer;	
	ID3D11ShaderResourceView* colorSRV;	
	ID3D11RenderTargetView* colorRTV;

	ID3D11Texture2D* depthBuffer;
	ID3D11ShaderResourceView* depthSRV;
	ID3D11DepthStencilView* depthRTV;

	core::u32 size;
};

struct SampleContext_s
{
	GPUBuffer_s					samples;
	GPUBuffer_s					indirectArgs;
};

struct OctreeContext_s
{
	GPUBuffer_s					sampleNodeOffsets;
	GPUBuffer_s					firstChildOffsets;
	ID3D11UnorderedAccessView*	firstChildOffsetsLimitedUAV;
	GPUBuffer_s					leaves;
	GPUBuffer_s					indirectArgs;
};

struct BlockContext_s
{
	GPUBuffer_s					blockPos;
	GPUBuffer_s					blockData;
	GPUBuffer_s					blockIndirectArgs;
};

struct SequenceContext_s
{	
	static const core::u32		GROUP_MAX_COUNT = 65536;

	core::CodecRange_s*			rangeDefs[CODEC_BUCKET_COUNT];
	hlsl::BlockRange			blockRanges[CODEC_BUCKET_COUNT][CODEC_RANGE_MAX_COUNT];
	core::u32					frameBlockPosOffsets[CODEC_FRAME_MAX_COUNT];
	core::u32					frameBlockDataOffsets[CODEC_FRAME_MAX_COUNT];
	GPUBuffer_s					blockPos;
	GPUBuffer_s					blockData;
	GPUBuffer_s					ranges[2];
	GPUBuffer_s					groups[2];
};

struct TraceContext_s
{
	GPUBuffer_s					traceCell;
	GPUBuffer_s					traceIndirectArgs;
	
	GPUBuffer_s					cellItems;
	GPUBuffer_s					cellItemCounters;

	GPUTexture2D_s				colors;

	GPUBuffer_s					cullStats;
	GPUBuffer_s					traceStats;
};

struct TraceData_s
{
	float						tIn;
	float						tOut;
	core::Vec3i					hitFoundCoords;
	core::u32					hitFailBits;
};

static float g_translation_speed			= 200.0f;
static bool g_mousePressed					= false;
static int g_mouseDeltaX					= 0;
static int g_mouseDeltaY					= 0;
static int g_mousePickPosX					= 0;
static int g_mousePickPosY					= 0;
static bool g_mousePicked					= false;
static core::u32 g_pickedPackedID			= (core::u32)-1;
static int g_keyLeftPressed					= false;	
static int g_keyRightPressed				= false;
static int g_keyUpPressed					= false;
static int g_keyDownPressed					= false;

static DrawMode_e g_drawMode				= DRAW_MODE_DEFAULT;

static int g_sample							= 0;

static bool g_keyPath						= false;
static bool g_showPath						= false;
static int g_limit							= false; 
static bool g_showMip						= false;
static bool g_showBucket					= false; 
static bool g_showOverdraw					= false;
static bool g_showHistory					= false;
static int g_pixelMode						= 0;
static bool g_randomBackground				= false;
static bool g_traceGrid						= false;
#if V6_SIMPLE_SCENE == 1
static bool g_useMSAA						= false;
#else
static bool g_useMSAA						= true;
#endif
static bool g_showObjects					= false;
static bool g_debugBlocks					= false;
static bool g_transparentDebug				= false;
static bool g_reloadShaders					= false;

static float s_yaw							= 0.0f;
static float s_pitch						= 0.0f;
static core::Vec3 s_headOffset				= core::Vec3_Zero();
static core::Vec3 s_buildOrigin				= core::Vec3_Zero();

static core::u32 gpuMemory					= 0;

static bool s_logReadBack					= false;

static Scene_s* s_activeScene				= nullptr;

static core::Vec3 s_gridCenter = {};
static float s_gridScale = 0;
static core::u32 s_gridOccupancy = 0;
static core::Vec3 s_rayOrg = {};
static core::Vec3 s_rayEnd = {};

static core::u32		s_activePath = PATH_CAMERA;
static Path_s			s_paths[PATH_COUNT];
static const float		s_defaultPathSpeed = 100.0f;
static PathPlayer_s		s_pathPlayer;

static core::u32 Scene_FindEntityByName( const Scene_s* scene, const char* entityName );

static void Path_Init( Path_s* path )
{
	memset( path, 0, sizeof( *path ) );
	path->entityID = (core::u32)-1;
	path->speed = s_defaultPathSpeed;
	path->dirty = true;
}

static void Path_Release( Path_s* path )
{
}

static void Path_SelectKey( Path_s* path, int key )
{
	path->activeKey = core::Clamp( key, 0, path->keyCount-1 );
	path->dirty = true;
}

static void Path_DeleteKey( Path_s* path, core::u32 key )
{
	V6_ASSERT( path->keyCount > 0 );
	V6_ASSERT( key < (core::u32)path->keyCount );
	for ( core::u32 keyNext = key+1; keyNext < (core::u32)path->keyCount; ++keyNext )
		path->positions[keyNext-1] = path->positions[keyNext];
	--path->keyCount;

	path->activeKey = core::Min( (int)key, path->keyCount );
	path->dirty = true;
}

static void Path_InsertKey( Path_s* path, const core::Vec3* position, core::u32 key )
{
	V6_ASSERT( path->keyCount < Path_s::MAX_POINT_COUNT );
	if ( key != 0 || path->keyCount != 0 )
	{
		++key;
		for ( core::u32 keyNext = path->keyCount; keyNext > key; --keyNext )
			path->positions[keyNext] = path->positions[keyNext-1];
	}
	V6_ASSERT( key <= (core::u32)path->keyCount );
	path->positions[key] = *position;
	++path->keyCount;

	path->activeKey = key;
	path->dirty = true;
}

static void Path_Load( Path_s* paths, core::u32 maxPathCount, const Scene_s* scene )
{
	V6_STATIC_ASSERT( SceneInfo_s::MAX_POSITION_COUNT == Path_s::MAX_POINT_COUNT );
	V6_ASSERT( maxPathCount <= SceneInfo_s::MAX_PATH_COUNT );
	const SceneInfo_s* sceneInfo = &scene->info;
	for ( core::u32 pathID = 0; pathID < maxPathCount; ++pathID )
	{
		for ( core::u32 positionID = 0; positionID < sceneInfo->paths[pathID].positionCount; ++positionID )
			paths[pathID].positions[positionID] = sceneInfo->paths[pathID].positions[positionID];
		paths[pathID].keyCount = sceneInfo->paths[pathID].positionCount;
		paths[pathID].entityID = Scene_FindEntityByName( scene, sceneInfo->paths[pathID].entityName );
		paths[pathID].speed = sceneInfo->paths[pathID].speed != 0.0f ? sceneInfo->paths[pathID].speed : s_defaultPathSpeed;
		paths[pathID].dirty = true;
	}
}

static void Path_Save( const Path_s* paths, core::u32 pathCount, Scene_s* scene )
{
	V6_ASSERT( pathCount <= SceneInfo_s::MAX_PATH_COUNT );
	SceneInfo_s* sceneInfo = &scene->info;
	for ( core::u32 pathID = 0; pathID < pathCount; ++pathID )
	{
		for ( core::u32 positionID = 0; positionID < (core::u32)paths[pathID].keyCount; ++positionID )
			sceneInfo->paths[pathID].positions[positionID] = paths[pathID].positions[positionID];
		sceneInfo->paths[pathID].positionCount = paths[pathID].keyCount;
		if ( paths[pathID].entityID != (core::u32)-1 )
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

static void PathPlayer_Compute( PathPlayer_s* pathPlayer, Path_s* paths, core::u32 pathCount )
{
	pathPlayer->paths = paths;
	pathPlayer->pathCount = pathCount;

	for ( core::u32 pathID = 0; pathID < pathCount; ++pathID )
	{
		Path_s* path = &paths[pathID];

		if ( !path->dirty )
			continue;

		if ( path->keyCount == 0 )
		{
			for ( core::u32 key = 0; key < (core::u32)path->keyCount; ++key )
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
			for ( core::u32 key = 1; key < (core::u32)path->keyCount; ++key )
			{
				const core::Vec3 delta = path->positions[key] - path->positions[key-1];
				const float interval = delta.Length() * invSpeed;
				V6_ASSERT( interval > 0.00001f );
				pathPlayer->times[pathID][key] = pathPlayer->times[pathID][key-1] + interval;
				pathPlayer->durations[pathID] += interval;
			}
		}

		path->dirty = false;
	}

	pathPlayer->durationMax = 0.0f;
	for ( core::u32 pathID = 0; pathID < pathCount; ++pathID )
		pathPlayer->durationMax = core::Max( pathPlayer->durationMax, pathPlayer->durations[pathID] );
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
	pathPlayer->pathFrame = core::Clamp( (int)pathPlayer->pathFrame + relativeFrameID, 0, (int)VIDEO_FRAME_MAX_COUNT-1 );
	pathPlayer->pathTime = pathPlayer->pathFrame * dt;
}

static core::u32 PathPlayer_GetFrame( PathPlayer_s* pathPlayer )
{
	if ( pathPlayer->isPlaying )
		return pathPlayer->pathFrame;
	return 0;
}

static bool PathPlayer_GetPosition( core::Vec3* position, const PathPlayer_s* pathPlayer, core::u32 pathID )
{
	V6_ASSERT( pathPlayer->paths != nullptr);
	V6_ASSERT( pathID < pathPlayer->pathCount );

	const Path_s* path = &pathPlayer->paths[pathID];

	V6_ASSERT( !path->dirty );

	if ( path->keyCount == 0 )
		return false;

	V6_ASSERT( pathPlayer->pathTime >= 0.0f );
	for ( core::u32 key = 1; key < (core::u32)path->keyCount; ++key )
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

static void Mouse_Capture( HWND hWnd )
{
	GetCursorPos( &s_mouseCursorPos );
	SetCapture( hWnd ) ;
	ShowCursor( false );
	g_mousePressed = true;
}

static void Mouse_Release()
{
	if ( g_mousePressed )
	{
		SetCursorPos( s_mouseCursorPos.x, s_mouseCursorPos.y );
		ShowCursor( true );
		ReleaseCapture();
		g_mousePressed = false;
	}
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
	case WM_ACTIVATE:
	case WM_KILLFOCUS:
	case WM_SETFOCUS:
		Mouse_Release();
		//V6_MSG( "WM_ACTIVATE: wParam=%08x lParam=%08x\n", (core::u32)wParam, (core::u32)lParam );
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		{
			
		}
		break;
#if 0
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		{
			g_mousePressed = message == WM_LBUTTONDOWN;
			g_mousePosX = GET_X_LPARAM( lParam ); 
			g_mousePosY = GET_Y_LPARAM( lParam );

			if ( g_mousePressed )
			{				
				SetCapture( hWnd ) ;
				ShowCursor( false );
			}
			else
			{
				ShowCursor( true );
				ReleaseCapture();
			}
		}
		break;
	case WM_RBUTTONDOWN:
		{
			g_mousePickPosX = GET_X_LPARAM( lParam ); 
			g_mousePickPosY = GET_Y_LPARAM( lParam );
			g_mousePicked = true;
			V6_MSG( "Pick %d, %d\n", g_mousePickPosX, g_mousePickPosY );
		}
		break;
	case WM_MOUSEMOVE:
		{
			if ( g_mousePressed )
			{
				g_mousePosX = GET_X_LPARAM( lParam ); 
				g_mousePosY = GET_Y_LPARAM( lParam );
			}
		}
		break;
#endif
	case WM_INPUT: 
	{		
		UINT dwSize;
		GetRawInputData( (HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
		
		LPBYTE lpb[4096];
		V6_ASSERT( dwSize <= sizeof( lpb ) );
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER) );

		RAWINPUT* raw = (RAWINPUT*)lpb;

		if ( raw->header.dwType == RIM_TYPEKEYBOARD ) 
		{
			const bool pressed = raw->data.keyboard.Message == 0x100;

			if ( g_keyPath )
			{
				if ( pressed )
				{
					switch( raw->data.keyboard.VKey )
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
						V6_MSG( "Unknow path key: %04x\n", raw->data.keyboard.VKey );
					}
				}
				else if ( raw->data.keyboard.VKey == 'C' )
				{
					g_keyPath = false;
				}
			}
			else
			{
				switch( raw->data.keyboard.VKey )
				{			
				case 0x1B:
					DestroyWindow( hWnd );
					break;
				case 'A': g_keyLeftPressed = pressed; break;
				case 'B': g_drawMode = pressed ? (g_drawMode == DRAW_MODE_BLOCK ? DRAW_MODE_DEFAULT : DRAW_MODE_BLOCK) : g_drawMode; break;
				case 'C': g_keyPath = pressed; break;
				case 'D': g_keyRightPressed = pressed; break;
				case 'E': g_useMSAA = pressed ? !g_useMSAA : g_useMSAA; break;
				case 'F': g_showObjects = pressed ? !g_showObjects : g_showObjects; break;
				case 'G': if ( pressed ) { g_debugBlocks = true; } break;
				case 'H': g_showHistory = pressed ? !g_showHistory: g_showHistory; break;
				case 'I': if ( pressed ) { s_logReadBack = true; } break;
				case 'L': g_limit = pressed ? !g_limit : g_limit; break;
				case 'M': g_showMip = pressed ? !g_showMip : g_showMip; break;
				case 'N': g_showBucket = pressed ? !g_showBucket : g_showBucket; break;
				case 'O': g_showOverdraw = pressed ? !g_showOverdraw : g_showOverdraw; break;
				case 'P': g_pixelMode = pressed ? ((g_pixelMode+1)%6) : g_pixelMode; break;
				case 'Q': if ( pressed ) { g_traceGrid = true; } break;
				case 'R': if ( pressed ) { g_sample = 0; } break;
				case 'S': g_keyDownPressed = pressed; break;
				case 'U': if ( pressed ) { s_activeScene->info.dirty = false; }; break;
				case 'W': g_keyUpPressed = pressed; break;
				case 'X': g_randomBackground = pressed ? !g_randomBackground : g_randomBackground; break;
				case 'Z': if ( pressed ) { g_reloadShaders = true; }; break;
				case ' ':
					if ( pressed ) 
					{
						s_yaw = core::DegToRad( s_activeScene->info.cameraYaw );
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
					if ( pressed ) 
					{
						const int key = core::Min( raw->data.keyboard.VKey - '0', s_paths[PATH_CAMERA].keyCount-1 );
						if ( s_paths[PATH_CAMERA].keyCount == 0 )
							s_headOffset = core::Vec3_Zero();
						else
							s_headOffset = s_paths[PATH_CAMERA].positions[key];
					}
					break;
				case 109:
					if ( pressed ) 
					{
						g_translation_speed *= 0.5f;
						V6_MSG( "Translation speed: %g\n", g_translation_speed );
					}
					break;
				case 107:
					if ( pressed ) 
					{
						g_translation_speed *= 2.0f;
						V6_MSG( "Translation speed: %g\n", g_translation_speed );
					}
					break;
				}
			}
#if 0
			V6_MSG( "Kbd: make=%04x Flags:%04x Reserved:%04x ExtraInformation:%08x, msg=%04x VK=%04x\n",
				raw->data.keyboard.MakeCode, 
				raw->data.keyboard.Flags, 
				raw->data.keyboard.Reserved, 
				raw->data.keyboard.ExtraInformation, 
				raw->data.keyboard.Message, 
				raw->data.keyboard.VKey );
#endif
		}
		else if ( raw->header.dwType == RIM_TYPEMOUSE ) 
		{
			
			if ( raw->data.mouse.ulButtons & 4 )
				Mouse_Capture( hWnd );
			else if ( raw->data.mouse.ulButtons & 8 )
				Mouse_Release();

			if ( g_mousePressed )
			{
				g_mouseDeltaX += raw->data.mouse.lLastX; 
				g_mouseDeltaY += raw->data.mouse.lLastY;
			}

#if 0
			V6_MSG( "Mouse: usFlags=%04x ulButtons=%04x usButtonFlags=%04x usButtonData=%04x ulRawButtons=%04x lLastX=%04x lLastY=%04x ulExtraInformation=%04x\n",
				raw->data.mouse.usFlags, 
				raw->data.mouse.ulButtons, 
				raw->data.mouse.usButtonFlags, 
				raw->data.mouse.usButtonData, 
				raw->data.mouse.ulRawButtons, 
				raw->data.mouse.lLastX, 
				raw->data.mouse.lLastY, 
				raw->data.mouse.ulExtraInformation );
#endif
		} 
		break;
	} 
	default:
		return DefWindowProcA(hWnd, message, wParam, lParam);
	}

	return 0;
}

static bool CaptureInputs( HWND hWnd )
{
	RAWINPUTDEVICE Rid[2];

	Rid[0].usUsagePage = 0x01; 
	Rid[0].usUsage = 0x02; 
	Rid[0].dwFlags = RIDEV_NOLEGACY;   // adds HID mouse and also ignores legacy mouse messages
	Rid[0].hwndTarget = hWnd;

	Rid[1].usUsagePage = 0x01; 
	Rid[1].usUsage = 0x06; 
	Rid[1].dwFlags = RIDEV_NOLEGACY;   // adds HID keyboard and also ignores legacy keyboard messages
	Rid[1].hwndTarget = hWnd;

	if ( RegisterRawInputDevices( Rid, 2, sizeof( Rid[0] ) ) == FALSE )
	{
		V6_ERROR( "Call to RegisterRawInputDevices failed!\n" );
		return false;
	}

	SetCursor( LoadCursor( NULL, IDC_ARROW ) );

	return true;
}

static HWND CreateMainWindow( const char * sTitle, int x, int y, int nWidth, int nHeight )
{
	WNDCLASSEXA wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = NULL;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "v6";
	wcex.hIconSm = NULL;

	if (!RegisterClassExA(&wcex))
	{
		V6_ERROR( "Call to RegisterClassEx failed!\n" );
		return 0;
	}

	const int style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU| WS_MINIMIZEBOX;
		
	RECT rect = { 0, 0, nWidth, nHeight };
	AdjustWindowRect( &rect, style, false );
	core::Vec2u dim = core::Vec2u_Make( rect.right - rect.left, rect.bottom - rect.top );

	HWND hWnd = CreateWindowA(
		"v6",
		sTitle,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		dim.x, dim.y,
		NULL,
		NULL,
		NULL,
		NULL
		);

	SetWindowPos( hWnd, nullptr, x - dim.x + nWidth, y - dim.y + nHeight, dim.x, dim.y, 0 );

	return hWnd;
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

static const char* FormatInteger_Unsafe( core::u32 n )
{
	static char buffer[10+3+1];
	char* s = buffer;
	if ( n > 1000000000 )
	{
		const core::u32 billion = n / 1000000000;
		s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%d,", billion );
		n -= billion * 1000000000;
	}
	if ( n > 1000000 )
	{
		const core::u32 million = n / 1000000;
		if ( s == buffer )
			s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%d,", million );
		else
			s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%03d,", million );
		n -= million * 1000000;
	}
	if ( n > 1000 )
	{
		const core::u32 thousand = n / 1000;
		if ( s == buffer )
			s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%d,", thousand );
		else
			s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%03d,", thousand );
		n -= thousand * 1000;
	}

	if ( s == buffer )
		s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%d", n );
	else
		s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%03d", n );

	*s = 0;

	return buffer;
}

static void RenderingView_MakeForStereo( RenderingView_s* renderingView, const core::Vec3* org, const core::Vec3* forward, const core::Vec3* up, const core::Vec3* right, const core::u32 eye, float aspectRatio )
{
	const core::Vec3 eyeOffset = *right * 0.5f * IPD;
	renderingView->texture2D = nullptr;
	renderingView->rtv = nullptr;
	renderingView->uav = nullptr;
	renderingView->org = *org + (eye == 0 ? -eyeOffset : eyeOffset);
	renderingView->forward = *forward;
	renderingView->right = *right;
	renderingView->up = *up;
	renderingView->viewMatrix = core::Mat4x4_View( &renderingView->org, forward, up, right );
	renderingView->projMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, FOV, aspectRatio );
	renderingView->eye = eye;
	renderingView->tanHalfFOVLeft = core::Tan( FOV * 0.5f );
	renderingView->tanHalfFOVRight = renderingView->tanHalfFOVLeft;
	renderingView->tanHalfFOVUp = renderingView->tanHalfFOVLeft;
	renderingView->tanHalfFOVDown = renderingView->tanHalfFOVLeft;
}

#if V6_USE_HMD

static void RenderingView_MakeForHMD( RenderingView_s* renderingView, const HmdEyePose_s* eyePose, const core::Vec3* orgOffset, float yawOffset, core::u32 eye )
{
	renderingView->texture2D = nullptr;
	renderingView->rtv = nullptr;
	renderingView->uav = nullptr;
	const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( s_yaw );
	core::Mat4x4_Mul( &renderingView->viewMatrix, yawMatrix, eyePose->lookAt );
	renderingView->org = *orgOffset + renderingView->viewMatrix.GetTranslation();
	core::Mat4x4_SetTranslation( &renderingView->viewMatrix, renderingView->org );
	core::Mat4x4_AffineInverse( &renderingView->viewMatrix );
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

static void Cube_GetLookAt( core::Vec3& lookAt, core::Vec3& up, CubeAxis_e axis )
{
	switch ( axis )
    {
        case CUBE_AXIS_POSITIVE_X:            
			lookAt  = core::Vec3_Make( 1.0f,  0.0f,  0.0f );
            up		= core::Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;								 	 	    
        case CUBE_AXIS_NEGATIVE_X:				 	 	    
            lookAt	= core::Vec3_Make( -1.0f , 0.0f, 0.0f );
            up		= core::Vec3_Make(  0.0f , 1.0f, 0.0f );
            break;								 	 	    
        case CUBE_AXIS_POSITIVE_Y:				 	 	    
            lookAt	= core::Vec3_Make( 0.0f,  1.0f,  0.0f );
            up		= core::Vec3_Make( 0.0f,  0.0f, -1.0f );
            break;						 		 	 	    
        case CUBE_AXIS_NEGATIVE_Y:		 		 	 	    
            lookAt	= core::Vec3_Make( 0.0f, -1.0f,  0.0f );
            up		= core::Vec3_Make( 0.0f,  0.0f,  1.0f );
            break;						 		 	 	    
        case CUBE_AXIS_POSITIVE_Z:		 		 	 	    
            lookAt	= core::Vec3_Make( 0.0f,  0.0f,  1.0f );
            up		= core::Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;						 		 	 	    
        case CUBE_AXIS_NEGATIVE_Z:		 		 	 	    
            lookAt	= core::Vec3_Make( 0.0f,  0.0f, -1.0f );
            up		= core::Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;
		default:
			V6_ASSERT_NOT_SUPPORTED();
    }
}

static void Cube_MakeViewMatrix( core::Mat4x4* matrix, const core::Vec3& center, CubeAxis_e axis )
{
	core::Vec3 lookAt;
	core::Vec3 up;
	Cube_GetLookAt( lookAt, up, axis );
	
	*matrix = Mat4x4_Rotation( lookAt, up );
	Mat4x4_SetTranslation( matrix, center );
	Mat4x4_AffineInverse( matrix );
}

static void GPUResource_LogMemory( const char* res, core::u32 size, const char* name )
{
	if ( core::DivMB( size ) >= 1 )
		V6_MSG( "%-16s %-30s: %8s MB\n", res, name, FormatInteger_Unsafe( core::DivMB( size ) ) );
	core::Atomic_Add( &gpuMemory, size );
}

static void GPUResource_LogMemoryUsage()
{
	V6_MSG( "%-16s %-30s: %8s MB\n", "GPU", "total", FormatInteger_Unsafe( core::DivMB( gpuMemory ) ) );
}

static core::u32 DXGIFormat_Size( DXGI_FORMAT format )
{
	switch( format )
	{	
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_SNORM:
		return 2;
	case DXGI_FORMAT_D32_FLOAT:		
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:	
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_TYPELESS:
		return 4;
	case DXGI_FORMAT_R32G32_FLOAT:
		return 8;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return 16;
	default:
		V6_ASSERT_NOT_SUPPORTED();
		return 0;
	}
}

static bool Compute_Create( ID3D11Device* device, GPUCompute_s* compute, const char* cs, core::CFileSystem* fileSystem, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	void* csBytecode = nullptr;
	const int csBytecodeSize = fileSystem->ReadFile( cs, &csBytecode, stack );
	if ( csBytecodeSize <= 0 )
	{
		V6_ERROR( "File %s not found!\n", cs );
		return false;
	}

	{
		HRESULT hRes = device->CreateComputeShader( csBytecode, csBytecodeSize, nullptr, &compute->m_computeShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateComputeShader failed!\n" );
			return false;
		}
	}	

	return true;
}

static void Compute_Release( GPUCompute_s* compute )
{	
	V6_RELEASE_D3D11( compute->m_computeShader );
}

static bool Shader_Create( ID3D11Device* device, GPUShader_s* shader, const char* vs, const char* ps, core::u32 vertexFormat, core::CFileSystem* fileSystem, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	void* vsBytecode = nullptr;
	const int vsBytecodeSize = fileSystem->ReadFile( vs, &vsBytecode, stack );
	if ( vsBytecodeSize <= 0 )
	{
		V6_ERROR( "Unable to read file %s!\n", vs );
		return false;
	}

	{
		HRESULT hRes = device->CreateVertexShader( vsBytecode, vsBytecodeSize, nullptr, &shader->m_vertexShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!\n" );
			return false;
		}
	}

	void* psBytecode = nullptr;
	const int psBytecodeSize = fileSystem->ReadFile( ps, &psBytecode, stack );
	if ( psBytecodeSize <= 0 )
	{
		V6_ERROR( "Unable to read file %s!\n", ps );
		return false;
	}

	{
		HRESULT hRes = device->CreatePixelShader( psBytecode, psBytecodeSize, nullptr, &shader->m_pixelShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreatePixelShader failed!\n" );
			return false;
		}
	}
	
	{
		D3D11_INPUT_ELEMENT_DESC idesc[VERTEX_INPUT_MAX_COUNT] = {};

		int stride = 0;
		int inputCount = 0;		
		
		if ( vertexFormat & VERTEX_FORMAT_POSITION )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "POSITION";
			idesc[inputCount].SemanticIndex = 0;
			idesc[inputCount].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 12;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_COLOR )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "COLOR";
			idesc[inputCount].SemanticIndex = 0;
			idesc[inputCount].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4;
			++inputCount;
		}

		const static DXGI_FORMAT widthToFloatFormats[] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };

		if ( vertexFormat & VERTEX_FORMAT_USER0_MASK )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 0;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER0_MASK ) >> VERTEX_FORMAT_USER0_SHIFT;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER1_MASK )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 1;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER1_MASK ) >> VERTEX_FORMAT_USER1_SHIFT;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER2_MASK )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 2;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER2_MASK ) >> VERTEX_FORMAT_USER2_SHIFT;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER3_MASK )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 3;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER3_MASK ) >> VERTEX_FORMAT_USER3_SHIFT;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4 * width;
			++inputCount;
		}

		HRESULT hRes = device->CreateInputLayout( idesc, inputCount, vsBytecode, vsBytecodeSize, &shader->m_inputLayout );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateInputLayout failed!\n" );
			return false;
		}

		shader->m_vertexFormat = vertexFormat;
	}

	return true;
}

static void Shader_Release( GPUShader_s* shader )
{
	V6_RELEASE_D3D11( shader->m_inputLayout );
	V6_RELEASE_D3D11( shader->m_vertexShader );
	V6_RELEASE_D3D11( shader->m_pixelShader );
}

static void ReadBack_Log( const char* res, core::u32 value, const char* name )
{
	V6_MSG( "%-16s %-30s: %10d\n", res, name, value );
}

static void ReadBack_Log( const char* res, core::hex32 value, const char* name )
{
	V6_MSG( "%-16s %-30s: 0x%08X\n", res, name, value.n );
}

static void ReadBack_Log( const char* res, float value, const char* name )
{
	V6_MSG( "%-16s %-30s: %g\n", res, name, value );
}

static void ReadBack_Log( const char* res, core::Vec2 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g)\n", res, name, value.x, value.y );
}

static void ReadBack_Log( const char* res, core::Vec3 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g, %g)\n", res, name, value.x, value.y, value.z );
}

static void ReadBack_Log( const char* res, core::Vec4 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g, %g, %g)\n", res, name, value.x, value.y, value.z, value.w );
}

static void ReadBack_Log( const char* res, core::Vec2u value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4u, %4u)\n", res, name, value.x, value.y );
}

static void ReadBack_Log( const char* res, core::Vec3u value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4u, %4u, %4u)\n", res, name, value.x, value.y, value.z );
}

static void ReadBack_Log( const char* res, core::Vec2i value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4d, %4d)\n", res, name, value.x, value.y );
}

static void ReadBack_Log( const char* res, core::Vec3i value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4d, %4d, %4d)\n", res, name, value.x, value.y, value.z );
}

static void ReadBack_Log( const char* res, core::Mat4x4 value, const char* name )
{
	V6_MSG( "%-16s %-30s:\n", res, name );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row0.x, value.m_row0.y, value.m_row0.z, value.m_row0.w );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row1.x, value.m_row1.y, value.m_row1.z, value.m_row1.w );	
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row2.x, value.m_row2.y, value.m_row2.z, value.m_row2.w );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row3.x, value.m_row3.y, value.m_row3.z, value.m_row3.w );
}

static void GPUBuffer_CreateIndirectArgs( ID3D11Device* device, GPUBuffer_s* buffer, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( core::u32 );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( core::u32 );
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

static void GPUBuffer_CreateIndirectArgsWithStaticData( ID3D11Device* device, GPUBuffer_s* buffer, const void* data, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( core::u32 );
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA dataDesc = {};
		dataDesc.pSysMem = data;
		dataDesc.SysMemPitch = 0;
		dataDesc.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, &dataDesc, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	V6_ASSERT( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) == 0 );

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}
}

static void GPUBuffer_CreateTyped( ID3D11Device* device, GPUBuffer_s* buffer, DXGI_FORMAT format, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) != 0 ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) == 0 )
			bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		bufferDesc.CPUAccessFlags = (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) != 0 ? D3D11_CPU_ACCESS_WRITE : 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) == 0 )
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

static void GPUBuffer_CreateTypedWithStaticData( ID3D11Device* device, GPUBuffer_s* buffer, const void* data, DXGI_FORMAT format, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA dataDesc = {};
		dataDesc.pSysMem = data;
		dataDesc.SysMemPitch = 0;
		dataDesc.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, &dataDesc, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	V6_ASSERT( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) == 0 );

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}
}

static void GPUBuffer_CreateStructured( ID3D11Device* device, GPUBuffer_s* buffer, core::u32 elementSize, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) != 0 ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) == 0 )
			bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		bufferDesc.CPUAccessFlags = (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) != 0 ? D3D11_CPU_ACCESS_WRITE : 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}
	
	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) == 0 )
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

static void GPUBuffer_CreateStructuredWithInitialData( ID3D11Device* device, GPUBuffer_s* buffer, const void* data, core::u32 elementSize, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;

		D3D11_SUBRESOURCE_DATA dataDesc = {};
		dataDesc.pSysMem = data;
		dataDesc.SysMemPitch = 0;
		dataDesc.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}
}

static void GPUBuffer_Release( ID3D11Device* device, GPUBuffer_s* buffer )
{
	V6_RELEASE_D3D11( buffer->buf );
	if ( buffer->staging )
		V6_RELEASE_D3D11( buffer->staging );
	V6_RELEASE_D3D11( buffer->srv );
	if ( buffer->uav )
		V6_RELEASE_D3D11( buffer->uav );
}

template < typename T >
static const T* GPUBuffer_MapReadBack( ID3D11DeviceContext* context, GPUBuffer_s* buffer )
{
	context->CopyResource( buffer->staging, buffer->buf );

	D3D11_MAPPED_SUBRESOURCE res;
	V6_ASSERT_D3D11( context->Map( buffer->staging, 0, D3D11_MAP_READ, 0, &res ) );
	return (T*)res.pData;
}

static void GPUBuffer_UnmapReadBack( ID3D11DeviceContext* context, GPUBuffer_s* buffer )
{
	context->Unmap( buffer->staging, 0 );
}

template < typename T >
static void GPUBuffer_Update( ID3D11DeviceContext* context, GPUBuffer_s* dstBuffer, core::u32 dstOffset, const T* srcData, core::u32 srcCount )
{
#if 0
	D3D11_BOX dstBox;
	dstBox.left = dstOffset * sizeof( T );
	dstBox.right = (dstOffset + srcCount) * sizeof( T );
	dstBox.front = 0;
	dstBox.back = 1;
	dstBox.top = 0;
	dstBox.bottom = 1;

	const core::u32 size = dstBox.right - dstBox.left;
	context->UpdateSubresource( dstBuffer->buf, 0, &dstBox, srcData, size, size );
#else
	D3D11_MAPPED_SUBRESOURCE res;
	V6_ASSERT_D3D11( context->Map( dstBuffer->buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &res ) );
	memcpy( (core::u8*)res.pData + dstOffset * sizeof( T ), srcData, srcCount * sizeof( T ) );
	context->Unmap( dstBuffer->buf, 0 );
#endif
}

static void Texture2D_Create( ID3D11Device* device, GPUTexture2D_s* tex, core::u32 width, core::u32 height, core::Color_s* pixels, bool mipmap, const char* name )
{
	mipmap = mipmap && core::IsPowOfTwo( width ) && core::IsPowOfTwo( height );

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = mipmap ? 0 : 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( mipmap )
			texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		if ( mipmap )
			texDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

		D3D11_SUBRESOURCE_DATA data[16] = {};
		void* mipPixels = nullptr;
		
		core::u32 pixelCount = 0;
		for ( core::u32 mip = 0; mip < 16; ++mip )
		{
			data[mip].pSysMem = pixels;
			data[mip].SysMemPitch = width * DXGIFormat_Size( texDesc.Format );
			data[mip].SysMemSlicePitch = width * height * DXGIFormat_Size( texDesc.Format );

			pixelCount += width * height;

			if ( !mipmap || (width == 1 && height == 1))
				break;

			if ( width > 1 )
				width >>= 1;

			if ( height > 1 )
				height >>= 1;
		}

		if ( pixelCount > width * height )
		
		V6_ASSERT( !mipmap || width == 1 );
		V6_ASSERT( !mipmap || height == 1 );
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, data, &tex->tex ) );

		GPUResource_LogMemory( "Texture2D", pixelCount * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = -1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( tex->tex, &srvDesc, &tex->srv ) );
	}
	
	tex->uav = nullptr;
	tex->mipmapState = mipmap ? GPUTEXTURE_MIPMAP_STATE_REQUIRED : GPUTEXTURE_MIPMAP_STATE_NONE;
}

static void Texture2D_CreateRW( ID3D11Device* device, GPUTexture2D_s* tex, core::u32 width, core::u32 height, const char* name )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &tex->tex ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;		
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( tex->tex, &srvDesc, &tex->srv ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;		
		uavDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( tex->tex, &uavDesc, &tex->uav ) );
	}

	tex->mipmapState = GPUTEXTURE_MIPMAP_STATE_NONE;
}

static void GPUTexture_Release( GPUTexture2D_s* tex )
{
	V6_RELEASE_D3D11( tex->tex );
	V6_RELEASE_D3D11( tex->srv );
	if ( tex->uav )
		V6_RELEASE_D3D11( tex->uav );
}

static void ConstantBuffer_Create( ID3D11Device* device, GPUConstantBuffer_s* buffer, core::u32 sizeOfStruct, const char* name )
{
	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = sizeOfStruct;
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufDesc.MiscFlags = 0;
	bufDesc.StructureByteStride = 0;

	V6_ASSERT_D3D11( device->CreateBuffer( &bufDesc, nullptr, &buffer->buf ) );
	GPUResource_LogMemory( "ConstantBuffer", bufDesc.ByteWidth, name );
}

static void ConstantBuffer_Release( GPUConstantBuffer_s* buffer )
{
	V6_RELEASE_D3D11( buffer->buf );
}

template < typename T >
static T* ConstantBuffer_MapWrite( ID3D11DeviceContext* context, GPUConstantBuffer_s* buffer )
{
	D3D11_MAPPED_SUBRESOURCE res;
	V6_ASSERT_D3D11( context->Map( buffer->buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );
	return (T*)res.pData;
}

static void ConstantBuffer_UnmapWrite( ID3D11DeviceContext* context, GPUConstantBuffer_s* buffer )
{
	context->Unmap( buffer->buf, 0 );
}

static void GPUQuery_CreateTimeStamp( ID3D11Device* device, GPUQuery_s* query )
{
	query->data = 0;

#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP;
    queryDesc.MiscFlags = 0;
	V6_ASSERT_D3D11( device->CreateQuery( &queryDesc, &query->query ) );
#endif
}

static void GPUQuery_CreateTimeStampDisjoint( ID3D11Device* device, GPUQuery_s* query )
{
	query->data = 0;

#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    queryDesc.MiscFlags = 0;
	V6_ASSERT_D3D11( device->CreateQuery( &queryDesc, &query->query ) );
#endif
}

static void GPUQuery_BeginTimeStampDisjoint( ID3D11DeviceContext* context, GPUQuery_s* query )
{
	V6_ASSERT( query->data != (core::u64)-1 );
	query->data = (core::u64)-1;
#if V6_GPU_PROFILING == 1
	context->Begin( query->query );
#endif
}

static void GPUQuery_EndTimeStampDisjoint( ID3D11DeviceContext* context, GPUQuery_s* query )
{
	V6_ASSERT( query->data == (core::u64)-1 );
	query->data = 0;
#if V6_GPU_PROFILING == 1
	context->End( query->query );
#endif
}

static bool GPUQuery_ReadTimeStampDisjoint( ID3D11DeviceContext* context, GPUQuery_s* query )
{
	V6_ASSERT( query->data != (core::u64)-1 );
#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampDisjoint = {};
	if ( context->GetData( query->query, &timestampDisjoint, sizeof( timestampDisjoint ), D3D11_ASYNC_GETDATA_DONOTFLUSH ) != S_OK )
		return false;
	if ( timestampDisjoint.Disjoint || timestampDisjoint.Frequency == 0 )
		return false;
	query->data = timestampDisjoint.Frequency;
	return true;
#else
	query->data = 0;
	return false;
#endif
	
}

static void GPUQuery_WriteTimeStamp( ID3D11DeviceContext* context, GPUQuery_s* query )
{
	query->data = 0;
#if V6_GPU_PROFILING == 1
	context->End( query->query );
#endif
}

static bool GPUQuery_ReadTimeStamp( ID3D11DeviceContext* context, GPUQuery_s* query )
{
#if V6_GPU_PROFILING == 1
	return context->GetData( query->query, &query->data, sizeof( query->data ), D3D11_ASYNC_GETDATA_DONOTFLUSH ) == S_OK;
#else
	return false;
#endif
}

static float GPUQuery_GetElpasedTime( const GPUQuery_s* queryStart, const GPUQuery_s* queryEnd, const GPUQuery_s* queryDisjoint )
{
	V6_ASSERT( queryStart->data != (core::u64)-1 );
	V6_ASSERT( queryEnd->data != (core::u64)-1 );
	V6_ASSERT( queryDisjoint->data != 0 );
	V6_ASSERT( queryDisjoint->data != (core::u64)-1 );
	return (float)(queryEnd->data - queryStart->data) / queryDisjoint->data;
}

static void GPUQuery_Release( GPUQuery_s* query )
{
#if V6_GPU_PROFILING == 1
	V6_RELEASE_D3D11( query->query );
#endif
}

static void GPUContext_CreateShaders( GPUContext_s* context, core::CFileSystem* fileSystem, core::IStack* stack )
{
	ID3D11Device* device = context->device;
	
	Compute_Create( device, &context->computes[COMPUTE_SAMPLECOLLECT], "sample_collect_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BUILDINNER], "octree_build_inner_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BUILDLEAF], "octree_build_leaf_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_FILLLEAF], "octree_fill_leaf_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_PACKCOLOR], "octree_pack_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL4], "block_cull_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL8], "block_cull_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL16], "block_cull_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL32], "block_cull_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL64], "block_cull_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS4], "block_cull_stats_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS8], "block_cull_stats_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS16], "block_cull_stats_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS32], "block_cull_stats_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS64], "block_cull_stats_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_INIT], "block_trace_init_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE4], "block_trace_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE8], "block_trace_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE16], "block_trace_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE32], "block_trace_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE64], "block_trace_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_DEBUG4], "block_trace_debug_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_DEBUG8], "block_trace_debug_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_DEBUG16], "block_trace_debug_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_DEBUG32], "block_trace_debug_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_DEBUG64], "block_trace_debug_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLENDPIXEL], "pixel_blend_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLENDPIXEL_OVERDRAW], "pixel_blend_overdraw_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_COMPOSESURFACE], "surface_compose_cs.cso", fileSystem, stack );

	Shader_Create( device, &context->shaders[SHADER_BASIC], "basic_vs.cso", "basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, fileSystem, stack );
	Shader_Create( device, &context->shaders[SHADER_FAKE_CUBE], "fake_cube_vs.cso", "fake_cube_ps.cso", 0, fileSystem, stack );
	Shader_Create( device, &context->shaders[SHADER_GENERIC], "generic_vs.cso", "generic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, fileSystem, stack );
	Shader_Create( device, &context->shaders[SHADER_GENERIC_ALPHA_TEST], "generic_vs.cso", "generic_alpha_test_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, fileSystem, stack );
}

static void GPUContext_ReleaseShaders( GPUContext_s* context )
{
	for ( core::u32 computeID = 0; computeID < COMPUTE_COUNT; ++computeID )
		Compute_Release( &context->computes[computeID] );

	for ( core::u32 shaderID = 0; shaderID < SHADER_COUNT; ++shaderID )
		Shader_Release( &context->shaders[shaderID] );
}

static void GPUContext_Create( GPUContext_s* context, core::u32 width, core::u32 height, HWND hWnd, core::CFileSystem* fileSystem, core::IAllocator* heap, core::IStack* stack )
{
	memset( context, 0, sizeof( *context ) );

	DXGI_SWAP_CHAIN_DESC oSwapChainDesc = {};

	DXGI_MODE_DESC & oModeDesc = oSwapChainDesc.BufferDesc;
	oModeDesc.Width = width * HLSL_EYE_COUNT;
	oModeDesc.Height = height;
	oModeDesc.RefreshRate.Numerator = 60;
	oModeDesc.RefreshRate.Denominator = 1;
	oModeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	oModeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	oModeDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	DXGI_SAMPLE_DESC & oSampleDesc = oSwapChainDesc.SampleDesc;
	oSampleDesc.Count = 1;
	oSampleDesc.Quality = 0;

	oSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
	oSwapChainDesc.BufferCount = 2;
	oSwapChainDesc.OutputWindow = hWnd;
	oSwapChainDesc.Windowed = true;
	oSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	oSwapChainDesc.Flags = 0;

	D3D_FEATURE_LEVEL pFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_1 };
	
#if V6_D3D_DEBUG == 1
	const core::u32 createFlags = D3D11_CREATE_DEVICE_DEBUG;		
#else
	const core::u32 createFlags = 0;		
#endif
	V6_ASSERT_D3D11( D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		createFlags | D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT,
		pFeatureLevels,
		1,
		D3D11_SDK_VERSION,
		&oSwapChainDesc,
		&context->swapChain,
		&context->device,
		&context->featureLevel,
		&context->deviceContext) );

	V6_ASSERT( context->featureLevel == D3D_FEATURE_LEVEL_11_1 );	

	ID3D11Device* device = context->device;
	ID3D11DeviceContext* deviceContext = context->deviceContext;

#if 0
	for ( core::u32 sampleCount = 1; sampleCount <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; sampleCount++)
	{
		core::u32 maxQualityLevel;
		HRESULT hr = m_device->CheckMultisampleQualityLevels( DXGI_FORMAT_R8G8B8A8_UNORM, MSAA_SAMPLE_QUALITY, &maxQualityLevel );
		
		if ( hr != S_OK )
			break;
		
		if ( maxQualityLevel > 0 )
			V6_MSG ("MSAA %dX supported with %d quality levels.\n", sampleCount, maxQualityLevel-1 );		
	}
#endif
	
	V6_ASSERT_D3D11( deviceContext->QueryInterface( IID_PPV_ARGS( &context->userDefinedAnnotation ) ) );

	V6_ASSERT_D3D11( context->swapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void **)&context->surfaceBuffer ) );

	V6_ASSERT_D3D11( device->CreateRenderTargetView( context->surfaceBuffer, 0, &context->surfaceView ) );
	V6_ASSERT_D3D11( device->CreateUnorderedAccessView( context->surfaceBuffer, 0, &context->surfaceUAV ) );
	
	for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
	{
		{
			D3D11_TEXTURE2D_DESC texDesc = {};
			texDesc.Width = width;
			texDesc.Height = height;
			texDesc.MipLevels = 1;
			texDesc.ArraySize = 1;
			texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Usage = D3D11_USAGE_DEFAULT;
			texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			texDesc.CPUAccessFlags = 0;
			texDesc.MiscFlags = 0;

			V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &context->colorBuffers[eye] ) );
			GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "mainColor" );
		}

		V6_ASSERT_D3D11( device->CreateRenderTargetView( context->colorBuffers[eye], 0, &context->colorViews[eye] ) );
		V6_ASSERT_D3D11( device->CreateShaderResourceView( context->colorBuffers[eye], 0, &context->colorSRVs[eye] ) );	
		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( context->colorBuffers[eye], 0, &context->colorUAVs[eye] ) );
	}

	{
		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_D32_FLOAT;	
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, 0, &context->depthStencilBuffer ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "mainDepth" );
	}

	V6_ASSERT_D3D11( device->CreateDepthStencilView( context->depthStencilBuffer, 0, &context->depthStencilView ) );

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 8;
		texDesc.SampleDesc.Quality = 16;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &context->colorBufferMSAA ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "mainColorMSAA" );
	}
		
	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

		V6_ASSERT_D3D11( device->CreateRenderTargetView( context->colorBufferMSAA, &viewDesc, &context->colorViewMSAA ) );
	}
	
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_TYPELESS;		
		texDesc.SampleDesc.Count = 8;
		texDesc.SampleDesc.Quality = 16;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &context->depthStencilBufferMSAA ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "mainDepthMSAA" );
	}
	
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		viewDesc.Flags = 0;

		V6_ASSERT_D3D11( device->CreateDepthStencilView( context->depthStencilBufferMSAA, &viewDesc, &context->depthStencilViewMSAA ) );
	}
		
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

		V6_ASSERT_D3D11( device->CreateDepthStencilState( &depthStencilDesc, &context->depthStencilStateNoZ ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( device->CreateDepthStencilState( &depthStencilDesc, &context->depthStencilStateZRO ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( device->CreateDepthStencilState( &depthStencilDesc, &context->depthStencilStateZRW ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = 0;
		
		V6_ASSERT_D3D11( device->CreateBlendState( &blendState, &context->blendStateNoColor ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( device->CreateBlendState( &blendState, &context->blendStateOpaque ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = true;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( device->CreateBlendState( &blendState, &context->blendStateAlphaCoverage ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = TRUE;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendState.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( device->CreateBlendState( &blendState, &context->blendStateAdditif ) );
	}

	{
		D3D11_RASTERIZER_DESC rasterDesc = {};
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FrontCounterClockwise = false;
		rasterDesc.DepthBias = 0;
		rasterDesc.DepthBiasClamp = 0;
		rasterDesc.SlopeScaledDepthBias = 0.0f;
		rasterDesc.DepthClipEnable = true;
		rasterDesc.ScissorEnable = false;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.AntialiasedLineEnable = false;
		
		V6_ASSERT_D3D11( device->CreateRasterizerState( &rasterDesc, &context->rasterState ) );
	}

	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = D3D11_REQ_MAXANISOTROPY;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0;
		samplerDesc.BorderColor[1] = 0;
		samplerDesc.BorderColor[2] = 0;
		samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		V6_ASSERT_D3D11( device->CreateSamplerState( &samplerDesc, &context->samplerState ) );		
	}

	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_BASIC], sizeof( v6::hlsl::CBBasic ), "basic" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_GENERIC], sizeof( v6::hlsl::CBGeneric ), "generic" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_SAMPLE], sizeof( v6::hlsl::CBSample ), "sample" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_OCTREE], sizeof( v6::hlsl::CBOctree ), "octreeContext" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_CULL], sizeof( v6::hlsl::CBCull ), "cull" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_BLOCK], sizeof( v6::hlsl::CBBlock ), "blockContext" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_PIXEL], sizeof( v6::hlsl::CBPixel), "pixel" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_COMPOSE], sizeof( v6::hlsl::CBCompose), "compose" );

	GPUContext_CreateShaders( context, fileSystem, stack );	

	for ( core::u32 bufferID = 0; bufferID < 2; ++bufferID )
	{
		for ( core::u32 queryID = 0; queryID < QUERY_COUNT; ++queryID )
		{
			if ( queryID == QUERY_FREQUENCY )
				GPUQuery_CreateTimeStampDisjoint( device, &context->queries[bufferID][queryID] );
			else
				GPUQuery_CreateTimeStamp( device, &context->queries[bufferID][queryID] );
		}
	}
}

void GPUContext_Release( GPUContext_s* context )
{
	context->deviceContext->ClearState();
	
	for ( core::u32 constantBufferID = 0; constantBufferID < CONSTANT_BUFFER_COUNT; ++constantBufferID )
	{
		if ( context->constantBuffers[constantBufferID ].buf )
			ConstantBuffer_Release( &context->constantBuffers[constantBufferID ] );
	}

	GPUContext_ReleaseShaders( context );

	for ( core::u32 bufferID = 0; bufferID < 2; ++bufferID )
	{
		for ( core::u32 queryID = 0; queryID < QUERY_COUNT; ++queryID )
			GPUQuery_Release( &context->queries[bufferID][queryID] );
	}
		
	V6_RELEASE_D3D11( context->surfaceBuffer );
	V6_RELEASE_D3D11( context->surfaceView );
	V6_RELEASE_D3D11( context->surfaceUAV );

	for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
	{
		V6_RELEASE_D3D11( context->colorBuffers[eye] );
		V6_RELEASE_D3D11( context->colorViews[eye] );
		V6_RELEASE_D3D11( context->colorSRVs[eye] );
		V6_RELEASE_D3D11( context->colorUAVs[eye] );
	}

	V6_RELEASE_D3D11( context->depthStencilBuffer );
	V6_RELEASE_D3D11( context->depthStencilView );

	V6_RELEASE_D3D11( context->colorBufferMSAA );
	V6_RELEASE_D3D11( context->colorViewMSAA );

	V6_RELEASE_D3D11( context->depthStencilBufferMSAA );
	V6_RELEASE_D3D11( context->depthStencilViewMSAA );

	V6_RELEASE_D3D11( context->swapChain );
	V6_RELEASE_D3D11( context->deviceContext );
	V6_RELEASE_D3D11( context->device );
}

static void Config_Init( Config_s* config, core::u32 screenWidth, core::u32 screenHeight, core::u32 cubeSize, core::u32 gridWidth )
{
	config->screenWidth = screenWidth;
	config->screenHeight = screenHeight;
	config->sampleCount = (core::u32)(cubeSize * cubeSize / AVERAGE_SAMPLE_PER_PIXEL);
	config->leafCount = (core::u32)(gridWidth * gridWidth * 6 * AVERAGE_LAYER_COUNT);
	config->nodeCount = config->leafCount * 3;	
	config->cellCount = config->leafCount * 33 / 16;
	config->blockCount = config->cellCount / 8;
	config->culledBlockCount = config->blockCount / 6;
	config->cellItemCount = (screenWidth * HLSL_EYE_COUNT * screenHeight) * HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT;
}

static void Config_Log( const Config_s* config )
{
	V6_MSG( "%-20s: %d\n", "config.screenWidth", config->screenWidth );
	V6_MSG( "%-20s: %d\n", "config.screenHeight", config->screenHeight );
	V6_MSG( "%-20s: %13s\n", "config.sample", FormatInteger_Unsafe( config->sampleCount ) );
	V6_MSG( "%-20s: %13s\n", "config.leaf", FormatInteger_Unsafe( config->leafCount ) );
	V6_MSG( "%-20s: %13s\n", "config.node", FormatInteger_Unsafe( config->nodeCount ) );
	V6_MSG( "%-20s: %13s\n", "config.cell", FormatInteger_Unsafe( config->cellCount ) );
	V6_MSG( "%-20s: %13s\n", "config.blockContext", FormatInteger_Unsafe( config->blockCount ) );
	V6_MSG( "%-20s: %13s\n", "config.culledBlock", FormatInteger_Unsafe( config->culledBlockCount ) );
	V6_MSG( "%-20s: %13s\n", "config.cellItem", FormatInteger_Unsafe( config->cellItemCount ) );
}

static void CubeContext_Create( ID3D11Device* device, CubeContext_s* cubeContext, core::u32 size )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cubeContext->colorBuffer) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "cubeColor" );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( cubeContext->colorBuffer, &viewDesc, &cubeContext->colorSRV ) );
	}

	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateRenderTargetView( cubeContext->colorBuffer, &viewDesc, &cubeContext->colorRTV ) );
	}

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_TYPELESS;		
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cubeContext->depthBuffer ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "cubeDepth" );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( cubeContext->depthBuffer, &viewDesc, &cubeContext->depthSRV ) );
	}

	{
		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		viewDesc.Flags = 0;
		viewDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateDepthStencilView( cubeContext->depthBuffer, &viewDesc, &cubeContext->depthRTV ) );
	}

	cubeContext->size = size;
}

static void CubeContext_Release( CubeContext_s* cubeContext )
{
	V6_RELEASE_D3D11( cubeContext->colorBuffer );
	V6_RELEASE_D3D11( cubeContext->depthBuffer );

	V6_RELEASE_D3D11( cubeContext->colorSRV );
	V6_RELEASE_D3D11( cubeContext->depthSRV );

	V6_RELEASE_D3D11( cubeContext->colorRTV );
	V6_RELEASE_D3D11( cubeContext->depthRTV );
}

static void SampleContext_Create( ID3D11Device* device, SampleContext_s* sampleContext, const Config_s* config )
{
	GPUBuffer_CreateStructured( device, &sampleContext->samples, sizeof( hlsl::Sample ), config->sampleCount, 0, "samples" );
	GPUBuffer_CreateIndirectArgs( device, &sampleContext->indirectArgs, sample_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "sampleIndirectArgs" );
}

static void SampleContext_Release( ID3D11Device* device, SampleContext_s* sampleContext )
{
	GPUBuffer_Release( device, &sampleContext->samples );
	GPUBuffer_Release( device, &sampleContext->indirectArgs );
}

static void OctreeContext_Create( ID3D11Device* device, OctreeContext_s* octreeContext, const Config_s* config )
{
	GPUBuffer_CreateTyped( device, &octreeContext->sampleNodeOffsets, DXGI_FORMAT_R32_UINT, config->sampleCount, 0, "octreeSampleNodeOffsets" );
	GPUBuffer_CreateTyped( device, &octreeContext->firstChildOffsets, DXGI_FORMAT_R32_UINT, config->nodeCount, 0, "octreeFirstChildOffsets" );
	
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = GRID_COUNT * 8;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( octreeContext->firstChildOffsets.buf, &uavDesc, &octreeContext->firstChildOffsetsLimitedUAV ) );
	}

	GPUBuffer_CreateStructured( device, &octreeContext->leaves, sizeof( hlsl::OctreeLeaf ), config->leafCount, 0, "octreeLeaves" );
	GPUBuffer_CreateIndirectArgs( device, &octreeContext->indirectArgs, octree_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "octreeIndirectArgs" );
}

static void OctreeContext_Release( ID3D11Device* device, OctreeContext_s* octreeContext )
{
	GPUBuffer_Release( device, &octreeContext->sampleNodeOffsets );
	GPUBuffer_Release( device, &octreeContext->firstChildOffsets );
	V6_RELEASE_D3D11( octreeContext->firstChildOffsetsLimitedUAV );
	GPUBuffer_Release( device, &octreeContext->leaves );
	GPUBuffer_Release( device, &octreeContext->indirectArgs );
}

static void BlockContext_Create( ID3D11Device* device, BlockContext_s* blockContext, const Config_s* config )
{
	GPUBuffer_CreateTyped( device, &blockContext->blockPos, DXGI_FORMAT_R32_UINT, config->blockCount, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockPositions" );
	GPUBuffer_CreateTyped( device, &blockContext->blockData, DXGI_FORMAT_R32_UINT, config->blockCount * 16, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockData" );
	GPUBuffer_CreateIndirectArgs( device, &blockContext->blockIndirectArgs, block_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockIndirectArgs" );
}

static void BlockContext_Release( ID3D11Device* device, BlockContext_s* blockContext )
{
	GPUBuffer_Release( device, &blockContext->blockPos );
	GPUBuffer_Release( device, &blockContext->blockData );
	GPUBuffer_Release( device, &blockContext->blockIndirectArgs );
}

static void SequenceContext_UpdateFrameData( ID3D11DeviceContext* context, SequenceContext_s* sequenceContext, core::u32 groupCounts[CODEC_BUCKET_COUNT], core::u32 frameID, const core::Sequence_s* sequence, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	core::Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
	float gridScale = sequence->desc.gridScaleMin;
	core::u32 gridMacroHalfWidth = 1 << (sequence->desc.gridMacroShift-1);
	for ( core::u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
		macroGridCoords[mip] = core::Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameID].origin, gridScale, gridMacroHalfWidth ); // patched per frame

	const core::u16* rangeIDs = sequence->frameDataArray[frameID].rangeIDs;
	hlsl::BlockRange* const blockRangeBuffer = stack->newArray< hlsl::BlockRange >( CODEC_BUCKET_COUNT * CODEC_RANGE_MAX_COUNT );
	hlsl::BlockRange* blockRanges = blockRangeBuffer;
	core::u32 blockGroupBuffer[SequenceContext_s::GROUP_MAX_COUNT];
	core::u32* blockGroups = blockGroupBuffer;
	core::u32 framePosCount = 0;
	core::u32 frameDataCount = 0;
	core::u32 frameBlockRangeCount = 0;
	core::u32 frameGroupCount = 0;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const core::u32 cellPerBucketCount = 1 << (bucket + 2);
		const core::u32 bucketBlockCount = sequence->frameDescArray[frameID].blockCounts[bucket];
		const core::u32 bucketBlockRangeCount = sequence->frameDescArray[frameID].blockRangeCounts[bucket];
		core::u32 firstThreadID = 0;
		core::u32 bucketGroupCount = 0;

		for ( core::u32 rangeRank = 0; rangeRank < bucketBlockRangeCount; ++rangeRank )
		{
			const core::u32 rangeID = rangeIDs[rangeRank];

			const core::CodecRange_s* codecRange = &sequenceContext->rangeDefs[bucket][rangeID];
			const core::u32 mip = (codecRange->frameID8_mip4_blockCount20 >> 20) & 0xF;
			const hlsl::BlockRange* srcBlockRange = &sequenceContext->blockRanges[bucket][rangeID];
			
			hlsl::BlockRange* dstBlockRange = &blockRanges[rangeRank];
			memcpy( dstBlockRange, srcBlockRange, sizeof( hlsl::BlockRange ) );
			dstBlockRange->macroGridOffset -= macroGridCoords[mip];
			dstBlockRange->firstThreadID = firstThreadID;

			const core::u32 blockCount = srcBlockRange->blockCount;
			const core::u32 groupCount = GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );

			for ( core::u32 groupRank = 0; groupRank < groupCount; ++groupRank )
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

	core::u32 bufferID = frameID & 1;
	GPUBuffer_Update( context, &sequenceContext->ranges[bufferID], 0, blockRangeBuffer, frameBlockRangeCount );
	GPUBuffer_Update( context, &sequenceContext->groups[bufferID], 0, blockGroupBuffer, frameGroupCount );
	GPUBuffer_Update( context, &sequenceContext->blockPos, sequenceContext->frameBlockPosOffsets[frameID], sequence->frameDataArray[frameID].blockPos, framePosCount );
	GPUBuffer_Update( context, &sequenceContext->blockData, sequenceContext->frameBlockDataOffsets[frameID], sequence->frameDataArray[frameID].blockData, frameDataCount );
}

static void SequenceContext_CreateFromData( ID3D11Device* device, SequenceContext_s* sequenceContext, const core::Sequence_s* sequence )
{
	{
		core::u32 rangeDefOffset = 0;
		for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			sequenceContext->rangeDefs[bucket] = sequence->data.rangeDefs + rangeDefOffset;
			rangeDefOffset += sequence->desc.rangeDefCounts[bucket];
		}
	}

	core::u32 nextRangeIDs[CODEC_BUCKET_COUNT] = {};
	core::u32 blockPosCount = 0;
	core::u32 blockDataCount = 0;
	for ( core::u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
	{
		sequenceContext->frameBlockPosOffsets[frameID] = blockPosCount;
		sequenceContext->frameBlockDataOffsets[frameID] = blockDataCount;

		core::Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
		float gridScale = sequence->desc.gridScaleMin;
		core::u32 gridMacroHalfWidth = 1 << (sequence->desc.gridMacroShift-1);
		for ( core::u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
			macroGridCoords[mip] = core::Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameID].origin, gridScale, gridMacroHalfWidth ); // patched per frame
		
		for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; )
		{
			const core::u32 rangeID = nextRangeIDs[bucket];

			if ( rangeID == sequence->desc.rangeDefCounts[bucket] )
			{
				++bucket;
				continue;
			}
			
			const core::CodecRange_s* codecRange = &sequenceContext->rangeDefs[bucket][rangeID];
			core::u32 rangeFrameID = codecRange->frameID8_mip4_blockCount20 >> 24;
			if ( frameID != rangeFrameID )
			{
				++bucket;
				continue;
			}

			const core::u32 blockCount = codecRange->frameID8_mip4_blockCount20 & 0xFFFFF;
			const core::u32 mip = (codecRange->frameID8_mip4_blockCount20 >> 20) & 0xF;

			hlsl::BlockRange* blockRange = &sequenceContext->blockRanges[bucket][rangeID];
			
			blockRange->macroGridOffset = macroGridCoords[mip]; // patched per frame
			blockRange->firstThreadID = 0; // patched per frame
			blockRange->blockCount = blockCount;
			blockRange->blockPosOffset = blockPosCount;
			blockRange->blockDataOffset = blockDataCount;

			const core::u32 cellPerBucketCount = 1 << (2 + bucket);
			blockPosCount += blockCount;
			blockDataCount += blockCount * cellPerBucketCount;

			++nextRangeIDs[bucket];
		}
	}

	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		V6_ASSERT( nextRangeIDs[bucket] == sequence->desc.rangeDefCounts[bucket] );

	core::u32 maxBlockRangeCount = 0;
	core::u32 maxBlockGroupCount = 0;

	for ( core::u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
	{
		const core::u16* rangeIDs = sequence->frameDataArray[frameID].rangeIDs;

		core::u32 frameBlockRangeCount = 0;
		core::u32 frameBlockGroupCount = 0;
		for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			core::u32 bucketBlockRangeCount = sequence->frameDescArray[frameID].blockRangeCounts[bucket];

			for ( core::u32 rangeRank = 0; rangeRank < bucketBlockRangeCount; ++rangeRank )
			{
				const core::u32 rangeID = rangeIDs[rangeRank];
				core::u32 blockCount = sequenceContext->rangeDefs[bucket][rangeID].frameID8_mip4_blockCount20 & 0xFFFFF;
				frameBlockGroupCount += GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
			}

			rangeIDs += bucketBlockRangeCount;
			frameBlockRangeCount += bucketBlockRangeCount;
		}
		maxBlockRangeCount = core::Max( maxBlockRangeCount, frameBlockRangeCount );
		maxBlockGroupCount = core::Max( maxBlockGroupCount, frameBlockGroupCount );
	}

	blockPosCount = core::Max( 1u, blockPosCount );
	blockDataCount = core::Max( 1u, blockDataCount );
	maxBlockRangeCount = core::Max( 1u, maxBlockRangeCount );
	maxBlockGroupCount = core::Max( 1u, maxBlockGroupCount );

	V6_ASSERT( maxBlockGroupCount <= SequenceContext_s::GROUP_MAX_COUNT );

	GPUBuffer_CreateTyped( device, &sequenceContext->blockPos, DXGI_FORMAT_R32_UINT, blockPosCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockPositions" );
	GPUBuffer_CreateTyped( device, &sequenceContext->blockData, DXGI_FORMAT_R32_UINT, blockDataCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockData" );
	GPUBuffer_CreateStructured( device, &sequenceContext->ranges[0], sizeof( hlsl::BlockRange ), maxBlockRangeCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockRanges0" );
	GPUBuffer_CreateStructured( device, &sequenceContext->ranges[1], sizeof( hlsl::BlockRange ), maxBlockRangeCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockRanges1" );
	GPUBuffer_CreateTyped( device, &sequenceContext->groups[0], DXGI_FORMAT_R32_UINT, maxBlockGroupCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockGroups0" );
	GPUBuffer_CreateTyped( device, &sequenceContext->groups[1], DXGI_FORMAT_R32_UINT, maxBlockGroupCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "sequenceBlockGroups1" );
}

static void SequenceContext_Release( ID3D11Device* device, SequenceContext_s* sequenceContext )
{
	GPUBuffer_Release( device, &sequenceContext->blockPos );
	GPUBuffer_Release( device, &sequenceContext->blockData );
	GPUBuffer_Release( device, &sequenceContext->ranges[0] );
	GPUBuffer_Release( device, &sequenceContext->ranges[1] );
	GPUBuffer_Release( device, &sequenceContext->groups[0] );
	GPUBuffer_Release( device, &sequenceContext->groups[1] );
}

static void TraceContext_Create( ID3D11Device* device, TraceContext_s* traceContext, const Config_s* config )
{
	GPUBuffer_CreateTyped( device, &traceContext->traceCell, DXGI_FORMAT_R32_UINT, config->culledBlockCount * 2, 0, "traceCell" );
	GPUBuffer_CreateIndirectArgs( device, &traceContext->traceIndirectArgs, trace_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "traceIndirectArgs" );

	GPUBuffer_CreateStructured( device, &traceContext->cellItems, sizeof( hlsl::BlockCellItem ), config->cellItemCount, 0, "blockCellItems" );
	GPUBuffer_CreateTyped( device, &traceContext->cellItemCounters, DXGI_FORMAT_R32_UINT, config->screenWidth * HLSL_EYE_COUNT * config->screenHeight, 0, "blockCellItemCounters" );	
	
	GPUBuffer_CreateStructured( device, &traceContext->cullStats, sizeof( hlsl::BlockCullStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockCullStats" );
	GPUBuffer_CreateStructured( device, &traceContext->traceStats, sizeof( hlsl::BlockTraceStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockTraceStats" );

	Texture2D_CreateRW( device, &traceContext->colors, config->screenWidth, config->screenHeight, "pixelColors" );
}

static void TraceContext_Release( ID3D11Device* device, TraceContext_s* traceContext )
{
	GPUBuffer_Release( device, &traceContext->traceCell );
	GPUBuffer_Release( device, &traceContext->traceIndirectArgs );

	GPUBuffer_Release( device, &traceContext->cellItems );
	GPUBuffer_Release( device, &traceContext->cellItemCounters );

	GPUBuffer_Release( device, &traceContext->cullStats );
	GPUBuffer_Release( device, &traceContext->traceStats );
	
	GPUTexture_Release( &traceContext->colors );
}

static void Mesh_UpdateVertices( ID3D11DeviceContext* context, GPUMesh_s* mesh, const void* vertices )
{
	V6_ASSERT( mesh->m_vertexBuffer );

	D3D11_BOX box;
	box.left = 0;
	box.right = mesh->m_vertexCount * mesh->m_vertexSize;
	box.front = 0;
	box.back = 1;
	box.top = 0;
	box.bottom = 1;
		
	context->UpdateSubresource( mesh->m_vertexBuffer, 0, &box, vertices, box.right, box.right );
}

static void Mesh_Create( ID3D11Device* device, GPUMesh_s* mesh, const void* vertices, core::u32 vertexCount, core::u32 vertexSize, core::u32 vertexFormat, const void* indices, core::u32 indexCount, core::u32 indexSize, D3D11_PRIMITIVE_TOPOLOGY topology )
{
	mesh->m_vertexBuffer = nullptr;
	mesh->m_vertexCount = vertexCount;
	mesh->m_vertexSize = 0;
	mesh->m_vertexFormat = 0;
	if ( vertexCount > 0 && vertexSize > 0 && vertices != nullptr )
	{	
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = vertexSize * vertexCount;
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = vertices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufDesc, &data, &mesh->m_vertexBuffer ) );
		GPUResource_LogMemory( "VertexBuffer", bufDesc.ByteWidth, "mesh" );

		mesh->m_vertexSize = vertexSize;
		mesh->m_vertexFormat = vertexFormat;
	}
	
	mesh->m_indexBuffer = nullptr;
	mesh->m_indexCount = 0;
	mesh->m_indexSize = 0;
	if ( indexCount > 0 && indexSize > 0 && indices != nullptr )
	{
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = indexSize * indexCount;
		bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = indices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufDesc, &data, &mesh->m_indexBuffer ) );
		GPUResource_LogMemory( "IndexBuffer", bufDesc.ByteWidth, "mesh" );

		mesh->m_indexCount = indexCount;
		mesh->m_indexSize = indexSize;
	}

	mesh->m_topology = topology;
}

static void Mesh_Release( GPUMesh_s* mesh )
{
	if ( mesh->m_vertexBuffer )
		V6_RELEASE_D3D11( mesh->m_vertexBuffer );
	if ( mesh->m_indexBuffer )
		V6_RELEASE_D3D11( mesh->m_indexBuffer );
}

static void Mesh_CreateTriangle( ID3D11Device* device, GPUMesh_s* mesh )
{
	const BasicVertex_s vertices[3] = 
	{
		{ core::Vec3_Make( 0.0f, 1.0f, 0.0f ), core::Color_Make( 255, 0, 0, 255) },
		{ core::Vec3_Make( 1.0f, -1.0f, 0.0f ), core::Color_Make( 0, 255, 0, 255) },
		{ core::Vec3_Make( -1.0f, -1.0f, 0.0f ), core::Color_Make( 0, 0, 255, 255) } 
	};

	const core::u16 indices[3] = { 0, 1, 2 };

	Mesh_Create( device, mesh, vertices, 3, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 3, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void Mesh_CreateBox( ID3D11Device* device, GPUMesh_s* mesh, const core::Color_s color, bool wireframe )
{
	const BasicVertex_s vertices[8] = 
	{
		{ core::Vec3_Make( -1.0f, -1.0f,  1.0f ), color },
		{ core::Vec3_Make(  1.0f, -1.0f,  1.0f ), color },
		{ core::Vec3_Make( -1.0f,  1.0f,  1.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f,  1.0f ), color },
		{ core::Vec3_Make( -1.0f, -1.0f, -1.0f ), color },
		{ core::Vec3_Make(  1.0f, -1.0f, -1.0f ), color },
		{ core::Vec3_Make( -1.0f,  1.0f, -1.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f, -1.0f ), color },
	};

	if ( wireframe )
	{
		const core::u16 indices[24] = { 
			0, 1, 1, 3, 3, 2, 2, 0,
			4, 5, 5, 7, 7, 6, 6, 4,
			1, 5, 0, 4, 3, 7, 2, 6 };

		Mesh_Create( device, mesh, vertices, 8, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 24, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
	}
	else
	{
		const core::u16 indices[36] = { 
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

		Mesh_Create( device, mesh, vertices, 8, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	}
}

static void Mesh_CreateQuad( ID3D11Device* device, GPUMesh_s* mesh, const core::Color_s color )
{
	const BasicVertex_s vertices[8] = 
	{
		{ core::Vec3_Make( -1.0f, -1.0f, 0.0f ), color },
		{ core::Vec3_Make(  1.0f, -1.0f, 0.0f ), color },
		{ core::Vec3_Make( -1.0f,  1.0f, 0.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f, 0.0f ), color },
	};

	const core::u16 indices[4] = { 	0, 2, 1, 3 };

	Mesh_Create( device, mesh, vertices, 4, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 4, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
}

static void Mesh_CreateVirtualQuad( ID3D11Device* device, GPUMesh_s* mesh )
{
	Mesh_Create( device, mesh, nullptr, 4, 0, 0, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
}

static void Mesh_CreateVirtualBox( ID3D11Device* device, GPUMesh_s* mesh )
{
	const core::u16 indices[36] = { 
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

	Mesh_Create( device, mesh, nullptr, 0, 0, 0, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );	
}

static void Mesh_CreateFakeCube( ID3D11Device* device, GPUMesh_s* mesh )
{
#if 0
	const core::u16 indices[36] = { 
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

	Mesh_Create( device, mesh, nullptr, 0, 0, 0, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
#else
	const core::u16 indices[5] = { 0, 2, 3, 1, 0 };

	Mesh_Create( device, mesh, nullptr, 0, 0, 0, indices, 5, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP );
#endif
}

static void Mesh_CreatePoint( ID3D11Device* device, GPUMesh_s* mesh )
{
	Mesh_Create( device, mesh, nullptr, 0, 0, 0, nullptr, 0, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
}

static void Mesh_CreateLine( ID3D11Device* device, GPUMesh_s* mesh, const core::Color_s color )
{
	const BasicVertex_s vertices[2] = 
	{
		{ core::Vec3_Make( -1.0f, -1.0f, -1.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f,  1.0f ), color },
	};

	Mesh_Create( device, mesh, vertices, 2, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, nullptr, 0, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
}

static void Mesh_Draw( GPUMesh_s* mesh, core::u32 instanceCount, GPUShader_s* shader, ID3D11DeviceContext* ctx, ID3D11Buffer* bufferArgs, core::u32 offsetArgs )
{
	V6_ASSERT( shader->m_vertexFormat == mesh->m_vertexFormat );
	V6_ASSERT( instanceCount > 0 );

	ctx->IASetInputLayout( shader->m_inputLayout );
	ctx->VSSetShader( shader->m_vertexShader, nullptr, 0 );
	ctx->PSSetShader( shader->m_pixelShader, nullptr, 0 );
		
	const core::u32 stride = mesh->m_vertexSize; 
	const core::u32 offset = 0;
			
	ctx->IASetVertexBuffers( 0, mesh->m_vertexBuffer != nullptr ? 1 : 0, &mesh->m_vertexBuffer, &stride, &offset );	
	ctx->IASetPrimitiveTopology( mesh->m_topology );

	if ( mesh->m_indexCount )
	{
		switch ( mesh->m_indexSize )
		{
		case 2:
			ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R16_UINT, 0 );
			break;
		case 4:
			ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
			break;
		default:
			V6_ASSERT_NOT_SUPPORTED();
		}

		if ( bufferArgs )
			ctx->DrawIndexedInstancedIndirect( bufferArgs, offsetArgs );
		else if ( instanceCount == 1 )
			ctx->DrawIndexed( mesh->m_indexCount, 0, 0 );
		else
			ctx->DrawIndexedInstanced( mesh->m_indexCount, instanceCount, 0, 0, 0 );
	}
	else
	{
		V6_ASSERT( mesh->m_indexBuffer == nullptr );
		ctx->IASetIndexBuffer( nullptr, DXGI_FORMAT_R32_UINT, 0 );
				
		if ( bufferArgs )
			ctx->DrawInstancedIndirect( bufferArgs, offsetArgs );
		else 
		{
			V6_ASSERT( mesh->m_vertexCount > 0 );
			if ( instanceCount == 1 )
				ctx->Draw( mesh->m_vertexCount, 0 );
			else
				ctx->DrawInstanced( mesh->m_vertexCount, instanceCount, 0, 0 );
		}
	}
}

static void Material_DrawBasic( Material_s* material, Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings )
{
#if V6_SIMPLE_SCENE == 0
	if ( settings->isCapturing )
		return;
#endif // #if V6_SIMPLE_SCENE == 0

	v6::hlsl::CBBasic* cbBasic = ConstantBuffer_MapWrite< v6::hlsl::CBBasic >( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC] );

	core::Mat4x4 worlMatrix;
	core::Mat4x4_Scale( &worlMatrix, entity->scale );
	core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );

	// use this order because one matrix is "from" local space and the other is "to" local space
	core::Mat4x4 objectToViewMatrix;
	core::Mat4x4_Mul( &objectToViewMatrix, view->viewMatrix, worlMatrix );	
	
	cbBasic->c_basicObjectToView = objectToViewMatrix;
	cbBasic->c_basicViewToProj = view->projMatrix;
	core::Mat4x4_Mul( &cbBasic->c_basicObjectToProj, objectToViewMatrix, view->projMatrix );

	ConstantBuffer_UnmapWrite( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC] );

	ctx->deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_BASIC, 1, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC].buf );
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &ctx->shaders[SHADER_BASIC];
	Mesh_Draw( mesh, 1, shader, ctx->deviceContext, nullptr, 0 );
}

static void Material_DrawFakeCube( Material_s* material, Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings )
{
	v6::hlsl::CBBasic* cbBasic = ConstantBuffer_MapWrite< v6::hlsl::CBBasic >( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC] );

	core::Mat4x4 worlMatrix;
	core::Mat4x4_Scale( &worlMatrix, entity->scale );
	core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
	// use this order because one matrix is "from" local space and the other is "to" local space
	core::Mat4x4_Mul( &cbBasic->c_basicObjectToView, view->viewMatrix, worlMatrix );
	cbBasic->c_basicViewToProj = view->projMatrix;

	ConstantBuffer_UnmapWrite( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC] );

	ctx->deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_BASIC, 1, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC].buf );
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &ctx->shaders[SHADER_FAKE_CUBE];
	Mesh_Draw( mesh, 1, shader, ctx->deviceContext, nullptr, 0 );
}

static void Material_DrawGeneric( Material_s* material, Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings )
{
	v6::hlsl::CBGeneric* cbGeneric = ConstantBuffer_MapWrite< v6::hlsl::CBGeneric >( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_GENERIC] );

	core::Mat4x4 worlMatrix;
	core::Mat4x4_Scale( &worlMatrix, entity->scale );
	core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
	// use this order because one matrix is "from" local space and the other is "to" local space
	const bool useAlbedo = material->textureIDs[TEXTURE_GENERIC_DIFFUSE] != ENTITY_TEXTURE_INVALID;
	const bool useAlpha = material->textureIDs[TEXTURE_GENERIC_ALPHA] != ENTITY_TEXTURE_INVALID;
	cbGeneric->c_genericObjectToWorld = worlMatrix;
	cbGeneric->c_genericWorldToView = view->viewMatrix;
	cbGeneric->c_genericViewToProj = view->projMatrix;
	if ( g_showObjects )
	{
		core::Color_s color;
		color.bits = core::HashPointer( entity );
		cbGeneric->c_genericDiffuse = core::Vec3_Make( color.r / 255.0f, color.g / 255.0f, color.b / 255.0f );
		cbGeneric->c_genericUseAlbedo = false;
	}
	else
	{
		cbGeneric->c_genericDiffuse = useAlbedo ? core::Vec3_Make( 1.0f, 1.0f, 1.0f ) : material->diffuse;
		cbGeneric->c_genericUseAlbedo = useAlbedo;
	}
	cbGeneric->c_genericUseAlpha = useAlpha;

	ConstantBuffer_UnmapWrite( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_GENERIC] );

	ctx->deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_GENERIC, 1, &ctx->constantBuffers[CONSTANT_BUFFER_GENERIC].buf );
	ctx->deviceContext->PSSetConstantBuffers( CONSTANT_BUFFER_GENERIC, 1, &ctx->constantBuffers[CONSTANT_BUFFER_GENERIC].buf );
	
	ctx->deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &ctx->samplerState );

	if ( material->textureIDs[TEXTURE_GENERIC_DIFFUSE] != ENTITY_TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[TEXTURE_GENERIC_DIFFUSE]];
		if ( texture->mipmapState == GPUTEXTURE_MIPMAP_STATE_REQUIRED )
		{
			ctx->deviceContext->GenerateMips( texture->srv );
			texture->mipmapState = GPUTEXTURE_MIPMAP_STATE_GENERATED;
		}
		ctx->deviceContext->PSSetShaderResources( HLSL_GENERIC_ALBEDO_SLOT, 1, &texture->srv );
	}

	if ( material->textureIDs[TEXTURE_GENERIC_ALPHA] != ENTITY_TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[TEXTURE_GENERIC_ALPHA]];
		if ( texture->mipmapState == GPUTEXTURE_MIPMAP_STATE_REQUIRED )
		{
			ctx->deviceContext->GenerateMips( texture->srv );
			texture->mipmapState = GPUTEXTURE_MIPMAP_STATE_GENERATED;
		}
		ctx->deviceContext->PSSetShaderResources( HLSL_GENERIC_ALPHA_SLOT, 1, &texture->srv );
	}
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = (useAlpha && !settings->useAlphaCoverage) ? &ctx->shaders[SHADER_GENERIC_ALPHA_TEST] : &ctx->shaders[SHADER_GENERIC];
	Mesh_Draw( mesh, 1, shader, ctx->deviceContext, nullptr, 0 );

	static const void* nulls[8] = {};
	if ( material->textureIDs[TEXTURE_GENERIC_DIFFUSE] )
		ctx->deviceContext->PSSetShaderResources( HLSL_GENERIC_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	if ( material->textureIDs[TEXTURE_GENERIC_ALPHA] )
		ctx->deviceContext->PSSetShaderResources( HLSL_GENERIC_ALPHA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
}

static void Material_Create( Material_s* material, MaterialDraw_f drawFunction )
{
	material->drawFunction = drawFunction;
	memset( material->textureIDs, 0xFF, sizeof( material->textureIDs ) );
}

static void Material_SetTexture( Material_s* material, core::u32 textureID, core::u32 textureSlot )
{
	V6_ASSERT( textureSlot < ENTITY_TEXTURE_MAX_COUNT );
	material->textureIDs[textureSlot] = textureID;
}

static void Entity_Create( Entity_s* entity, core::u32 materialID, core::u32 meshID, const core::Vec3& pos, float scale )
{
	memset( entity->name, 0, sizeof( entity->name ) );
	entity->materialID = materialID;
	entity->meshID = meshID;
	entity->pos = pos;
	entity->scale = scale;
	entity->visible = true;
}

static void Entity_Draw( Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings )
{
	if ( !entity->visible )
		return;

	Material_s* material = &scene->materials[entity->materialID];
	material->drawFunction( material, entity, scene, ctx, view, settings );
}

static void Entity_SetName( Entity_s* entity, const char* name )
{
	if ( name )
		strcpy_s( entity->name, sizeof( entity->name ), name );
	else
		entity->name[0] = 0;
}

static void Entity_SetVisible( Entity_s* entity, bool visible )
{
	entity->visible = visible;
}

static void Entity_SetPos( Entity_s* entity, const core::Vec3* pos )
{
	entity->pos = *pos;
}

static void Entity_SetScale( Entity_s* entity, float scale )
{
	entity->scale = scale;
}

static void Scene_Create( Scene_s* scene )
{
	memset( scene, 0, sizeof( *scene) );
}

static void Scene_SetFilename( Scene_s* scene, const char* filename )
{
	strcpy_s( scene->filename, sizeof( scene->filename ), filename );
}

static void Scene_SetInfo( Scene_s* scene, const SceneInfo_s* sceneInfo )
{
	memcpy( &scene->info, sceneInfo, sizeof( scene->info ) );
}

static void Scene_SaveInfo( Scene_s* scene )
{
	if ( !scene->filename[0] )
		return;

	V6_ASSERT( !core::FilePath_HasExtension( scene->filename, "info" ) );
	char fileinfo[256];
	core::FilePath_ChangeExtension( fileinfo, sizeof( fileinfo ), scene->filename, "info" );

	SceneInfo_WriteToFile( &scene->info, fileinfo );
}

static void Scene_MakeSequenceFilename( const Scene_s* scene, char* path, core::u32 maxPathSize )
{
	V6_ASSERT( scene->filename[0] );

	core::FilePath_ChangeExtension( path, maxPathSize, scene->filename, "v6s" );
}

static void Scene_MakeRawFrameFileTemplate( const Scene_s* scene, char* path, core::u32 maxPathSize )
{
	V6_ASSERT( scene->filename[0] );

	char filepath[256];
	core::FilePath_ExtractPath( filepath, sizeof( filepath ), scene->filename );

	char filename[256];
	core::FilePath_ExtractFilename( filename, sizeof( filename ), scene->filename );

	char filenameWithoutExtension[256];
	core::FilePath_TrimExtension( filenameWithoutExtension, sizeof( filenameWithoutExtension ), filename );

	core::FilePath_Make( path, maxPathSize, filepath, filenameWithoutExtension );
	sprintf_s( path, maxPathSize, "%s_%s.v6f", path, "%06d" );
}

static void Scene_MakeRawFrameFilename( const Scene_s* scene, char* path, core::u32 maxPathSize, core::u32 frame  )
{
	V6_ASSERT( scene->filename[0] );

	char filepath[256];
	core::FilePath_ExtractPath( filepath, sizeof( filepath ), scene->filename );

	char filename[256];
	core::FilePath_ExtractFilename( filename, sizeof( filename ), scene->filename );

	char filenameWithoutExtension[256];
	core::FilePath_TrimExtension( filenameWithoutExtension, sizeof( filenameWithoutExtension ), filename );

	core::FilePath_Make( path, maxPathSize, filepath, filenameWithoutExtension );
	sprintf_s( path, maxPathSize, "%s_%06u.v6f", path, frame );
}

static core::u32 Scene_FindEntityByName( const Scene_s* scene, const char* entityName )
{
	if ( entityName != nullptr && entityName[0] != 0 )
	{
		for ( core::u32 entityID = 0; entityID < scene->entityCount; ++entityID )
		{
			if ( strcmp( scene->entities[entityID].name, entityName ) == 0 )
				return entityID;
		}
	}

	return (core::u32)-1;
}

static void Scene_Release( Scene_s* scene )
{
	for ( core::u32 meshID = 0; meshID < scene->meshCount; ++meshID )
		Mesh_Release( &scene->meshes[meshID] );
	for ( core::u32 textureID = 0; textureID < scene->textureCount; ++textureID )
		GPUTexture_Release( &scene->textures[textureID] );
	
	scene->meshCount = 0;
	scene->textureCount = 0;
	scene->materialCount = 0;
	scene->entityCount = 0;
}

static void SceneContext_Create( SceneContext_s* sceneContext, v6::core::IStack* stack )
{
	stack->push();

	memset( sceneContext, 0, sizeof( SceneContext_s ) );
	sceneContext->stack = stack;
	core::Signal_Create( &sceneContext->deviceReady );
	core::Signal_Create( &sceneContext->loadDone );
}

static void SceneContext_SetFilename( SceneContext_s* sceneContext, const char* filename )
{
	strcpy_s( sceneContext->filename, sizeof( sceneContext->filename ), filename );
}

static void SceneContext_Release( SceneContext_s* sceneContext )
{
	if ( sceneContext->scene )
		Scene_Release( sceneContext->scene );
	core::Signal_Release( &sceneContext->deviceReady );
	core::Signal_Release( &sceneContext->loadDone );

	sceneContext->stack->pop();
}

static void SceneContext_SetDevice( SceneContext_s* sceneContext, ID3D11Device* device )
{
	sceneContext->device = device;
	Signal_Emit( &sceneContext->deviceReady );
}

static void SceneContext_Load( SceneContext_s* sceneContext )
{
	V6_MSG( "Load scene\n" );

	V6_ASSERT( !core::FilePath_HasExtension( sceneContext->filename, "info" ) );
	char fileinfo[256];
	core::FilePath_ChangeExtension( fileinfo, sizeof( fileinfo ), sceneContext->filename, "info" );
	
	SceneInfo_s info;
	if ( !SceneInfo_ReadFromFile( &info, fileinfo ) )
		V6_WARNING( "Unable to read info file\n" );
	
	if ( !Obj_ReadObjectFile( &sceneContext->objScene, sceneContext->filename, sceneContext->stack ) )
	{
		sceneContext->objScene.meshCount = 0;
		V6_ERROR( "Unable to load %s\n", sceneContext->filename );
		core::Signal_Emit( &sceneContext->loadDone );
		return;
	}
	
	V6_MSG( "%d meshes loaded\n",  sceneContext->objScene.meshCount );
	
	core::Signal_Wait( &sceneContext->deviceReady );

	V6_MSG( "Init scene\n" );

	ObjScene_s* objScene = &sceneContext->objScene;
	Scene_s* scene = sceneContext->stack->newInstance< Scene_s >();
	Scene_Create( scene );
	Scene_SetFilename( scene, sceneContext->filename );
	Scene_SetInfo( scene, &info );

	for ( core::u32 materialID = 0; materialID < objScene->materialCount; ++materialID )
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

		for ( core::u32 textureSlot = 0; textureSlot < TEXTURE_GENERIC_COUNT; ++textureSlot )
		{
			const char* textureFilename = textureFilenames[textureSlot];
			if ( !*textureFilename )
				continue;

			core::Image_s image = {};
			core::CFileReader fileReader;
			if ( core::FilePath_HasExtension( textureFilename, "tga" ) && fileReader.Open( textureFilename ) && core::Image_ReadTga( &image, &fileReader, sceneContext->stack ) )
			{
				static const char* textureNames[TEXTURE_GENERIC_COUNT] = { "diffuse", "alpha", "bump" };
				const core::u32 textureID = scene->textureCount;
				Texture2D_Create( sceneContext->device, &scene->textures[scene->textureCount], image.width, image.height, image.pixels, true, textureNames[textureSlot] );
				++scene->textureCount;

				Material_SetTexture( material, textureID, textureSlot );
			}
			else
				V6_WARNING( "Unable to load %s for material %s\n", textureFilename, objMaterial->name );
		}

		sceneContext->stack->pop();
	}

	float maxDim = 0.0f;

	for ( core::u32 meshID = 0; meshID < objScene->meshCount; ++meshID )
	{
		V6_ASSERT( meshID < MESH_MAX_COUNT );
		V6_ASSERT( meshID < ENTITY_MAX_COUNT );

		sceneContext->stack->push();

		ObjMesh_s* mesh = &objScene->meshes[meshID];
		
		GenericVertex_s* vertices = sceneContext->stack->newArray< GenericVertex_s >( mesh->triangleCount * 3 );
		
		ObjTriangle_s* triangle = &objScene->triangles[mesh->firstTriangleID];
		
		core::Vec3 meshCenter = core::Vec3_Zero();
		for ( core::u32 triangleID = 0; triangleID < mesh->triangleCount; ++triangleID, ++triangle )
		{
			for ( core::u32 vertexID = 0; vertexID < 3; ++vertexID )
			{
				const core::Vec3 worldPos = objScene->positions[triangle->vertices[vertexID].posID] * info.worldUnitToCM;
				meshCenter += worldPos;
				maxDim = core::Max( maxDim, worldPos.x );
				maxDim = core::Max( maxDim, worldPos.y );
				maxDim = core::Max( maxDim, worldPos.z );
			}
		}
		if ( mesh->triangleCount )
			meshCenter *= 1.0f / (mesh->triangleCount * 3);

		triangle = &objScene->triangles[mesh->firstTriangleID];
		GenericVertex_s* vertex = vertices;
		for ( core::u32 triangleID = 0; triangleID < mesh->triangleCount; ++triangleID, ++triangle, vertex += 3 )
		{
			vertex[0].position = objScene->positions[triangle->vertices[0].posID] * info.worldUnitToCM - meshCenter;
			vertex[1].position = objScene->positions[triangle->vertices[1].posID] * info.worldUnitToCM - meshCenter;
			vertex[2].position = objScene->positions[triangle->vertices[2].posID] * info.worldUnitToCM - meshCenter;

			if ( objScene->normals && triangle->vertices[0].normalID != (core::u32)-1 )
			{
				vertex[0].normal = objScene->normals[triangle->vertices[0].normalID];
				vertex[1].normal = objScene->normals[triangle->vertices[1].normalID];
				vertex[2].normal = objScene->normals[triangle->vertices[2].normalID];
			}
			else
			{
				const core::Vec3 edge1 = vertex[1].position - vertex[0].position;
				const core::Vec3 edge2 = vertex[2].position - vertex[0].position;
				const core::Vec3 normal = core::Cross( edge1, edge2 ).Normalized();
				vertex[0].normal = normal;
				vertex[1].normal = normal;
				vertex[2].normal = normal;
			}

			if ( objScene->uvs && triangle->vertices[0].uvID != (core::u32)-1 )
			{
				vertex[0].uv = objScene->uvs[triangle->vertices[0].uvID];
				vertex[1].uv = objScene->uvs[triangle->vertices[1].uvID];
				vertex[2].uv = objScene->uvs[triangle->vertices[2].uvID];
			}
			else
			{
				vertex[0].uv = core::Vec2_Make( 0.0f, 0.0f );
				vertex[1].uv = core::Vec2_Make( 0.0f, 0.0f );
				vertex[2].uv = core::Vec2_Make( 0.0f, 0.0f );
			}
		}	

		Mesh_Create( sceneContext->device, &scene->meshes[meshID], vertices, mesh->triangleCount * 3, sizeof( GenericVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		++scene->meshCount;

		Entity_Create( &scene->entities[meshID], mesh->materialID, meshID, meshCenter, 1.0f );
		Entity_SetName( &scene->entities[meshID], mesh->name );
		++scene->entityCount;

		sceneContext->stack->pop();
	}	

	const core::u32 materialID = scene->materialCount;
	Material_Create( &scene->materials[materialID], Material_DrawBasic );
	++scene->materialCount;

	for ( core::u32 boxID = 0; boxID < 3; ++boxID )
	{
		core::u32 meshID = scene->meshCount;
	
		V6_ASSERT( meshID < MESH_MAX_COUNT );
		V6_ASSERT( meshID < ENTITY_MAX_COUNT );

		switch ( boxID )
		{
		case 0:
			{
				Mesh_CreateBox( sceneContext->device, &scene->meshes[meshID], core::Color_Make( 127, 127, 127, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, core::Vec3_Make( 0.0f, 0.0f, 0.0f), maxDim );
				++scene->entityCount;
			}
			break;
		case 1:
			{
				Mesh_CreateBox( sceneContext->device, &scene->meshes[meshID], core::Color_Make( 255, 0, 0, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, core::Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MIN_SCALE );
				++scene->entityCount;
			}
			break;
		case 2:
			{
				Mesh_CreateBox( sceneContext->device, &scene->meshes[meshID], core::Color_Make( 0, 0, 255, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, core::Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MAX_SCALE );
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
	s_yaw = core::DegToRad( info.cameraYaw );
	if ( s_paths[PATH_CAMERA].keyCount )
		s_headOffset = s_paths[PATH_CAMERA].positions[0];
	for ( core::u32 pathID = 0; pathID < PATH_COUNT; ++pathID )
	{
		if ( s_paths[pathID].keyCount && s_paths[pathID].entityID != (core::u32)-1 )
			Entity_SetPos( &scene->entities[s_paths[pathID].entityID], &s_paths[pathID].positions[0] );
	}

	core::Signal_Emit( &sceneContext->loadDone );
}

void Scene_CreateDebug( SceneDebug_s* scene, ID3D11Device* device )
{
	Scene_Create( scene );

	scene->meshLineID = scene->meshCount;
	Mesh_CreateLine( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 255, 255, 255 ) );
	
	scene->meshGridID = scene->meshCount;
	Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 255, 255, 255 ), true );
		
	Material_Create( &scene->materials[scene->materialCount++], Material_DrawBasic );
	
	core::u32 cellID = 0;
	for ( int z = 0; z < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++z )
	{
		for ( int y = 0; y < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++y )
		{
			for ( int x = 0; x < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++x, ++cellID )
			{						
				for ( core::u32 cellType = 0; cellType < 2; ++cellType )
				{
					scene->meshCellIDs[cellID][cellType] = scene->meshCount;
					Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( 0, (cellType == 0) * 255, (cellType == 1) * 255, 255 ), cellType == 0 );

					scene->entityCellIDs[cellID][cellType] = scene->entityCount;
					Entity_s* cellEntity = &scene->entities[scene->entityCount++];
					Entity_Create( cellEntity, 0, scene->meshCellIDs[cellID][cellType], core::Vec3_Zero(), 1.0f );
					Entity_SetVisible( cellEntity, false );
				}
			}
		}
	}

	scene->entityGridID = scene->entityCount;
	Entity_Create( &scene->entities[scene->entityGridID], 0, scene->meshGridID, core::Vec3_Zero(), 1.0f );
	Entity_SetVisible( &scene->entities[scene->entityGridID], false );
	++scene->entityCount;

	scene->entityLineID = scene->entityCount;
	Entity_Create( &scene->entities[scene->entityLineID], 0, scene->meshLineID, core::Vec3_Zero(), 1.0f );
	Entity_SetVisible( &scene->entities[scene->entityLineID], false );
	++scene->entityCount;

	for ( int debugBlockID = 0; debugBlockID < DEBUG_BLOCK_MAX_COUNT; ++debugBlockID )
	{
		for ( core::u32 cellType = 0; cellType < 2; ++cellType )
		{
			core::u32 hashColor = 1 + (debugBlockID%7);
			scene->meshBlockIDs[debugBlockID][cellType] = scene->meshCount;
			Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( (hashColor & 1) ? 255 : 0, (hashColor & 2) ? 255 : 0, (hashColor & 4) ? 255 : 0, 255 ), cellType == 0 );

			scene->entityBlockIDs[debugBlockID][cellType] = scene->entityCount;
			Entity_s* blockEntity = &scene->entities[scene->entityCount++];
			Entity_Create( blockEntity, 0, scene->meshBlockIDs[debugBlockID][cellType], core::Vec3_Zero(), 1.0f );
			Entity_SetVisible( blockEntity, false );
		}

		const core::u32 meshTraceID = scene->meshCount;
		Mesh_CreateLine( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 255, 255, 255 ) );

		scene->entityTraceIDs[debugBlockID] = scene->entityCount;
		Entity_s* traceEntity = &scene->entities[scene->entityCount++];
		Entity_Create( traceEntity, 0, meshTraceID, core::Vec3_Zero(), 1.0f );
		Entity_SetVisible( traceEntity, false );
	}
}

void Scene_CreatePathGeo( ScenePathGeo_s* scene, ID3D11Device* device )
{
	Scene_Create( scene );

	for ( core::u32 lineRank = 0; lineRank < Path_s::MAX_POINT_COUNT-1; ++lineRank )
	{
		scene->meshLineIDs[lineRank] = scene->meshCount;
		Mesh_CreateLine( device, &scene->meshes[scene->meshCount++], core::Color_Make( 128, 128, 128, 255 ) );
	}

	scene->meshBoxID = scene->meshCount;
	Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 255, 255, 255 ), true );

	scene->meshSelectedBoxID = scene->meshCount;
	Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 0, 0, 255 ), true );

	Material_Create( &scene->materials[scene->materialCount++], Material_DrawBasic );
}

void Scene_UpdatePathGeo( ScenePathGeo_s* scene, const Path_s* path, ID3D11DeviceContext* context )
{
	if ( !path->dirty )
		return;

	scene->entityCount = 0;
	
	for ( core::u32 key = 0; key < (core::u32)path->keyCount; ++key )
	{
		const bool isSelected = key == path->activeKey;
		Entity_Create( &scene->entities[scene->entityCount++], 0, isSelected ? scene->meshSelectedBoxID : scene->meshBoxID, path->positions[key], 5.0f );
	}

	int lineRank;
	for ( lineRank = 0; lineRank < path->keyCount-1; ++lineRank )
	{		
		const core::u32 meshLineID = scene->meshLineIDs[lineRank];
		const BasicVertex_s vertices[2] = 
		{
			{ path->positions[lineRank], core::Color_Make( 128, 128, 128, 255 ) },
			{ path->positions[lineRank+1], core::Color_Make( 128, 128, 128, 255 ) },
		};
		Mesh_UpdateVertices( context, &scene->meshes[meshLineID], vertices );
		Entity_Create( &scene->entities[scene->entityCount++], 0, meshLineID, core::Vec3_Zero(), 1.0f );
	}
}

#if V6_SIMPLE_SCENE == 1

void Scene_CreateDefault( Scene_s* scene, ID3D11Device* device )
{
	const char* filename = "D:/media/obj/default/default.obj";

	V6_ASSERT( !core::FilePath_HasExtension( filename, "info" ) );
	char fileinfo[256];
	core::FilePath_ChangeExtension( fileinfo, sizeof( fileinfo ), filename, "info" );

	SceneInfo_s info;
	if ( !SceneInfo_ReadFromFile( &info, fileinfo ) )
		V6_WARNING( "Unable to read info file\n" );

	Scene_Create( scene );
	Scene_SetFilename( scene, filename );
	Scene_SetInfo( scene, &info );

	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_RED], core::Color_Make( 255, 0, 0, 255 ), false );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_BLUE], core::Color_Make( 0, 0, 255, 255 ), false );

	Material_Create( &scene->materials[MATERIAL_DEFAULT_BASIC], Material_DrawBasic );
	
	const core::u32 screenWidth = HLSL_GRID_WIDTH >> 1;
	//const float depth = -99.0001f;
	const float depth = -100.0001f;
	const float pixelRadius = 0.5f * (200.0f / screenWidth);
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_RED, core::Vec3_Make( 0, 0, depth ), pixelRadius * 8 );
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_BLUE, core::Vec3_Make( -pixelRadius * 32, 0, 0.5f * depth ), pixelRadius * 16 );

	CameraPath_Load( &s_cameraPath, &info );
}

#else

void Scene_CreateDefault( Scene_s* scene, ID3D11Device* device )
{
	Scene_Create( scene );

	Mesh_CreateTriangle( device, &scene->meshes[MESH_TRIANGLE] );	
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_WIREFRAME], core::Color_Make( 255, 255, 255, 255 ), true );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_RED], core::Color_Make( 255, 0, 0, 255 ), false );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_GREEN], core::Color_Make( 0, 255, 0, 255 ), false );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_BLUE], core::Color_Make( 0, 0, 255, 255 ), false );
	Mesh_CreateVirtualQuad( device, &scene->meshes[MESH_VIRTUAL_QUAD] );
	Mesh_CreateFakeCube( device, &scene->meshes[MESH_FAKE_CUBE] );
	Mesh_CreatePoint( device, &scene->meshes[MESH_POINT] );
	Mesh_CreateLine( device, &scene->meshes[MESH_LINE],  core::Color_Make( 255, 255, 255, 255 ) );
	Mesh_CreateVirtualBox( device, &scene->meshes[MESH_VIRTUAL_BOX] );

	Material_Create( &scene->materials[MATERIAL_DEFAULT_BASIC], Material_DrawBasic );
	Material_Create( &scene->materials[MATERIAL_DEFAULT_FAKE_CUBE], Material_DrawFakeCube );
		
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_WIREFRAME, core::Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MAX_SCALE );
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_TRIANGLE, core::Vec3_Make( 0.0f, 0.0f, -GRID_MAX_SCALE ), 5.0f );
	for ( core::u32 randomCubeID = 0; randomCubeID < RANDOM_CUBE_COUNT;  )
	{
		const core::Vec3 center = core::Vec3_Rand() * (GRID_MAX_SCALE - FREE_SCALE);
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

core::u32 Grid_Trace( const core::Vec3* gridCenter, float gridScale, core::u32 gridOccupancy, const core::Vec3* rayOrg, const core::Vec3* rayDir, TraceData_s* traceData )
{
	const core::Vec3 invDir = rayDir->Rcp();
	const core::Vec3 alpha = (*gridCenter - *rayOrg) * invDir;
	const core::Vec3 beta = gridScale * invDir;	
	const core::Vec3 t0 = alpha + beta;
	const core::Vec3 t1 = alpha - beta;
	const core::Vec3 tMin = core::Min( t0, t1 );
	const core::Vec3 tMax = core::Max( t0, t1 );
	const float tIn = core::Max( core::Max( tMin.x, tMin.y ), tMin.z );
	const float tOut = core::Min( core::Min( tMax.x, tMax.y ), tMax.z );

	if ( traceData )
	{
		traceData->tIn = tIn;
		traceData->tOut = tOut;
		traceData->hitFoundCoords = core::Vec3i_Zero();
		traceData->hitFailBits = 0;
	}
		
	if ( tIn > tOut )
		return 0;

	const float cellSize = (gridScale * 2.0f) / HLSL_CELL_SUPER_SAMPLING_WIDTH;
	const float scale = 1.0f / cellSize;
	const float offset = HLSL_CELL_SUPER_SAMPLING_WIDTH * 0.5f;
	
	const core::Vec3 tDelta = cellSize * invDir.Abs();	

	const core::Vec3 pIn = *rayOrg + tIn * *rayDir;
	const core::Vec3 coordIn = (pIn - *gridCenter) * scale + offset;
		
	const int x = min( (int)coordIn.x, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	const int y = min( (int)coordIn.y, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	const int z = min( (int)coordIn.z, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	core::Vec3i coords = core::Vec3i_Make( x, y, z );
	const core::Vec3i step = core::Vec3i_Make( rayDir->x < 0.0f ? -1 : 1, rayDir->y < 0.0f ? -1 : 1, rayDir->z < 0.0f ? -1 : 1 );
	core::Vec3 tCur = tMin;
	for ( core::u32 pass = 0; pass < 2; ++pass )
	{
		const core::Vec3 tNext = tCur + tDelta;
		tCur.x = tNext.x < tIn ? tNext.x : tCur.x;
		tCur.y = tNext.y < tIn ? tNext.y : tCur.y;
		tCur.z = tNext.z < tIn ? tNext.z : tCur.z;
	}
	
	core::u32 hitFound = 0;

	for ( ;; )
	{
		const core::u32 cellID = coords.z * HLSL_CELL_SUPER_SAMPLING_WIDTH_SQ + coords.y * HLSL_CELL_SUPER_SAMPLING_WIDTH + coords.x;
		const core::u32 occupancyBit = 1 << cellID;
		hitFound |= (gridOccupancy & occupancyBit);
		if ( hitFound )
		{
			if ( traceData)
				traceData->hitFoundCoords = coords;
			break;
		}

		if ( traceData)
			traceData->hitFailBits |= occupancyBit;

		const core::Vec3 tNext = tCur + tDelta;
		core::u32 nextAxis;
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

void Block_TraceDisplay( GPUContext_s* gpuContext, SceneDebug_s* scene, const core::Vec3* gridCenter, float gridScale, core::u32 gridOccupancy, const core::Vec3* rayOrg, const core::Vec3* rayEnd )
{
	s_gridCenter = *gridCenter;
	s_gridScale = gridScale;
	s_gridOccupancy = gridOccupancy;
	s_rayOrg = *rayOrg;
	s_rayEnd = *rayEnd;

	Entity_SetPos( &scene->entities[scene->entityGridID], gridCenter );
	Entity_SetScale( &scene->entities[scene->entityGridID], gridScale );
	Entity_SetVisible( &scene->entities[scene->entityGridID], true );

	const core::Vec3 rayDir = (*rayEnd - *rayOrg).Normalized();
	
	BasicVertex_s vertices[2];
	vertices[0].position = *rayOrg;
	vertices[0].color = core::Color_Make( 255, 0, 0, 255 );	
	vertices[1].position = *rayOrg + 1000000.0f * rayDir;
	vertices[1].color = core::Color_Make( 0, 255, 0, 255 );
	Mesh_UpdateVertices( gpuContext->deviceContext, &scene->meshes[scene->meshLineID], vertices );
	Entity_SetVisible( &scene->entities[scene->entityLineID], true );
	
	const float cellSize = (gridScale * 2.0f) / HLSL_CELL_SUPER_SAMPLING_WIDTH;
	const core::Vec3 cellOrg = *gridCenter - gridScale + cellSize * 0.5f;
	
	core::u32 cellID = 0;
	for ( int z = 0; z < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++z )
	{
		for ( int y = 0; y < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++y )
		{
			for ( int x = 0; x < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++x, ++cellID )
			{
				const core::Vec3 cellCenter = cellOrg + core::Vec3_Make( (float)x, (float)y, (float)z ) * cellSize;
				
				Entity_s* cellEntity = &scene->entities[scene->entityCellIDs[cellID][0]];
				Entity_SetVisible( cellEntity, (gridOccupancy & (1 << cellID)) != 0 );
				Entity_SetPos( cellEntity, &cellCenter );
				Entity_SetScale( cellEntity, cellSize * 0.5f );
				
				cellEntity = &scene->entities[scene->entityCellIDs[cellID][1]];
				Entity_SetVisible( cellEntity, false );
				Entity_SetPos( cellEntity, &cellCenter );
				Entity_SetScale( cellEntity, cellSize * 0.5f );
			}
		}
	}
	
	const core::u32 hitFound = Grid_Trace( gridCenter, gridScale, gridOccupancy, rayOrg, &rayDir, nullptr );

	for ( core::u32 cellID = 0; cellID < HLSL_CELL_SUPER_SAMPLING_WIDTH_CUBE; ++cellID )
		Entity_SetVisible( &scene->entities[scene->entityCellIDs[cellID][1]], (hitFound & (1 << cellID)) != 0 );
}

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
	void BlendPixel( const RenderingView_s* view );
	bool BuildBlock( core::u32 frameID );
	core::u32 BuildNode();
	void Capture( const core::Vec3* sampleOffset, core::u32 faceID );
	void ClearNode();
	void Collect( const core::Vec3* sampleOffset, core::u32 faceID );
	bool Create(int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IAllocator* heap, core::IStack* stack );
	void CullBlock( const RenderingView_s* views, const core::Vec3* buildOrigin, float gridMinScale, core::u32 groupCounts[CODEC_BUCKET_COUNT], core::u32 frameID );
	void Draw( float dt );
	void DrawCameraPath( const RenderingView_s* view );
	void DrawDebug( const RenderingView_s* view );
	void DrawScene( Scene_s* scene, const RenderingView_s* view, const RenderingSettings_s* settings );
	void DrawWorld( const RenderingView_s* view );	
	void FillLeaf();
	bool HasValidRawFrameFile( core::u32 frameID );
	bool InitTraceMode( core::u32 frameCount );
	void Output( ID3D11ShaderResourceView* srvLeft, ID3D11ShaderResourceView* srvRight );
	void PackColor( core::u32 blockCounts[HLSL_BUCKET_COUNT], BlockContext_s* blockContext );
	void Present();
	void Release();
	void ReleaseTraceMode();
	void ResetDrawMode();
	void TraceBlock( const RenderingView_s* views, const core::Vec3* buildOrigin );
	bool WriteRawFrameFile( const core::u32 blockCounts[HLSL_BUCKET_COUNT], BlockContext_s* blockContext, core::u32 frame );

	GPUContext_s		gpuContext;
		
	Config_s			m_config;
	
	core::Vec3			m_sampleOffsets[SAMPLE_MAX_COUNT];
	
	CubeContext_s		m_cubeContext;
	SampleContext_s		m_sampleContext;
	OctreeContext_s		m_octreeContext;
	SequenceContext_s*	m_sequenceContext;
	TraceContext_s*		m_traceContext;
	core::Sequence_s	m_sequence;
	int					m_bakedFrameCount;

	Scene_s*			m_defaultScene;
	SceneDebug_s*		m_debugScene;
	ScenePathGeo_s*		m_pathGeoScene;

	core::IAllocator*	m_heap;
	core::IStack*		m_stack;

	core::u32			m_width;
	core::u32			m_height;
	float				m_aspectRatio;
};

CRenderingDevice::CRenderingDevice()
{
	memset( this, 0, sizeof( CRenderingDevice ) );
}

CRenderingDevice::~CRenderingDevice()
{
}

bool CRenderingDevice::Create( int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IAllocator* heap, core::IStack* stack )
{
	m_heap = heap;
	m_stack = stack;
	core::ScopedStack scopedStack( stack );

	for ( core::u32 pathID = 0; pathID < PATH_COUNT; ++pathID )
		Path_Init( &s_paths[pathID] );
	PathPlayer_Init( &s_pathPlayer );

	m_width = nWidth;
	m_height = nHeight;
	m_aspectRatio = (float)nWidth / nHeight;	

	GPUContext_Create( &gpuContext, nWidth, nHeight, hWnd, fileSystem, heap, stack );

	m_defaultScene = heap->newInstance< Scene_s >();
	Scene_CreateDefault( m_defaultScene, gpuContext.device );
	s_activeScene = m_defaultScene;

	m_debugScene = heap->newInstance< SceneDebug_s >();
	Scene_CreateDebug( m_debugScene, gpuContext.device );

	m_pathGeoScene = heap->newInstance< ScenePathGeo_s >();
	Scene_CreatePathGeo( m_pathGeoScene, gpuContext.device );

	Config_Init( &m_config, m_width, m_height, CUBE_SIZE, HLSL_GRID_WIDTH );

	g_sample = 0;
	m_sampleOffsets[0] = core::Vec3_Make( 0.0f, 0.0f, 0.0f );
	for ( core::u32 sample = 1; sample < SAMPLE_MAX_COUNT; ++sample )
	{
		if ( sample <= 8 )
		{
			core::u32 vertexID = sample-1;
			m_sampleOffsets[sample].x = (vertexID&1) == 0 ? -FREE_SCALE : FREE_SCALE;
			m_sampleOffsets[sample].y = (vertexID&2) == 0 ? -FREE_SCALE : FREE_SCALE;
			m_sampleOffsets[sample].z = (vertexID&4) == 0 ? -FREE_SCALE : FREE_SCALE;
		}
		else
		{
			m_sampleOffsets[sample] = core::Vec3_Rand() * FREE_SCALE;
		}
	}

	GPUResource_LogMemoryUsage();

	Config_Log( &m_config );
	
	return true;
}

void CRenderingDevice::DrawScene( Scene_s* scene, const RenderingView_s* view, const RenderingSettings_s* settings )
{
	for ( core::u32 entityRank = 0; entityRank < scene->entityCount; ++entityRank )
	{
		Entity_s* entity = &scene->entities[entityRank];
		Entity_Draw( entity, scene, &gpuContext, view, settings );		
	}
}

void CRenderingDevice::DrawWorld( const RenderingView_s* view )
{	
	// Rasterization state
	gpuContext.deviceContext->OMSetDepthStencilState( gpuContext.depthStencilStateZRW, 0 );
#if V6_USE_ALPHA_COVERAGE == 1
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateAlphaCoverage, nullptr, 0XFFFFFFFF );	
#else
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateOpaque, nullptr, 0XFFFFFFFF );	
#endif
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		gpuContext.deviceContext->RSSetViewports( 1, &viewport );
		gpuContext.deviceContext->RSSetState( gpuContext.rasterState );
	}
	
	// RT
	if ( g_useMSAA )
		gpuContext.deviceContext->OMSetRenderTargets( 1, &gpuContext.colorViewMSAA, gpuContext.depthStencilViewMSAA );
	else
		gpuContext.deviceContext->OMSetRenderTargets( 1, &view->rtv, gpuContext.depthStencilView );

	// Clear
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if ( g_useMSAA )
	{
		gpuContext.deviceContext->ClearRenderTargetView( gpuContext.colorViewMSAA, pRGBA );
		gpuContext.deviceContext->ClearDepthStencilView( gpuContext.depthStencilViewMSAA, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
	}
	else
	{
		gpuContext.deviceContext->ClearRenderTargetView( view->rtv, pRGBA );
		gpuContext.deviceContext->ClearDepthStencilView( gpuContext.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
	}
		
	// Settings
	RenderingSettings_s settings;
#if V6_USE_ALPHA_COVERAGE == 1
	settings.useAlphaCoverage = true;
#else
	settings.useAlphaCoverage = false;
#endif
	settings.isCapturing = false;

	DrawScene( s_activeScene, view, &settings );

	// un RT
	gpuContext.deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );

	if ( g_useMSAA )
		gpuContext.deviceContext->ResolveSubresource( view->texture2D, 0, gpuContext.colorBufferMSAA, 0, DXGI_FORMAT_R8G8B8A8_UNORM );
}

void CRenderingDevice::DrawCameraPath( const RenderingView_s* view )
{
	// Rasterization state
	gpuContext.deviceContext->OMSetDepthStencilState( gpuContext.depthStencilStateZRW, 0 );
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateOpaque, nullptr, 0XFFFFFFFF );

	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		gpuContext.deviceContext->RSSetViewports( 1, &viewport );
		gpuContext.deviceContext->RSSetState( gpuContext.rasterState );
	}

	// RT
	gpuContext.deviceContext->OMSetRenderTargets( 1, &view->rtv, gpuContext.depthStencilView );

	// Settings
	RenderingSettings_s settings;
	settings.useAlphaCoverage = false;
	settings.isCapturing = false;

	DrawScene( m_pathGeoScene, view, &settings );

	// un RT
	gpuContext.deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );
}

void CRenderingDevice::DrawDebug( const RenderingView_s* view )
{	
	if ( g_traceGrid )
	{		
		Block_TraceDisplay( &gpuContext, m_debugScene, &s_gridCenter, s_gridScale, s_gridOccupancy, &s_rayOrg, &s_rayEnd );
		
		g_traceGrid = false;
	}

	// Rasterization state
	gpuContext.deviceContext->OMSetDepthStencilState( g_transparentDebug ? gpuContext.depthStencilStateNoZ : gpuContext.depthStencilStateZRW, 0 );
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateOpaque, nullptr, 0XFFFFFFFF );
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		gpuContext.deviceContext->RSSetViewports( 1, &viewport );
		gpuContext.deviceContext->RSSetState( gpuContext.rasterState );
	}
	
	// RT
	gpuContext.deviceContext->OMSetRenderTargets( 1, &view->rtv, g_transparentDebug ? nullptr : gpuContext.depthStencilView );

	// Clear
	if ( !g_transparentDebug )
		gpuContext.deviceContext->ClearDepthStencilView( gpuContext.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
				
	// Settings
	RenderingSettings_s settings;
	settings.useAlphaCoverage = false;
	settings.isCapturing = false;

	DrawScene( m_debugScene, view, &settings );

	// un RT
	gpuContext.deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );
}

void CRenderingDevice::Capture( const core::Vec3* samplePos, core::u32 faceID )
{
	// Rasterization state
	gpuContext.deviceContext->OMSetDepthStencilState( gpuContext.depthStencilStateZRW, 0 );
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateOpaque, nullptr, 0XFFFFFFFF );

	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_cubeContext.size;
		viewport.Height = (float)m_cubeContext.size;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		gpuContext.deviceContext->RSSetViewports( 1, &viewport );
		gpuContext.deviceContext->RSSetState( gpuContext.rasterState );
	}

	gpuContext.userDefinedAnnotation->BeginEvent( L"Capture");
		
	// RT
	gpuContext.deviceContext->OMSetRenderTargets( 1, &m_cubeContext.colorRTV, m_cubeContext.depthRTV );

	// Clear
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	gpuContext.deviceContext->ClearRenderTargetView( m_cubeContext.colorRTV, pRGBA );
	gpuContext.deviceContext->ClearDepthStencilView( m_cubeContext.depthRTV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );						

	// View
	RenderingView_s view;
	Cube_MakeViewMatrix( &view.viewMatrix, *samplePos, (CubeAxis_e)faceID );
	view.projMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, core::DegToRad( 90.0f ), 1.0f );
		
	// Settings
	RenderingSettings_s settings;
	settings.useAlphaCoverage = false;
	settings.isCapturing = true;

	DrawScene( s_activeScene, &view, &settings );

	// un RT
	gpuContext.deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );
			
	gpuContext.userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::Collect( const core::Vec3* samplePos, core::u32 faceID )
{
	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"Collect");

	// Update buffers
				
	{			
		V6_ASSERT( GRID_COUNT <= HLSL_MIP_MAX_COUNT );

		float gridScales[HLSL_MIP_MAX_COUNT];
		core::Vec3 gridCenters[HLSL_MIP_MAX_COUNT];
		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID, gridScale *= 2 )
		{
			gridScales[gridID] = gridScale;
			gridCenters[gridID] = Codec_ComputeGridCenter( samplePos, gridScale, HLSL_GRID_MACRO_HALF_WIDTH );
		}
		for ( core::u32 gridID = GRID_COUNT; gridID < 16; ++gridID )
		{
			gridScales[gridID] = gridScales[GRID_COUNT-1];
			gridCenters[gridID] = gridCenters[GRID_COUNT-1];
		}

		v6::hlsl::CBSample* cbSample = ConstantBuffer_MapWrite< v6::hlsl::CBSample >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_SAMPLE] );

		cbSample->c_sampleDepthLinearScale = -1.0f / ZNEAR;
		cbSample->c_sampleDepthLinearBias = 1.0f / ZNEAR;
		cbSample->c_sampleInvCubeSize.x = 1.0f / CUBE_SIZE;
		cbSample->c_sampleInvCubeSize.y = 1.0f / CUBE_SIZE;
		cbSample->c_samplePos = *samplePos;
		cbSample->c_sampleFaceID = faceID;
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleMipBoundaries[gridID] = core::Vec4_Make( &gridCenters[gridID], gridScales[gridID] );
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleInvGridScales[gridID] = core::Vec4_Make( 1.0f / gridScales[gridID], 0.0f, 0.0f , 0.0f );

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_SAMPLE] );
	}

	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_sampleContext.indirectArgs.uav, values );
		
	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBSampleSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_SAMPLE].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, &m_cubeContext.colorSRV );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, &m_cubeContext.depthSRV );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_SLOT, 1, &m_sampleContext.samples.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sampleContext.indirectArgs.uav, nullptr );
	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_SAMPLECOLLECT].m_computeShader, nullptr, 0 );
		
	const core::u32 cubeGroupCount = (m_cubeContext.size / HLSL_CELL_SUPER_SAMPLING_WIDTH) >> 3;
	gpuContext.deviceContext->Dispatch( cubeGroupCount, cubeGroupCount, 1 );

	// Unset		
	gpuContext.deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	gpuContext.userDefinedAnnotation->EndEvent();
	
	if ( s_logReadBack )
	{
		// Read back
		const core::u32* collectedIndirectArgs = GPUBuffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &m_sampleContext.indirectArgs );
		
		V6_MSG( "\n" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_groupCountX_offset], "groupCountX" );
		V6_ASSERT( collectedIndirectArgs[sample_groupCountY_offset] == 1 );
		V6_ASSERT( collectedIndirectArgs[sample_groupCountZ_offset] == 1 );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_count_offset], "count" );		
		V6_ASSERT( collectedIndirectArgs[sample_count_offset] <= m_config.sampleCount );
#if HLSL_DEBUG_COLLECT == 1
		ReadBack_Log( "sample", collectedIndirectArgs[sample_pixelCount_offset], "pixelCount" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_pixelSampleCount_offset], "pixelSampleCount" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_out_offset], "out" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_error_offset], "error" );
#if 0
		ReadBack_Log( "sample", collectedIndirectArgs[sample_occupancy_offset], "occupancy" );
		for ( core::u32 sampleID = 0; sampleID < 144; ++sampleID )
		{
			core::u32 value = collectedIndirectArgs[sample_cellCoords_offset( sampleID )];
			ReadBack_Log( "sample", *((float*)&value), "cellCoords.x" );
		}
#endif
		V6_ASSERT( collectedIndirectArgs[sample_error_offset] == 0 );
#endif // #if HLSL_DEBUG_COLLECT == 1

		GPUBuffer_UnmapReadBack( gpuContext.deviceContext, &m_sampleContext.indirectArgs );
	}
}

void CRenderingDevice::ClearNode()
{
	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_octreeContext.indirectArgs.uav, values );
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_octreeContext.firstChildOffsetsLimitedUAV, values );
}

core::u32 CRenderingDevice::BuildNode()
{
	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"BuildNode");

	V6_ASSERT( GRID_COUNT <= HLSL_MIP_MAX_COUNT );	

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, &m_sampleContext.samples.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sampleContext.indirectArgs.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, &m_octreeContext.sampleNodeOffsets.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octreeContext.firstChildOffsets.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, &m_octreeContext.leaves.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, &m_octreeContext.indirectArgs.uav, nullptr );		

	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BUILDINNER].m_computeShader, nullptr, 0 );

	for ( core::u32 level = 0; level < HLSL_GRID_SHIFT; ++level )
	{
		// Update buffers				
		{
			v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
			cbOctree->c_octreeCurrentLevel = level;
			cbOctree->c_octreeCurrentBucket = 0;
			ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
		}

		if ( level == HLSL_GRID_SHIFT-1 )
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BUILDLEAF].m_computeShader, nullptr, 0 );

		gpuContext.deviceContext->DispatchIndirect( m_sampleContext.indirectArgs.buf, sample_groupCountX_offset * sizeof( core::u32 ) );
	}

	// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	gpuContext.userDefinedAnnotation->EndEvent();

	const core::u32* octreeIndirectArgs = GPUBuffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &m_octreeContext.indirectArgs );	

	if ( s_logReadBack )
	{
		V6_MSG( "\n" );
		ReadBack_Log( "octreeContext", octreeIndirectArgs[octree_nodeCount_offset], "nodeCount" );
		V6_ASSERT( octreeIndirectArgs[octree_nodeCount_offset] <= m_config.nodeCount );
		ReadBack_Log( "octreeContext", octreeIndirectArgs[octree_leafGroupCountX_offset], "leafGroupCountX" );
		V6_ASSERT( octreeIndirectArgs[octree_leafGroupCountY_offset] == 1 );
		V6_ASSERT( octreeIndirectArgs[octree_leafGroupCountZ_offset] == 1 );
		ReadBack_Log( "octreeContext", octreeIndirectArgs[octree_leafCount_offset], "leafCount" );
	}

	const core::u32 leafCount = octreeIndirectArgs[octree_leafCount_offset];
	V6_ASSERT( leafCount <= m_config.leafCount );

	GPUBuffer_UnmapReadBack( gpuContext.deviceContext, &m_octreeContext.indirectArgs );

	return leafCount;
}

void CRenderingDevice::FillLeaf()
{
	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"FillLeaf");

	V6_ASSERT( GRID_COUNT <= HLSL_MIP_MAX_COUNT );

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, &m_sampleContext.samples.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sampleContext.indirectArgs.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, &m_octreeContext.sampleNodeOffsets.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octreeContext.firstChildOffsets.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, &m_octreeContext.leaves.uav, nullptr );
	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_FILLLEAF].m_computeShader, nullptr, 0 );

	// Update buffers				
	{
		v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
		cbOctree->c_octreeCurrentLevel = 0;
		cbOctree->c_octreeCurrentBucket = 0;
		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );		
	}

	gpuContext.deviceContext->DispatchIndirect( m_sampleContext.indirectArgs.buf, sample_groupCountX_offset * sizeof( core::u32 ) );

	// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	gpuContext.userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::PackColor( core::u32 blockCounts[HLSL_BUCKET_COUNT], BlockContext_s* blockContext )
{
	static const void* nulls[8] = {};
	
	gpuContext.userDefinedAnnotation->BeginEvent( L"Pack");

	V6_ASSERT( GRID_COUNT <= HLSL_MIP_MAX_COUNT );

	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( blockContext->blockIndirectArgs.uav, values );

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octreeContext.firstChildOffsets.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_LEAF_SLOT, 1, &m_octreeContext.leaves.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, &m_octreeContext.indirectArgs.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_POS_SLOT, 1, &blockContext->blockPos.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_DATA_SLOT, 1, &blockContext->blockData.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &blockContext->blockIndirectArgs.uav, nullptr );
	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_PACKCOLOR].m_computeShader, nullptr, 0 );

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		// Update buffers
		{
			v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
			cbOctree->c_octreeCurrentLevel = 0;
			cbOctree->c_octreeCurrentBucket = bucket;
			ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
		}

		gpuContext.deviceContext->DispatchIndirect( m_octreeContext.indirectArgs.buf, octree_leafGroupCountX_offset * sizeof( core::u32 ) );
	}

	// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_POS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	gpuContext.userDefinedAnnotation->EndEvent();

	const core::u32* blockIndirectArgs = GPUBuffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &blockContext->blockIndirectArgs );

	core::u32 allBlockCount = 0;
	core::u32 allRealCellCount = 0;
	core::u32 allMaxCellCount = 0; 

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		blockCounts[bucket] = block_count( bucket );
		if ( block_count( bucket ) == 0 )
			continue;

		static const core::u32 cellPerBucketCounts[] = { 4, 8, 16, 32, 64 };
		const core::u32 maxCellCount = block_count( bucket ) * cellPerBucketCounts[bucket];

		if ( s_logReadBack )
		{
			V6_MSG( "\n" );
			ReadBack_Log( "blockContext", bucket, "bucket" );
			ReadBack_Log( "blockContext", block_groupCountX( bucket ), "groupCountX" );
			V6_ASSERT( block_groupCountY( bucket ) == 1 );
			V6_ASSERT( block_groupCountZ( bucket ) == 1 );
			ReadBack_Log( "blockContext", block_count( bucket ), "blockCount" );
			ReadBack_Log( "blockContext", block_posOffset( bucket ), "posOffset" );
			ReadBack_Log( "blockContext", block_dataOffset( bucket ), "dataOffset" );
			ReadBack_Log( "blockContext", block_cellCount( bucket ), "realCellCount" );
			ReadBack_Log( "blockContext", maxCellCount, "maxCellCount" );
#if HLSL_DEBUG_OCCUPANCY == 1
			ReadBack_Log( "blockContext", block_uniqueOccupancyCount( bucket ) / (float)block_count( bucket ), "avgOccupancyCount" );
			ReadBack_Log( "blockContext", block_uniqueOccupancyMax( bucket ), "maxOccupancyCount" );
			ReadBack_Log( "blockContext", block_slotOccupancyCount( bucket ) / (float)block_cellCount( bucket ), "avgOccupancySlot" );
#endif // #if HLSL_DEBUG_OCCUPANCY == 1
		}

		allBlockCount += block_count( bucket );
		allRealCellCount += block_cellCount( bucket );
		allMaxCellCount += maxCellCount;
	}		

	V6_MSG( "\n" );
	ReadBack_Log( "packed", allBlockCount, "blockCount" );
	ReadBack_Log( "packed", allRealCellCount, "realCellCount" );
	ReadBack_Log( "packed", allMaxCellCount, "maxCellCount" );
	V6_ASSERT( allMaxCellCount <= m_config.cellCount );

	GPUBuffer_UnmapReadBack( gpuContext.deviceContext, &blockContext->blockIndirectArgs );
}

void CRenderingDevice::CullBlock( const RenderingView_s* views, const core::Vec3* buildOrigin, float gridMinScale, core::u32 groupCounts[CODEC_BUCKET_COUNT], core::u32 frameID )
{		
	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"Cull Blocks");

	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_traceContext->traceIndirectArgs.uav, values );
	if ( s_logReadBack )
		gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_traceContext->cullStats.uav, values );

	v6::hlsl::CBCull cbCullData = {};
	{
		float gridScale = gridMinScale;
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID, gridScale *= 2.0f )
		{
			const float cellScale = gridScale * HLSL_GRID_INV_WIDTH;
			cbCullData.c_cullGridScales[gridID] = core::Vec4_Make( gridScale, 0.0f, 0.0f, 0.0f );
			const core::Vec3 center = Codec_ComputeGridCenter( buildOrigin, gridScale, HLSL_GRID_MACRO_HALF_WIDTH );
			cbCullData.c_cullCenters[gridID] = core::Vec4_Make( &center, 0.0f );
		}

		V6_ASSERT( views[LEFT_EYE].forward == views[RIGHT_EYE].forward );
		V6_ASSERT( views[LEFT_EYE].right == views[RIGHT_EYE].right );
		V6_ASSERT( views[LEFT_EYE].up == views[RIGHT_EYE].up );

		const float tanHalfFOVLeft = core::Max( views[LEFT_EYE].tanHalfFOVLeft, views[RIGHT_EYE].tanHalfFOVLeft );
		const float tanHalfFOVRight = core::Max( views[LEFT_EYE].tanHalfFOVRight, views[RIGHT_EYE].tanHalfFOVRight );
		const float tanHalfFOVUp = core::Max( views[LEFT_EYE].tanHalfFOVUp, views[RIGHT_EYE].tanHalfFOVUp );
		const float tanHalfFOVDown = core::Max( views[LEFT_EYE].tanHalfFOVDown, views[RIGHT_EYE].tanHalfFOVDown );

		core::Vec3 leftPlane = (views[ANY_EYE].forward * tanHalfFOVLeft + views[ANY_EYE].right).Normalized();
		core::Vec3 rightPlane = (views[ANY_EYE].forward * tanHalfFOVRight - views[ANY_EYE].right).Normalized();
		core::Vec3 upPlane = (views[ANY_EYE].forward * tanHalfFOVUp - views[ANY_EYE].up).Normalized();
		core::Vec3 bottomPlane = (views[ANY_EYE].forward * tanHalfFOVDown + views[ANY_EYE].up).Normalized();
		
		cbCullData.c_cullFrustumPlanes[0] = core::Vec4_Make( &leftPlane, -core::Dot( leftPlane, views[LEFT_EYE].org ) );
		cbCullData.c_cullFrustumPlanes[1] = core::Vec4_Make( &rightPlane, -core::Dot( rightPlane, views[RIGHT_EYE].org ) );
		cbCullData.c_cullFrustumPlanes[2] = core::Vec4_Make( &upPlane, -core::Dot( upPlane, views[ANY_EYE].org ) );
		cbCullData.c_cullFrustumPlanes[3] = core::Vec4_Make( &bottomPlane, -core::Dot( bottomPlane, views[ANY_EYE].org ) );
	}

	// set

	core::u32 bufferID = frameID & 1;
	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBCullSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_CULL].buf );	
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_GROUP_SLOT, 1, &m_sequenceContext->groups[bufferID].srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_RANGE_SLOT, 1, &m_sequenceContext->ranges[bufferID].srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, &m_sequenceContext->blockPos.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELLS_SLOT, 1, &m_traceContext->traceCell.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_traceContext->traceIndirectArgs.uav, nullptr );
	if ( s_logReadBack )
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, &m_traceContext->cullStats.uav, nullptr );

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		gpuContext.userDefinedAnnotation->BeginEvent( L"Cull Bucket");

		// update
		{
			v6::hlsl::CBCull* cbCull = ConstantBuffer_MapWrite< v6::hlsl::CBCull >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_CULL] );
			memcpy( cbCull, &cbCullData, sizeof( cbCullData ) );
			ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_CULL] );

			cbCullData.c_cullBlockGroupOffset += groupCounts[bucket];
			cbCullData.c_cullBlockRangeOffset += m_sequence.frameDescArray[frameID].blockRangeCounts[bucket];
		}

		// dispach
		if ( s_logReadBack )
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_CULL_STATS4+bucket].m_computeShader, nullptr, 0 );
		else
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_CULL4+bucket].m_computeShader, nullptr, 0 );
		gpuContext.deviceContext->Dispatch( groupCounts[bucket], 1, 1 );

		gpuContext.userDefinedAnnotation->EndEvent();
	}
	
	// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_GROUP_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_RANGE_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELLS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( s_logReadBack )
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	gpuContext.userDefinedAnnotation->EndEvent();

	if ( s_logReadBack )
	{
		V6_MSG( "\n" );

		{
			const hlsl::BlockCullStats* blockCullStats = GPUBuffer_MapReadBack< hlsl::BlockCullStats >( gpuContext.deviceContext, &m_traceContext->cullStats );

			ReadBack_Log( "blockCull", blockCullStats->blockInputCount, "blockInputCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockProcessedCount, "blockProcessedCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockPassedCount, "blockPassedCount" );
			ReadBack_Log( "blockCull", blockCullStats->cellOutputCount, "cellOutputCount" );

			GPUBuffer_UnmapReadBack( gpuContext.deviceContext, &m_traceContext->cullStats );
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

bool CRenderingDevice::InitTraceMode( core::u32 frameCount )
{
	char templateFilename[256];
	char sequenceFilename[256];

	Scene_MakeRawFrameFileTemplate( s_activeScene, templateFilename, sizeof( templateFilename ) );
	Scene_MakeSequenceFilename( s_activeScene, sequenceFilename, sizeof( sequenceFilename ) );

	if ( !core::Sequence_Encode( templateFilename, frameCount, sequenceFilename, m_heap ) )
		return false;

	if ( !core::Sequence_Load( sequenceFilename, &m_sequence, m_heap ) )
		return false;
	
	m_sequenceContext = m_heap->newInstance< SequenceContext_s >();
	m_traceContext = m_heap->newInstance< TraceContext_s >();

	SequenceContext_CreateFromData( gpuContext.device, m_sequenceContext, &m_sequence );
	TraceContext_Create( gpuContext.device, m_traceContext, &m_config );

	return true;
}

void CRenderingDevice::ReleaseTraceMode()
{
	core::Sequence_Release( &m_sequence, m_heap );
	SequenceContext_Release( gpuContext.device, m_sequenceContext );
	TraceContext_Release( gpuContext.device, m_traceContext );

	m_heap->deleteInstance( m_sequenceContext );
	m_heap->deleteInstance( m_traceContext );
}

void CRenderingDevice::TraceBlock( const RenderingView_s* views, const core::Vec3* buildOrigin )
{		
	static const void* nulls[8] = {};
	
	gpuContext.userDefinedAnnotation->BeginEvent( L"Trace Blocks");

	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_traceContext->cellItemCounters.uav, values );
	if ( s_logReadBack )
		gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_traceContext->traceStats.uav, values );

	float gridScale = GRID_MIN_SCALE;

	{
		v6::hlsl::CBBlock* cbBlock = ConstantBuffer_MapWrite< v6::hlsl::CBBlock >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_BLOCK] );

		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID, gridScale *= 2.0f )
		{
			const float cellScale = gridScale * HLSL_GRID_INV_WIDTH;
			cbBlock->c_blockGridScales[gridID] = core::Vec4_Make( gridScale, cellScale, ((1 << 21) - 1) / gridScale, 0.0f );
			const core::Vec3 center = Codec_ComputeGridCenter( buildOrigin, gridScale, HLSL_GRID_MACRO_HALF_WIDTH );
			cbBlock->c_blockGridCenters[gridID] = core::Vec4_Make( &center, 0.0f );
		}

		const core::Vec2 frameSize = core::Vec2_Make( (float)m_width, (float)m_height );
		cbBlock->c_blockFrameSize = frameSize;
				
		cbBlock->c_blockFrameSize.x = (float)m_config.screenWidth;
		cbBlock->c_blockFrameSize.y = (float)m_config.screenHeight;

		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
		{	
			hlsl::BlockPerEye blockPerEye;
			blockPerEye.objectToView = views[eye].viewMatrix;
			blockPerEye.viewToProj = views[eye].projMatrix;

			const float scaleRight = (views[eye].tanHalfFOVLeft + views[eye].tanHalfFOVRight) / m_config.screenWidth;
			const float scaleUp = (views[eye].tanHalfFOVUp + views[eye].tanHalfFOVDown) / m_config.screenHeight;

			const core::Vec3 forward = views[eye].forward;
			const core::Vec3 right = views[eye].right * scaleRight;
			const core::Vec3 up = views[eye].up * scaleUp;
				
			blockPerEye.org = views[eye].org;

			blockPerEye.rayDirBase = forward - views[eye].up * views[eye].tanHalfFOVDown + 0.5f * up - views[eye].right * views[eye].tanHalfFOVLeft + 0.5f * right;
			blockPerEye.rayDirUp = up;
			blockPerEye.rayDirRight = right;

			cbBlock->c_blockEyes[eye] = blockPerEye;

			cbBlock->c_blockGetStats = s_logReadBack;
			cbBlock->c_blockShowFlag = (g_showMip ? HLSL_BLOCK_SHOW_FLAG_MIPS : 0) | (g_showBucket ? HLSL_BLOCK_SHOW_FLAG_BUCKETS : 0);
		}

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_BLOCK] );
	}

	// set

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBBlockSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_BLOCK].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, &m_sequenceContext->blockData.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_CELLS_SLOT, 1, &m_traceContext->traceCell.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_SLOT, 1, &m_traceContext->cellItems.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, &m_traceContext->cellItemCounters.uav, nullptr );
	if ( s_logReadBack )
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, &m_traceContext->traceStats.uav, nullptr );
	
	{
		gpuContext.userDefinedAnnotation->BeginEvent( L"Init Trace");

		// set
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_traceContext->traceIndirectArgs.uav, nullptr );
				
		// dispatch
		gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_TRACE_INIT].m_computeShader, nullptr, 0 );				
		gpuContext.deviceContext->Dispatch( 1, 1, 1 );

		// unset
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

		gpuContext.userDefinedAnnotation->EndEvent();
	}

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		gpuContext.userDefinedAnnotation->BeginEvent( L"Trace Bucket");

		// set

		gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_traceContext->traceIndirectArgs.srv );

		// dispach
		if ( s_logReadBack | g_showMip | g_showBucket )
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_TRACE_DEBUG4+bucket].m_computeShader, nullptr, 0 );
		else
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_TRACE4+bucket].m_computeShader, nullptr, 0 );
		gpuContext.deviceContext->DispatchIndirect( m_traceContext->traceIndirectArgs.buf, trace_cellGroupCountX_offset( bucket ) * sizeof( core::u32 ) );

		// unset		
		gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );

		gpuContext.userDefinedAnnotation->EndEvent();
	}

	// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_CELLS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( s_logReadBack )
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	
	gpuContext.userDefinedAnnotation->EndEvent();

	if ( s_logReadBack )
	{			
		V6_MSG( "\n" );

		{
			const hlsl::BlockTraceStats* blockTraceStats = GPUBuffer_MapReadBack< hlsl::BlockTraceStats >( gpuContext.deviceContext, &m_traceContext->traceStats );

			ReadBack_Log( "blockTrace", blockTraceStats->cellInputCount, "cellInputCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellProcessedCount, "cellProcessedCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelSampleCount, "pixelSampleCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellItemCount, "cellItemCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellItemMaxCountPerPixel, "cellItemMaxCountPerPixel" );
			V6_ASSERT( blockTraceStats->cellItemCount < m_config.cellItemCount );

			GPUBuffer_UnmapReadBack( gpuContext.deviceContext, &m_traceContext->traceStats );
		}
	}
}

void CRenderingDevice::BlendPixel( const RenderingView_s* view )
{
	// Render

	gpuContext.userDefinedAnnotation->BeginEvent( L"Blend Pixels");
	
	// set

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBPixelSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_PIXEL].buf );

	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_SLOT, 1, &m_traceContext->cellItems.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, &m_traceContext->cellItemCounters.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_LCOLOR_SLOT, 1, &view->uav, nullptr );
	if ( g_showOverdraw	)
		gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLENDPIXEL_OVERDRAW].m_computeShader, nullptr, 0 );
	else
		gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLENDPIXEL].m_computeShader, nullptr, 0 );

	{
		v6::hlsl::CBPixel* cbPixel = ConstantBuffer_MapWrite< v6::hlsl::CBPixel >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_PIXEL] );

		cbPixel->c_pixelFrameSize.x = m_config.screenWidth;
		cbPixel->c_pixelFrameSize.y = m_config.screenHeight;

		if ( g_randomBackground )
		{
			const float r = core::RandFloat();
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

		cbPixel->c_eye = view->eye;

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_PIXEL]  );
	}		

	V6_ASSERT( (m_width & 0x7) == 0 );
	V6_ASSERT( (m_height & 0x7) == 0 );
	const core::u32 pixelGroupWidth = m_width >> 3;
	const core::u32 pixelGroupHeight = m_height >> 3;
	gpuContext.deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_LCOLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	gpuContext.userDefinedAnnotation->EndEvent();	
}

void CRenderingDevice::Output( ID3D11ShaderResourceView* srvLeft, ID3D11ShaderResourceView* srvRight )
{
#if HLSL_STEREO == 1
	// Render

	gpuContext.userDefinedAnnotation->BeginEvent( L"Compose Surface" );

	// set

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBComposeSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_COMPOSE].buf );

	gpuContext.deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, &srvLeft );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, &srvRight );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, &gpuContext.surfaceUAV, nullptr );
		
	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_COMPOSESURFACE].m_computeShader, nullptr, 0 );

	{
		v6::hlsl::CBCompose* cbCompose = ConstantBuffer_MapWrite< v6::hlsl::CBCompose >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_COMPOSE] );
		
		cbCompose->c_composeFrameWidth = m_config.screenWidth;

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_COMPOSE]  );
	}		

	V6_ASSERT( (m_width & 0x7) == 0 );
	V6_ASSERT( (m_height & 0x7) == 0 );
	const core::u32 pixelGroupWidth = (m_width >> 3) * HLSL_EYE_COUNT;
	const core::u32 pixelGroupHeight = m_height >> 3;
	gpuContext.deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	gpuContext.deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	gpuContext.userDefinedAnnotation->EndEvent();	
#else	
	ID3D11Resource* colorBuffer;
	srvLeft->GetResource( &colorBuffer );
	gpuContext.deviceContext->CopyResource( gpuContext.surfaceBuffer, colorBuffer );
#endif
}

bool CRenderingDevice::HasValidRawFrameFile( core::u32 frameID )
{
	if ( s_activeScene->filename[0] == 0 )
	{
		V6_ERROR( "Null scene file.\n" );
		return false;
	}

	char path[256];
	Scene_MakeRawFrameFilename( s_activeScene, path, sizeof( path ), frameID );

	core::CFileReader fileReader;
	if ( !fileReader.Open( path ) )
	{
		V6_ERROR( "Unable to open file %s.\n", path );
		return false;
	}

	core::CodecRawFrameDesc_s frameDesc;

	if ( !core::Codec_ReadRawFrameHeader( &fileReader, &frameDesc ) )
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

bool CRenderingDevice::WriteRawFrameFile( const core::u32 blockCounts[HLSL_BUCKET_COUNT], BlockContext_s* blockContext, core::u32 frameID )
{
	if ( s_activeScene->filename[0] == 0 )
	{
		V6_ERROR( "Null scene file.\n" );
		return false;
	}

	bool hasError = false;

	core::u32 blockCount = 0;
	core::u32 cellCount = 0;

	core::u32 cellPerBucketCount = 4;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockCount += blockCounts[bucket];
		cellCount += blockCounts[bucket] * cellPerBucketCount;
	}

	{
		char path[256];
		Scene_MakeRawFrameFilename( s_activeScene, path, sizeof( path ), frameID );

		core::CFileWriter fileWriter;
		if ( fileWriter.Open( path ) )
		{
			core::CodecRawFrameDesc_s frameDesc = {};
			frameDesc.origin = s_buildOrigin;
			frameDesc.frameID = frameID;
			frameDesc.sampleCount = SAMPLE_MAX_COUNT;
			frameDesc.gridMacroShift = HLSL_GRID_MACRO_SHIFT;
			frameDesc.gridScaleMin = GRID_MIN_SCALE;
			frameDesc.gridScaleMax = GRID_MAX_SCALE;
			memcpy( frameDesc.blockCounts, blockCounts, sizeof( frameDesc.blockCounts ) );
			
			core::CodecRawFrameData_s frameData = {};
			frameData.blockPos = (void*)GPUBuffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &blockContext->blockPos );
			frameData.blockData = (void*)GPUBuffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &blockContext->blockData );

			core::Codec_WriteRawFrame( &fileWriter, &frameDesc, &frameData );
	
			GPUBuffer_UnmapReadBack( gpuContext.deviceContext, &blockContext->blockPos );
			GPUBuffer_UnmapReadBack( gpuContext.deviceContext, &blockContext->blockData );

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

bool CRenderingDevice::BuildBlock( core::u32 frameID )
{
	static core::u32 lastSumLeafCount = 0;

	if ( g_sample == 0 )
	{
		s_buildOrigin = s_headOffset;

		if ( HasValidRawFrameFile( frameID ) )
		{
			g_sample = SAMPLE_MAX_COUNT;
			return true;
		}

		CubeContext_Create( gpuContext.device, &m_cubeContext, CUBE_SIZE );
		SampleContext_Create( gpuContext.device, &m_sampleContext, &m_config );
		OctreeContext_Create( gpuContext.device, &m_octreeContext, &m_config );

		lastSumLeafCount = 0;
		ClearNode();
	}

	V6_MSG( "Capturing sample #%03d...", g_sample );

	const core::Vec3 sampleOffset = m_sampleOffsets[g_sample];
	const core::Vec3 samplePos = s_buildOrigin + sampleOffset;

	core::u32 sumLeafCount = 0;

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		Capture( &samplePos, faceID );
		Collect( &samplePos, faceID );
		sumLeafCount += BuildNode();
		FillLeaf();
	}

	const core::u32 newLeafCount = sumLeafCount - lastSumLeafCount;
	lastSumLeafCount = sumLeafCount;

	V6_MSG( "\r" );
	V6_MSG( "Captured  sample #%03d: %13s cells added\n", g_sample, FormatInteger_Unsafe( newLeafCount ) );

	++g_sample;

	v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T1] );

	if ( g_sample == SAMPLE_MAX_COUNT )
	{
		V6_MSG( "Packing all samples..." );

		CubeContext_Release( &m_cubeContext );
		SampleContext_Release( gpuContext.device, &m_sampleContext );

		{
			BlockContext_s blockContext = {};
			BlockContext_Create( gpuContext.device, &blockContext, &m_config);

			core::u32 blockCounts[HLSL_BUCKET_COUNT] = {};
			PackColor( blockCounts, &blockContext );

			OctreeContext_Release( gpuContext.device, &m_octreeContext );

			if ( !WriteRawFrameFile( blockCounts, &blockContext, frameID ) )
				return false;

			BlockContext_Release( gpuContext.device, &blockContext );
		}
		
		V6_MSG( "\r" );
		V6_MSG( "Packed  all samples: %13s cells added @ frame %d\n", FormatInteger_Unsafe( sumLeafCount ), frameID );
		s_logReadBack = false;
	}

	return true;
}

void CRenderingDevice::Draw( float dt )
{
	static int lastKeyPosX = -1;
	static int lastKeyPosZ = -1;

	const core::u32 frameID = PathPlayer_GetFrame( &s_pathPlayer );
	
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
	
	core::Mat4x4 orientationMatrix;
#if V6_USE_HMD
	HmdRenderTarget_s renderTargets[2];
	HmdEyePose_s eyePoses[2];
	s_hmdState = v6::viewer::Hmd_BeginRendering( renderTargets, eyePoses, ZNEAR, ZFAR );
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( s_yaw );
		core::Mat4x4_Mul3x3( &orientationMatrix, yawMatrix, eyePoses[0].lookAt );
		core::Mat4x4_Transpose( &orientationMatrix );
	}
	else
#endif // #if V6_USE_HMD
	{
		s_yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
		s_pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

		const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( s_yaw );
		const core::Mat4x4 pitchMatrix = core::Mat4x4_RotationX( s_pitch );
		core::Mat4x4_Mul( &orientationMatrix, yawMatrix, pitchMatrix );
		core::Mat4x4_Transpose( &orientationMatrix );
	}

	const core::Vec3 forward = -orientationMatrix.GetZAxis()->Normalized();
	const core::Vec3 right = orientationMatrix.GetXAxis()->Normalized();
	const core::Vec3 up = orientationMatrix.GetYAxis()->Normalized();

	if ( keyDeltaX || keyDeltaZ )
	{
		s_headOffset += right * (float)keyDeltaX * g_translation_speed * dt;
		s_headOffset += forward * (float)keyDeltaZ * g_translation_speed * dt;
	}
	
	if ( g_limit )
	{
		core::Vec3 distanceToCenter = s_headOffset - s_buildOrigin;
		distanceToCenter.x = core::Clamp( distanceToCenter.x, -FREE_SCALE, FREE_SCALE );
		distanceToCenter.y = core::Clamp( distanceToCenter.y, -FREE_SCALE, FREE_SCALE );
		distanceToCenter.z = core::Clamp( distanceToCenter.z, -FREE_SCALE, FREE_SCALE );
		s_headOffset = s_buildOrigin + distanceToCenter;
	}

	if ( g_showPath || s_pathPlayer.isPlaying )
	{
		Scene_UpdatePathGeo( m_pathGeoScene, &s_paths[s_activePath], gpuContext.deviceContext );
		PathPlayer_Compute( &s_pathPlayer, s_paths, PATH_COUNT );
	}

	if ( s_pathPlayer.isPlaying )
	{
		if ( s_activePath == PATH_CAMERA )
			PathPlayer_GetPosition( &s_headOffset, &s_pathPlayer, PATH_CAMERA );
		for ( core::u32 pathID = PATH_CAMERA+1; pathID < PATH_COUNT; ++pathID )
		{
			core::Vec3 entityPosition;
			const core::u32 entityID = s_paths[pathID].entityID;
			if ( entityID != (core::u32)-1 && PathPlayer_GetPosition( &entityPosition, &s_pathPlayer, pathID ) )
				Entity_SetPos( &s_activeScene->entities[entityID], &entityPosition );
		}
	}

	RenderingView_s views[HLSL_EYE_COUNT];
#if V6_USE_HMD
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
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
		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
		{
			RenderingView_MakeForStereo( &views[eye], &s_headOffset, &forward, &up, &right, eye, m_aspectRatio );
			views[eye].texture2D = gpuContext.colorBuffers[eye];
			views[eye].rtv = gpuContext.colorViews[eye];
			views[eye].uav = gpuContext.colorUAVs[eye];
		}
	}

	if ( g_drawMode == DRAW_MODE_DEFAULT )
	{
		// draw mode

		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T0] );

		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
			DrawWorld( &views[eye] );
		
		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T1] );
		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T2] );
		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T3] );
		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T4] );

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

			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T0] );

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

			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T2] );
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T3] );
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T4] );
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

			V6_ASSERT( m_bakedFrameCount == -1 );
			const core::CodecFrameDesc_s* frameDesc = &m_sequence.frameDescArray[frameID];

			core::u32 groupCounts[CODEC_BUCKET_COUNT] = {};
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T0] );
			{
				SequenceContext_UpdateFrameData( gpuContext.deviceContext, m_sequenceContext, groupCounts, frameID, &m_sequence, m_stack );
			}
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T1] );
			{
				CullBlock( views, &frameDesc->origin, m_sequence.desc.gridScaleMin, groupCounts, frameID );
			}
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T2] );
			{
				TraceBlock( views, &frameDesc->origin );
			}
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T3] );
			{
				for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
					BlendPixel( &views[eye] );
			}
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T4] );

			s_logReadBack = false;
			g_mousePicked = false;

			PathPlayer_AddTimeStep( &s_pathPlayer, 1.0f / HMD_FPS );
		}
	}	
	
#if V6_USE_HMD
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		HmdOuput_s output;
		if ( Hmd_EndRendering( &output ) )
			gpuContext.deviceContext->CopyResource( gpuContext.surfaceBuffer, (ID3D11Texture2D*)output.texture2D );
	}
	else
#endif // #if V6_USE_HMD
	{
		if ( g_showPath )
		{
			for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
				DrawCameraPath( &views[eye] );
		}

		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
			DrawDebug( &views[eye] );

		Output( gpuContext.colorSRVs[LEFT_EYE], gpuContext.colorSRVs[RIGHT_EYE] );
	}
}

void CRenderingDevice::Present()
{
	gpuContext.swapChain->Present( 0, 0 );
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

	for ( core::u32 pathID = 0; pathID < PATH_COUNT; ++pathID )
		Path_Release( &s_paths[pathID] );

	GPUContext_Release( &gpuContext );
}

END_V6_VIEWER_NAMESPACE

int main()
{
	V6_MSG( "Viewer 0.1\n" );

	v6::core::CHeap heap;
	v6::core::Stack stack( &heap, 100 * 1024 * 1024 );
	v6::core::CFileSystem filesystem;
		
#if V6_LOAD_EXTERNAL == 1
	v6::core::Stack stackScene( &heap, 400 * 1024 * 1024 );

	const char* filename = "D:/media/obj/crytek-sponza/sponza.obj";
	//const char* filename = "D:/media/obj/dragon/dragon.obj";
	//const char* filename = "D:/media/obj/buddha/buddha.obj";
	//const char* filename = "D:/media/obj/head/head.obj";
	//const char* filename = "D:/media/obj/hairball/hairball.obj"; // 100.0f
	//const char* filename = "D:/media/obj/hairball/hairball_simple.obj"; // 100.0f
	//const char* filename = "D:/media/obj/hairball/hairball_simple2.obj"; // 100.0f
	//const char* filename = "D:/media/obj/sibenik/sibenik.obj"; // 100.0f
	//const char* filename = "D:/media/obj/conference/conference.obj";

	v6::viewer::SceneContext_s sceneContext;
	SceneContext_Create( &sceneContext, &stackScene );
	SceneContext_SetFilename( &sceneContext, filename );

	v6::core::Job_Launch( v6::viewer::SceneContext_Load, &sceneContext );
#endif	

	const char* const title = "V6";

#if V6_USE_HMD
	if ( !v6::viewer::Hmd_Init() )
	{
		V6_ERROR( "Call to Hmd_Init failed!\n" );
		return -1;
	}

	v6::core::Vec2i renterTargerSize = v6::viewer::Hmd_GetRecommendedRenderTargetSize();
	v6::core::u32 maxRenderTargetSize = HLSL_GRID_WIDTH >> 1;
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
	v6::core::Vec2i renterTargerSize = v6::core::Vec2i_Make( HLSL_GRID_WIDTH >> 1, HLSL_GRID_WIDTH >> 1 );
#endif // #if V6_USE_HMD

	HWND hWnd = v6::viewer::CreateMainWindow( title, 1920 - renterTargerSize.x, 48, renterTargerSize.x * HLSL_EYE_COUNT, renterTargerSize.y );
	if (!hWnd)
	{
		V6_ERROR( "Call to CreateWindow failed!\n" );
		return -1;
	}

	if ( !v6::viewer::CaptureInputs( hWnd ) )
	{
		V6_ERROR( "Call to CaptureInputs failed!\n" );
		return -1;
	}

	v6::viewer::CRenderingDevice oRenderingDevice;
	if ( !oRenderingDevice.Create( renterTargerSize.x, renterTargerSize.y, hWnd, &filesystem, &heap, &stack ) )
	{
		V6_ERROR( "Call to CRenderingDevice::Create failed!\n" );
		return -1;
	}

#if V6_USE_HMD
	if ( !v6::viewer::Hmd_CreateResources( oRenderingDevice.gpuContext.device, &renterTargerSize ) )
	{
		V6_ERROR( "Call to Hmd_CreateResources failed!\n" );
		return -1;
	}
#endif // #if V6_USE_HMD

#if V6_LOAD_EXTERNAL == 1
	SceneContext_SetDevice( &sceneContext, oRenderingDevice.gpuContext.device );
#endif

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	__int64 frameTickLast = GetTickCount(); 
	for ( __int64 frameId = 0; ; ++frameId )
	{
		const v6::core::u32 bufferID = frameId & 1;

		v6::viewer::GPUQuery_s* pendingQueries = oRenderingDevice.gpuContext.queries[bufferID];
		static const int tMaxCount = 64;
		static float dts[tMaxCount] = {};
		static float tfTimes[tMaxCount] = {};
		static float tbTimes[v6::viewer::QUERY_TIME_COUNT][tMaxCount] = {};
	static int tID = 0;
		
		if ( v6::viewer::GPUQuery_ReadTimeStampDisjoint( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FREQUENCY] ) )
		{
			for ( v6::core::u32 queryID = v6::viewer::QUERY_FRAME_BEGIN; queryID < v6::viewer::QUERY_COUNT; ++queryID )
				v6::viewer::GPUQuery_ReadTimeStamp( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[queryID] );
			
			tfTimes[tID] = v6::viewer::GPUQuery_GetElpasedTime( &pendingQueries[v6::viewer::QUERY_FRAME_BEGIN], &pendingQueries[v6::viewer::QUERY_FRAME_END], &pendingQueries[v6::viewer::QUERY_FREQUENCY] );
			for ( v6::core::u32 timeID = 0; timeID < v6::viewer::QUERY_TIME_COUNT; ++timeID )
				tbTimes[timeID][tID] = v6::viewer::GPUQuery_GetElpasedTime( &pendingQueries[v6::viewer::QUERY_T0+timeID], &pendingQueries[v6::viewer::QUERY_T1+timeID], &pendingQueries[v6::viewer::QUERY_FREQUENCY] );
			tID = (tID + 1) & (tMaxCount-1);
		}
		
		oRenderingDevice.gpuContext.pendingQueries = pendingQueries;
		
		const __int64 frameTick = v6::core::GetTickCount();
		
		__int64 frameUpdatedTick = frameTick;
		float dt = 0.0f;
		for (;;)
		{
			const __int64 frameDelta = frameUpdatedTick - frameTickLast;
			dt = v6::core::Min( v6::core::ConvertTicksToSeconds( frameDelta ), 0.1f );
#if !V6_USE_HMD
			if ( dt + 0.0001f >= 1.0f / v6::viewer::HMD_FPS )
				break;
			SwitchToThread();
			frameUpdatedTick = v6::core::GetTickCount();
#endif // #if !V6_USE_HMD
		}
		dts[frameId & (tMaxCount-1)] = dt;

		frameTickLast = frameTick;

		if ( (frameId % 30) == 0 || v6::viewer::IsBakingMode( v6::viewer::g_drawMode ) ) 
		{
			float ifps = 0;
			float tfTime = 0;
			float tbTime[v6::viewer::QUERY_TIME_COUNT] = {};
				
			for ( int t = 0; t < tMaxCount; ++t )
			{
				ifps += dts[t];
				tfTime += tfTimes[t];
				for ( v6::core::u32 timeID = 0; timeID < v6::viewer::QUERY_TIME_COUNT; ++timeID )
					tbTime[timeID] += tbTimes[timeID][t];
			}

			ifps *= 1.0f / tMaxCount;
			tfTime *= 1.0f / tMaxCount;
			for ( v6::core::u32 timeID = 0; timeID < v6::viewer::QUERY_TIME_COUNT; ++timeID )
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
				v6::viewer::ModeToString( v6::viewer::g_drawMode ),
				v6::viewer::s_hmdState,
				v6::viewer::s_activeScene->info.dirty ? "| *" : "" );
			SetWindowTextA( hWnd, text );
		}
		
		MSG msg;
		while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

			if ( msg.message == WM_QUIT )
			{
#if V6_LOAD_EXTERNAL == 1
				v6::core::Signal_Wait( &sceneContext.loadDone );
				Scene_SaveInfo( v6::viewer::s_activeScene );
				SceneContext_Release( &sceneContext );
#else
				Scene_SaveInfo( v6::viewer::s_activeScene );
#endif
				oRenderingDevice.Release();
				return 0;
			}
		}

		if ( v6::viewer::g_reloadShaders )
		{
			v6::viewer::GPUContext_ReleaseShaders( &oRenderingDevice.gpuContext );
			v6::viewer::GPUContext_CreateShaders( &oRenderingDevice.gpuContext, &filesystem, &stack );
			v6::viewer::g_reloadShaders = false;
		}
				
		v6::viewer::GPUQuery_BeginTimeStampDisjoint( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FREQUENCY] );
		v6::viewer::GPUQuery_WriteTimeStamp( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FRAME_BEGIN] );

		oRenderingDevice.Draw( dt );

		v6::viewer::GPUQuery_WriteTimeStamp( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FRAME_END] );
		v6::viewer::GPUQuery_EndTimeStampDisjoint( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FREQUENCY] );

		oRenderingDevice.Present();
	}

#if V6_USE_HMD
	v6::viewer::Hmd_ReleaseResources();

	v6::viewer::Hmd_Shutdown();
#endif // #if V6_USE_HMD
}
