/*V6*/

#include <v6/core/common.h>

#include <v6/graphic/view.h>

BEGIN_V6_NAMESPACE

void Camera_Create( Camera_s* camera, const Vec3* pos, float znear, float fov, float aspectRatio )
{
	camera->pos = *pos;
	camera->znear = znear;
	camera->fov = fov;
	camera->aspectRatio = aspectRatio;
	camera->yaw = 0.0f;
	camera->pitch = 0.0f;
	Camera_UpdateBasis( camera );
}

void Camera_MakeView( Camera_s* camera, View_s* view )
{
	view->org = camera->pos;
	view->forward = camera->forward;
	view->right = camera->right;
	view->up = camera->up;

	view->viewMatrix = Mat4x4_View( &camera->pos, &camera->forward, &camera->up, &camera->right );
	view->projMatrix = Mat4x4_Projection( camera->znear, camera->fov, camera->aspectRatio );

	const float tanHalfFovH = Tan( 0.5f * camera->fov );
	const float tanHalfFovV = tanHalfFovH / camera->aspectRatio;
	view->tanHalfFOVLeft = tanHalfFovH;
	view->tanHalfFOVRight = tanHalfFovH;
	view->tanHalfFOVUp = tanHalfFovV;
	view->tanHalfFOVDown = tanHalfFovV;
}

void Camera_UpdateBasis( Camera_s* camera )
{
	Mat4x4 orientationMatrix;
	const Mat4x4 yawMatrix = Mat4x4_RotationY( camera->yaw );
	const Mat4x4 pitchMatrix = Mat4x4_RotationX( camera->pitch );
	Mat4x4_Mul( &orientationMatrix, yawMatrix, pitchMatrix );

	orientationMatrix.GetZAxis( &camera->forward );
	camera->forward = -camera->forward;
	orientationMatrix.GetXAxis( &camera->right );
	orientationMatrix.GetYAxis( &camera->up );
}

END_V6_NAMESPACE
