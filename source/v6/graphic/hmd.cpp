/*V6*/

#include <OVR_CAPI_D3D.h>
#include <Extras/OVR_Math.h>

#include <v6/core/common.h>

#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/vec2i.h>
#include <v6/core/vec3.h>
#include <v6/graphic/hmd.h>

BEGIN_V6_NAMESPACE

static ovrSession					s_session = nullptr;
static ovrHmdDesc					s_hmdDesc;
static const u32					s_textureCount = 2;
static Vec2i						s_eyeRenderTargetSize;
static ovrSwapTextureSet*			s_textureSets[2] = { nullptr, nullptr };
static ID3D11RenderTargetView*		s_textureRTVs[2][s_textureCount] = {};
static ID3D11UnorderedAccessView*	s_textureUAVs[2][s_textureCount] = {};
static ovrTexture*					s_mirrorTexture = nullptr;
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
			ovr_DestroySwapTextureSet( s_session, s_textureSets[eye] );
			s_textureSets[eye] = nullptr;
		}

		for ( int i = 0; i < s_textureSets[eye]->TextureCount; ++i )
		{
			if ( s_textureRTVs[eye][i] )
			{
				s_textureRTVs[eye][i]->Release();
				s_textureRTVs[eye][i] = nullptr;
			}

			if ( s_textureUAVs[eye][i] )
			{
				s_textureUAVs[eye][i]->Release();
				s_textureUAVs[eye][i] = nullptr;
			}
		}
	}

	if ( s_mirrorTexture )
	{
		ovr_DestroyMirrorTexture( s_session, s_mirrorTexture );
		s_mirrorTexture = nullptr;
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
	V6_MSG( "hmd.cameraFrustumHFovInDegrees: %g\n", RadToDeg( s_hmdDesc.CameraFrustumHFovInRadians ) );
	V6_MSG( "hmd.cameraFrustumVFovInDegrees: %g\n", RadToDeg( s_hmdDesc.CameraFrustumVFovInRadians ) );
	V6_MSG( "hmd.cameraFrustumNearZInMeters: %g\n", s_hmdDesc.CameraFrustumNearZInMeters );
	V6_MSG( "hmd.cameraFrustumFarZInMeters : %g\n", s_hmdDesc.CameraFrustumFarZInMeters );
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
	V6_ASSERT( s_mirrorTexture == nullptr );

	const OVR::Sizei recommendedTex0Size = ovr_GetFovTextureSize( s_session, ovrEye_Left, GetFovPort( 0 ), 1.0f );
	const OVR::Sizei recommendedTex1Size = ovr_GetFovTextureSize( s_session, ovrEye_Right, GetFovPort( 1 ), 1.0f );

	V6_ASSERT( recommendedTex0Size.w == recommendedTex1Size.w );
	return Vec2i_Make( recommendedTex0Size.w, Max( recommendedTex0Size.h, recommendedTex1Size.h ) );
}

bool Hmd_CreateResources( void* device, const Vec2i* eyeRenderTargetSize )
{
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] == nullptr && s_textureSets[1] == nullptr );
	V6_ASSERT( s_mirrorTexture == nullptr );

	s_eyeRenderTargetSize = *eyeRenderTargetSize;

	V6_MSG( "hmd.renderTarget              : %dx%d\n", s_eyeRenderTargetSize.x, s_eyeRenderTargetSize.y );

	ID3D11Device* d3d11Device = (ID3D11Device*)device;

	{
		D3D11_TEXTURE2D_DESC td = {};
		td.Width = s_eyeRenderTargetSize.x;
		td.Height = s_eyeRenderTargetSize.y;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.CPUAccessFlags = 0;
		td.MiscFlags = 0;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;

		for ( u32 eye = 0; eye < 2; ++eye )
		{
			const ovrResult result = ovr_CreateSwapTextureSetD3D11( s_session, d3d11Device, &td, ovrSwapTextureSetD3D11_Typeless, &s_textureSets[eye] );

			if ( OVR_FAILURE( result ) )
			{
				ovrErrorInfo errorInfo;
				ovr_GetLastErrorInfo( &errorInfo );
				V6_ERROR( "ovr_CreateSwapTextureSetD3D11 failed: %s\n", errorInfo.ErrorString );
				ReleaseResources();
				return false;
			}

			V6_ASSERT( s_textureCount == s_textureSets[eye]->TextureCount );
			for ( int i = 0; i < s_textureSets[eye]->TextureCount; ++i )
			{
				ovrD3D11Texture* tex = (ovrD3D11Texture*)&s_textureSets[eye]->Textures[i];

				{
					D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
					rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					if ( d3d11Device->CreateRenderTargetView( tex->D3D11.pTexture, &rtvd, &s_textureRTVs[eye][i] ) != S_OK )
					{
						V6_ERROR( "CreateRenderTargetView failed\n" );
						ReleaseResources();
						return false;
					}
				}

				{
					D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
					uavd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
					if ( d3d11Device->CreateUnorderedAccessView( tex->D3D11.pTexture, &uavd, &s_textureUAVs[eye][i] ) != S_OK )
					{
						V6_ERROR( "CreateUnorderedAccessView failed\n" );
						ReleaseResources();
						return false;
					}
				}
			}
		}
	}

	{
		D3D11_TEXTURE2D_DESC td = {};
		td.Width = s_eyeRenderTargetSize.x * 2;
		td.Height = s_eyeRenderTargetSize.y;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.CPUAccessFlags = 0;
		td.MiscFlags = 0;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

		const ovrResult result = ovr_CreateMirrorTextureD3D11( s_session, d3d11Device, &td, 0, &s_mirrorTexture );
		if ( OVR_FAILURE( result ) )
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo( &errorInfo );
			V6_ERROR( "ovr_CreateMirrorTextureD3D11 failed: %s\n", errorInfo.ErrorString );
			ReleaseResources();
			return false;
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

	if ( (ts.StatusFlags & ovrStatus_HmdConnected) == 0 )
		return HMD_TRACKING_STATE_OFF;

	u32 state = HMD_TRACKING_STATE_ON;

	state |= (ts.StatusFlags & ovrStatus_OrientationTracked) != 0 ? HMD_TRACKING_STATE_ORIENTATION : 0;
	state |= (ts.StatusFlags & ovrStatus_PositionTracked) != 0 ? HMD_TRACKING_STATE_POS: 0;

	ovrEyeRenderDesc eyeRenderDesc[2];
	eyeRenderDesc[0] = ovr_GetRenderDesc( s_session, ovrEye_Left, GetFovPort( 0 ) );
	eyeRenderDesc[1] = ovr_GetRenderDesc( s_session, ovrEye_Right, GetFovPort( 1 ) );
	
	ovrVector3f hmdToEyeViewOffset[2];
	hmdToEyeViewOffset[0] = eyeRenderDesc[0].HmdToEyeViewOffset;
	hmdToEyeViewOffset[1] = eyeRenderDesc[1].HmdToEyeViewOffset;

#if 1
	s_layer.Header.Type = ovrLayerType_EyeFov;
#else
	s_layer.Header.Type = ovrLayerType_Direct;
#endif
	s_layer.Header.Flags = 0;
	s_layer.EyeFov.ColorTexture[0] = s_textureSets[0];
	s_layer.EyeFov.ColorTexture[1] = s_textureSets[1];
	s_layer.EyeFov.Fov[0] = eyeRenderDesc[0].Fov;
	s_layer.EyeFov.Fov[1] = eyeRenderDesc[1].Fov;
	s_layer.EyeFov.Viewport[0] = OVR::Recti( 0, 0, s_eyeRenderTargetSize.x, s_eyeRenderTargetSize.y );
	s_layer.EyeFov.Viewport[1] = OVR::Recti( 0, 0, s_eyeRenderTargetSize.x, s_eyeRenderTargetSize.y );
	s_layer.EyeFov.SensorSampleTime = ovr_GetTimeInSeconds();
	ovr_CalcEyePoses( ts.HeadPose.ThePose, hmdToEyeViewOffset, s_layer.EyeFov.RenderPose );
	
	for ( u32 eye = 0; eye < 2; ++eye )
	{
		const u32 textureIndex = s_textureSets[eye]->CurrentIndex;
		renderTargets[eye].texture2D = ((ovrD3D11Texture*)&s_textureSets[eye]->Textures[textureIndex])->D3D11.pTexture;
		renderTargets[eye].rtv = s_textureRTVs[eye][textureIndex];
		renderTargets[eye].uav = s_textureUAVs[eye][textureIndex];

		const OVR::Matrix4f mxLookAt( s_layer.EyeFov.RenderPose[eye] );
		memcpy( &poses[eye].lookAt, &mxLookAt, sizeof( mxLookAt ) );

		poses[eye].lookAt.m_row0.w *= M_TO_CM;
		poses[eye].lookAt.m_row1.w *= M_TO_CM;
		poses[eye].lookAt.m_row2.w *= M_TO_CM;

		const OVR::Matrix4f mxProj = ovrMatrix4f_Projection( s_layer.EyeFov.Fov[eye], zNear, zFar, ovrProjection_RightHanded );
		memcpy( &poses[eye].projection, &mxProj, sizeof( mxProj ) );

		poses[eye].tanHalfFOVLeft = eyeRenderDesc[eye].Fov.LeftTan;
		poses[eye].tanHalfFOVRight = eyeRenderDesc[eye].Fov.RightTan;
		poses[eye].tanHalfFOVUp = eyeRenderDesc[eye].Fov.UpTan;
		poses[eye].tanHalfFOVDown = eyeRenderDesc[eye].Fov.DownTan;
	}

	return state;
}

bool Hmd_EndRendering( HmdOuput_s* output )
{
	V6_ASSERT( s_session != nullptr );
	V6_ASSERT( s_textureSets[0] != nullptr && s_textureSets[1] != nullptr );
	V6_ASSERT( s_layer.Header.Type != 0 );

	ovrLayerHeader* layers = &s_layer.Header;
	const ovrResult result = ovr_SubmitFrame( s_session, 0, nullptr, &layers, 1 );

	for ( u32 eye = 0; eye < 2; ++eye )
		s_textureSets[eye]->CurrentIndex = (s_textureSets[eye]->CurrentIndex + 1) % s_textureSets[eye]->TextureCount;

	s_layer.Header.Type = ovrLayerType_Disabled;

	if ( result == ovrSuccess )
	{
		output->texture2D = reinterpret_cast< ovrD3D11Texture* >( s_mirrorTexture )->D3D11.pTexture;
		return true;
	}

	output->texture2D = nullptr;
	return true;
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

END_V6_NAMESPACE
