/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#include <Windowsx.h>
#include <d3d11_1.h>
#pragma warning( pop )

#include <v6/viewer/common.h>
#include <v6/viewer/common_shared.h>

#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/time.h>
#include <v6/core/vec2.h>
#include <v6/core/vec3.h>

#pragma comment(lib, "d3d11.lib")

#define V6_DISABLE_OLD_GRID		1
#define V6_D3D_DEBUG			0

#define V6_ASSERT_D3D11( EXP )  { HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }

#define KB( X )		((X) >> 10)
#define MB( X )		((X) >> 20)
#define GB( X )		((X) >> 30)

BEGIN_V6_VIEWER_NAMESPACE

enum DrawMode_e
{
	DRAW_MODE_DEFAULT,
	DRAW_MODE_CUBE,
	DRAW_MODE_GET,
	
	DRAW_MODE_COUNT
};

static const core::u32 ZOOM					= 1;
static const float ZNEAR					= 10.0f;
static const float ZFAR						= 10000.0f;
static const core::u32 CUBE_SIZE			= HLSL_GRID_WIDTH;
static const core::u32 GRID_MAX_COUNT		= 10;
static const float GRID_MAX_SCALE			= 1000.0f;
static const float GRID_MIN_SCALE			= 100.0f;
static const core::u32 GRID_COUNT			= 1 + core::u32( ceil( log2f( (float)GRID_MAX_SCALE / GRID_MIN_SCALE ) ) );
static const int SAMPLE_MAX_COUNT			= 1;
static const float FREE_SCALE				= 50.0f;
static const core::u32 RANDOM_CUBE_COUNT	= 64;

static const uint VERTEX_INPUT_MAX_COUNT	= 6;
static const uint ENTITY_MAX_COUNT			= 4096;

static bool g_mousePressed = false;
static int g_mousePosX = 0;
static int g_mousePosY = 0;
static int g_keyLeftPressed = false;
static int g_keyRightPressed = false;
static int g_keyUpPressed = false;
static int g_keyDownPressed = false;

static int g_frameLimitation = true;

static DrawMode_e g_drawMode = DRAW_MODE_DEFAULT;

static int g_sample = 0;

static int g_limit = false; 

static float s_yaw = 0.0f;
static float s_pitch = 0.0f;
static core::Vec3 s_headOffset = core::Vec3_Zero();

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
	case WM_CHAR:
		switch (wParam)
		{
		case 0x1B:
			DestroyWindow(hWnd);
			break;
		};
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		{
			const bool pressed = message == WM_KEYDOWN;
			switch( wParam )
			{
			case 'A': g_keyLeftPressed = pressed; break;
			case 'D': g_keyRightPressed = pressed; break;
			case 'S': g_keyDownPressed = pressed; break;
			case 'W': g_keyUpPressed = pressed; break;
			case 'F': g_frameLimitation = !pressed; break;
			case 'C': g_drawMode = pressed ? (g_drawMode == DRAW_MODE_CUBE ? DRAW_MODE_DEFAULT : DRAW_MODE_CUBE) : g_drawMode; break;
			case 'G': g_drawMode = pressed ? (g_drawMode == DRAW_MODE_GET ? DRAW_MODE_DEFAULT : DRAW_MODE_GET) : g_drawMode; break;
			case 'R': g_sample = 0; break;
			case 'L': g_limit = pressed ? !g_limit : g_limit; break;
			case ' ':
				s_yaw = 0.0f;
				s_pitch = 0.0f;
				s_headOffset = core::Vec3_Zero();
				break;
			}
		}
		break;
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
	case WM_MOUSEMOVE:
		{
			if ( g_mousePressed )
			{
				g_mousePosX = GET_X_LPARAM( lParam ); 
				g_mousePosY = GET_Y_LPARAM( lParam );
			}
		}
		break;
	default:
		return DefWindowProcA(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

HWND CreateMainWindow( const char * sTitle, int nWidth, int nHeight )
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
		V6_ERROR("Call to RegisterClassEx failed!");
		return 0;
	}

	const int style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU| WS_MINIMIZEBOX;
		
	RECT rect = { 0, 0, nWidth, nHeight };
	AdjustWindowRect( &rect, style, false );

	HWND hWnd = CreateWindowA(
		"v6",
		sTitle,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL,
		NULL,
		NULL,
		NULL
		);

	return hWnd;
}

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
#if V6_DISABLE_OLD_GRID == 0
	COMPUTE_GRIDCLEAR,
	COMPUTE_GRIDFILL,
	COMPUTE_GRIDPACK,
#endif // #if V6_DISABLE_OLD_GRID == 0

	COMPUTE_SAMPLECOLLECT,
	COMPUTE_SAMPLESORT,

	COMPUTE_COUNT
};

enum
{
	SHADER_BASIC,
	SHADER_CUBE_RENDER,
#if V6_DISABLE_OLD_GRID == 0
	SHADER_GRID_RENDER4,
	SHADER_GRID_RENDER8,
	SHADER_GRID_RENDER16,
	SHADER_GRID_RENDER32,
	SHADER_GRID_RENDER64,
#endif // #if V6_DISABLE_OLD_GRID == 0

	SHADER_COUNT
};

enum
{
	MESH_TRIANGLE,
	MESH_BOX_WIREFRAME,
	MESH_BOX_RED,
	MESH_BOX_BLUE,
	MESH_BOX_GREEN,
	MESH_VIRTUAL_TRIANGLE,
	MESH_CUBE,
	MESH_GRID,

	MESH_COUNT
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
	GRID_BUFFER_BLOCK_ID,
	GRID_BUFFER_BLOCK_COLOR,
	GRID_BUFFER_BLOCK_POS,
	GRID_BUFFER_BLOCK_INDIRECT_ARGS,
	GRID_BUFFER_BLOCK_PACKED_COLOR4,
	GRID_BUFFER_BLOCK_PACKED_COLOR8,
	GRID_BUFFER_BLOCK_PACKED_COLOR16,
	GRID_BUFFER_BLOCK_PACKED_COLOR32,
	GRID_BUFFER_BLOCK_PACKED_COLOR64,

	GRID_BUFFER_COUNT
};

struct BasicVertex_s
{
	core::Vec3 position;
	core::SColor color;
};

struct CubeVertex_s
{
	core::Vec3 pos;
	core::Vec2 uv;
};

struct GPUBuffer_s
{
	ID3D11Buffer*					buf;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
};

struct Compute_s
{
	ID3D11ComputeShader* m_computeShader;
};

struct Shader_s
{
	ID3D11VertexShader* m_vertexShader;
	ID3D11PixelShader* m_pixelShader;

	ID3D11InputLayout* m_inputLayout;

	uint m_vertexFormat;
};

struct Mesh_s
{
	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;	
	uint m_vertexCount;
	uint m_vertexSize;
	uint m_vertexFormat;
	uint m_indexCount;
	uint m_indexSize;
	D3D11_PRIMITIVE_TOPOLOGY m_topology;	
};

struct Entity_s
{
	uint meshID;
	uint shaderID;
	core::Vec3 pos;
	float scale;
};

struct RenderingView_s
{
	core::Mat4x4 viewMatrix;
	core::Mat4x4 projMatrix;
	core::u16 frameWidth;
	core::u16 frameHeight;
};

struct Cube_s
{	
	ID3D11Texture2D* colorBuffer;
	ID3D11ShaderResourceView* colorSRV;
	ID3D11RenderTargetView* colorRTVs[CUBE_AXIS_COUNT];
	
	ID3D11Texture2D* depthBuffer;
	ID3D11ShaderResourceView* depthSRV;
	ID3D11DepthStencilView* depthRTVs[CUBE_AXIS_COUNT];

	core::u32 size;
};

#if V6_DISABLE_OLD_GRID == 0

struct Grid_s
{	
	ID3D11Buffer* buffers[GRID_BUFFER_COUNT];
	ID3D11Buffer* indirectArgsInit;
	ID3D11Buffer* indirectArgsStaging;
	ID3D11ShaderResourceView* srvs[GRID_BUFFER_COUNT];
	ID3D11UnorderedAccessView* uavs[GRID_BUFFER_COUNT];	
	core::u32 blockMaxCount;
	core::u32 bucketMaxCounts[HLSL_GRID_BUCKET_COUNT];
};

#endif // #if V6_DISABLE_OLD_GRID == 0

struct Sample_s
{
	GPUBuffer_s collectedSamples;
	GPUBuffer_s sortedSamples;
	GPUBuffer_s collectedIndirectArgs;
	GPUBuffer_s sortedIndirectArgs;
};

static const char* ModeToString( DrawMode_e drawMode )
{
	switch ( drawMode )
	{
	case DRAW_MODE_DEFAULT: return "default";
	case DRAW_MODE_CUBE: return "cube";
	case DRAW_MODE_GET: return "get";
	}
	return "unknown";
}

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
			V6_ASSERT( "Not supported" );
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

static bool Compute_Create( ID3D11Device* device, Compute_s* compute, const char* cs, core::CFileSystem* fileSystem, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	void* csBytecode = nullptr;
	const int csBytecodeSize = fileSystem->ReadFile( cs, &csBytecode, stack );
	if ( csBytecodeSize <= 0 )
	{
		return false;
	}	

	{
		HRESULT hRes = device->CreateComputeShader( csBytecode, csBytecodeSize, nullptr, &compute->m_computeShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateComputeShader failed!" );
		}
	}	

	return true;
}

static bool Shader_Create( ID3D11Device* device, Shader_s* shader, const char* vs, const char* ps, uint vertexFormat, core::CFileSystem* fileSystem, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	void* vsBytecode = nullptr;
	const int vsBytecodeSize = fileSystem->ReadFile( vs, &vsBytecode, stack );
	if ( vsBytecodeSize <= 0 )
	{
		return false;
	}	

	{
		HRESULT hRes = device->CreateVertexShader( vsBytecode, vsBytecodeSize, nullptr, &shader->m_vertexShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!" );
		}
	}

	void* psBytecode = nullptr;
	const int psBytecodeSize = fileSystem->ReadFile( ps, &psBytecode, stack );
	if ( psBytecodeSize <= 0 )
	{
		return false;
	}

	{
		HRESULT hRes = device->CreatePixelShader( psBytecode, psBytecodeSize, nullptr, &shader->m_pixelShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreatePixelShader failed!" );
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

			stride = 4 * width;
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

			stride = 4 * width;
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

			stride = 4 * width;
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

			stride = 4 * width;
			++inputCount;
		}

		HRESULT hRes = device->CreateInputLayout( idesc, inputCount, vsBytecode, vsBytecodeSize, &shader->m_inputLayout );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateInputLayout failed!" );
			return false;
		}

		shader->m_vertexFormat = vertexFormat;
	}

	return true;
}

static core::u32 DXGIFormat_Size( DXGI_FORMAT format )
{
	switch( format )
	{
	case DXGI_FORMAT_R32_UINT:
		return 4;
	default:
		V6_ASSERT( !"Not supported" );
		return 0;
	}
}

void GPUBuffer_CreateIndirectArgs( ID3D11Device* device, GPUBuffer_s* buffer, core::u32 count, const char* name )
{
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( core::u32 );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		printf( "GPUBuffer %s: %d MB\n", name, MB( bufferDesc.ByteWidth ) );
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

void GPUBuffer_CreateTyped( ID3D11Device* device, GPUBuffer_s* buffer, DXGI_FORMAT format, core::u32 count, const char* name )
{
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		printf( "GPUBuffer %s: %d MB\n", name, MB( bufferDesc.ByteWidth ) );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

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

void GPUBuffer_CreateStructured( ID3D11Device* device, GPUBuffer_s* buffer, core::u32 elementSize, core::u32 count, const char* name )
{
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		printf( "GPUBuffer %s: %d MB\n", name, MB( bufferDesc.ByteWidth ) );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

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

void GPUBuffer_Release( ID3D11Device* device, GPUBuffer_s* buffer )
{
	buffer->buf->Release();
	buffer->srv->Release();
	buffer->uav->Release();
}

static void Cube_Create( ID3D11Device* device, Cube_s* cube, core::u32 size )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 6;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cube->colorBuffer) );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.MipLevels = 1;
		viewDesc.Texture2DArray.ArraySize = 6;
		viewDesc.Texture2DArray.FirstArraySlice = 0;		
		viewDesc.Texture2DArray.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( cube->colorBuffer, &viewDesc, &cube->colorSRV ) );
	}

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.ArraySize = 1;
		viewDesc.Texture2DArray.FirstArraySlice = faceID;
		viewDesc.Texture2DArray.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateRenderTargetView( cube->colorBuffer, &viewDesc, &cube->colorRTVs[faceID] ) );
	}
	
	{
		D3D11_TEXTURE2D_DESC depthStencilDesc = {};
		depthStencilDesc.Width = size;
		depthStencilDesc.Height = size;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 6;
		depthStencilDesc.Format = DXGI_FORMAT_R32_TYPELESS;		
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		depthStencilDesc.CPUAccessFlags = 0;
		depthStencilDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &depthStencilDesc, nullptr, &cube->depthBuffer ) );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.MipLevels = 1;
		viewDesc.Texture2DArray.ArraySize = 6;
		viewDesc.Texture2DArray.FirstArraySlice = 0;		
		viewDesc.Texture2DArray.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( cube->depthBuffer, &viewDesc, &cube->depthSRV ) );
	}

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Flags = 0;
		viewDesc.Texture2DArray.ArraySize = 1;
		viewDesc.Texture2DArray.FirstArraySlice = faceID;
		viewDesc.Texture2DArray.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateDepthStencilView( cube->depthBuffer, &viewDesc, &cube->depthRTVs[faceID] ) );
	}
	
	cube->size = size;
}

static void Cube_Release( Cube_s* cube )
{
	cube->colorBuffer->Release();
	cube->depthBuffer->Release();

	cube->colorSRV->Release();	
	cube->depthSRV->Release();	

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		cube->colorRTVs[faceID]->Release();
		cube->depthRTVs[faceID]->Release();
	}
}

#if V6_DISABLE_OLD_GRID == 0

static void Grid_Create( ID3D11Device* device, Grid_s* grid, core::u32 macroShift, core::IHeap* heap )
{
	const core::u32 width = 1 << macroShift;
	const core::u32 realBlockCount = width * width * width;
	const core::u32 estimatedBlockCount = 2 * width * width * 6; // 1 layer of full panorama ( 6 cube faces )
	printf( "estimated block count: %d\n", estimatedBlockCount );
	grid->blockMaxCount = estimatedBlockCount;

	// RWBuffer< uint > gridBlockIDs

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = realBlockCount * sizeof( uint );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		void* ids = heap->alloc( bufferDesc.ByteWidth );
		memset( ids, 0xFF, bufferDesc.ByteWidth );

		D3D11_SUBRESOURCE_DATA subRes = {}; 
		subRes.pSysMem = ids;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, &subRes, &grid->buffers[GRID_BUFFER_BLOCK_ID] ) );
		printf( "gridBlockIDs: %d MB\n",  MB( bufferDesc.ByteWidth ) );

		heap->free( ids );
	}

	grid->srvs[GRID_BUFFER_BLOCK_ID] = nullptr;
	
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = realBlockCount;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( grid->buffers[GRID_BUFFER_BLOCK_ID], &uavDesc, &grid->uavs[GRID_BUFFER_BLOCK_ID] ) );
	}

	// RWStructuredBuffer< GridBlockColor > gridBlockColors

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = estimatedBlockCount * sizeof( hlsl::GridBlockColor );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof( hlsl::GridBlockColor );
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &grid->buffers[GRID_BUFFER_BLOCK_COLOR] ) );
		printf( "gridBlockColors: %d MB\n", MB( bufferDesc.ByteWidth ) );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = estimatedBlockCount;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( grid->buffers[GRID_BUFFER_BLOCK_COLOR], &srvDesc, &grid->srvs[GRID_BUFFER_BLOCK_COLOR] ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = estimatedBlockCount;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( grid->buffers[GRID_BUFFER_BLOCK_COLOR], &uavDesc, &grid->uavs[GRID_BUFFER_BLOCK_COLOR] ) );
	}

	// RWBuffer< uint > gridBlockPositions

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = estimatedBlockCount * sizeof( uint );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &grid->buffers[GRID_BUFFER_BLOCK_POS] ) );
		printf( "gridBlockPositions: %d MB\n", MB( bufferDesc.ByteWidth ) );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = estimatedBlockCount;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( grid->buffers[GRID_BUFFER_BLOCK_POS], &srvDesc, &grid->srvs[GRID_BUFFER_BLOCK_POS] ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = estimatedBlockCount;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( grid->buffers[GRID_BUFFER_BLOCK_POS], &uavDesc, &grid->uavs[GRID_BUFFER_BLOCK_POS] ) );
	}

	// RWStructuredBuffer< GridIndirectArgs > gridIndirectArgs

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof( hlsl::GridIndirectArgs );
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		v6::hlsl::GridIndirectArgs indirectArgs = {};
		indirectArgs.packArgs.threadGroupCountY = 1; 
		indirectArgs.packArgs.threadGroupCountZ = 1;
#if HLSL_GRIDBLOCK_USE_POINTS
		for ( core::u32 bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket)
			indirectArgs.renderArgs[bucket].vertexCountPerInstance = 4;
#else
		for ( core::u32 bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket)
			indirectArgs.renderArgs[bucket].indexCountPerInstance = 36;
#endif

		D3D11_SUBRESOURCE_DATA subRes = {}; 
		subRes.pSysMem = &indirectArgs;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, &subRes, &grid->indirectArgsInit ) );
		printf( "gridIndirectArgs: %d MB\n",  MB( bufferDesc.ByteWidth ) );
	}

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof( hlsl::GridIndirectArgs );
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &grid->indirectArgsStaging ) );
	}

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof( hlsl::GridIndirectArgs );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &grid->buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS] ) );
	}
	
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = sizeof( hlsl::GridIndirectArgs ) / sizeof( core::u32 );

		V6_ASSERT_D3D11( device->CreateShaderResourceView( grid->buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS], &srvDesc, &grid->srvs[GRID_BUFFER_BLOCK_INDIRECT_ARGS] ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = sizeof( hlsl::GridIndirectArgs ) / sizeof( core::u32 );
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( grid->buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS], &uavDesc, &grid->uavs[GRID_BUFFER_BLOCK_INDIRECT_ARGS] ) );
	}

	// RWBuffer< uint > gridBlockPackedColors

	static const core::u32 repartition[HLSL_GRID_BUCKET_COUNT] = { 32, 32, 128, 16, 1 };
	static core::u32 sumRepartion = 0;
	if ( sumRepartion  == 0 ) 
	{
		for ( core::u32 bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket )
			sumRepartion += repartition[bucket];
	}

	for ( core::u32 bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket )
	{
		const core::u32 packedCount = 1 + (1 << (bucket+2));
		const core::u32 estimatedBlockCountPerBucket = estimatedBlockCount * repartition[bucket] / sumRepartion;
		printf( "estimated block count for bucket #%d: %d\n", bucket ,estimatedBlockCountPerBucket );
		grid->bucketMaxCounts[bucket] = estimatedBlockCountPerBucket;

		{
			D3D11_BUFFER_DESC bufferDesc = {};
			bufferDesc.ByteWidth = estimatedBlockCountPerBucket * packedCount * sizeof( core::u32 );
			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			bufferDesc.StructureByteStride = 0;
		
			V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &grid->buffers[GRID_BUFFER_BLOCK_PACKED_COLOR4 + bucket] ) );
			printf( "packedColors%d: %d MB\n", bucket, MB( bufferDesc.ByteWidth ) );
		}

		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_R32_UINT;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = estimatedBlockCountPerBucket * packedCount;

			V6_ASSERT_D3D11( device->CreateShaderResourceView( grid->buffers[GRID_BUFFER_BLOCK_PACKED_COLOR4 + bucket], &srvDesc, &grid->srvs[GRID_BUFFER_BLOCK_PACKED_COLOR4 + bucket] ) );
		}

		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_R32_UINT;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = estimatedBlockCountPerBucket * packedCount;
			uavDesc.Buffer.Flags = 0;

			V6_ASSERT_D3D11( device->CreateUnorderedAccessView( grid->buffers[GRID_BUFFER_BLOCK_PACKED_COLOR4 + bucket], &uavDesc, &grid->uavs[GRID_BUFFER_BLOCK_PACKED_COLOR4 + bucket] ) );
		}
	}
}

static void Grid_Release( Grid_s* grid )
{
	for ( core::u32 bufferID = 0; bufferID < GRID_BUFFER_COUNT; ++bufferID )
	{
		grid->buffers[bufferID]->Release();
		if ( grid->srvs[bufferID] )
			grid->srvs[bufferID]->Release();
		if ( grid->uavs[bufferID] )
			grid->uavs[bufferID]->Release();
	}
	grid->indirectArgsInit->Release();
	grid->indirectArgsStaging->Release();
}

#endif // #if V6_DISABLE_OLD_GRID == 0

static void Sample_Create( ID3D11Device* device, Sample_s* sample, core::u32 cubeSize, core::IHeap* heap )
{
	const core::u32 sampleCount = cubeSize * cubeSize * 6;
	GPUBuffer_CreateStructured( device, &sample->collectedSamples, sizeof( hlsl::Sample ), sampleCount, "collectedSamples" );
	GPUBuffer_CreateStructured( device, &sample->sortedSamples, sizeof( hlsl::Sample ), sampleCount, "sortedSamples" );
	GPUBuffer_CreateIndirectArgs( device, &sample->collectedIndirectArgs, 4, "collectedIndirectArgs" );
	GPUBuffer_CreateIndirectArgs( device, &sample->sortedIndirectArgs, HLSL_MIP_MAX_COUNT, "sortedIndirectArgs" );
}

static void Sample_Release( ID3D11Device* device, Sample_s* sample )
{
	GPUBuffer_Release( device, &sample->collectedSamples );
	GPUBuffer_Release( device, &sample->sortedSamples );
	GPUBuffer_Release( device, &sample->collectedIndirectArgs );
	GPUBuffer_Release( device, &sample->sortedIndirectArgs );
}

static void Mesh_Create( ID3D11Device* device, Mesh_s* mesh, const void* vertices, uint vertexCount, uint vertexSize, uint vertexFormat, const void* indices, uint indexCount, uint indexSize, D3D11_PRIMITIVE_TOPOLOGY topology )
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

		mesh->m_indexCount = indexCount;
		mesh->m_indexSize = indexSize;
	}

	mesh->m_topology = topology;
}

static void Mesh_CreateTriangle( ID3D11Device* device, Mesh_s* mesh )
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

static void Mesh_CreateBox( ID3D11Device* device, Mesh_s* mesh, const core::SColor color, bool wireframe )
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

static void Mesh_CreateVirtualTriangle( ID3D11Device* device, Mesh_s* mesh )
{
	Mesh_Create( device, mesh, nullptr, 3, 0, 0, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );	
}

static void Mesh_CreateCube( ID3D11Device* device, Mesh_s* mesh )
{
	CubeVertex_s vertices[24];
	core::u16 indices[36];

	core::u32 vertexID = 0;
	core::u32 indexID = 0;
	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		core::Vec3 lookAt;
		core::Vec3 up;
		Cube_GetLookAt( lookAt, up, (CubeAxis_e)faceID );
		const core::Vec3 right = core::Cross( lookAt, up );		

		vertices[vertexID+0].pos = (lookAt - right + up) * ZFAR;
		vertices[vertexID+0].uv = core::Vec2_Make( 0.0f, 0.0f );
		vertices[vertexID+1].pos = (lookAt + right + up) * ZFAR;
		vertices[vertexID+1].uv = core::Vec2_Make( 1.0f, 0.0f );
		vertices[vertexID+2].pos = (lookAt - right - up) * ZFAR;
		vertices[vertexID+2].uv = core::Vec2_Make( 0.0f, 1.0f );
		vertices[vertexID+3].pos = (lookAt + right - up) * ZFAR;
		vertices[vertexID+3].uv = core::Vec2_Make( 1.0f, 1.0f );
		
		indices[indexID+0] = vertexID+0;
		indices[indexID+1] = vertexID+1;
		indices[indexID+2] = vertexID+2;

		indices[indexID+3] = vertexID+2;
		indices[indexID+4] = vertexID+1;
		indices[indexID+5] = vertexID+3;

		vertexID += 4;
		indexID += 6;
	}

	Mesh_Create( device, mesh, vertices, 24, sizeof( CubeVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void Mesh_CreateGrid( ID3D11Device* device, Mesh_s* mesh )
{
#if 0
	const core::u16 indices[6] = { 0, 1, 2, 2, 1, 3 };

	Mesh_Create( device, mesh, nullptr, 4, 0, 0, indices, 6, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
#elif HLSL_GRIDBLOCK_USE_POINTS
	Mesh_Create( device, mesh, nullptr, 0, 0, 0, nullptr, 0, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
#else
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

	Mesh_Create( device, mesh, nullptr, 4, 0, 0, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
#endif	
}

void Entity_Create( Entity_s* entity, core::u32 meshID, core::u32 shaderID, const core::Vec3& pos, float scale )
{
	entity->meshID = meshID;
	entity->shaderID = shaderID;
	entity->pos = pos;
	entity->scale = scale;
}

void Mesh_Draw( Mesh_s* mesh, core::u32 instanceCount, Shader_s* shader, ID3D11DeviceContext* ctx, ID3D11Buffer* bufferArgs, core::u32 offsetArgs )
{
	V6_ASSERT( shader->m_vertexFormat == mesh->m_vertexFormat );
	V6_ASSERT( instanceCount > 0 );

	ctx->IASetInputLayout( shader->m_inputLayout );
	ctx->VSSetShader( shader->m_vertexShader, nullptr, 0 );
	ctx->PSSetShader( shader->m_pixelShader, nullptr, 0 );
		
	const uint stride = mesh->m_vertexSize; 
	const uint offset = 0;
			
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
			V6_ASSERT( !"Not supported");
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
		ctx->IASetIndexBuffer( nullptr, DXGI_FORMAT_R16_UINT, 0 );
				
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

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
#if V6_DISABLE_OLD_GRID == 0
	void Accumulate( Grid_s* grid, const core::Vec3* sampleOffset, bool clear, float gridScale );
#endif // #if V6_DISABLE_OLD_GRID == 0
	void Capture( const core::Vec3* sampleOffset );
	void Collect( const core::Vec3* sampleOffset );
	bool Create(int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IHeap* heap, core::IStack* stack );
	void Draw( float dt );	
	void DrawEntities( Entity_s* entities, core::u32 count, const RenderingView_s* view );
#if V6_DISABLE_OLD_GRID == 0
	void DrawGrid( Grid_s* grid, float gridScale );
#endif // #if V6_DISABLE_OLD_GRID == 0
	void DrawWorld( const core::Mat4x4* viewMatrix );
	void Present();
	void Release();
	void ReleaseObject(IUnknown** unknow);
	void Sort();

	IDXGISwapChain* m_pSwapChain;
	ID3D11Device* m_device;
	D3D_FEATURE_LEVEL m_nFeatureLevel;
	ID3D11DeviceContext* m_ctx;
	ID3DUserDefinedAnnotation* m_userDefinedAnnotation;
	ID3D11Texture2D* m_pColorBuffer;
	ID3D11Texture2D* m_pDepthStencilBuffer;
	ID3D11RenderTargetView* m_pColorView;
	ID3D11DepthStencilView* m_pDepthStencilView;
	ID3D11RenderTargetView* m_pLinearDepthView;
	ID3D11DepthStencilState*  m_depthStencilStateNoZ;
	ID3D11DepthStencilState*  m_depthStencilStateZRO;
	ID3D11DepthStencilState*  m_depthStencilStateZRW;
	ID3D11BlendState* m_blendStateNoColor;
	ID3D11BlendState* m_blendStateOpaque;
	ID3D11Buffer* m_viewCBUF;
#if V6_DISABLE_OLD_GRID == 0
	ID3D11Buffer* m_gridCBUF;
#endif // #if V6_DISABLE_OLD_GRID == 0
	ID3D11Buffer* m_sampleCBUF;

	Compute_s m_computes[COMPUTE_COUNT];
	Shader_s m_shaders[SHADER_COUNT];
	Mesh_s m_meshes[MESH_COUNT];
	Entity_s m_entities[ENTITY_MAX_COUNT];
	core::Vec3 m_sampleOffsets[SAMPLE_MAX_COUNT];

	uint m_entityCount;

	Cube_s m_cube;

#if V6_DISABLE_OLD_GRID == 0
	Grid_s m_grids[GRID_MAX_COUNT];
#endif // #if V6_DISABLE_OLD_GRID == 0

	Sample_s m_sample;

	core::IHeap* m_heap;
	core::IStack* m_stack;

	uint m_width;
	uint m_height;
	float m_aspectRatio;
	core::Mat4x4 m_projMatrix;
	core::Mat4x4 m_cubeProjMatrix;
};

CRenderingDevice::CRenderingDevice()
{
	memset( this, 0, sizeof( CRenderingDevice ) );
}

CRenderingDevice::~CRenderingDevice()
{
}

bool CRenderingDevice::Create( int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IHeap* heap, core::IStack* stack )
{
	m_heap = heap;
	m_stack = stack;
	core::ScopedStack scopedStack( stack );

	{
		DXGI_SWAP_CHAIN_DESC oSwapChainDesc = {};

		DXGI_MODE_DESC & oModeDesc = oSwapChainDesc.BufferDesc;
		oModeDesc.Width = nWidth;
		oModeDesc.Height = nHeight;
		oModeDesc.RefreshRate.Numerator = 60;
		oModeDesc.RefreshRate.Denominator = 1;
		oModeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		oModeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		oModeDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		DXGI_SAMPLE_DESC & oSampleDesc = oSwapChainDesc.SampleDesc;
		oSampleDesc.Count = 1;
		oSampleDesc.Quality = 0;

		oSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		oSwapChainDesc.BufferCount = 2;
		oSwapChainDesc.OutputWindow = hWnd;
		oSwapChainDesc.Windowed = true;
		oSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		oSwapChainDesc.Flags = 0;

		D3D_FEATURE_LEVEL pFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_1 };
		m_pSwapChain = NULL;
		m_device = NULL;
		m_nFeatureLevel = (D3D_FEATURE_LEVEL)0;
		m_ctx = NULL;

#if V6_D3D_DEBUG == 1
		const core::u32 createFlags = D3D11_CREATE_DEVICE_DEBUG;		
#else
		const core::u32 createFlags = 0;		
#endif
		V6_ASSERT_D3D11( D3D11CreateDeviceAndSwapChain(
			NULL,
			D3D_DRIVER_TYPE_HARDWARE,
			NULL,
			createFlags,
			pFeatureLevels,
			1,
			D3D11_SDK_VERSION,
			&oSwapChainDesc,
			&m_pSwapChain,
			&m_device,
			&m_nFeatureLevel,
			&m_ctx) );

		V6_ASSERT( m_nFeatureLevel == D3D_FEATURE_LEVEL_11_1 );
	}

#if 0
	for ( core::u32 sampleCount = 1; sampleCount <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; sampleCount++)
	{
		core::u32 maxQualityLevel;
		HRESULT hr = m_device->CheckMultisampleQualityLevels( DXGI_FORMAT_R8G8B8A8_UNORM, MSAA_SAMPLE_QUALITY, &maxQualityLevel );
		
		if ( hr != S_OK )
			break;
		
		if ( maxQualityLevel > 0 )
			printf ("MSAA %dX supported with %d quality levels.\n", sampleCount, maxQualityLevel-1 );		
	}
#endif

	V6_ASSERT_D3D11( m_ctx->QueryInterface( IID_PPV_ARGS( &m_userDefinedAnnotation ) ) );

	V6_ASSERT_D3D11( m_pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void **)&m_pColorBuffer ) );
	
	V6_ASSERT_D3D11( m_device->CreateRenderTargetView( m_pColorBuffer, 0, &m_pColorView ) );
	
	{
		D3D11_TEXTURE2D_DESC oDepthStencilDesc;
		oDepthStencilDesc.Width = nWidth;
		oDepthStencilDesc.Height = nHeight;
		oDepthStencilDesc.MipLevels = 1;
		oDepthStencilDesc.ArraySize = 1;
		oDepthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;	
		oDepthStencilDesc.SampleDesc.Count = 1;
		oDepthStencilDesc.SampleDesc.Quality = 0;
		oDepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		oDepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		oDepthStencilDesc.CPUAccessFlags = 0;
		oDepthStencilDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( m_device->CreateTexture2D( &oDepthStencilDesc, 0, &m_pDepthStencilBuffer ) );
	}

	{
		D3D11_DEPTH_STENCIL_VIEW_DESC oDepthStencilViewDesc = {};
		oDepthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		oDepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		oDepthStencilViewDesc.Flags = 0;
		oDepthStencilViewDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( m_device->CreateDepthStencilView( m_pDepthStencilBuffer, &oDepthStencilViewDesc, &m_pDepthStencilView ) );
	}
		
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

		V6_ASSERT_D3D11( m_device->CreateDepthStencilState( &depthStencilDesc, &m_depthStencilStateNoZ ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( m_device->CreateDepthStencilState( &depthStencilDesc, &m_depthStencilStateZRO ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( m_device->CreateDepthStencilState( &depthStencilDesc, &m_depthStencilStateZRW ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = 0;
		
		V6_ASSERT_D3D11( m_device->CreateBlendState( &blendState, &m_blendStateNoColor ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( m_device->CreateBlendState( &blendState, &m_blendStateOpaque ) );
	}

	{
		D3D11_BUFFER_DESC bufDesc = {};	
		bufDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufDesc.ByteWidth = sizeof( v6::hlsl::CBView );
		bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		V6_ASSERT_D3D11( m_device->CreateBuffer( &bufDesc, nullptr, &m_viewCBUF ) );
	}

#if V6_DISABLE_OLD_GRID == 0
	{
		D3D11_BUFFER_DESC bufDesc = {};	
		bufDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufDesc.ByteWidth = sizeof( v6::hlsl::CBGrid );
		bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		V6_ASSERT_D3D11( m_device->CreateBuffer( &bufDesc, nullptr, &m_gridCBUF ) );
	}
#endif // #if V6_DISABLE_OLD_GRID == 0

	{
		D3D11_BUFFER_DESC bufDesc = {};	
		bufDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufDesc.ByteWidth = sizeof( v6::hlsl::CBSample );
		bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		V6_ASSERT_D3D11( m_device->CreateBuffer( &bufDesc, nullptr, &m_sampleCBUF ) );
	}

#if V6_DISABLE_OLD_GRID == 0
	Compute_Create( m_device, &m_computes[COMPUTE_GRIDCLEAR], "grid_clear_cs.cso", fileSystem, stack );
	Compute_Create( m_device, &m_computes[COMPUTE_GRIDFILL], "grid_fill_cs.cso", fileSystem, stack );
	Compute_Create( m_device, &m_computes[COMPUTE_GRIDPACK], "grid_pack_cs.cso", fileSystem, stack );
#endif // #if V6_DISABLE_OLD_GRID == 0
	
	Compute_Create( m_device, &m_computes[COMPUTE_SAMPLECOLLECT], "sample_collect_cs.cso", fileSystem, stack );
	Compute_Create( m_device, &m_computes[COMPUTE_SAMPLESORT], "sample_sort_cs.cso", fileSystem, stack );
		
	Shader_Create( m_device, &m_shaders[SHADER_BASIC], "basic_vs.cso", "basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_CUBE_RENDER], "cube_render_vs.cso", "cube_render_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2, fileSystem, stack );

#if V6_DISABLE_OLD_GRID == 0
	Shader_Create( m_device, &m_shaders[SHADER_GRID_RENDER4], "grid_render_x4_vs.cso", "grid_render_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_GRID_RENDER8], "grid_render_x8_vs.cso", "grid_render_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_GRID_RENDER16], "grid_render_x16_vs.cso", "grid_render_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_GRID_RENDER32], "grid_render_x32_vs.cso", "grid_render_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_GRID_RENDER64], "grid_render_x64_vs.cso", "grid_render_ps.cso", 0, fileSystem, stack );
#endif // #if V6_DISABLE_OLD_GRID == 0

	Mesh_CreateTriangle( m_device, &m_meshes[MESH_TRIANGLE] );	
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_WIREFRAME], core::Color_Make( 255, 255, 255, 255 ), true );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_RED], core::Color_Make( 255, 0, 0, 255 ), false );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_GREEN], core::Color_Make( 0, 255, 0, 255 ), false );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_BLUE], core::Color_Make( 0, 0, 255, 255 ), false );
	Mesh_CreateVirtualTriangle( m_device, &m_meshes[MESH_VIRTUAL_TRIANGLE] );
	Mesh_CreateCube( m_device, &m_meshes[MESH_CUBE] );
	Mesh_CreateGrid( m_device, &m_meshes[MESH_GRID] );
	
	Entity_Create( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, core::Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MAX_SCALE );
	Entity_Create( &m_entities[m_entityCount++], MESH_TRIANGLE, SHADER_BASIC, core::Vec3_Make( 0.0f, 0.0f, -GRID_MAX_SCALE ), 5.0f );
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
		
		Entity_Create( &m_entities[m_entityCount++], MESH_BOX_RED + (randomCubeID % 3), SHADER_BASIC, center, size );
		Entity_Create( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, center, size );

		++randomCubeID;
	}
	
	Cube_Create( m_device, &m_cube, CUBE_SIZE );

#if V6_DISABLE_OLD_GRID == 0
	V6_ASSERT( GRID_COUNT < GRID_MAX_COUNT );
	for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID )
		Grid_Create( m_device, &m_grids[gridID], HLSL_GRID_MACRO_SHIFT, heap );
#endif // #if V6_DISABLE_OLD_GRID == 0

	Sample_Create( m_device, &m_sample, CUBE_SIZE, heap );
	
	m_width = nWidth;
	m_height = nHeight;
	m_aspectRatio = (float)nWidth / nHeight;
	m_projMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, core::DegToRad( 70.0f ), m_aspectRatio );	
	m_cubeProjMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, core::DegToRad( 90.0f ), 1.0f );

	g_sample = 0;
	m_sampleOffsets[0] = core::Vec3_Make( 0.0f, 0.0f, 0.0f );
	for ( core::u32 sample = 1; sample < SAMPLE_MAX_COUNT; ++sample )
		m_sampleOffsets[sample] = core::Vec3_Rand() * FREE_SCALE;
	
	return true;
}

void CRenderingDevice::DrawEntities( Entity_s* entities, core::u32 count, const RenderingView_s* view )
{
	for ( uint entityRank = 0; entityRank < count; ++entityRank )
	{
		Entity_s* entity = &entities[entityRank];
		
		{
			D3D11_MAPPED_SUBRESOURCE res;
			V6_ASSERT_D3D11( m_ctx->Map( m_viewCBUF, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

			v6::hlsl::CBView* cbView = (v6::hlsl::CBView*)res.pData;

			core::Mat4x4 worlMatrix;
			core::Mat4x4_Scale( &worlMatrix, entity->scale );
			core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
			// use this order because one matrix is "from" local space and the other is "to" local space
			core::Mat4x4_Mul( &cbView->objectToView, view->viewMatrix, worlMatrix );
			cbView->viewToProj = view->projMatrix;
			cbView->frameWidth = view->frameWidth;
			cbView->frameHeight = view->frameHeight;

			m_ctx->Unmap( m_viewCBUF, 0 );
		}

		m_ctx->VSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF );
		
		Mesh_s* mesh = &m_meshes[entity->meshID];
		Shader_s* shader = &m_shaders[entity->shaderID];
		Mesh_Draw( mesh, 1, shader, m_ctx, nullptr, 0 );
	}
}

void CRenderingDevice::DrawWorld( const core::Mat4x4* viewMatrix )
{	
	// Rasterization state
	m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
	m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		m_ctx->RSSetViewports( 1, &viewport );
	}
	
	// RT
	m_ctx->OMSetRenderTargets( 1, &m_pColorView, m_pDepthStencilView );

	// Clear
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_ctx->ClearRenderTargetView( m_pColorView, pRGBA );
	m_ctx->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
		
	// View
	RenderingView_s view;
	view.viewMatrix = *viewMatrix;
	view.projMatrix = m_projMatrix;	
	view.frameWidth = m_width;
	view.frameHeight = m_height;

	DrawEntities( m_entities, m_entityCount, &view );

	// un RT
	m_ctx->OMSetRenderTargets( 0, nullptr, nullptr );
}

void CRenderingDevice::Capture( const core::Vec3* sampleOffset )
{
	m_userDefinedAnnotation->BeginEvent( L"Draw Cube Map");
		
	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		m_userDefinedAnnotation->BeginEvent( L"Draw Face");

		// RT
		m_ctx->OMSetRenderTargets( 1, &m_cube.colorRTVs[faceID], m_cube.depthRTVs[faceID] );

		// Clear
		float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_ctx->ClearRenderTargetView( m_cube.colorRTVs[faceID], pRGBA );
		m_ctx->ClearDepthStencilView( m_cube.depthRTVs[faceID], D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );						

		// View
		RenderingView_s view;
		Cube_MakeViewMatrix( &view.viewMatrix, *sampleOffset, (CubeAxis_e)faceID );
		view.projMatrix = m_cubeProjMatrix;
		view.frameWidth = m_cube.size;	
		view.frameHeight = m_cube.size;
		
		DrawEntities( m_entities, m_entityCount, &view );

		// un RT
		m_ctx->OMSetRenderTargets( 0, nullptr, nullptr );
			
		m_userDefinedAnnotation->EndEvent();
	}

	m_userDefinedAnnotation->EndEvent();
}

#if V6_DISABLE_OLD_GRID == 0

void CRenderingDevice::Accumulate( Grid_s* grid, const core::Vec3* sampleOffset, bool clear, float gridScale  )
{
	static const void* nulls[8] = {};
		
	m_ctx->CSSetUnorderedAccessViews( HLSL_GRIDBLOCK_ID_SLOT, 2, &grid->uavs[GRID_BUFFER_BLOCK_ID], nullptr );

	if ( clear )
	{
		// Clear

		m_userDefinedAnnotation->BeginEvent( L"Clear");		

		m_ctx->CSSetShaderResources( HLSL_GRIDBLOCK_POS_SLOT, 2, &grid->srvs[GRID_BUFFER_BLOCK_POS] );		
		m_ctx->CSSetShader( m_computes[COMPUTE_GRIDCLEAR].m_computeShader, nullptr, 0 );

		m_ctx->DispatchIndirect( grid->buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS], offsetof( v6::hlsl::GridIndirectArgs, packArgs.threadGroupCountX ) );

		// Unset		
		m_ctx->CSSetShaderResources( HLSL_GRIDBLOCK_POS_SLOT, 2, (ID3D11ShaderResourceView**)nulls );

		m_ctx->CopyResource( grid->buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS], grid->indirectArgsInit );
		
		m_userDefinedAnnotation->EndEvent();
	}

	// Fill

	m_userDefinedAnnotation->BeginEvent( L"Fill");

	// Update buffers
		
	{
		D3D11_MAPPED_SUBRESOURCE res;
		V6_ASSERT_D3D11( m_ctx->Map( m_gridCBUF, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

		v6::hlsl::CBGrid* cbGrid = (v6::hlsl::CBGrid*)res.pData;

		cbGrid->depthLinearScale = -1.0f / ZNEAR;
		cbGrid->depthLinearBias = 1.0f / ZNEAR;
		cbGrid->invFrameSize = 1.0f / m_cube.size;
		cbGrid->gridScale = gridScale;
		cbGrid->invGridScale = 1.0f / gridScale;
		cbGrid->offset = *sampleOffset;

		m_ctx->Unmap( m_gridCBUF, 0 );
	}
		
	m_ctx->CSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF );
	m_ctx->CSSetConstantBuffers( v6::hlsl::CBGridSlot, 1, &m_gridCBUF );
	m_ctx->CSSetShaderResources( HLSL_COLOR_SLOT, 1, &m_cube.colorSRV );
	m_ctx->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, &m_cube.depthSRV );
	m_ctx->CSSetUnorderedAccessViews( HLSL_GRIDBLOCK_POS_SLOT, 2, &grid->uavs[GRID_BUFFER_BLOCK_POS], nullptr );
	m_ctx->CSSetShader( m_computes[COMPUTE_GRIDFILL].m_computeShader, nullptr, 0 );
		
	const core::u32 cubeGroupCount = m_cube.size >> 4;
	m_ctx->Dispatch( cubeGroupCount, cubeGroupCount, CUBE_AXIS_COUNT );

	// Unset		
	m_ctx->CSSetShaderResources( HLSL_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	m_ctx->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	m_ctx->CSSetUnorderedAccessViews( HLSL_GRIDBLOCK_ID_SLOT, 3, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	m_userDefinedAnnotation->EndEvent();

	// Pack

	m_userDefinedAnnotation->BeginEvent( L"Pack");
		
	m_ctx->CSSetShaderResources( HLSL_GRIDBLOCK_COLOR_SLOT, 2, &grid->srvs[GRID_BUFFER_BLOCK_COLOR] );
	m_ctx->CSSetUnorderedAccessViews( HLSL_GRIDBLOCK_PACKEDCOLOR4_SLOT, 5, &grid->uavs[GRID_BUFFER_BLOCK_PACKED_COLOR4], nullptr );
	m_ctx->CSSetShader( m_computes[COMPUTE_GRIDPACK].m_computeShader, nullptr, 0 );
		
	m_ctx->DispatchIndirect( grid->buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS], offsetof( v6::hlsl::GridIndirectArgs, packArgs.threadGroupCountX ) );

	// Unset
	m_ctx->CSSetShaderResources( HLSL_GRIDBLOCK_COLOR_SLOT, 2, (ID3D11ShaderResourceView**)nulls );
	m_ctx->CSSetUnorderedAccessViews( HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT, 6, (ID3D11UnorderedAccessView**)nulls, nullptr );

	m_userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::DrawGrid( Grid_s* grid, float gridScale )
{
	m_userDefinedAnnotation->BeginEvent( L"Render");

	{
		D3D11_MAPPED_SUBRESOURCE res;
		V6_ASSERT_D3D11( m_ctx->Map( m_gridCBUF, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

		v6::hlsl::CBGrid* cbGrid = (v6::hlsl::CBGrid*)res.pData;

		cbGrid->depthLinearScale = -1.0f / ZNEAR;
		cbGrid->depthLinearBias = 1.0f / ZNEAR;
		cbGrid->invFrameSize = 1.0f / m_cube.size;
		cbGrid->gridScale = gridScale;
		cbGrid->invGridScale = 1.0f / gridScale;
		cbGrid->offset = core::Vec3_Zero();

		m_ctx->Unmap( m_gridCBUF, 0 );
	}
		
	m_ctx->VSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF );
	m_ctx->VSSetConstantBuffers( v6::hlsl::CBGridSlot, 1, &m_gridCBUF );

	for ( core::u32 bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket )
	{
		m_ctx->VSSetShaderResources( HLSL_GRIDBLOCK_PACKEDCOLOR4_SLOT + bucket, 1, &grid->srvs[GRID_BUFFER_BLOCK_PACKED_COLOR4 + bucket] );

#if HLSL_GRIDBLOCK_USE_POINTS
		Mesh_Draw( &m_meshes[MESH_GRID], -1, &m_shaders[SHADER_GRID_RENDER4+bucket], m_ctx, grid->buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS], offsetof( v6::hlsl::GridIndirectArgs, renderArgs[bucket].vertexCountPerInstance ) );
#else
		Mesh_Draw( &m_meshes[MESH_GRID], -1, &m_shaders[SHADER_GRID_RENDER4+bucket], m_ctx, grid->buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS], offsetof( v6::hlsl::GridIndirectArgs, renderArgs[bucket].indexCountPerInstance ) );
#endif

		// unset
		static const void* nulls[8] = {};
		m_ctx->VSSetShaderResources( HLSL_GRIDBLOCK_PACKEDCOLOR4_SLOT + bucket, 1, (ID3D11ShaderResourceView**)nulls );
	}	

	m_userDefinedAnnotation->EndEvent();
}

#endif // #if V6_DISABLE_OLD_GRID == 0

void CRenderingDevice::Collect( const core::Vec3* sampleOffset  )
{
	static const void* nulls[8] = {};

	m_userDefinedAnnotation->BeginEvent( L"Collect");

	// Update buffers
				
	{			
		V6_ASSERT( GRID_COUNT < HLSL_MIP_MAX_COUNT );

		float gridScales[HLSL_MIP_MAX_COUNT];
		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID, gridScale *= 2 )
			gridScales[gridID] = gridScale;
		for ( core::u32 gridID = GRID_COUNT; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			gridScales[gridID] = gridScales[GRID_COUNT-1];

		D3D11_MAPPED_SUBRESOURCE res;
		V6_ASSERT_D3D11( m_ctx->Map( m_sampleCBUF, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

		v6::hlsl::CBSample* cbSample = (v6::hlsl::CBSample*)res.pData;

		cbSample->c_depthLinearScale = -1.0f / ZNEAR;
		cbSample->c_depthLinearBias = 1.0f / ZNEAR;
		cbSample->c_invCubeSize.x = 1.0f / CUBE_SIZE;
		cbSample->c_invCubeSize.y = 1.0f / CUBE_SIZE;
		cbSample->c_cubeCenter = *sampleOffset;
		cbSample->c_currentMip = 0;
		cbSample->c_mipBoundariesA = core::Vec4_Make( gridScales[0], gridScales[1], gridScales[2], gridScales[3] );
		cbSample->c_mipBoundariesB = core::Vec4_Make( gridScales[4], gridScales[5], gridScales[6], gridScales[7] );
		cbSample->c_mipBoundariesC = core::Vec4_Make( gridScales[8], gridScales[9], gridScales[10], gridScales[11] );
		cbSample->c_mipBoundariesD = core::Vec4_Make( gridScales[12], gridScales[13], gridScales[14], gridScales[15] );
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			cbSample->c_invGridScales[gridID] = core::Vec4_Make( 1.0f / gridScales[gridID], 0.0f, 0.0f , 0.0f );

		m_ctx->Unmap( m_sampleCBUF, 0 );		
	}

	core::u32 values[4] = {};
	m_ctx->ClearUnorderedAccessViewUint( m_sample.collectedIndirectArgs.uav, values );
		
	m_ctx->CSSetConstantBuffers( v6::hlsl::CBSampleSlot, 1, &m_sampleCBUF );
	m_ctx->CSSetShaderResources( HLSL_COLOR_SLOT, 1, &m_cube.colorSRV );
	m_ctx->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, &m_cube.depthSRV );
	m_ctx->CSSetUnorderedAccessViews( HLSL_COLLECTED_SAMPLE_SLOT, 1, &m_sample.collectedSamples.uav, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.collectedIndirectArgs.uav, nullptr );
	m_ctx->CSSetShader( m_computes[COMPUTE_SAMPLECOLLECT].m_computeShader, nullptr, 0 );
		
	const core::u32 cubeGroupCount = m_cube.size >> 4;
	m_ctx->Dispatch( cubeGroupCount, cubeGroupCount, CUBE_AXIS_COUNT );

	// Unset		
	m_ctx->CSSetShaderResources( HLSL_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	m_ctx->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	m_ctx->CSSetUnorderedAccessViews( HLSL_COLLECTED_SAMPLE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	m_userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::Sort()
{
	static const void* nulls[8] = {};

	m_userDefinedAnnotation->BeginEvent( L"Sort");

	V6_ASSERT( GRID_COUNT < HLSL_MIP_MAX_COUNT );

	core::u32 values[4] = {};
	m_ctx->ClearUnorderedAccessViewUint( m_sample.sortedIndirectArgs.uav, values );

	m_ctx->CSSetConstantBuffers( v6::hlsl::CBSampleSlot, 1, &m_sampleCBUF );
	m_ctx->CSSetShaderResources( HLSL_COLLECTED_SAMPLE_SLOT, 1, &m_sample.collectedSamples.srv );
	m_ctx->CSSetShaderResources( HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.collectedIndirectArgs.srv );
	m_ctx->CSSetUnorderedAccessViews( HLSL_SORTED_SAMPLE_SLOT, 1, &m_sample.sortedSamples.uav, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_SORTED_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.sortedIndirectArgs.uav, nullptr );
	m_ctx->CSSetShader( m_computes[COMPUTE_SAMPLESORT].m_computeShader, nullptr, 0 );

	for ( core::u32 mip = 0; mip < GRID_COUNT; ++mip )
	{
		// Update buffers				
		{
			D3D11_MAPPED_SUBRESOURCE res;
			V6_ASSERT_D3D11( m_ctx->Map( m_sampleCBUF, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

			v6::hlsl::CBSample* cbSample = (v6::hlsl::CBSample*)res.pData;
			cbSample->c_currentMip = mip;
			m_ctx->Unmap( m_sampleCBUF, 0 );		
		}		
		
		m_ctx->DispatchIndirect( m_sample.collectedIndirectArgs.buf, 0 );		
	}

	// Unset
	m_ctx->CSSetShaderResources( HLSL_COLLECTED_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetShaderResources( HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetUnorderedAccessViews( HLSL_SORTED_SAMPLE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_SORTED_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	m_userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::Draw( float dt )
{
	static int lastMousePosX = -1;
	static int lastMousePosY = -1;
	static int lastKeyPosX = -1;
	static int lastKeyPosZ = -1;
	
	int mouseDeltaX = 0;
	int mouseDeltaY = 0;
	int keyDeltaX = 0;
	int keyDeltaZ = 0;

	if ( g_mousePressed )
	{		
		mouseDeltaX = lastMousePosX < 0 ? 0 : g_mousePosX - lastMousePosX;
		mouseDeltaY = lastMousePosY < 0 ? 0 : g_mousePosY - lastMousePosY;
		lastMousePosX = g_mousePosX;
		lastMousePosY = g_mousePosY;
	}
	else
	{
		mouseDeltaX = 0;
		mouseDeltaY = 0;
		lastMousePosX = -1;
		lastMousePosY = -1;
	}

	if ( g_keyLeftPressed != g_keyRightPressed )
	{
		keyDeltaX = g_keyLeftPressed ? -1 : 1;
	}

	if ( g_keyDownPressed != g_keyUpPressed )
	{
		keyDeltaZ = g_keyDownPressed ? -1 : 1;
	}
		
	const static float MOUSE_ROTATION_SPEED = 0.5f;
	const static float KEY_TRANSLATION_SPEED = 100.0f;
	
	s_yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
	s_pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

	const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( s_yaw );
	const core::Mat4x4 pitchMatrix = core::Mat4x4_RotationX( s_pitch );
	core::Mat4x4 orientationMatrix;
	core::Mat4x4_Mul( &orientationMatrix, yawMatrix, pitchMatrix );	
	core::Mat4x4_Transpose( &orientationMatrix );

	if ( keyDeltaX || keyDeltaZ )
	{
		s_headOffset += *orientationMatrix.GetXAxis() * (float)keyDeltaX * KEY_TRANSLATION_SPEED * dt;
		s_headOffset += -*orientationMatrix.GetZAxis() * (float)keyDeltaZ * KEY_TRANSLATION_SPEED * dt;
	}
	else if ( g_limit && s_headOffset.LengthSq() > 0.001f )
	{
		s_headOffset -= s_headOffset * core::Min( 0.5f, KEY_TRANSLATION_SPEED * dt );
	}

	if ( g_limit )
	{
		s_headOffset.x = core::Clamp( s_headOffset.x, -FREE_SCALE, FREE_SCALE );
		s_headOffset.y = core::Clamp( s_headOffset.y, -FREE_SCALE, FREE_SCALE );
		s_headOffset.z = core::Clamp( s_headOffset.z, -FREE_SCALE, FREE_SCALE );
	}

	core::Mat4x4 viewMatrix;
	core::Mat4x4_Mul( &viewMatrix, yawMatrix, pitchMatrix );
	core::Mat4x4_SetTranslation( &viewMatrix, s_headOffset );
	core::Mat4x4_AffineInverse( &viewMatrix );
	
	if ( g_drawMode == DRAW_MODE_DEFAULT )
	{
		DrawWorld( &viewMatrix );
	}
	else if ( g_drawMode == DRAW_MODE_CUBE )
	{
		// Rasterization state
		m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
		m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );

		// Viewport
		{
			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.Width = (float)m_cube.size;
			viewport.Height = (float)m_cube.size;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			m_ctx->RSSetViewports( 1, &viewport );
		}

		Capture( &s_headOffset );

		// Rasterization state
		m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
		m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );
		
		// Viewport
		{
			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.Width = (float)m_width;
			viewport.Height = (float)m_height;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			m_ctx->RSSetViewports( 1, &viewport );
		}

		m_userDefinedAnnotation->BeginEvent( L"Render");

		// RT
		m_ctx->OMSetRenderTargets( 1, &m_pColorView, m_pDepthStencilView );

		{
			D3D11_MAPPED_SUBRESOURCE res;
			V6_ASSERT_D3D11( m_ctx->Map( m_viewCBUF, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

			v6::hlsl::CBView* cbView = (v6::hlsl::CBView*)res.pData;

			cbView->objectToView = viewMatrix;
			cbView->viewToProj = m_projMatrix;
			cbView->frameWidth = m_cube.size;	
			cbView->frameHeight = m_cube.size;

			m_ctx->Unmap( m_viewCBUF, 0 );		
		}

		// Render
		float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_ctx->ClearRenderTargetView( m_pColorView, pRGBA );
		m_ctx->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
		
		m_ctx->VSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF );
		m_ctx->PSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF );
		m_ctx->PSSetShaderResources( HLSL_COLOR_SLOT, 1, &m_cube.colorSRV );

		Mesh_Draw( &m_meshes[MESH_CUBE], 1, &m_shaders[SHADER_CUBE_RENDER], m_ctx, nullptr, 0 );

		// unset
		static const void* nulls[8] = {};
		m_ctx->PSSetShaderResources( HLSL_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
		
		// un RT
		m_ctx->OMSetRenderTargets( 0, nullptr, nullptr );

		m_userDefinedAnnotation->EndEvent();
	}
	else if ( g_drawMode == DRAW_MODE_GET )
	{
		const core::Vec3 sampleOffset = m_sampleOffsets[0];

		// Rasterization state
		m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
		m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );

		// Viewport
		{
			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.Width = (float)m_cube.size;
			viewport.Height = (float)m_cube.size;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			m_ctx->RSSetViewports( 1, &viewport );
		}

		Capture( &sampleOffset );
		Collect( &sampleOffset );
		Sort();
	}
#if V6_DISABLE_OLD_GRID == 0
	else if ( g_drawMode == DRAW_MODE_GRID )
	{
#if 0
		g_sample = 0;
#endif

		for ( ; g_sample < SAMPLE_MAX_COUNT; ++g_sample )
		{
			const core::Vec3 sampleOffset = m_sampleOffsets[g_sample];

			// Rasterization state
			m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
			m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );

			// Viewport
			{
				D3D11_VIEWPORT viewport = {};
				viewport.TopLeftX = 0;
				viewport.TopLeftY = 0;
				viewport.Width = (float)m_cube.size;
				viewport.Height = (float)m_cube.size;
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;

				m_ctx->RSSetViewports( 1, &viewport );
			}

			Capture( &sampleOffset );
			
			float gridScale = GRID_MIN_SCALE;
			for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID, gridScale *= 2 )
			{				
				Accumulate( &m_grids[gridID], &sampleOffset, g_sample == 0, gridScale );

				{
					m_ctx->CopyResource( m_grids[gridID].indirectArgsStaging, m_grids[gridID].buffers[GRID_BUFFER_BLOCK_INDIRECT_ARGS] );

					D3D11_MAPPED_SUBRESOURCE res;
					V6_ASSERT_D3D11( m_ctx->Map( m_grids[gridID].indirectArgsStaging, 0, D3D11_MAP_READ, 0, &res ) );

					v6::hlsl::GridIndirectArgs* indirectArgs = (v6::hlsl::GridIndirectArgs*)res.pData;
					V6_ASSERT( indirectArgs->blockCount <= m_grids[gridID].blockMaxCount );
					for ( core::u32 bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket)
					{
						if ( indirectArgs->packedBlockCounts[bucket] > m_grids[gridID].bucketMaxCounts[bucket] )
						{
							printf( "bucket #%d overflows with %d blocks used for %d blocks allocated", bucket, indirectArgs->packedBlockCounts[bucket], m_grids[gridID].bucketMaxCounts[bucket] );
							V6_ASSERT( !"Bucket overflow" );
						}
					}
					printf( "Sample #%03d/%d [%g->%g]: %d blocks\n", g_sample, gridID, gridScale * 0.5f, gridScale, indirectArgs->blockCount );				
					printf( " => %dx4, %dx8, %dx16, %dx32, %dx64\n",				
						indirectArgs->packedBlockCounts[0], indirectArgs->packedBlockCounts[1], indirectArgs->packedBlockCounts[2], indirectArgs->packedBlockCounts[3], indirectArgs->packedBlockCounts[4] );
					const core::u32 combinedCellCount = 
						indirectArgs->packedBlockCounts[0] * 4 + 
						indirectArgs->packedBlockCounts[1] * 8 + 
						indirectArgs->packedBlockCounts[2] * 16 + 
						indirectArgs->packedBlockCounts[3] * 32 +
						indirectArgs->packedBlockCounts[4] * 64;
					printf( " => %d combined cells, %d total cells, overhead %.1f%%\n", combinedCellCount, indirectArgs->cellCount, (combinedCellCount - indirectArgs->cellCount) * 100.0f / indirectArgs->cellCount );
				
					m_ctx->Unmap( m_grids[gridID].indirectArgsStaging, 0 );
				}
			}			
		}

		// Rasterization state
		m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
		m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );
		
		// Viewport
		{
			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.Width = (float)m_width;
			viewport.Height = (float)m_height;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			m_ctx->RSSetViewports( 1, &viewport );
		}

		// RT
		m_ctx->OMSetRenderTargets( 1, &m_pColorView, m_pDepthStencilView );

		{
			D3D11_MAPPED_SUBRESOURCE res;
			V6_ASSERT_D3D11( m_ctx->Map( m_viewCBUF, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

			v6::hlsl::CBView* cbView = (v6::hlsl::CBView*)res.pData;

			cbView->objectToView = viewMatrix;
			cbView->viewToProj = m_projMatrix;

			m_ctx->Unmap( m_viewCBUF, 0 );
		
		}

		// Render
		float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_ctx->ClearRenderTargetView( m_pColorView, pRGBA );
		m_ctx->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID, gridScale *= 2 )
			DrawGrid( &m_grids[gridID], gridScale );

		// un RT
		m_ctx->OMSetRenderTargets( 0, nullptr, nullptr );
	}
#endif // #if V6_DISABLE_OLD_GRID == 0
}

void CRenderingDevice::Present()
{
	m_pSwapChain->Present( 0, 0 );
}

void CRenderingDevice::Release()
{
	m_ctx->ClearState();
	
	Cube_Release( &m_cube );

#if V6_DISABLE_OLD_GRID == 0
	for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID )
		Grid_Release( &m_grids[gridID] );
#endif // #if V6_DISABLE_OLD_GRID == 0

	m_viewCBUF->Release();
#if V6_DISABLE_OLD_GRID == 0
	m_gridCBUF->Release();
#endif // #if V6_DISABLE_OLD_GRID == 0
	m_sampleCBUF->Release();

	for ( uint meshID = 0; meshID < MESH_COUNT; ++meshID )
	{
		Mesh_s* mesh = &m_meshes[meshID];
		if ( mesh->m_vertexBuffer )
			mesh->m_vertexBuffer->Release();
		if ( mesh->m_indexBuffer )
			mesh->m_indexBuffer->Release();
	}

	for ( uint computeID = 0; computeID < COMPUTE_COUNT; ++computeID )
	{
		Compute_s* compute = &m_computes[computeID];
		compute->m_computeShader->Release();		
	}

	for ( uint shaderID = 0; shaderID < SHADER_COUNT; ++shaderID )
	{
		Shader_s* shader = &m_shaders[shaderID];
		shader->m_vertexShader->Release();
		shader->m_pixelShader->Release();
		shader->m_inputLayout->Release();
	}
		
	m_pDepthStencilView->Release();
	m_pDepthStencilBuffer->Release();

	m_pColorView->Release();
	m_pColorBuffer->Release();

	m_pSwapChain->Release();
	m_ctx->Release();
	m_device->Release();	
}

void CRenderingDevice::ReleaseObject(IUnknown** unknow)
{
	const ULONG refCount = (*unknow)->Release();
	V6_ASSERT(refCount == 0);
	*unknow = nullptr;
}

END_V6_VIEWER_NAMESPACE

int main()
{
	V6_LOG( "Viewer 0.1" );

	v6::core::CHeap heap;
	v6::core::Stack stack( &heap, 100 * 1024 * 1024 );
	v6::core::CFileSystem filesystem;

	const int nWidth = (HLSL_GRID_WIDTH >> 1) * v6::viewer::ZOOM;
	const int nHeight = (HLSL_GRID_WIDTH >> 1) * v6::viewer::ZOOM;	

	const char* const title = "V6 Player | version: 0.1";

	HWND hWnd = v6::viewer::CreateMainWindow( title, nWidth, nHeight );
	if (!hWnd)
	{
		V6_ERROR("Call to CreateWindow failed!");
		return -1;
	}

	v6::viewer::CRenderingDevice oRenderingDevice;
	if ( !oRenderingDevice.Create( nWidth, nHeight, hWnd, &filesystem, &heap, &stack ) )
	{
		V6_ERROR("Call to CRenderingDevice::Create failed!");
		return -1;
	}

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	__int64 frameTickLast = GetTickCount(); 
	for ( __int64 frameId = 0; ; ++frameId )
	{
		__int64 frameTick = v6::core::GetTickCount(); 
		__int64 frameDelta = frameTick - frameTickLast;

		float dt = v6::core::Min( v6::core::ConvertTicksToSeconds( frameDelta ), 0.1f );
		while ( dt < 0.0095f && v6::viewer::g_frameLimitation )
		{
			Sleep( 1 );
			frameTick = v6::core::GetTickCount(); 
			frameDelta = frameTick - frameTickLast;
			dt = v6::core::ConvertTicksToSeconds( frameDelta );
		}

		frameTickLast = frameTick;

		if ( (frameId % 10) == 0 )
		{
			static __int64 fpsTickLast = frameTick;
			const __int64 fpsDelta = frameTick - fpsTickLast;
			fpsTickLast = frameTick;
			if ( fpsDelta > 0.0f )
			{
				const float fps = 10 / v6::core::ConvertTicksToSeconds( fpsDelta );
				char text[256];
				sprintf_s( text, sizeof( text ), "%s | fps: %.1f | %s mode", title, fps, v6::viewer::ModeToString( v6::viewer::g_drawMode ) );
				SetWindowTextA( hWnd, text );
			}
		}
		
		MSG msg;
		while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

			if (msg.message == WM_QUIT)
			{
				oRenderingDevice.Release();
				return 0;
			}
		}
			
		oRenderingDevice.Draw( dt );
		oRenderingDevice.Present();
	}
}