/*V6*/

#include <OVR.h>
#include <Extras/OVR_Math.h>

#include <v6/viewer/common.h>

#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/vec3.h>

#include <v6/viewer/hmd.h>


BEGIN_V6_VIEWER_NAMESPACE

static ovrSession   s_session;
static ovrHmdDesc	s_hmdDesc;

static const core::u32 s_ovrHmdTypeCount = 9;
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

bool Hmd_Init()
{
	{
		const ovrResult result = ovr_Initialize( NULL );
		if( OVR_FAILURE(result) )
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo( &errorInfo );
			V6_ERROR( "ovr_Initialize failed: %s", errorInfo.ErrorString );
			return false;
		}
	}
	
	{
		ovrGraphicsLuid luid;
		ovrResult result = ovr_Create( &s_session, &luid );
		if ( OVR_FAILURE( result ) )
		{
			ovr_Shutdown();
			return false;
		}
	}

	s_hmdDesc = ovr_GetHmdDesc( s_session );

	if ( (core::u32)s_hmdDesc.Type < s_ovrHmdTypeCount )
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
	V6_MSG( "hmd.cameraFrustumHFovInDegrees: %g\n", core::RadToDeg( s_hmdDesc.CameraFrustumHFovInRadians ) );
	V6_MSG( "hmd.cameraFrustumVFovInDegrees: %g\n", core::RadToDeg( s_hmdDesc.CameraFrustumVFovInRadians ) );
	V6_MSG( "hmd.cameraFrustumNearZInMeters: %g\n", s_hmdDesc.CameraFrustumNearZInMeters );
	V6_MSG( "hmd.cameraFrustumFarZInMeters : %g\n", s_hmdDesc.CameraFrustumFarZInMeters );
	V6_MSG( "hmd.availableHmdCaps          : 0x%08X\n", s_hmdDesc.AvailableHmdCaps );
	V6_MSG( "hmd.defaultHmdCaps            : 0x%08X\n", s_hmdDesc.DefaultHmdCaps );
	V6_MSG( "hmd.availableTrackingCaps     : 0x%08X\n", s_hmdDesc.AvailableTrackingCaps );
	V6_MSG( "hmd.defaultTrackingCaps       : 0x%08X\n", s_hmdDesc.DefaultTrackingCaps );
	V6_MSG( "hmd.defaultEyeFov[0]          : up %g, down %g, left %g, right %g\n", core::RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[0].UpTan ) ),	core::RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[0].DownTan ) ),		core::RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[0].LeftTan ) ),	core::RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[0].RightTan ) ) );
	V6_MSG( "hmd.defaultEyeFov[1]          : up %g, down %g, left %g, right %g\n", core::RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[1].UpTan ) ),	core::RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[1].DownTan ) ),		core::RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[1].LeftTan ) ),	core::RadToDeg( atanf( s_hmdDesc.DefaultEyeFov[1].RightTan ) ) );
	V6_MSG( "hmd.maxEyeFov[0]              : up %g, down %g, left %g, right %g\n", core::RadToDeg( atanf( s_hmdDesc.MaxEyeFov[0].UpTan ) ),		core::RadToDeg( atanf( s_hmdDesc.MaxEyeFov[0].DownTan ) ),			core::RadToDeg( atanf( s_hmdDesc.MaxEyeFov[0].LeftTan ) ),		core::RadToDeg( atanf( s_hmdDesc.MaxEyeFov[0].RightTan ) ) );
	V6_MSG( "hmd.maxEyeFov[1]              : up %g, down %g, left %g, right %g\n", core::RadToDeg( atanf( s_hmdDesc.MaxEyeFov[1].UpTan ) ),		core::RadToDeg( atanf( s_hmdDesc.MaxEyeFov[1].DownTan ) ),			core::RadToDeg( atanf( s_hmdDesc.MaxEyeFov[1].LeftTan ) ),		core::RadToDeg( atanf( s_hmdDesc.MaxEyeFov[1].RightTan ) ) );
	V6_MSG( "hmd.resolution                : %dx%d\n", s_hmdDesc.Resolution.w, s_hmdDesc.Resolution.h );
	V6_MSG( "hmd.displayRefreshRate        : %g hz\n", s_hmdDesc.DisplayRefreshRate );

	return true;
}

core::u32 Hmd_Track( core::Mat4x4* view )
{	
	const ovrTrackingState ts = ovr_GetTrackingState( s_session, ovr_GetTimeInSeconds(), false );

	if ( (ts.StatusFlags & ovrStatus_HmdConnected) == 0 )
		return HMD_TRACKING_STATE_OFF;

	core::u32 state = HMD_TRACKING_STATE_ON;

	const OVR::Matrix4f mx( ts.HeadPose.ThePose );
	memcpy( view, &mx, sizeof( mx ) );

	state |= (ts.StatusFlags & ovrStatus_OrientationTracked) != 0 ? HMD_TRACKING_STATE_ORIENTATION : 0;
	state |= (ts.StatusFlags & ovrStatus_PositionTracked) != 0 ? HMD_TRACKING_STATE_POS: 0;

	return state;
}

void Hmd_Shutdown()
{
	ovr_Destroy( s_session );

	ovr_Shutdown();
}

END_V6_VIEWER_NAMESPACE
