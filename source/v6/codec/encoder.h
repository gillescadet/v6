/*V6*/

#pragma once

#ifndef __V6_CORE_ENCODER_H__
#define __V6_CORE_ENCODER_H__

#define ENCODER_STRICT_CELL		0

BEGIN_V6_NAMESPACE

class IAllocator;
class IStack;
struct Process_s;

void	VideoStream_CancelEncodingInSeparateProcess( Process_s* process );
bool	VideoStream_Encode( const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, u32 compressionQuality, bool extend, IAllocator* heap );
bool	VideoStream_EncodeInSeparateProcess( const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, u32 compressionQuality, bool extend );
void	VideoStream_DeleteRawFrameFiles( const char* templateRawFilename, u32 frameOffset, u32 frameCount );
bool	VideoStream_StartEncodingInSeparateProcess( Process_s* process, const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, u32 compressionQuality, bool extend );
bool	VideoStream_SetKeyValue( const char* streamFilename, const char* newKey, const u8* newValue, u32 newValueSize, IStack* stack );
bool	VideoStream_WaitEncodingInSeparateProcess( Process_s* process );

END_V6_NAMESPACE

#endif // __V6_CORE_ENCODER_H__
