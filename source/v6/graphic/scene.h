/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_SCENE_H__
#define __V6_GRAPHIC_SCENE_H__

BEGIN_V6_NAMESPACE

struct Material_s;
struct Entity_s;
struct Scene_s;
struct View_s;

typedef void (*MaterialDraw_f)( Material_s* material, Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags );

struct Material_s
{
	static const u32 TEXTURE_MAX_COUNT	= 4;
	static const u32 TEXTURE_INVALID	= (u32)-1;

	MaterialDraw_f	drawFunction;
	u32				textureIDs[TEXTURE_MAX_COUNT];
	Vec3			diffuse;
};

struct Entity_s
{
	const char*		name;
	u32				materialID;
	u32				meshID;
	Vec3			pos;
	float			scale;
	bool			visible;
};

struct Camera_s
{
	Vec3			pos;
	Vec3			forward;
	Vec3			right;
	Vec3			up;
	float			znear;
	float			fov;
	float			aspectRatio;
	float			yaw;
	float			pitch;
};

struct Scene_s
{
	static const u32 MESH_MAX_COUNT		= 4096;
	static const u32 TEXTURE_MAX_COUNT	= 1024;
	static const u32 MATERIAL_MAX_COUNT	= 256;
	static const u32 ENTITY_MAX_COUNT	= 16384;

	GPUMesh_s		meshes[MESH_MAX_COUNT];
	GPUTexture2D_s	textures[TEXTURE_MAX_COUNT];
	Material_s		materials[MATERIAL_MAX_COUNT];
	Entity_s		entities[ENTITY_MAX_COUNT];
	u32				meshCount;
	u32				textureCount;
	u32				materialCount;
	u32				entityCount;
};

struct View_s
{
	Mat4x4			viewMatrix;
	Mat4x4			projMatrix;
};

void	Camera_Create( Camera_s* camera, const Vec3* pos, float znear, float fov, float aspectRatio );
void	Camera_MakeView( Camera_s* camera, View_s* view );
void	Camera_UpdateBasis( Camera_s* camera );

void	Entity_Create( Entity_s* entity, u32 materialID, u32 meshID, const Vec3& pos, float scale );
void	Entity_Draw( Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags );

void	Material_Create( Material_s* material, MaterialDraw_f drawFunction );
void	Material_SetTexture( Material_s* material, u32 textureID, u32 textureSlot );

void	Scene_Create( Scene_s* scene );
void	Scene_Draw( Scene_s* scene, const View_s* view, u32 flags );
u32		Scene_FindEntityByName( const Scene_s* scene, const char* entityName );
u32		Scene_GetNewEntityID( Scene_s* scene );
u32		Scene_GetNewMaterialID( Scene_s* scene );
u32		Scene_GetNewMeshID( Scene_s* scene );
u32		Scene_GetNewTextureID( Scene_s* scene );
void	Scene_Release( Scene_s* scene );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_SCENE_H__
