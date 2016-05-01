/*V6*/

#pragma once

#ifndef __V6_VIEWER_HMD_H__
#define __V6_VIEWER_HMD_H__

BEGIN_V6_NAMESPACE

class IAllocator;

END_V6_NAMESPACE

BEGIN_V6_NAMESPACE

enum
{
	HMD_TRACKING_STATE_OFF,
	HMD_TRACKING_STATE_ON			= 1 << 0,
	HMD_TRACKING_STATE_ORIENTATION	= 1 << 1,
	HMD_TRACKING_STATE_POS			= 1 << 2,
};

struct HmdEyePose_s
{	
	Mat4x4	lookAt;
	Mat4x4	projection;
	float			tanHalfFOVLeft;
	float			tanHalfFOVRight;
	float			tanHalfFOVUp;
	float			tanHalfFOVDown;
};

struct HmdOuput_s
{
	void*	texture2D;
};

struct HmdRenderTarget_s
{
	void*	texture2D;
	void*	rtv;
	void*	uav;
};

u32		Hmd_BeginRendering( HmdRenderTarget_s renderTargets[2], HmdEyePose_s poses[2], float zNear, float zFar );
bool			Hmd_CreateResources( void* device, const Vec2i* eyeRenderTargetSize );
bool			Hmd_EndRendering( HmdOuput_s* output );
Vec2i		Hmd_GetRecommendedRenderTargetSize();
bool			Hmd_Init();
void			Hmd_ReleaseResources();
void			Hmd_Shutdown();

END_V6_NAMESPACE

#endif // __V6_VIEWER_HMD_H__
