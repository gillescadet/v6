/*V6*/

#pragma once

#ifndef __V6_CORE_ENCODER_H__
#define __V6_CORE_ENCODER_H__

#define ENCODER_STRICT_CELL		0
#define ENCODER_STRICT_BUCKET	0

BEGIN_V6_NAMESPACE

class IAllocator;

bool VideoStream_Encode( const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, IAllocator* heap );

END_V6_NAMESPACE

#endif // __V6_CORE_ENCODER_H__
