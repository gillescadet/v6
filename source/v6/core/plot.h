/*V6*/

#pragma once

#ifndef __V6_CORE_PLOT_H__
#define __V6_CORE_PLOT_H__

#include <v6/core/color.h>
#include <v6/core/stream.h>

BEGIN_V6_NAMESPACE

struct Vec3;

struct Plot_s
{
	FILE* fileOBJ;
	FILE* fileMTL;
	u32 objectCount;
	u32 vertexCount;
};

void Plot_Create( Plot_s* plot, const char* filename );
void Plot_Release( Plot_s* plot );

void Plot_NewObject( Plot_s* plot, Color_s color );
void Plot_AddLine( Plot_s* plot, const Vec3* p0, const Vec3* p1);
void Plot_AddPoint( Plot_s* plot, const Vec3* p );

END_V6_NAMESPACE

#endif // __V6_CORE_PLOT_H__
