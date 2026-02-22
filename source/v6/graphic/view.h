/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_VIEW_H__
#define __V6_GRAPHIC_VIEW_H__

#include <v6/core/mat4x4.h>

BEGIN_V6_NAMESPACE

struct Camera_s
{
	Mat4x4			stereoOrientation;
	Vec3			stereoEyePosLS[2];
	Vec3			stereoEyePosHS[2];
	Vec3			stereoEyePosWS[2];

	Vec3			pos;
	Vec3			posOffset;

	Vec3			right;
	Vec3			up;
	Vec3			forward;

	float			znear;
	float			zfar;
	float			tanHalfFov;
	float			aspectRatio;

	float			yaw;
	float			yawOffset;
	float			pitch;
};

struct ViewProjection_s
{
	Mat4x4			projMatrix;

	float			tanHalfFOVLeft;
	float			tanHalfFOVRight;
	float			tanHalfFOVUp;
	float			tanHalfFOVDown;
};

struct View_s
{
	Mat4x4			lockedViewMatrix;
	Mat4x4			viewMatrix;
	Mat4x4			projMatrix;

	Vec3			org;
	Vec3			right;
	Vec3			up;
	Vec3			forward;
	
	float			tanHalfFOVLeft;
	float			tanHalfFOVRight;
	float			tanHalfFOVUp;
	float			tanHalfFOVDown;
};

void	Camera_Create( Camera_s* camera, float znear, float zfar, float tanFov, float aspectRatio );
void	Camera_MakeView( View_s* view, const Camera_s* camera, u32 eye, const ViewProjection_s* overridenViewProjection );
void	Camera_SetPosOffset( Camera_s* camera, const Vec3* pos );
void	Camera_SetStereoUsingIPD( Camera_s* camera, float ipd );
void	Camera_SetStereoUsingOrientation( Camera_s* camera, const Mat4x4* orientation, const Vec3 eyeOffsets[2], const Vec3 eyePos[2] );
void	Camera_SetYawOffset( Camera_s* camera, float yaw );
void	Camera_ResetOffsets( Camera_s* camera );
void	Camera_ResetStereo( Camera_s* camera );
void	Camera_UpdateBasis( Camera_s* camera );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_VIEW_H__