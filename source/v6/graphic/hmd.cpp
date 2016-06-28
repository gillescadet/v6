/*V6*/

#include <v6/core/common.h>

#if V6_UE4_PLUGIN == 0

#include <v6/core/windows_begin.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <OVR_CAPI_D3D.h>
#include <Extras/OVR_Math.h>

#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/vec2i.h>
#include <v6/core/vec3.h>
#include <v6/graphic/hmd.h>
#include <v6/graphic/view.h>

#define V6_MIRROR_TEXTURE 1

BEGIN_V6_NAMESPACE

static ovrSession					s_session = nullptr;
static ovrHmdDesc					s_hmdDesc;
static const u32					s_textureCount = 3;
static Vec2i						s_eyeRenderTargetSize;
static ovrTextureSwapChain			s_textureSets[2] = { nullptr, nullptr };
static ID3D11RenderTargetView*		s_textureRTVs[2][s_textureCount] = {};
static ID3D11ShaderResourceView*	s_textureSRVs[2][s_textureCount] = {};
static ID3D11UnorderedAccessView*	s_textureUAVs[2][s_textureCount] = {};
static ovrLayer_Union				s_layer = {};

static const u32 s_ovrHmdTypeCount = 9;
static const char* const s_ovrHmdTypeNames[s_ovrHmdTypeCount] = 
{
	"ovrHmd_None",
	"ovrHmd_DK1",
	"ovrHmd_DKHD",
	"ovrHmd_DK2",
	"ovrHmd_CB",
	"ovrHmd_Other",
	"ovrHmd_E3_2015",
	"ovrHmd_ES06",
	"ovrHmd_ES09",
};

static ovrFovPort GetFovPort( u32 eye )
{
#if 1
	return s_hmdDesc.DefaultEyeFov[eye];
#else
	ovrFovPort fov;
	fov.UpTan = 1.0f;
	fov.DownTan = 1.0f;
	fov.LeftTan = 1.0f;
	fov.RightTan = 1.0f;
	return fov;
#endif
}

static void ReleaseResources()
{
	for ( u32 eye = 0; eye < 2; ++eye )
	{
		if ( s_textureSets[eye] )
		{
			ovr_DestroyTextureSwapChain( s_session, s_textureSets[eye] );
			s_textureSets[eye] = nullptr;
		}

		for ( int i = 0; i < s_textureCount; ++i )
		{
			if ( s_textureRTVs[eye][i] )
			{
				s_textureRTVs[eye][i]->Release();
				s_textureRTVs[eye][i] = nullptr;
			}

			if ( s_textureSRVs[eye][i] )
			{
				s_textureSRVs[eye][i]->Release();
				s_textureSRVs[eye][i] = nullptr;
			}

			if ( s_textureUAVs[eye][i] )
			{
				s_textureUAVs[eye][i]->Release();
				s_textureUAVs[eye][i] = nullptr;
			}
		}
	}
}

bool Hmd_Init()
{
	V6_ASSERT( s_session == nullptr );

	{
		const ovrResult result = ovr_Initialize( NULL );
		if( OVR_FAILURE(result) )
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo( &errorInfo );
			V6_ERROR( "ovr_Initialize failed: %s\n", errorInfo.ErrorString );
			return false;
		}
	}
	
	{
		ovrGraphicsLuid luid;
		ovrResult result = ovr_Create( &s_session, &luid );
		if ( OVR_FAILURE( result ) )
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo( &errorInfo );
			V6_ERROR( "ovr_Create failed: %s\n", errorInfo.ErrorString );
			ovr_Shutdown();
			s_session = nullptr;
			return false;
		}
	}

	s_hmdDesc = ovr_GetHmdDesc( s_session );

	if ( (u32)s_hmdDesc.Type < s_ovrHmdTypeCount )
	{
		V6_MSG( "hmd.type                      : %s\n", s_ovrHmdTypeNames[s_hmdDesc.Type] );
	}
	else
	{
		V6_MSG( "hmd.type                      : unknown\n" );
	}
	V6_MSG( "hmd.productName               : %s\n", s_hmdDesc.ProductName );
	V6_MSG( "hmd.manufacturer              : %s\n", s_hmdDesc.Manufacturer );	
	V6_MSG( "hmd.vendorId                  : %d\n", s_hmdDesc.VendorId );
	V6_MSG( "hmd.productId                 : %d\n", s_hmdDesc.ProductId );
	V6_MSG( "hmd.serialNumber              : %s\n", s_hmdDesc.SerialNumber );
	V6_MSG( "hmd.firmwareMajor             : %d\n", s_hmdDesc.FirmwareMajor );
	V6_MSG( "hmd.firmwareMinor             : %d\n", s_hmdDesc.FirmwareMinor );
	V6_MSG( "hmd.availableHmdCaps          : 0x%08X\n", s_hmdDesc.AvailableHmdCaps );
	V6_MSG( "hmd.defaultHmdCaps            : 0x%08X\n", s_hmdDesc.DefaultHmdCaps );
	V6_MSG( "hmd.availableTrackingCaps     : 0x%08X\n", s_hmdDesc.AvailableTrackingCaps );
	V6_MSG( "hmd.defaultTrackingCaps       : 0x%08X\n", s_hmdDesc.DefaultTrackingCaps );
	V6_MSG( "hmd.defaultEyeFov[0]          : up %g, down %g, left %g, right %g\n", RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[0].UpTan ) ),	RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[0].DownTan ) ),		RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[0].LeftTan ) ),	RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[0].RightTan ) ) );
	V6_MSG( "hmd.defaultEyeFov[1]          : up %g, down %g, left %g, right %g\n", RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[1].UpTan ) ),	RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[1].DownTan ) ),		RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[1].LeftTan ) ),	RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[1].RightTan ) ) );
	V6_MSG( "hmd.maxEyeFov[0]              : up %g, down %g, left %g, right %g\n", RadToDeg( atanf( s_hmdDesc.MaxEyeFov[0].UpTan ) ),		RadToDeg( atanf( s_hmdDesc.MaxEyeFov[0].DownTan ) ),			RadToDeg( atanf( s_hmdDesc.MaxEyeFov[0].LeftTan ) ),		RadToDeg( atanf( s_hmdDesc.MaxEyeFov[0].RightTan ) ) );
	V6_MSG( "hmd.maxEyeFov[1]              : up %g, down %g, left %g, right %g\n", RadToDeg( atanf( s_hmdDesc.MaxEyeFov[1].UpTan ) ),		RadToDeg( atanf( s_hmdDesc.MaxEyeFov[1].DownTan ) ),			RadToDeg( atanf( s_hmdDesc.MaxEyeFov[1].LeftTan ) ),		RadToDeg( atanf( s_hmdDesc.MaxEyeFov[1].RightTan ) ) );
	V6_MSG( "hmd.resolution                : %dx%d\n", s_hmdDesc.Resolution.w, s_hmdDesc.Resolution.h );
	V6_MSG( "hmd.displayRefreshRate        : %g hz\n", s_hmdDesc.DisplayRefreshRate );

	return true;
}

Vec2i Hmd_GetRecommendedRenderTargetSize()
{
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] == nullptr && s_textureSets[1] == nullptr );

	const OVR::Sizei recommendedTex0Size = ovr_GetFovTextureSize( s_session, ovrEye_Left, GetFovPort( 0 ), 1.0f );
	const OVR::Sizei recommendedTex1Size = ovr_GetFovTextureSize( s_session, ovrEye_Right, GetFovPort( 1 ), 1.0f );

	V6_ASSERT( recommendedTex0Size.w == recommendedTex1Size.w );
	return Vec2i_Make( recommendedTex0Size.w, Max( recommendedTex0Size.h, recommendedTex1Size.h ) );
}

Vec2 Hmd_GetRecommendedFOV()
{
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] == nullptr && s_textureSets[1] == nullptr );

	Vec2 fov;

	fov.x = Max(  GetFovPort( 0 ).LeftTan + GetFovPort( 0 ).RightTan, GetFovPort( 1 ).LeftTan + GetFovPort( 1 ).RightTan );
	fov.y = Max( GetFovPort( 0 ).DownTan + GetFovPort( 0 ).UpTan, GetFovPort( 1 ).DownTan + GetFovPort( 1 ).UpTan );
	
	return fov;
}

bool Hmd_CreateResources( void* device, const Vec2i* eyeRenderTargetSize, bool createMirrorTexture )
{
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] == nullptr && s_textureSets[1] == nullptr );

	s_eyeRenderTargetSize = *eyeRenderTargetSize;

	V6_MSG( "hmd.renderTarget              : %dx%d\n", s_eyeRenderTargetSize.x, s_eyeRenderTargetSize.y );

	ID3D11Device* d3d11Device = (ID3D11Device*)device;

	{
		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.Width = s_eyeRenderTargetSize.x;
		desc.Height = s_eyeRenderTargetSize.y;
		desc.MipLevels = 1;
		desc.SampleCount = 1;
		desc.MiscFlags = ovrTextureMisc_DX_Typeless;
		desc.BindFlags = ovrTextureBind_DX_RenderTarget | ovrTextureBind_DX_UnorderedAccess;
		desc.StaticImage = ovrFalse;

		for ( u32 eye = 0; eye < 2; ++eye )
		{
			const ovrResult result = ovr_CreateTextureSwapChainDX( s_session, d3d11Device, &desc, &s_textureSets[eye] );

			if ( OVR_FAILURE( result ) )
			{
				ovrErrorInfo errorInfo;
				ovr_GetLastErrorInfo( &errorInfo );
				V6_ERROR( "ovr_CreateTextureSwapChainDX failed: %s\n", errorInfo.ErrorString );
				ReleaseResources();
				return false;
			}

			int textureCount = 0;
			ovr_GetTextureSwapChainLength( s_session, s_textureSets[eye], &textureCount );
			V6_ASSERT( s_textureCount == textureCount );

			for ( int i = 0; i < textureCount; ++i )
			{
				ID3D11Texture2D* texture = nullptr;
				ovr_GetTextureSwapChainBufferDX( s_session, s_textureSets[eye], i, IID_PPV_ARGS( &texture ) );

				{
					D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
					rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					rtvd.Texture2D.MipSlice = 0;
					if ( d3d11Device->CreateRenderTargetView( texture, &rtvd, &s_textureRTVs[eye][i] ) != S_OK )
					{
						V6_ERROR( "CreateRenderTargetView failed\n" );
						ReleaseResources();
						return false;
					}
				}

				{
					D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
					srvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					srvd.Texture2D.MipLevels = 1;
					srvd.Texture2D.MostDetailedMip = 0;
					if ( d3d11Device->CreateShaderResourceView( texture, &srvd, &s_textureSRVs[eye][i] ) != S_OK )
					{
						V6_ERROR( "CreateShaderResourceView failed\n" );
						ReleaseResources();
						return false;
					}
				}

				{
					D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
					uavd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
					uavd.Texture2D.MipSlice = 0;
					if ( d3d11Device->CreateUnorderedAccessView( texture, &uavd, &s_textureUAVs[eye][i] ) != S_OK )
					{
						V6_ERROR( "CreateUnorderedAccessView failed\n" );
						ReleaseResources();
						return false;
					}
				}
			}
		}
	}

	return true;
}

void Hmd_ReleaseResources()
{
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] != nullptr && s_textureSets[1] != nullptr );
	V6_ASSERT( s_layer.Header.Type == 0 );

	ReleaseResources();
}

u32 Hmd_BeginRendering( HmdRenderTarget_s renderTargets[2], HmdEyePose_s poses[2], float zNear, float zFar )
{	
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] != nullptr && s_textureSets[1] != nullptr );
	V6_ASSERT( s_layer.Header.Type == 0 );

	const double displayMidpointSeconds = ovr_GetPredictedDisplayTime( s_session, 0 );
	const ovrTrackingState ts = ovr_GetTrackingState( s_session, displayMidpointSeconds, ovrTrue );

	if ( (ts.StatusFlags & ovrStatus_OrientationTracked) == 0 )
		return HMD_TRACKING_STATE_OFF;

	u32 state = HMD_TRACKING_STATE_ON;

	state |= (ts.StatusFlags & ovrStatus_PositionTracked) != 0 ? HMD_TRACKING_STATE_POS: 0;

	ovrEyeRenderDesc eyeRenderDesc[2];
	eyeRenderDesc[0] = ovr_GetRenderDesc( s_session, ovrEye_Left, GetFovPort( 0 ) );
	eyeRenderDesc[1] = ovr_GetRenderDesc( s_session, ovrEye_Right, GetFovPort( 1 ) );
	
	ovrVector3f hmdToEyeOffset[2];
	hmdToEyeOffset[0] = eyeRenderDesc[0].HmdToEyeOffset;
	hmdToEyeOffset[1] = eyeRenderDesc[1].HmdToEyeOffset;

	s_layer.Header.Type = ovrLayerType_EyeFov;
	s_layer.Header.Flags = 0;
	s_layer.EyeFov.ColorTexture[0] = s_textureSets[0];
	s_layer.EyeFov.ColorTexture[1] = s_textureSets[1];
	s_layer.EyeFov.Fov[0] = eyeRenderDesc[0].Fov;
	s_layer.EyeFov.Fov[1] = eyeRenderDesc[1].Fov;
	s_layer.EyeFov.Viewport[0] = OVR::Recti( 0, 0, s_eyeRenderTargetSize.x, s_eyeRenderTargetSize.y );
	s_layer.EyeFov.Viewport[1] = OVR::Recti( 0, 0, s_eyeRenderTargetSize.x, s_eyeRenderTargetSize.y );
	s_layer.EyeFov.SensorSampleTime = ovr_GetTimeInSeconds();
	ovr_CalcEyePoses( ts.HeadPose.ThePose, hmdToEyeOffset, s_layer.EyeFov.RenderPose );
	
	for ( u32 eye = 0; eye < 2; ++eye )
	{
		int textureIndex;
		ovr_GetTextureSwapChainCurrentIndex( s_session, s_textureSets[eye], &textureIndex );
		
		ID3D11Texture2D* texture = nullptr;
		ovr_GetTextureSwapChainBufferDX( s_session, s_textureSets[eye], textureIndex, IID_PPV_ARGS( &texture ) );

		renderTargets[eye].tex = texture;
		renderTargets[eye].rtv = s_textureRTVs[eye][textureIndex];
		renderTargets[eye].srv = s_textureSRVs[eye][textureIndex];
		renderTargets[eye].uav = s_textureUAVs[eye][textureIndex];

		const OVR::Matrix4f mxLookAt( s_layer.EyeFov.RenderPose[eye] );
		memcpy( &poses[eye].lookAt, &mxLookAt, sizeof( mxLookAt ) );

		poses[eye].lookAt.m_row0.w *= V6_M_TO_CM;
		poses[eye].lookAt.m_row1.w *= V6_M_TO_CM;
		poses[eye].lookAt.m_row2.w *= V6_M_TO_CM;

		const OVR::Matrix4f mxProj = ovrMatrix4f_Projection( s_layer.EyeFov.Fov[eye], zNear, zFar, 0 );
		memcpy( &poses[eye].projection, &mxProj, sizeof( mxProj ) );

		poses[eye].tanHalfFOVLeft = eyeRenderDesc[eye].Fov.LeftTan;
		poses[eye].tanHalfFOVRight = eyeRenderDesc[eye].Fov.RightTan;
		poses[eye].tanHalfFOVUp = eyeRenderDesc[eye].Fov.UpTan;
		poses[eye].tanHalfFOVDown = eyeRenderDesc[eye].Fov.DownTan;
	}

	return state;
}

bool Hmd_EndRendering()
{
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] != nullptr && s_textureSets[1] != nullptr );
	V6_ASSERT( s_layer.Header.Type != 0 );

	for ( u32 eye = 0; eye < 2; ++eye )
		ovr_CommitTextureSwapChain( s_session, s_textureSets[eye] );

	ovrLayerHeader* layers = &s_layer.Header;
	const ovrResult result = ovr_SubmitFrame( s_session, 0, nullptr, &layers, 1 );

	s_layer.Header.Type = ovrLayerType_Disabled;

	return result == ovrSuccess;
}

void Hmd_Shutdown()
{
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] == nullptr && s_textureSets[1] == nullptr );
	V6_ASSERT( s_layer.Header.Type == 0 );

	ovr_Destroy( s_session );

	ovr_Shutdown();
	s_session = nullptr;
}

void Hmd_MakeView( View_s* renderingView, const HmdEyePose_s* eyePose, const Vec3* orgOffset, float yawOffset, u32 eye )
{
	const Mat4x4 yawMatrix = Mat4x4_RotationY( yawOffset );
	Mat4x4_Mul( &renderingView->viewMatrix, yawMatrix, eyePose->lookAt );
	renderingView->org = *orgOffset + renderingView->viewMatrix.GetTranslation();
	renderingView->forward = -renderingView->viewMatrix.GetZAxis().Normalized();
	renderingView->right = renderingView->viewMatrix.GetXAxis().Normalized();
	renderingView->up = renderingView->viewMatrix.GetYAxis().Normalized();
	Mat4x4_SetTranslation( &renderingView->viewMatrix, renderingView->org );
	Mat4x4_AffineInverse( &renderingView->viewMatrix );
	renderingView->projMatrix = eyePose->projection;
	renderingView->tanHalfFOVLeft = eyePose->tanHalfFOVLeft;
	renderingView->tanHalfFOVRight = eyePose->tanHalfFOVRight;
	renderingView->tanHalfFOVUp = eyePose->tanHalfFOVUp;
	renderingView->tanHalfFOVDown = eyePose->tanHalfFOVDown;
}

void Hmd_Recenter()
{
	V6_ASSERT( s_session != nullptr );
	
	ovr_RecenterTrackingOrigin( s_session );
}

END_V6_NAMESPACE

#endif // #if V6_UE4_PLUGIN == 0