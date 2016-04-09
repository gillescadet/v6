/*V6*/

#pragma once

#ifndef __V6_SCENE_INFO_READER_H__
#define __V6_SCENE_INFO_READER_H__

#include <v6/core/vec2.h>
#include <v6/core/vec3.h>

BEGIN_V6_VIEWER_NAMESPACE

struct SceneInfo_s
{	
	static const core::u32	MAX_PATH_COUNT = 2;
	static const core::u32	MAX_POSITION_COUNT = 128;
	struct
	{
		char				entityName[64];
		core::Vec3			positions[MAX_POSITION_COUNT];
		core::u32			positionCount;
		float				speed;
	}						paths[MAX_PATH_COUNT];

	float					cameraYaw;
	float					worldUnitToCM;
	bool					dirty;
};

void SceneInfo_Clear( SceneInfo_s* sceneInfo );
bool SceneInfo_ReadFromFile( SceneInfo_s* sceneInfo, const char* filename );
bool SceneInfo_WriteToFile( const SceneInfo_s* sceneInfo, const char* filename );

END_V6_VIEWER_NAMESPACE

#endif // __V6_VIEWER_SCENE_INFO_H__