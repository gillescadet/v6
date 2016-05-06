/*V6*/

#pragma once

#ifndef __V6_CORE_OBJ_READER_H__
#define __V6_CORE_OBJ_READER_H__

#include <v6/core/image.h>
#include <v6/core/vec2.h>
#include <v6/core/vec3.h>

BEGIN_V6_NAMESPACE

class IAllocator;

END_V6_NAMESPACE

BEGIN_V6_NAMESPACE

struct ObjMaterial_s
{
	char			name[64];
	char			mapKd[256];
	char			mapD[256];
	char			mapBump[256];
	Vec3		ka;
	Vec3		kd;
	Vec3		ks;
	Vec3		ke;
	float			ns;
	float			ni;
	float			d;
	float			tr;
	Vec3		tf;
	int				illum;
};

struct ObjVertex_s
{
	u32		posID;
	u32		normalID;
	u32		uvID;
};

struct ObjTriangle_s
{
	ObjVertex_s		vertices[3];
};

struct ObjMesh_s
{
	char			name[64];
	u32		materialID;
	u32		firstTriangleID;
	u32		triangleCount;
};

struct ObjScene_s
{
	Vec3*		positions;
	Vec3*		normals;
	Vec2*		uvs;
	ObjTriangle_s*	triangles;
	ObjMaterial_s*	materials;
	ObjMesh_s*		meshes;
	u32		materialCount;
	u32		meshCount;
};

u32		Obj_ReadMaterialFile( ObjMaterial_s** materials, const char* filenameMTL, IAllocator* allocator );
bool			Obj_ReadObjectFile( ObjScene_s* scene, const char* filenameOBJ, IAllocator* allocator );

END_V6_NAMESPACE

#endif // __V6_CORE_OBJ_READER_H__