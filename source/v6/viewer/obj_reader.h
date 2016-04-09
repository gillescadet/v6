/*V6*/

#pragma once

#ifndef __V6_VIEWER_OBJ_READER_H__
#define __V6_VIEWER_OBJ_READER_H__

#include <v6/core/image.h>
#include <v6/core/vec2.h>
#include <v6/core/vec3.h>

BEGIN_V6_CORE_NAMESPACE

class IAllocator;

END_V6_CORE_NAMESPACE

BEGIN_V6_VIEWER_NAMESPACE

struct ObjMaterial_s
{
	char			name[64];
	char			mapKd[256];
	char			mapD[256];
	char			mapBump[256];
	core::Vec3		ka;
	core::Vec3		kd;
	core::Vec3		ks;
	core::Vec3		ke;
	float			ns;
	float			ni;
	float			d;
	float			tr;
	core::Vec3		tf;
	int				illum;
};

struct ObjVertex_s
{
	core::u32		posID;
	core::u32		normalID;
	core::u32		uvID;
};

struct ObjTriangle_s
{
	ObjVertex_s		vertices[3];
};

struct ObjMesh_s
{
	char			name[64];
	core::u32		materialID;
	core::u32		firstTriangleID;
	core::u32		triangleCount;
};

struct ObjScene_s
{
	core::Vec3*		positions;
	core::Vec3*		normals;
	core::Vec2*		uvs;
	ObjTriangle_s*	triangles;
	ObjMaterial_s*	materials;
	ObjMesh_s*		meshes;
	core::u32		materialCount;
	core::u32		meshCount;
};

core::u32		Obj_ReadMaterialFile( ObjMaterial_s** materials, const char* filenameMTL, core::IAllocator* allocator );
bool			Obj_ReadObjectFile( ObjScene_s* scene, const char* filenameOBJ, core::IAllocator* allocator );

END_V6_VIEWER_NAMESPACE

#endif // __V6_VIEWER_OBJ_READER_H__