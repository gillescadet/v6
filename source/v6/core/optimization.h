/*V6*/

#pragma once

#ifndef __V6_CORE_OPTIMIZATION_H__
#define __V6_CORE_OPTIMIZATION_H__

BEGIN_V6_NAMESPACE

class Stack;
struct Vec2;
struct Vec3;

void Optimization_FindBestFittingLine2D( Vec2* center, Vec2* dir, const Vec2* points, u32 pointCount );
void Optimization_FindBestFittingLine3D( Vec3* center, Vec3* dir, Mat3x3* covariance_optional, const Vec3* points, u32 pointCount );
void Optimization_FindBestFittingLine3DPrecentred( Vec3* dir, Mat3x3* covariance_optional, const Vec3* points, u32 pointCount );

END_V6_NAMESPACE

#endif // __V6_CORE_OPTIMIZATION_H__
