/*V6*/

#include <v6/viewer/common.h>
#include <v6/viewer/common_shared.h>

#include <v6/core/image.h>
#include <v6/core/memory.h>
#include <v6/core/vec2.h>
#include <v6/core/vec3.h>
#include <v6/core/filesystem.h>
#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/frame_manager.h>
#include <v6/core/stream.h>
#include <v6/core/time.h>

#include <windows.h>
#include <Windowsx.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

#define V6_ASSERT_D3D11( EXP )  { HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }

#define USE_PP 1

BEGIN_V6_VIEWER_NAMESPACE

static const float ZNEAR			= 10.0f;
static const float ZFAR				= 1000.0f;
static const core::u32 FRAME_SIZE	= 1024;

static bool g_mousePressed = false;
static int g_mousePosX = 0;
static int g_mousePosY = 0;
static int g_keyLeftPressed = false;
static int g_keyRightPressed = false;
static int g_keyUpPressed = false;
static int g_keyDownPressed = false;

static int g_frameLimitation = true;

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

	VERTEX_FORMAT_USER0			= 7 << 2,
	VERTEX_FORMAT_USER0_F1		= VERTEX_FORMAT_USER0 + 1,
	VERTEX_FORMAT_USER0_F2		= VERTEX_FORMAT_USER0 + 2,
	VERTEX_FORMAT_USER0_F3		= VERTEX_FORMAT_USER0 + 3,
	VERTEX_FORMAT_USER0_F4		= VERTEX_FORMAT_USER0 + 4,
	
	VERTEX_FORMAT_USER1			= 7 << 5,
	VERTEX_FORMAT_USER1_F1		= VERTEX_FORMAT_USER1 + 1,
	VERTEX_FORMAT_USER1_F2		= VERTEX_FORMAT_USER1 + 2,
	VERTEX_FORMAT_USER1_F3		= VERTEX_FORMAT_USER1 + 3,
	VERTEX_FORMAT_USER1_F4		= VERTEX_FORMAT_USER1 + 4,
	
	VERTEX_FORMAT_USER2			= 7 << 7,
	VERTEX_FORMAT_USER2_F1		= VERTEX_FORMAT_USER2 + 1,
	VERTEX_FORMAT_USER2_F2		= VERTEX_FORMAT_USER2 + 2,
	VERTEX_FORMAT_USER2_F3		= VERTEX_FORMAT_USER2 + 3,
	VERTEX_FORMAT_USER2_F4		= VERTEX_FORMAT_USER2 + 4,
	
	VERTEX_FORMAT_USER3			= 7 << 12,
	VERTEX_FORMAT_USER3_F1		= VERTEX_FORMAT_USER3 + 1,
	VERTEX_FORMAT_USER3_F2		= VERTEX_FORMAT_USER3 + 2,
	VERTEX_FORMAT_USER3_F3		= VERTEX_FORMAT_USER3 + 3,
	VERTEX_FORMAT_USER3_F4		= VERTEX_FORMAT_USER3 + 4,
	
};

struct BasicVertex_s
{
	core::Vec3 position;
	core::SColor color;
};

struct CubeVertex_s
{
	core::Vec3 position;
	core::Vec2 uv;
};

struct Shader
{
	ID3D11VertexShader* m_vertexShader;
	ID3D11PixelShader* m_pixelShader;

	ID3D11InputLayout* m_inputLayout;

	uint m_vertexFormat;
};

struct Mesh
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

struct Entity
{
	uint meshID;
	uint shaderID;
	core::Vec3 pos;
	float scale;
};

struct PostProcess
{
	uint shaderID;
};

struct RenderingView_s
{
	core::Mat4x4 viewMatrix;
	core::Mat4x4 projMatrix;
	core::u16 frameWidth;
	core::u16 frameHeight;
};

enum
{
	SHADER_BASIC,
	SHADER_CLOUD,
	SHADER_DEPTH_LINEARISATION,

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

	MESH_COUNT
};

enum
{
	POST_PROCESS_DEPTH_LINEARISATION,
	
	POST_PROCESS_COUNT
};

static const uint VERTEX_INPUT_MAX_COUNT = 2;
static const uint ENTITY_MAX_COUNT = 256;

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

struct Cube_s
{	
	ID3D11Texture2D* colorBuffer;
	ID3D11RenderTargetView* colorViews[CUBE_AXIS_COUNT];
	
	ID3D11Texture2D* depthBuffer;
	ID3D11DepthStencilView* depthViews[CUBE_AXIS_COUNT];

	ID3D11Texture2D* linearDepthBuffer;
	ID3D11RenderTargetView* linearDepthViews[CUBE_AXIS_COUNT];

	core::u32 size;
};

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
    }
}

static void Cube_MakeViewMatrix( core::Mat4x4* matrix, const core::Vec3& center, CubeAxis_e axis )
{
	core::Vec3 lookAt;
	core::Vec3 up;
	Cube_GetLookAt( lookAt, up, axis );
	
	const core::Vec3 right = core::Cross( lookAt, up );

	*matrix = Mat4x4_Rotation( right, up, lookAt );
	Mat4x4_SetTranslation( matrix, center );
	Mat4x4_AffineInverse( matrix );
}

static bool Shader_Create( ID3D11Device* device, Shader* shader, const char* vs, const char* ps, uint vertexFormat, core::CFileSystem* fileSystem, core::IStack* stack )
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
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!" );
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

		const static DXGI_FORMAT widthToFloatFormats[] = { DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };

		if ( vertexFormat & VERTEX_FORMAT_USER0 )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 0;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER0 ) - VERTEX_FORMAT_USER0;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride = 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER1 )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 1;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER1 ) - VERTEX_FORMAT_USER1;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride = 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER2 )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 2;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER2 ) - VERTEX_FORMAT_USER2;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride = 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER3 )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 3;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER3 ) - VERTEX_FORMAT_USER3;
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

static void Cube_Create( ID3D11Device* device, Cube_s* cube, core::u32 size )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 6;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cube->colorBuffer) );
	}

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.ArraySize = 1;
		viewDesc.Texture2DArray.FirstArraySlice = faceID;
		viewDesc.Texture2DArray.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateRenderTargetView( cube->colorBuffer, &viewDesc, &cube->colorViews[faceID] ) );
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

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		depthStencilViewDesc.Flags = 0;
		depthStencilViewDesc.Texture2DArray.ArraySize = 1;
		depthStencilViewDesc.Texture2DArray.FirstArraySlice = faceID;
		depthStencilViewDesc.Texture2DArray.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateDepthStencilView( cube->depthBuffer, &depthStencilViewDesc, &cube->depthViews[faceID] ) );
	}

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 6;
		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cube->linearDepthBuffer ) );
	}

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.ArraySize = 1;
		viewDesc.Texture2DArray.FirstArraySlice = faceID;
		viewDesc.Texture2DArray.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateRenderTargetView( cube->linearDepthBuffer, &viewDesc, &cube->linearDepthViews[faceID] ) );
	}

	cube->size = size;
}

static void Cube_Release( Cube_s* cube )
{
	cube->colorBuffer->Release();	
	cube->depthBuffer->Release();	
	cube->linearDepthBuffer->Release();

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		cube->colorViews[faceID]->Release();
		cube->depthViews[faceID]->Release();
		cube->linearDepthViews[faceID]->Release();
	}
}

static void Mesh_Create( ID3D11Device* device, Mesh* mesh, const void* vertices, uint vertexCount, uint vertexSize, uint vertexFormat, const void* indices, uint indexCount, uint indexSize, D3D11_PRIMITIVE_TOPOLOGY topology )
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

static void Mesh_CreateTriangle( ID3D11Device* device, Mesh* mesh )
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

static void Mesh_CreateBox( ID3D11Device* device, Mesh* mesh, const core::SColor color, bool wireframe )
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

static void Mesh_CreateCloud( ID3D11Device* device, Mesh* mesh, const core::FrameBuffer* frameBuffer, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	const uint faceWidth = frameBuffer->width / 3;
	const uint faceHeight = frameBuffer->height / 2;

	const uint vertexCount = frameBuffer->width * frameBuffer->height;

#if 1
	const uint indexCount = 6 * (faceHeight - 1) * ( faceWidth * 2 + 2 ) - 1;

	core::u32* const indices = (core::u32*)stack->alloc( indexCount * sizeof( core::u32 ) );
	core::u32* index = indices;
		
	for ( int faceID = 0; faceID < 6; ++faceID )
	{		
		const uint faceY = faceID / 3;
		const uint faceX = faceID - faceY * 3;
		uint offset = faceY * frameBuffer->width * faceHeight + faceX * faceWidth;
		for ( uint y = 0; y < faceHeight-1; ++y )
		{
			const uint bot = offset + ( faceY ? 0 : frameBuffer->width );
			const uint top = offset + ( faceY ? frameBuffer->width : 0 );
			if ( index != indices )
			{
				*(index++) = bot;
			}
			for ( uint x = 0; x < faceWidth; ++x )
			{
				*(index++) = bot + x;
				*(index++) = top + x;
			}
			*(index++) = top + faceWidth-1;			

			offset += frameBuffer->width;
		}
	}
#else
	const uint indexCount = 6 * faceHeight * faceWidth;

	core::u32* const indices = (core::u32*)stack->alloc( indexCount * sizeof( core::u32 ) );
	core::u32* index = indices;
		
	for ( int vertexID = 0; vertexID < vertexCount; ++vertexID )
	{		
		*(index++) = vertexID;
	}
#endif

	V6_ASSERT( index - indices == indexCount );
	
	Mesh_Create( device, mesh, nullptr, vertexCount, 0, 0, indices, indexCount, sizeof( core::u32 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
	// RenderableCreate( device, renderable, nullptr, vertexCount, 0, indices, indexCount, sizeof( core::u32 ), D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, shaderID );
	//RenderableCreate( device, renderable, nullptr, vertexCount, 0, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, shaderID );
}

static void Mesh_CreateVirtualTriangle( ID3D11Device* device, Mesh* mesh )
{
	Mesh_Create( device, mesh, nullptr, 3, 0, 0, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );	
}

static void Mesh_CreateCube( ID3D11Device* device, Mesh* mesh )
{
	CubeVertex_s vertices[24];
	core::u8 indices[36];

	core::u32 vertexID = 0;
	core::u32 indexID = 0;
	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		core::Vec3 lookAt;
		core::Vec3 up;
		Cube_GetLookAt( lookAt, up, (CubeAxis_e)faceID );
		
		const core::Vec3 right = core::Cross( lookAt, up );
		
		vertices[vertexID+0].position	= lookAt - right - up;
		vertices[vertexID+0].uv			= core::Vec2_Make( 0.0f, 0.0f );
		vertices[vertexID+1].position	= lookAt - right + up;
		vertices[vertexID+1].uv			= core::Vec2_Make( 0.0f, 1.0f );
		vertices[vertexID+2].position	= lookAt + right - up;
		vertices[vertexID+2].uv			= core::Vec2_Make( 1.0f, 0.0f );
		vertices[vertexID+3].position	= lookAt + right + up;
		vertices[vertexID+3].uv			= core::Vec2_Make( 1.0f, 1.0f );
		
		indices[indexID+0] = vertexID+0;
		indices[indexID+1] = vertexID+1;
		indices[indexID+2] = vertexID+2;

		indices[indexID+3] = vertexID+2;
		indices[indexID+4] = vertexID+1;
		indices[indexID+5] = vertexID+3;

		vertexID += 4;
		indexID += 6;
	}

	Mesh_Create( device, mesh, vertices, 24, sizeof( CubeVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2, indices, 36, sizeof( core::u8 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

void Entity_Create( Entity* entity, core::u32 meshID, core::u32 shaderID, const core::Vec3& pos, float scale )
{
	entity->meshID = meshID;
	entity->shaderID = shaderID;
	entity->pos = pos;
	entity->scale = scale;
}

void PostProcess_Create( PostProcess* postProcess, core::u32 shaderID )
{
	postProcess->shaderID = shaderID;
}

void Mesh_Draw( Mesh* mesh, Shader* shader, ID3D11DeviceContext* ctx )
{
	V6_ASSERT( shader->m_vertexFormat == mesh->m_vertexFormat );

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
		case 1:
			ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R8_UINT, 0 );
			break;
		case 2:
			ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R16_UINT, 0 );
			break;
		case 4:
			ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
			break;
		default:
			V6_ASSERT( !"Not supported");
		}

		ctx->DrawIndexed( mesh->m_indexCount, 0, 0 );
	}
	else
	{
		V6_ASSERT( mesh->m_indexBuffer == nullptr );
		ctx->IASetIndexBuffer( nullptr, DXGI_FORMAT_R16_UINT, 0 );

		V6_ASSERT( mesh->m_vertexCount > 0 );
		ctx->Draw( mesh->m_vertexCount, 0 );
	}
}

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
	bool Create(int nWidth, int nHeight, HWND hWnd, core::FrameBuffer* frameBuffer, core::CFileSystem* fileSystem, core::IHeap* heap, core::IStack* stack );
	void Draw( float dt );
	void DrawWorld( float dt, const RenderingView_s* view );
	void PostProcessDone( uint ppID );
	void PostProcessPrepare( uint ppID );
	void Present();
	void Release();
	void ReleaseObject(IUnknown** unknow);

	IDXGISwapChain* m_pSwapChain;
	ID3D11Device* m_device;
	D3D_FEATURE_LEVEL m_nFeatureLevel;
	ID3D11DeviceContext* m_ctx;
	ID3D11Texture2D* m_pColorBuffer;
	ID3D11Texture2D* m_pColorBufferStaging;
	ID3D11Texture2D* m_pDepthStencilBuffer;
	ID3D11Texture2D* m_pLinearDepthBuffer;
	ID3D11Texture2D* m_pLinearDepthBufferStaging;
	ID3D11RenderTargetView* m_pColorView;
	ID3D11DepthStencilView* m_pDepthStencilView;
	ID3D11RenderTargetView* m_pLinearDepthView;
#if USE_PP
	ID3D11ShaderResourceView* m_depthSRV;
#endif
	ID3D11DepthStencilState*  m_depthStencilStateNoZ;
	ID3D11DepthStencilState*  m_depthStencilStateZRO;
	ID3D11DepthStencilState*  m_depthStencilStateZRW;
	ID3D11BlendState* m_blendStateOpaque;
	ID3D11Buffer* m_cbuffer;

	Shader m_shaders[SHADER_COUNT];
	Mesh m_meshes[MESH_COUNT];
	Entity m_worldEntities[ENTITY_MAX_COUNT];
	PostProcess m_postProcesses[POST_PROCESS_COUNT];
	uint m_entityCount;

	Cube_s m_cube;

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

void CRenderingDevice::PostProcessPrepare( uint ppID )
{
	switch (ppID)
	{
		case POST_PROCESS_DEPTH_LINEARISATION:
		{
			m_ctx->OMSetDepthStencilState( m_depthStencilStateNoZ, 0 );
			m_ctx->OMSetRenderTargets( 1, &m_pLinearDepthView, nullptr );
			m_ctx->PSSetShaderResources( HLSL_DEPTH_SLOT, 1, &m_depthSRV );
		}
		break;
	}
}

void CRenderingDevice::PostProcessDone( uint ppID )
{
}

bool CRenderingDevice::Create( int nWidth, int nHeight, HWND hWnd, core::FrameBuffer* frameBuffer, core::CFileSystem* fileSystem, core::IHeap* heap, core::IStack* stack )
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

		D3D_FEATURE_LEVEL pFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1 };
		m_pSwapChain = NULL;
		m_device = NULL;
		m_nFeatureLevel = (D3D_FEATURE_LEVEL)0;
		m_ctx = NULL;

		V6_ASSERT_D3D11( D3D11CreateDeviceAndSwapChain(
			NULL,
			D3D_DRIVER_TYPE_HARDWARE,
			NULL,
			0,
			pFeatureLevels,
			2,
			D3D11_SDK_VERSION,
			&oSwapChainDesc,
			&m_pSwapChain,
			&m_device,
			&m_nFeatureLevel,
			&m_ctx) );
	}

	V6_ASSERT_D3D11( m_pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void **)&m_pColorBuffer ) );
	
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = nWidth;
		texDesc.Height = nHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_STAGING;
		texDesc.BindFlags = 0;
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( m_device->CreateTexture2D( &texDesc, nullptr, &m_pColorBufferStaging ) );
	}

	V6_ASSERT_D3D11( m_device->CreateRenderTargetView( m_pColorBuffer, 0, &m_pColorView ) );
	
	{
		D3D11_TEXTURE2D_DESC oDepthStencilDesc;
		oDepthStencilDesc.Width = nWidth;
		oDepthStencilDesc.Height = nHeight;
		oDepthStencilDesc.MipLevels = 1;
		oDepthStencilDesc.ArraySize = 1;
#if USE_PP
		oDepthStencilDesc.Format = DXGI_FORMAT_R32_TYPELESS;		
#else
		oDepthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;	
#endif
		oDepthStencilDesc.SampleDesc.Count = 1;
		oDepthStencilDesc.SampleDesc.Quality = 0;
		oDepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		oDepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
#if USE_PP
		oDepthStencilDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
#endif
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
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = nWidth;
		texDesc.Height = nHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( m_device->CreateTexture2D( &texDesc, nullptr, &m_pLinearDepthBuffer ) );
	}

	V6_ASSERT_D3D11( m_device->CreateRenderTargetView( m_pLinearDepthBuffer, nullptr, &m_pLinearDepthView ) );

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = nWidth;
		texDesc.Height = nHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_STAGING;
		texDesc.BindFlags = 0;
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( m_device->CreateTexture2D( &texDesc, nullptr, &m_pLinearDepthBufferStaging ) );
	}

#if USE_PP
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = -1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( m_device->CreateShaderResourceView( m_pDepthStencilBuffer, &viewDesc, &m_depthSRV ) );
	}
#endif

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

		V6_ASSERT_D3D11( m_device->CreateBuffer( &bufDesc, nullptr, &m_cbuffer ) );
	}
		
	Shader_Create( m_device, &m_shaders[SHADER_BASIC], "basic_vs.cso", "basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_CLOUD], "cloud_vs.cso", "cloud_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_DEPTH_LINEARISATION], "fullscreen_triangle_vs.cso", "fullscreen_depth_linearisation_ps.cso", 0, fileSystem, stack );

	Mesh_CreateTriangle( m_device, &m_meshes[MESH_TRIANGLE] );	
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_WIREFRAME], core::Color_Make( 255, 255, 255, 255 ), true );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_RED], core::Color_Make( 255, 0, 0, 255 ), false );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_GREEN], core::Color_Make( 0, 255, 0, 255 ), false );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_BLUE], core::Color_Make( 0, 0, 255, 255 ), false );
	Mesh_CreateVirtualTriangle( m_device, &m_meshes[MESH_VIRTUAL_TRIANGLE] );
	Mesh_CreateCube( m_device, &m_meshes[MESH_CUBE] );
	
	//RenderableCreateCloud( m_device, &m_renderables[m_renderableCount++], SHADER_CLOUD, frameBuffer, stack );

	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, core::Vec3_Make( 0.0f, 0.0f, 0.0f), 500.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_TRIANGLE, SHADER_BASIC, core::Vec3_Make( 0.0f, 0.0f, -500.0f), 5.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_RED, SHADER_BASIC, core::Vec3_Make( -190.0f, 100.0f, -200.0f), 20.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, core::Vec3_Make( -190.0f, 100.0f, -200.0f), 20.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_GREEN, SHADER_BASIC, core::Vec3_Make( 110.0f, 200.0f, -120.0f), 100.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, core::Vec3_Make( 110.0f, 200.0f, -120.0f), 100.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_BLUE, SHADER_BASIC, core::Vec3_Make( 10.0f, -300.0f, -300.0f), 50.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, core::Vec3_Make( 10.0f, -300.0f, -300.0f), 50.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_GREEN, SHADER_BASIC, core::Vec3_Make( -120.0f, -150.0f, -50.0f), 80.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, core::Vec3_Make( -120.0f, -150.0f, -50.0f), 80.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_BLUE, SHADER_BASIC, core::Vec3_Make( 300.0f, 0.0f, 400.0f), 120.0f );
	Entity_Create( &m_worldEntities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, core::Vec3_Make( 300.0f, 0.0f, 400.0f), 120.0f );
	
	PostProcess_Create( &m_postProcesses[POST_PROCESS_DEPTH_LINEARISATION], SHADER_DEPTH_LINEARISATION );
	
	Cube_Create( m_device, &m_cube, FRAME_SIZE );
	
	m_width = nWidth;
	m_height = nHeight;
	m_aspectRatio = (float)nWidth / nHeight;
	m_projMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, core::DegToRad( 70.0f ), m_aspectRatio );	
	m_cubeProjMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, core::DegToRad( 90.0f ), 1.0f );
	
	return true;
}

void CRenderingDevice::DrawWorld( float dt, const RenderingView_s* view )
{	
	for ( uint entityID = 0; entityID < m_entityCount; ++entityID )
	{
		Entity* entity = &m_worldEntities[entityID];
		
		{
			D3D11_MAPPED_SUBRESOURCE res;
			V6_ASSERT_D3D11( m_ctx->Map( m_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

			v6::hlsl::CBView* cbView = (v6::hlsl::CBView*)res.pData;

			core::Mat4x4 worlMatrix;
			core::Mat4x4_Scale( &worlMatrix, entity->scale );
			core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
			// use this order because one matrix is "from" local space and the other is "to" local space
			core::Mat4x4_Mul( &cbView->objectToView, view->viewMatrix, worlMatrix );
			cbView->viewToProj = view->projMatrix;
			cbView->frameWidth = view->frameWidth;
			cbView->frameHeight = view->frameHeight;
			cbView->zFar = ZFAR;

			m_ctx->Unmap( m_cbuffer, 0 );
		}

		m_ctx->VSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_cbuffer );
		
		Mesh* mesh = &m_meshes[entity->meshID];
		Shader* shader = &m_shaders[entity->shaderID];
		Mesh_Draw( mesh, shader, m_ctx );		
	}
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

	static float yaw = 0.0f;
	static float pitch = 0.0f;
	static float headOffsetX = 0.0f;
	static float headOffsetZ = 0.0f;
	const static float MOUSE_ROTATION_SPEED = 0.5f;
	const static float KEY_TRANSLATION_SPEED = 1.0f;
	
	yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
	pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

	if ( keyDeltaX )
	{
		headOffsetX += keyDeltaX * KEY_TRANSLATION_SPEED * dt;
	}
	else if ( fabs( headOffsetX - 0.0f ) > 0.001f )
	{
		headOffsetX += (headOffsetX > 0 ? -1 : 1) * KEY_TRANSLATION_SPEED * dt;
	}
	if ( keyDeltaZ )
	{
		headOffsetZ += -keyDeltaZ * KEY_TRANSLATION_SPEED * dt;
	}
	else if ( fabs( headOffsetZ - 0.0f ) > 0.001f )
	{
		headOffsetZ += (headOffsetZ > 0 ? -1 : 1) * KEY_TRANSLATION_SPEED * dt;
	}

	const core::Vec3 headOffset = core::Vec3_Make( headOffsetX, 0.0, headOffsetZ );

	const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( yaw );
	const core::Mat4x4 pitchMatrix = core::Mat4x4_RotationX( pitch );
	
	// Depth write pass
	m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
	m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );

	// Render cube map

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

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		m_ctx->OMSetRenderTargets( 1, &m_cube.colorViews[faceID], m_cube.depthViews[faceID] );

		float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_ctx->ClearRenderTargetView( m_cube.colorViews[faceID], pRGBA );
		m_ctx->ClearDepthStencilView( m_cube.depthViews[faceID], D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

		RenderingView_s view;
		Cube_MakeViewMatrix( &view.viewMatrix, headOffset, (CubeAxis_e)faceID );
		view.projMatrix = m_cubeProjMatrix;
		view.frameWidth = m_cube.size;	
		view.frameHeight = m_cube.size;
		
		DrawWorld( dt, &view );
	}
	
	// Render world

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

	{		
		m_ctx->OMSetRenderTargets( 1, &m_pColorView, m_pDepthStencilView );

		float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_ctx->ClearRenderTargetView( m_pColorView, pRGBA );
		m_ctx->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

		RenderingView_s view;
		core::Mat4x4_Mul( &view.viewMatrix, yawMatrix, pitchMatrix );
		core::Mat4x4_SetTranslation( &view.viewMatrix, headOffset );
		core::Mat4x4_AffineInverse( &view.viewMatrix );
		view.projMatrix = m_projMatrix;
		view.frameWidth = m_width;	
		view.frameHeight = m_height;
		
		DrawWorld( dt, &view );
	}

#if USE_PP

	// Post-Processes

	{
		D3D11_MAPPED_SUBRESOURCE res;
		V6_ASSERT_D3D11( m_ctx->Map( m_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

		v6::hlsl::CBView* cbView = (v6::hlsl::CBView*)res.pData;

		core::Mat4x4_Identity( &cbView->objectToView );
		cbView->viewToProj = m_projMatrix;
		cbView->frameWidth = m_width;
		cbView->frameHeight = m_height;
		cbView->zFar = ZFAR;
		
		cbView->depthLinearScale = 1.0f / m_projMatrix.m_row2.w;
		cbView->depthLinearBias = 1.0f / ZNEAR;

		m_ctx->Unmap( m_cbuffer, 0 );
	}

	m_ctx->PSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_cbuffer );

	for ( uint ppID = 0; ppID < POST_PROCESS_COUNT; ++ppID )
	{
		PostProcess* pp = &m_postProcesses[ppID];

		PostProcessPrepare( ppID );
		
		Shader* shader = &m_shaders[pp->shaderID];		

		Mesh_Draw( &m_meshes[MESH_VIRTUAL_TRIANGLE], shader, m_ctx );

		PostProcessDone( ppID );
	}

#endif
}

void CRenderingDevice::Present()
{
	m_pSwapChain->Present( 0, 0 );
}

void CRenderingDevice::Release()
{
	m_ctx->ClearState();
	
	Cube_Release( &m_cube );

	m_cbuffer->Release();

	for ( uint meshID = 0; meshID < MESH_COUNT; ++meshID )
	{
		Mesh* mesh = &m_meshes[meshID];
		if ( mesh->m_vertexBuffer )
			mesh->m_vertexBuffer->Release();
		if ( mesh->m_indexBuffer )
			mesh->m_indexBuffer->Release();
	}

	for ( uint shaderID = 0; shaderID < SHADER_COUNT; ++shaderID )
	{
		Shader* shader = &m_shaders[shaderID];
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
	V6_LOG("Viewer 0.0");

	v6::core::CHeap heap;
	v6::core::Stack stack( &heap, 100 * 1024 * 1024 );
	v6::core::CFileSystem filesystem;
	v6::core::FrameManager frameManager( &heap );
	v6::core::FrameBuffer* frameBuffer = nullptr;

	{
		stack.push();
		void* data;
		const int nSize = filesystem.ReadFile( "d:/data/v6/frameBuffer0.frm", &data, &stack );
		if ( nSize == -1 )
		{
			V6_ERROR( "Bad file" );
			return false;
		}

		if ( nSize < 4 + sizeof(v6::core::FrameDesc) )
		{
			V6_ERROR( "Bad header file size" );
			return false;
		}

		v6::core::CBufferReader bufferReader( data, nSize );
		char magic[5] = {};
		bufferReader.Read( 4, magic );
		if ( magic[0] != 'V' || magic[1] != '6' || magic[2] != 'F' || magic[3] != '0')
		{
			V6_ERROR( "Bad magic: %s", magic );
			return false;
		}

		v6::core::FrameDesc frameDesc;
		bufferReader.Read( sizeof( v6::core::FrameDesc ), &frameDesc );

		if ( bufferReader.GetRamaining() != v6::core::FrameManager::GetFrameBufferColorSize( &frameDesc ) + v6::core::FrameManager::GetFrameBufferDepthSize( &frameDesc ) )
		{
			V6_ERROR( "Bad data file size" );
			return false;
		}
		
		frameBuffer = frameManager.CreateFrameBuffer( &frameDesc );
		bufferReader.Read( v6::core::FrameManager::GetFrameBufferColorSize( &frameDesc ), frameBuffer->colors );
		bufferReader.Read( v6::core::FrameManager::GetFrameBufferDepthSize( &frameDesc ), frameBuffer->depths );
				
		stack.pop();
	}

	const int nWidth = 1280;
	const int nHeight = 720;	

	const char* const title = "V6 Player | version: 0.1";

	HWND hWnd = v6::viewer::CreateMainWindow( title, nWidth, nHeight );
	if (!hWnd)
	{
		V6_ERROR("Call to CreateWindow failed!");
		return -1;
	}

	v6::viewer::CRenderingDevice oRenderingDevice;
	if ( !oRenderingDevice.Create( nWidth, nHeight, hWnd, frameBuffer, &filesystem, &heap, &stack ) )
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

		if ( (frameId % 100) == 0 )
		{
			static __int64 fpsTickLast = frameTick;
			const __int64 fpsDelta = frameTick - fpsTickLast;
			fpsTickLast = frameTick;
			if ( fpsDelta > 0.0f )
			{
				const float fps = 100 / v6::core::ConvertTicksToSeconds( fpsDelta );
				char text[256];
				sprintf_s( text, sizeof( text ), "%s | fps: %.1f", title, fps );
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