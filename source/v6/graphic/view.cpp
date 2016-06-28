/*V6*/

#include <v6/core/common.h>

#include <v6/graphic/view.h>

BEGIN_V6_NAMESPACE

void Camera_Create( Camera_s* camera, const Vec3* pos, float znear, float zfar, float fov, float aspectRatio, float ipd )
{
	camera->pos = *pos;
	camera->znear = znear;
	camera->zfar = zfar;
	camera->fov = fov;
	camera->aspectRatio = aspectRatio;
	camera->yaw = 0.0f;
	camera->pitch = 0.0f;
	camera->ipdHalf = ipd * 0.5f;
	Camera_UpdateBasis( camera, 0.0f, nullptr );
}

void Camera_MakeView( View_s* view, const Camera_s* camera, u32 eye )
{
	const Vec3 eyeOffset = camera->right * camera->ipdHalf;

	view->org = camera->pos + (eye == 0 ? -eyeOffset : eyeOffset);
	view->forward = camera->forward;
	view->right = camera->right;
	view->up = camera->up;

	view->viewMatrix = Mat4x4_View( &view->org, &camera->right, &camera->up, &camera->forward );
	view->projMatrix = Mat4x4_Projection( camera->znear, camera->fov, camera->aspectRatio );

	const float tanHalfFovH = Tan( 0.5f * camera->fov );
	const float tanHalfFovV = tanHalfFovH / camera->aspectRatio;
	view->tanHalfFOVLeft = tanHalfFovH;
	view->tanHalfFOVRight = tanHalfFovH;
	view->tanHalfFOVUp = tanHalfFovV;
	view->tanHalfFOVDown = tanHalfFovV;
}

void Camera_MakeView( View_s* view, const Camera_s* camera, const ViewEyeInfo_s* eyeInfo )
{
	view->org = camera->pos + eyeInfo->offset;
	view->forward = camera->forward;
	view->right = camera->right;
	view->up = camera->up;

	view->viewMatrix = Mat4x4_View( &view->org, &camera->right, &camera->up, &camera->forward );
	view->projMatrix = eyeInfo->projMatrix;

	view->tanHalfFOVLeft = eyeInfo->tanHalfFOVLeft;
	view->tanHalfFOVRight = eyeInfo->tanHalfFOVRight;
	view->tanHalfFOVUp = eyeInfo->tanHalfFOVUp;
	view->tanHalfFOVDown = eyeInfo->tanHalfFOVDown;
}

void Camera_UpdateBasis( Camera_s* camera, float preYaw, const Mat4x4* preRotationMatrix )
{
	Mat4x4 cameraRotationMatrix;
	Mat4x4_Mul3x3( &cameraRotationMatrix, Mat4x4_RotationY( preYaw + camera->yaw ), Mat4x4_RotationX( camera->pitch ) );

	Mat4x4 finalMatrix;

	if ( preRotationMatrix )
		Mat4x4_Mul3x3( &finalMatrix, *preRotationMatrix, cameraRotationMatrix );
	else
		finalMatrix = cameraRotationMatrix;

	finalMatrix.GetZAxis( &camera->forward );
	camera->forward = -camera->forward;
	finalMatrix.GetXAxis( &camera->right );
	finalMatrix.GetYAxis( &camera->up );
}

END_V6_NAMESPACE
