/*V6*/

#include <v6/viewer/common.h>
#include <v6/viewer/obj_reader.h>

#include <v6/core/filesystem.h>
#include <v6/core/memory.h>

BEGIN_V6_VIEWER_NAMESPACE

static bool IsSpaceCar( char c )
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char* SkipSpace( char* token )
{
	while ( IsSpaceCar( *token ) ) ++token;
	return token;
}

static core::u32 Count( const char* str, char token )
{
	core::u32 count = 0;
	
	while ( *str )
	{
		count += *str == token ? 1 : 0;
		++str;
	}

	return count;
}

static char* NextToken( char* token )
{
	while ( *token && !IsSpaceCar( *token ) ) ++token;
	while ( IsSpaceCar( *token ) ) ++token;
	return token;
}

static void TrimRight( char* token )
{
	while ( *token && !IsSpaceCar( *token ) ) ++token;
	*token = 0;
}

static ObjMaterial_s* Obj_CreateDefaultMaterial( core::IAllocator* allocator )
{
	ObjMaterial_s* material = (ObjMaterial_s*)allocator->alloc( sizeof( ObjMaterial_s ) );
	memset( material, 0, sizeof( ObjMaterial_s ) );	
	material->kd = core::Vec3_Make( 1.0f, 1.0f, 1.0f );
	return material;
}

core::u32 Obj_ReadMaterialFile( ObjMaterial_s** materials, const char* filenameMTL, core::IAllocator* allocator )
{
	FILE* fileMTL = nullptr;
	if ( fopen_s( &fileMTL, filenameMTL, "rb" ) != 0 )
		return 0;

	core::u32 materialCount = 0;

	char line[4096];
	while ( fgets( line, sizeof( line ), fileMTL ) )
	{
		if ( line[0] == '0' || line[0] == '\r' || line[0] == '\n' || line[0] == '#' )
			continue;

		char* token = SkipSpace( line );

		if ( _strnicmp( token, "newmtl", 6 ) == 0 )
			++materialCount;
	}

	if ( !materialCount )
		return 0;

	char path[256];
	core::FilePath_ExtractPath( path, sizeof( path ), filenameMTL );

	*materials = (ObjMaterial_s*)allocator->alloc( materialCount * sizeof( ObjMaterial_s ) );

	ObjMaterial_s* curMaterial = nullptr;
	core::u32 materialID = 0;

	fseek( fileMTL, 0, SEEK_SET );
	while ( fgets( line, sizeof( line ), fileMTL ) )
	{
		if ( line[0] == '0' || line[0] == '\r' || line[0] == '\n' || line[0] == '#' )
			continue;

		char* token = SkipSpace( line );

		if ( _strnicmp( token, "newmtl", 6 ) == 0 )
		{
			curMaterial = &(*materials)[materialID];
			char* name = NextToken( token );
			TrimRight( name );
			memset( curMaterial, 0, sizeof( *curMaterial ) );
			strcpy_s( curMaterial->name, sizeof( curMaterial->name ), name );
			curMaterial->kd = core::Vec3_Make( 1.0f, 1.0f, 1.0f );

			++materialID;
			continue;
		}

		if ( !curMaterial )
			continue;

		if ( _strnicmp( token, "ns", 2 ) == 0 )
		{
			const char* ns = NextToken( token );
			sscanf_s( ns, "%g", &curMaterial->ns );
		}
		else if ( _strnicmp( token, "ni", 2 ) == 0 )
		{
			const char* ni = NextToken( token );
			sscanf_s( ni, "%g", &curMaterial->ni );
		}
		else if ( _strnicmp( token, "d", 1 ) == 0 )
		{
			const char* d = NextToken( token );
			sscanf_s( d, "%g", &curMaterial->d );
		}
		else if ( _strnicmp( token, "tr", 2 ) == 0 )
		{
			const char* tr = NextToken( token );
			sscanf_s( tr, "%g", &curMaterial->tr );
		}
		else if ( _strnicmp( token, "tf", 2 ) == 0 )
		{
			const char* tf = NextToken( token );
			sscanf_s( tf, "%g %g %g", &curMaterial->tf.x, &curMaterial->tf.y, &curMaterial->tf.z );
		}
		else if ( _strnicmp( token, "illum", 5 ) == 0 )
		{
			const char* illum = NextToken( token );
			sscanf_s( illum, "%d", &curMaterial->illum );
		}
		else if ( _strnicmp( token, "ka", 2 ) == 0 )
		{
			const char* ka = NextToken( token );
			sscanf_s( ka, "%g %g %g", &curMaterial->ka.x, &curMaterial->ka.y, &curMaterial->ka.z );
		}
		else if ( _strnicmp( token, "kd", 2 ) == 0 )
		{
			const char* kd = NextToken( token );
			sscanf_s( kd, "%g %g %g", &curMaterial->kd.x, &curMaterial->kd.y, &curMaterial->kd.z );
		}
		else if ( _strnicmp( token, "ks", 2 ) == 0 )
		{
			const char* ks = NextToken( token );
			sscanf_s( ks, "%g %g %g", &curMaterial->ks.x, &curMaterial->ks.y, &curMaterial->ks.z );
		}
		else if ( _strnicmp( token, "ke", 2 ) == 0 )
		{
			const char* ke = NextToken( token );
			sscanf_s( ke, "%g %g %g", &curMaterial->ke.x, &curMaterial->ke.y, &curMaterial->ke.z );
		}
		else if ( _strnicmp( token, "map_ka", 6 ) == 0 || _strnicmp( token, "map_kd", 6 ) == 0 )
		{
			char* mapKd = NextToken( token );
			TrimRight( mapKd );
			core::FilePath_Make( curMaterial->mapKd, sizeof( curMaterial->mapKd ), path, mapKd );
		}
		else if ( _strnicmp( token, "map_d", 5 ) == 0 )
		{
			char* mapD = NextToken( token );
			TrimRight( mapD );
			core::FilePath_Make( curMaterial->mapD, sizeof( curMaterial->mapD ), path, mapD );
		}
		else if ( _strnicmp( token, "map_bump", 8 ) == 0 || _strnicmp( token, "bump", 4 ) == 0 )
		{
			char* mapBump = NextToken( token );
			TrimRight( mapBump );
			core::FilePath_Make( curMaterial->mapBump, sizeof( curMaterial->mapBump ), path, mapBump );		}
		else
		{
			V6_WARNING( "Unknown token %s\n", token );
		}
	}

	fclose( fileMTL );

	V6_ASSERT( materialID == materialCount );

	return materialCount;
}

bool Obj_ReadObjectFile( ObjScene_s* scene, const char* filenameOBJ, core::IAllocator* allocator )
{
	FILE* fileOBJ = nullptr;
	if ( fopen_s( &fileOBJ, filenameOBJ, "rb" ) != 0 )
		return false;

	ObjMaterial_s* materials = nullptr;
	core::u32 materialCount = 0;
	core::u32 positionCount = 0;
	core::u32 normalCount = 0;
	core::u32 uvCount = 0;
	core::u32 meshCount = 0;
	core::u32 triangleCount = 0;

	char path[256];
	core::FilePath_ExtractPath( path, sizeof( path ), filenameOBJ );

	char line[4096];
	while ( fgets( line, sizeof( line ), fileOBJ ) )
	{
		if ( line[0] == '0' || line[0] == '\r' || line[0] == '\n' || line[0] == '#' )
			continue;

		char* token = SkipSpace( line );

		if ( _strnicmp( token, "mtllib", 6 ) == 0 )
		{
			V6_ASSERT( materials == nullptr );
			char* filenameMTL = NextToken( token );
			TrimRight( filenameMTL );

			char filepathMTL[256];
			core::FilePath_Make( filepathMTL, sizeof( filepathMTL ), path, filenameMTL );
			materialCount = Obj_ReadMaterialFile( &materials, filepathMTL, allocator );
			if ( materialCount == 0 )
			{
				fclose( fileOBJ );
				V6_ERROR( "No material found.\n" );
				return false;
			}
			continue;
		}
						
		if ( _strnicmp( token, "v ", 2 ) == 0 )
			++positionCount;
		else if ( _strnicmp( token, "vn ", 3 ) == 0 )
			++normalCount;
		else if ( _strnicmp( token, "vt ", 3 ) == 0 )
			++uvCount;		
		else if ( _strnicmp( token, "usemtl ", 7 ) == 0 )
			++meshCount;		
		else if ( _strnicmp( token, "f ", 2 ) == 0 )
		{
			core::u32 vertexCount = 0;
			char* ids = NextToken( token );
			while ( *ids )
			{
				if ( IsSpaceCar( *ids ) )
				{
					++vertexCount;
					ids = SkipSpace( ids );
				}
				else
				{
					++ids;
				}
			}
			V6_ASSERT( vertexCount == 3 || vertexCount == 4 );
			triangleCount += vertexCount-2;
		}		
	}

	if ( positionCount == 0 || triangleCount == 0 )
	{
		fclose( fileOBJ );
		return false;
	}

	if ( materialCount == 0 )
	{
		materials = Obj_CreateDefaultMaterial( allocator );
		materialCount = 1;
	}

	scene->materials = materials;
	scene->positions = allocator->newArray< core::Vec3 >( positionCount );
	scene->normals = normalCount > 0 ? allocator->newArray< core::Vec3 >( normalCount ) : nullptr;
	scene->uvs = uvCount > 0 ? allocator->newArray< core::Vec2 >( uvCount ) : nullptr;
	scene->triangles = allocator->newArray< ObjTriangle_s >( triangleCount );
	scene->meshes = allocator->newArray< ObjMesh_s >( core::Max( 1u, meshCount ) );
	scene->materialCount = materialCount;
	scene->meshCount = core::Max( 1u, meshCount );

	core::u32 positionID = 0;
	core::u32 normalID = 0;
	core::u32 uvID = 0;
	core::u32 meshID = 0;
	core::u32 triangleID = 0;
	core::u32 lastDisplayTriangleID = 0;

	ObjMesh_s* mesh = nullptr;

	if ( meshCount == 0 )
	{
		mesh = &scene->meshes[meshID];
		mesh->firstTriangleID = triangleID;
		mesh->triangleCount = 0;
		mesh->materialID = 0;
		meshID = 1;
		meshCount = 1;
	}

	V6_MSG( "Loading...\n" );
	
	fseek( fileOBJ, 0, SEEK_SET );
	while ( fgets( line, sizeof( line ), fileOBJ ) )
	{
		if ( line[0] == '0' || line[0] == '\r' || line[0] == '\n' || line[0] == '#' )
			continue;

		char* token = SkipSpace( line );

		if ( _strnicmp( token, "v ", 2 ) == 0 )
		{
			V6_ASSERT( positionID < positionCount );
			char* v = NextToken( token );
			core::Vec3* pos = &scene->positions[positionID];
			const core::u32 n = sscanf_s( v, "%g %g %g", &pos->x, &pos->y, &pos->z );
			V6_ASSERT( n == 3 );
			++positionID;
		}
		else if ( _strnicmp( token, "vn", 2 ) == 0 )
		{
			V6_ASSERT( normalID < normalCount );
			char* v = NextToken( token );
			core::Vec3* normal = &scene->normals[normalID];
			const core::u32 n = sscanf_s( v, "%g %g %g", &normal->x, &normal->y, &normal->z );
			V6_ASSERT( n == 3 );
			++normalID;
		}
		else if ( _strnicmp( token, "vt", 2 ) == 0 )
		{
			V6_ASSERT( uvID < uvCount );
			char* v = NextToken( token );
			core::Vec2* uv = &scene->uvs[uvID];
			const core::u32 n = sscanf_s( v, "%g %g", &uv->x, &uv->y );
			V6_ASSERT( n == 2 );
			++uvID;
		}
		else if ( _strnicmp( token, "usemtl ", 7 ) == 0 )
		{
			V6_ASSERT( meshID < meshCount );

			char* materialName = NextToken( token );
			TrimRight( materialName );

			core::u32 materialID;
			for ( materialID = 0; materialID < materialCount; ++materialID )
			{
				if ( _stricmp( materials[materialID].name, materialName ) == 0 )
					break;
			}

			V6_ASSERT( materialID < materialCount );

			mesh = &scene->meshes[meshID];
			mesh->firstTriangleID = triangleID;
			mesh->triangleCount = 0;
			mesh->materialID = materialID;
			++meshID;

			printf( "\r" );
			V6_MSG( "%d/%d loaded triangles", triangleID, triangleCount );
			lastDisplayTriangleID = triangleID;
		}
		else if ( _strnicmp( token, "g ", 2 ) == 0 )
			; //
		else if ( _strnicmp( token, "s ", 2 ) == 0 )
			; //
		else if ( _strnicmp( token, "f ", 2 ) == 0 )
		{
			V6_ASSERT( mesh );
			char* f = NextToken( token );
			const core::u32 separatorCount = Count( f, '/' );

			auto Triangle_InitVertices = [ positionID, uvID, normalID ]( ObjTriangle_s* triangle, const int* indices, const core::u32 indexRanks[9] )
			{
				for ( core::u32 vertexID = 0; vertexID < 3; ++vertexID )
				{				
					if ( indexRanks[vertexID*3+0] == (core::u32)-1 )
						triangle->vertices[vertexID].posID = (core::u32)-1;
					else if ( indices[indexRanks[vertexID*3+0]] < 0 )
						triangle->vertices[vertexID].posID = positionID + indices[indexRanks[vertexID*3+0]];
					else
						triangle->vertices[vertexID].posID = indices[indexRanks[vertexID*3+0]]-1;

					if ( indexRanks[vertexID*3+1] == (core::u32)-1 )
						triangle->vertices[vertexID].uvID = (core::u32)-1;
					else if ( indices[indexRanks[vertexID*3+1]] < 0 )
						triangle->vertices[vertexID].uvID = uvID + indices[indexRanks[vertexID*3+1]];
					else
						triangle->vertices[vertexID].uvID = indices[indexRanks[vertexID*3+1]]-1;

					if ( indexRanks[vertexID*3+2] == (core::u32)-1 )
						triangle->vertices[vertexID].normalID = (core::u32)-1;
					else if ( indices[indexRanks[vertexID*3+2]] < 0 )
						triangle->vertices[vertexID].normalID = normalID + indices[indexRanks[vertexID*3+2]];
					else
						triangle->vertices[vertexID].normalID = indices[indexRanks[vertexID*3+2]]-1;
				}
			};
			
			static const core::u32 NA = (core::u32)-1;
			int ids[12];

			if ( separatorCount == 0 )
			{				
				const core::u32 vertexCount = sscanf_s( f, "%d %d %d %d", ids+0, ids+1, ids+2, ids+3 );
				V6_ASSERT( vertexCount == 3 || vertexCount == 4 );
			
				{
					V6_ASSERT( triangleID < triangleCount );		
					const core::u32 indexRanks[] = { 0, NA, NA,  1, NA, NA, 2, NA, NA };
					Triangle_InitVertices( &scene->triangles[triangleID], ids, indexRanks );
					++triangleID;
					++mesh->triangleCount;
				}
						
				if ( vertexCount == 4 )
				{
					V6_ASSERT( triangleID < triangleCount );			
					const core::u32 indexRanks[] = { 0, NA, NA,  2, NA, NA, 3, NA, NA };
					Triangle_InitVertices( &scene->triangles[triangleID], ids, indexRanks );
					++triangleID;
					++mesh->triangleCount;
				}
			}
			else if ( separatorCount == 3 || separatorCount == 4 )
			{
				const core::u32 n = sscanf_s( f, "%d/%d %d/%d %d/%d %d/%d", ids+0, ids+1, ids+2, ids+3, ids+4, ids+5, ids+6, ids+7 );
				V6_ASSERT( n % 2 == 0 );
				const core::u32 vertexCount = n / 2;
				V6_ASSERT( vertexCount == 3 || vertexCount == 4 );
			
				{
					V6_ASSERT( triangleID < triangleCount );			
					ObjTriangle_s* triangle = &scene->triangles[triangleID];
					const core::u32 indexRanks[] = { 0, 1, NA,  2, 3, NA, 4, 5, NA };
					Triangle_InitVertices( &scene->triangles[triangleID], ids, indexRanks );
					++triangleID;
					++mesh->triangleCount;
				}
						
				if ( vertexCount == 4 )
				{
					V6_ASSERT( triangleID < triangleCount );			
					ObjTriangle_s* triangle = &scene->triangles[triangleID];
					const core::u32 indexRanks[] = { 0, 1, NA,  4, 5, NA, 6, 7, NA };
					Triangle_InitVertices( &scene->triangles[triangleID], ids, indexRanks );
					++triangleID;
					++mesh->triangleCount;
				}
			}
			else if ( separatorCount == 6 || separatorCount == 8 )
			{
				const core::u32 n = sscanf_s( f, "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", ids+0, ids+1, ids+2, ids+3, ids+4, ids+5, ids+6, ids+7, ids+8, ids+9, ids+10, ids+11 );
				if ( n == 1 )
				{
					const core::u32 n = sscanf_s( f, "%d//%d %d//%d %d//%d %d//%d", ids+0, ids+1, ids+2, ids+3, ids+4, ids+5, ids+6, ids+7 );
					V6_ASSERT( n % 2 == 0 );
					const core::u32 vertexCount = n / 2;
					V6_ASSERT( vertexCount == 3 || vertexCount == 4 );
			
					{
						V6_ASSERT( triangleID < triangleCount );			
						ObjTriangle_s* triangle = &scene->triangles[triangleID];
						const core::u32 indexRanks[] = { 0, NA, 1, 2, NA, 3, 4, NA, 5 };
						Triangle_InitVertices( &scene->triangles[triangleID], ids, indexRanks );
						++triangleID;
						++mesh->triangleCount;
					}
						
					if ( vertexCount == 4 )
					{
						V6_ASSERT( triangleID < triangleCount );			
						ObjTriangle_s* triangle = &scene->triangles[triangleID];
						const core::u32 indexRanks[] = { 0, NA, 1, 4, NA, 5, 6, NA, 7 };
						Triangle_InitVertices( &scene->triangles[triangleID], ids, indexRanks );
						++triangleID;
						++mesh->triangleCount;
					}
				}
				else
				{
					V6_ASSERT( n % 3 == 0 );
					const core::u32 vertexCount = n / 3;
					V6_ASSERT( vertexCount == 3 || vertexCount == 4 );
			
					{
						V6_ASSERT( triangleID < triangleCount );			
						ObjTriangle_s* triangle = &scene->triangles[triangleID];
						const core::u32 indexRanks[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
						Triangle_InitVertices( &scene->triangles[triangleID], ids, indexRanks );
						++triangleID;
						++mesh->triangleCount;
					}
						
					if ( vertexCount == 4 )
					{
						V6_ASSERT( triangleID < triangleCount );			
						ObjTriangle_s* triangle = &scene->triangles[triangleID];
						const core::u32 indexRanks[] = { 0, 1, 2, 6, 7, 8, 9, 10, 11 };
						Triangle_InitVertices( &scene->triangles[triangleID], ids, indexRanks );
						++triangleID;
						++mesh->triangleCount;
					}
				}				
			}
			else
			{
				V6_ASSERT_ALWAYS( "Unsupported number of separators\n" );
			}

			if ( triangleID - lastDisplayTriangleID >= 100000 )
			{
				printf( "\r" );
				V6_MSG( "%d/%d loaded triangles", triangleID, triangleCount );
				lastDisplayTriangleID = triangleID;
			}
		}
		else if ( _strnicmp( token, "mtllib", 6 ) == 0 )
		{
		}
		else
		{
			V6_WARNING( "Unknown token %s\n", token );
		}
	}
	
	fclose( fileOBJ );

	printf( "\r" );
	V6_MSG( "%d/%d loaded triangles\n", triangleID, triangleCount );

	V6_ASSERT( positionID == positionCount );
	V6_ASSERT( normalID == normalCount );
	V6_ASSERT( uvID == uvCount );
	V6_ASSERT( meshID == meshCount );
	V6_ASSERT( triangleID == triangleCount );

	return true;
}

END_V6_VIEWER_NAMESPACE
