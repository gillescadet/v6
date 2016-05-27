/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/core/mat4x4.h>
#include <v6/core/vec3.h>
#include <v6/graphic/gpu.h>
#include <v6/graphic/scene.h>

BEGIN_V6_NAMESPACE

void Material_Create( Material_s* material, MaterialDraw_f drawFunction )
{
	material->drawFunction = drawFunction;
	memset( material->textureIDs, 0xFF, sizeof( material->textureIDs ) );
	material->diffuse = Vec3_Make( 1.0f, 1.0f, 1.0f );
}

void Material_SetTexture( Material_s* material, u32 textureID, u32 textureSlot )
{
	V6_ASSERT( textureSlot < Material_s::TEXTURE_MAX_COUNT );
	material->textureIDs[textureSlot] = textureID;
}

void Entity_Create( Entity_s* entity, u32 materialID, u32 meshID, const Vec3& pos, float scale )
{
	entity->name = nullptr;
	entity->materialID = materialID;
	entity->meshID = meshID;
	entity->pos = pos;
	entity->scale = scale;
	entity->visible = true;
}

void Entity_Draw( Entity_s* entity, Scene_s* scene, const View_s* view, u32 flags )
{
	if ( !entity->visible )
		return;

	Material_s* material = &scene->materials[entity->materialID];
	material->drawFunction( material, entity, scene, view, flags );
}

void Scene_Create( Scene_s* scene )
{
	memset( scene, 0, sizeof( *scene) );
}

u32 Scene_FindEntityByName( const Scene_s* scene, const char* entityName )
{
	if ( entityName != nullptr && entityName[0] != 0 )
	{
		for ( u32 entityID = 0; entityID < scene->entityCount; ++entityID )
		{
			if ( scene->entities[entityID].name && strcmp( scene->entities[entityID].name, entityName ) == 0 )
				return entityID;
		}
	}

	return (u32)-1;
}

void Scene_Draw( Scene_s* scene, const View_s* view, u32 flags )
{
	for ( u32 entityID = 0; entityID < scene->entityCount; ++entityID )
		Entity_Draw( &scene->entities[entityID], scene, view, flags );
}

u32 Scene_GetNewMeshID( Scene_s* scene )
{
	V6_ASSERT( scene->meshCount < Scene_s::MESH_MAX_COUNT );
	const u32 meshID = scene->meshCount;
	++scene->meshCount;
	return meshID;
}

u32 Scene_GetNewTextureID( Scene_s* scene )
{
	V6_ASSERT( scene->textureCount < Scene_s::TEXTURE_MAX_COUNT );
	const u32 textureID = scene->textureCount;
	++scene->textureCount;
	return textureID;
}

u32 Scene_GetNewMaterialID( Scene_s* scene )
{
	V6_ASSERT( scene->materialCount < Scene_s::MATERIAL_MAX_COUNT );
	const u32 materialID = scene->materialCount;
	++scene->materialCount;
	return materialID;
}

u32 Scene_GetNewEntityID( Scene_s* scene )
{
	V6_ASSERT( scene->entityCount < Scene_s::ENTITY_MAX_COUNT );
	const u32 entityID = scene->entityCount;
	++scene->entityCount;
	return entityID;
}

void Scene_Release( Scene_s* scene )
{
	for ( u32 meshID = 0; meshID < scene->meshCount; ++meshID )
		GPUMesh_Release( &scene->meshes[meshID] );
	
	for ( u32 textureID = 0; textureID < scene->textureCount; ++textureID )
		GPUTexture2D_Release( &scene->textures[textureID] );
	
	scene->meshCount = 0;
	scene->textureCount = 0;
	scene->materialCount = 0;
	scene->entityCount = 0;
}

END_V6_NAMESPACE
