/*V6*/

#include <v6/core/common.h>

#include <v6/graphic/view.h>

BEGIN_V6_NAMESPACE

void Camera_Create( Camera_s* camera, float znear, float zfar, float fov, float aspectRatio )
{
	memset( camera, 0, sizeof( *camera ) );

	camera->znear = znear;
	camera->zfar = zfar;
	camera->fov = fov;
	camera->aspectRatio = aspectRatio;

	camera->stereoOrientation = Mat4x4_Identity();

	Camera_UpdateBasis( camera );
}

void Camera_SetPosOffset( Camera_s* camera, const Vec3* pos )
{
	camera->posOffset = *pos;
}

void Camera_SetYawOffset( Camera_s* camera, float yaw )
{
	camera->yawOffset = yaw;
}

void Camera_ResetOffsets( Camera_s* camera )
{
	camera->posOffset = Vec3_Zero();
	camera->yawOffset = 0.0f;
}

void Camera_SetStereoUsingIPD( Camera_s* camera, float ipd )
{
	const float halfIPD = 0.5f * ipd;

	camera->stereoOrientation = Mat4x4_Identity();
	camera->stereoEyePosLS[0] = Vec3_Make( -halfIPD, 0.0f, 0.0f );
	camera->stereoEyePosLS[1] = Vec3_Make( +halfIPD, 0.0f, 0.0f );
}

void Camera_SetStereoUsingOrientation( Camera_s* camera, const Mat4x4* orientation, const Vec3 eyePos[2] )

{
	camera->stereoOrientation = *orientation;
	camera->stereoEyePosLS[0] = eyePos[0];
	camera->stereoEyePosLS[1] = eyePos[1];
}

void Camera_ResetStereo( Camera_s* camera )
{
	camera->stereoOrientation = Mat4x4_Identity();
	camera->stereoEyePosLS[0] = Vec3_Zero();
	camera->stereoEyePosLS[1] = Vec3_Zero();
}


void Camera_UpdateBasis( Camera_s* camera )
{
	Mat4x4 cameraRotationMatrix;
	Mat4x4_Mul3x3( &cameraRotationMatrix, Mat4x4_RotationY( camera->yaw + camera->yawOffset ), Mat4x4_RotationX( camera->pitch ) );

	Mat4x4 finalMatrix;
	Mat4x4_Mul3x3( &finalMatrix, cameraRotationMatrix, camera->stereoOrientation );
	
	finalMatrix.GetZAxis( &camera->forward );
	camera->forward = -camera->forward;
	finalMatrix.GetXAxis( &camera->right );
	finalMatrix.GetYAxis( &camera->up );

	Mat4x4_TransformDir( &camera->stereoEyePosWS[0], cameraRotationMatrix, camera->stereoEyePosLS[0] );
	Mat4x4_TransformDir( &camera->stereoEyePosWS[1], cameraRotationMatrix, camera->stereoEyePosLS[1] );

	V6_ASSERT( Abs( Dot( camera->forward, camera->right ) ) < 0.00001f );
	V6_ASSERT( Abs( Dot( camera->right, camera->up ) ) < 0.00001f );
	V6_ASSERT( Abs( Dot( camera->up, camera->forward ) ) < 0.00001f );
}

void Camera_MakeView( View_s* view, const Camera_s* camera, u32 eye, const ViewProjection_s* overridenViewProjection )
{
	view->org = camera->pos + camera->posOffset + camera->stereoEyePosWS[eye];
	view->forward = camera->forward;
	view->right = camera->right;
	view->up = camera->up;

	view->viewMatrix = Mat4x4_View( &view->org, &camera->right, &camera->up, &camera->forward );
	
	if ( overridenViewProjection )
	{
		view->projMatrix = overridenViewProjection->projMatrix;

		view->tanHalfFOVLeft = overridenViewProjection->tanHalfFOVLeft;
		view->tanHalfFOVRight = overridenViewProjection->tanHalfFOVRight;
		view->tanHalfFOVUp = overridenViewProjection->tanHalfFOVUp;
		view->tanHalfFOVDown = overridenViewProjection->tanHalfFOVDown;
	}
	else
	{
		view->projMatrix = Mat4x4_Projection( camera->znear, camera->fov, camera->aspectRatio );

		const float tanHalfFovH = Tan( 0.5f * camera->fov );
		const float tanHalfFovV = tanHalfFovH / camera->aspectRatio;
		view->tanHalfFOVLeft = tanHalfFovH;
		view->tanHalfFOVRight = tanHalfFovH;
		view->tanHalfFOVUp = tanHalfFovV;
		view->tanHalfFOVDown = tanHalfFovV;
	}
}

END_V6_NAMESPACE
