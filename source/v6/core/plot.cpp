/*V6*/

#include <v6/core/common.h>

#include <v6/core/plot.h>
#include <v6/core/string.h>
#include <v6/core/vec3.h>

BEGIN_V6_NAMESPACE

void Plot_Create( Plot_s* plot, const char* filename )
{
	memset( plot, 0, sizeof( *plot ) );
	fopen_s( &plot->fileOBJ, String_Format( "%s.obj", filename ), "wt" );
	fopen_s( &plot->fileMTL, String_Format( "%s.mtl", filename ), "wt" );
	V6_ASSERT( plot->fileOBJ );
	V6_ASSERT( plot->fileMTL );
}

void Plot_Release( Plot_s* plot )
{
	fclose( plot->fileOBJ );
	fclose( plot->fileMTL );
}

void Plot_NewObject( Plot_s* plot, Color_s color )
{
	const char* materialName = String_Format( "mat%d", plot->objectCount );
	fprintf_s( plot->fileMTL, "newmtl %s\n", materialName );
	fprintf_s( plot->fileMTL, "kd %g %g %g\n", color.r / 255.0f, color.g / 255.0f, color.b / 255.0f );
	fprintf_s( plot->fileMTL, "d %g\n", color.a / 255.0f );

	fprintf_s( plot->fileOBJ, "usemtl %s\n", materialName );
	fprintf_s( plot->fileOBJ, "o obj%d\n", plot->objectCount );

	++plot->objectCount;
}

void Plot_AddBox( Plot_s* plot, const Vec3* p0, const Vec3* p1, bool wireframe )
{
	Vec3 vertices[8];
	for ( u32 vertexID = 0; vertexID < 8; ++vertexID )
	{
		vertices[vertexID].x = (vertexID & 1) ? p0->x : p1->x; 
		vertices[vertexID].y = (vertexID & 2) ? p0->y : p1->y;
		vertices[vertexID].z = (vertexID & 4) ? p0->z : p1->z;
	}

	if ( wireframe )
	{
		const u16 indices[24] = { 
			0, 1, 1, 3, 3, 2, 2, 0,
			4, 5, 5, 7, 7, 6, 6, 4,
			1, 5, 0, 4, 3, 7, 2, 6 };

		for ( u32 lineID = 0; lineID < 12; ++lineID )
			Plot_AddLine( plot, &vertices[indices[lineID*2+0]], &vertices[indices[lineID*2+1]] );
	}
	else
	{
		const u16 indices[36] = { 
			0, 2, 3,
			0, 3, 1,
			1, 3, 7, 
			1, 7, 5, 
			5, 7, 6,
			5, 6, 4,
			4, 6, 2,
			4, 2, 0, 
			2, 6, 7, 
			2, 7, 3,
			1, 5, 4,
			1, 4, 0 };

		for ( u32 triangleID = 0; triangleID < 12; ++triangleID )
			Plot_AddTriangle( plot, &vertices[indices[triangleID*3+0]], &vertices[indices[triangleID*3+1]], &vertices[indices[triangleID*3+2]] );
	}
}

void Plot_AddLine( Plot_s* plot, const Vec3* p0, const Vec3* p1 )
{
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p0->x, p0->y, p0->z );
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p1->x, p1->y, p1->z );
	fprintf_s( plot->fileOBJ, "f %d %d\n", plot->vertexCount+1, plot->vertexCount+2 );

	plot->vertexCount += 2;
}

void Plot_AddTriangle( Plot_s* plot, const Vec3* p0, const Vec3* p1, const Vec3* p2 )
{
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p0->x, p0->y, p0->z );
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p1->x, p1->y, p1->z );
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p2->x, p2->y, p2->z );
	fprintf_s( plot->fileOBJ, "f %d %d %d\n", plot->vertexCount+1, plot->vertexCount+2, plot->vertexCount+3 );

	plot->vertexCount += 3;
}

void Plot_AddPoint( Plot_s* plot, const Vec3* p)
{
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p->x, p->y, p->z );
	fprintf_s( plot->fileOBJ, "f %d\n", plot->vertexCount+1 );

	++plot->vertexCount;
}

END_V6_NAMESPACE
