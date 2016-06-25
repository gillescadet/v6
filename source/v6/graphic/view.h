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
	float			fov;
	float			aspectRatio;
	float			yaw;
	float			pitch;
	float			ipdHalf;
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

void	Camera_Create( Camera_s* camera, const Vec3* pos, float znear, float fov, float aspectRatio, const Mat4x4* lookAt, float ipd );
void	Camera_MakeView( Camera_s* camera, View_s* view, u32 eye );
void	Camera_UpdateBasis( Camera_s* camera, const Mat4x4* lookAt );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_VIEW_H__