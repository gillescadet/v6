/*V6*/

#pragma once

#ifndef __V6_VIEWER_TGA_READER_H__
#define __V6_VIEWER_TGA_READER_H__

BEGIN_V6_CORE_NAMESPACE

struct Color_s;
struct Image_s;

class IAllocator;

END_V6_CORE_NAMESPACE

BEGIN_V6_VIEWER_NAMESPACE

bool Tga_ReadFromFile( core::Image_s* image, const char* filenameTGA, core::IAllocator* allocator );

END_V6_VIEWER_NAMESPACE

#endif // __V6_VIEWER_TGA_READER_H__