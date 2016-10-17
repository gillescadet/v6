/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_FONT_H__
#define __V6_GRAPHIC_FONT_H__

#include <v6/core/color.h>
#include <v6/core/vec3.h>
#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

struct GPUFontResources_s;

struct FontText_s
{
	u16			x;
	u16			y;
	Color_s		color;
	FontText_s*	next;
};

struct FontTextEx_s : FontText_s
{
	char		str[256];
};

static_assert( offsetof( FontText_s, next ) + sizeof( FontText_s* ) == offsetof( FontTextEx_s, str ), "Need to pad FontText_s" );

struct FontContext_s
{
	GPUFontResources_s*		res;
	u8*						textBuffer;
	u32						textBufferSize;
	FontText_s*				firstText;
	void*					characters;
	u32						characterCount;
};

void	FontSystem_Create();
void	FontSystem_Release();

void	FontContext_Create( FontContext_s* fontContext );
void	FontContext_AddLine( FontContext_s* fontContext, u32 x, u32 y, Color_s color, const char* str );
void	FontContext_AddLineWithSize( FontContext_s* fontContext, u32 x, u32 y, Color_s color, const char* str, u32 strSize );
u32		FontContext_AddText( FontContext_s* fontContext, u32 x, u32 y, u32 lineHeight, Color_s color, const char* text );
void	FontContext_Draw( FontContext_s* fontContext, GPURenderTargetSet_s* renderTargetSet, bool leftEye, bool rightEye );
u32		FontContext_GetLineHeight( const FontContext_s* fontContext );
void	FontContext_Release( FontContext_s* fontContext );

void	Font_Draw();

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_FONT_H__
