/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_VIEW_H__
#define __V6_GRAPHIC_VIEW_H__

#include <v6/core/mat4x4.h>

BEGIN_V6_NAMESPACE

struct Camera_s
{
	Vec3			pos;
	Vec3			right;
	Vec3			up;
	Vec3			forward;
	float			znear;
	float			zfar;
	float			fov;
	float			aspectRatio;
	float			yaw;
	float			pitch;
	float			ipdHalf;
};

struct ViewEyeInfo_s
{
	Mat4x4			projMatrix;
	
	Vec3			offset;

	float			tanHalfFOVLeft;
	float			tanHalfFOVRight;
	float			tanHalfFOVUp;
	float			tanHalfFOVDown;
};

struct View_s
{
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

void	Camera_Create( Camera_s* camera, const Vec3* pos, float znear, float zfar, float fov, float aspectRatio, float ipd );
void	Camera_MakeView( View_s* view, const Camera_s* camera, u32 eye );
void	Camera_MakeView( View_s* view, const Camera_s* camera, const ViewEyeInfo_s* eyeInfo );
void	Camera_UpdateBasis( Camera_s* camera, float preYaw, const Mat4x4* preRotationMatrix );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_VIEW_H__