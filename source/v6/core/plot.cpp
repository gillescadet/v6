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

void Plot_AddLine( Plot_s* plot, const Vec3* p0, const Vec3* p1 )
{
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p0->x, p0->y, p0->z );
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p1->x, p1->y, p1->z );
	fprintf_s( plot->fileOBJ, "f %d %d\n", plot->vertexCount+1, plot->vertexCount+2 );

	plot->vertexCount += 2;
}

void Plot_AddPoint( Plot_s* plot, const Vec3* p)
{
	fprintf_s( plot->fileOBJ, "v %g %g %g\n", p->x, p->y, p->z );
	fprintf_s( plot->fileOBJ, "f %d\n", plot->vertexCount+1 );

	++plot->vertexCount;
}

END_V6_NAMESPACE
