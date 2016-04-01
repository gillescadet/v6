/*V6*/

#pragma once

#ifndef __V6_CORE_ENCODER_H__
#define __V6_CORE_ENCODER_H__

BEGIN_V6_CORE_NAMESPACE

class IAllocator;

bool Sequence_Encode( const char* templateFilename, u32 fileCount, const char* sequenceFilename, IAllocator* heap );

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_ENCODER_H__
