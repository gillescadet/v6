/*V6*/

#include <v6/core/common.h>

#include <v6/core/mat3x3.h>
#include <v6/core/memory.h>
#include <v6/core/optimization.h>
#include <v6/core/vec2.h>
#include <v6/core/vec3.h>

BEGIN_V6_NAMESPACE

void Optimization_FindBestFittingLine2D( Vec2* center, Vec2* dir, const Vec2* points, u32 pointCount )
{
	V6_ASSERT( pointCount > 0 );

	*center = Vec2_Zero();
	for ( u32 pointID = 0; pointID < pointCount; ++pointID )
		*center += points[pointID];
	*center *= 1.0f / pointCount;

	float sumXX = 0.0f;
	float sumXY = 0.0f;
	float sumYY = 0.0f;

	for ( u32 pointID = 0; pointID < pointCount; ++pointID )
	{
		const Vec2 sample = points[pointID] - *center;

		sumXX += sample.x * sample.x;
		sumXY += sample.x * sample.y;
		sumYY += sample.y * sample.y;
	}

	if ( sumXX > sumYY )
	{
		dir->x = 1.0f;
		dir->y = sumXY / sumXX;
	}
	else
	{
		dir->x = sumXY / sumYY;
		dir->y = 1.0f;
	}

	dir->Normalize();
}

template < bool PRECENTRED >
void Optimization_FindBestFittingLine3D_Guts( Vec3* center, Vec3* dir, Mat3x3* covariance, const Vec3* points, u32 pointCount )
{
	Mat3x3 covarianceBuffer;
	if ( covariance == nullptr )
		covariance = &covarianceBuffer;
	Mat3x3_Covariance_Guts< PRECENTRED >( covariance, center, points, pointCount );

	const float oneOverSqrtOf3 = 0.5773502f;
	Vec3_Make( dir, oneOverSqrtOf3, oneOverSqrtOf3, oneOverSqrtOf3 );

	if ( Abs( covariance->m_row0.y ) < FLT_EPSILON && Abs( covariance->m_row0.z ) < FLT_EPSILON && Abs( covariance->m_row1.z ) < FLT_EPSILON )
		return;
	
	// search for the longest eigenvector using the power method

	for ( u32 step = 0; step < 8; ++step )
	{
		Vec3 r;
		Mat3x3_TransformDir( &r, *covariance, *dir );
		r.Normalize();
		const float dot = Dot( r, *dir );
		*dir = r;
		if ( dot > 0.999999f )
			break;
	}
}

void Optimization_FindBestFittingLine3D( Vec3* center, Vec3* dir, Mat3x3* covariance_optional, const Vec3* points, u32 pointCount )
{
	Optimization_FindBestFittingLine3D_Guts< false >( center, dir, covariance_optional, points, pointCount );
}

void Optimization_FindBestFittingLine3DPrecentred( Vec3* dir, Mat3x3* covariance_optional, const Vec3* points, u32 pointCount )
{
	Optimization_FindBestFittingLine3D_Guts< true >( nullptr, dir, covariance_optional, points, pointCount );
}

END_V6_NAMESPACE
