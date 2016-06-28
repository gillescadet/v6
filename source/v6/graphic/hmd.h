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

u32		Hmd_BeginRendering( HmdRenderTarget_s renderTargets[2], HmdEyePose_s poses[2], float zNear, float zFar );
bool	Hmd_CreateResources( void* device, const Vec2i* eyeRenderTargetSize, bool createMirrorTexture );
bool	Hmd_EndRendering();
Vec2	Hmd_GetRecommendedFOV();
Vec2i	Hmd_GetRecommendedRenderTargetSize();
bool	Hmd_Init();
void	Hmd_MakeView( View_s* renderingView, const HmdEyePose_s* eyePose, const Vec3* orgOffset, float yawOffset, u32 eye );
void	Hmd_Recenter();
void	Hmd_ReleaseResources();
void	Hmd_Shutdown();

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_HMD_H__
