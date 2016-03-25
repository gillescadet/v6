/*V6*/

#pragma once

#ifndef __V6_CORE_ENCODER_H__
#define __V6_CORE_ENCODER_H__

BEGIN_V6_CORE_NAMESPACE

class IAllocator;

bool Encoder_EncodeFrames( const char* templateFilename, u32 fileCount, const char* streamFilename, IAllocator* heap );

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_ENCODER_H__
