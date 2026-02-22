/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_HMD_H__
#define __V6_GRAPHIC_HMD_H__

#include <v6/core/vec2.h>
#include <v6/core/vec2i.h>

BEGIN_V6_NAMESPACE

class IAllocator;
struct View_s;

END_V6_NAMESPACE

BEGIN_V6_NAMESPACE

enum
{
	HMD_TRACKING_STATE_OFF,
	HMD_TRACKING_STATE_ON	= 1 << 0,
	HMD_TRACKING_STATE_POS	= 1 << 1,
};

struct HmdEyePose_s
{	
	Mat4x4	lookAt;
	Mat4x4	projection;
	Vec3	eyeOffset;
	float	tanHalfFOVLeft;
	float	tanHalfFOVRight;
	float	tanHalfFOVUp;
	float	tanHalfFOVDown;
};

struct HmdRenderTarget_s
{
	void*	tex;
	void*	rtv;
	void*	srv;
	void*	uav;
};

struct HmdStatus_s
{
	bool isVisible;			// True if the process has VR focus and thus is visible in the HMD.
	bool hmdPresent;		// True if an HMD is present.
	bool hmdMounted;		// True if the HMD is on the user's head.
	bool displayLost;		// True if the session is in a display-lost state. See ovr_SubmitFrame.
	bool shouldQuit;		// True if the application should initiate shutdown.
	bool shouldRecenter;	// True if UX has requested re-centering. Must call ovr_ClearShouldRecenterFlag or ovr_RecenterTrackingOrigin.
};

enum HmdInputEvent_e : u8
{
	HMD_INPUT_EVENT_NONE,
	HMD_INPUT_EVENT_PRESSED,
	HMD_INPUT_EVENT_RELEASED,
};

struct HmdInputState_s
{
	HmdInputEvent_e up;
	HmdInputEvent_e down;
	HmdInputEvent_e left;
	HmdInputEvent_e right;
	HmdInputEvent_e enter;
	HmdInputEvent_e back;
};

u32		Hmd_BeginRendering( HmdRenderTarget_s renderTargets[2], HmdEyePose_s poses[2], float zNear, float zFar );
bool	Hmd_CreateMirrorResources( void* device, const Vec2i* winRenderTargetSize );
bool	Hmd_CreateResources( void* device, const Vec2i* eyeRenderTargetSize );
bool	Hmd_CreateMirrorResources( void* device, const Vec2i* winRenderTargetSize );
bool	Hmd_EndRendering();
float	Hmd_GetDisplayRefreshRate();
bool	Hmd_GetInputState( HmdInputState_s* inputState );
void*	Hmd_GetMirrorTexture();
Vec2	Hmd_GetRecommendedTanHalfFOV();
Vec2i	Hmd_GetRecommendedRenderTargetSize();
Vec2i	Hmd_GetResolution();
bool	Hmd_GetStatus( HmdStatus_s* status );
bool	Hmd_Init();
void	Hmd_MakeView( View_s* renderingView, const HmdEyePose_s* eyePose, const Vec3* orgOffset, float yawOffset, u32 eye );
void	Hmd_Recenter();
void	Hmd_ReleaseMirrorResources();
void	Hmd_ReleaseMirrorTexture( void* tex );
void	Hmd_ReleaseResources();
void	Hmd_SetPerfHUdMode( u32 mode );
void	Hmd_Shutdown();

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_HMD_H__
