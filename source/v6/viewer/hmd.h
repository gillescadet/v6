/*V6*/

#pragma once

#ifndef __V6_VIEWER_HMD_H__
#define __V6_VIEWER_HMD_H__

BEGIN_V6_CORE_NAMESPACE

class IAllocator;

END_V6_CORE_NAMESPACE

BEGIN_V6_VIEWER_NAMESPACE

enum
{
	HMD_TRACKING_STATE_OFF,
	HMD_TRACKING_STATE_ON			= 1 << 0,
	HMD_TRACKING_STATE_ORIENTATION	= 1 << 1,
	HMD_TRACKING_STATE_POS			= 1 << 2,
};

bool Hmd_Init();
void Hmd_Shutdown();
core::u32 Hmd_Track( core::Mat4x4* view );

END_V6_VIEWER_NAMESPACE

#endif // __V6_VIEWER_HMD_H__