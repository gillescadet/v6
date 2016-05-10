/*V6*/

#ifdef __V6_WINDOWS_BEGIN_H__
	#undef __V6_WINDOWS_BEGIN_H__
#else
	#error windows_begin.h should be called first
#endif

#if V6_UE4_PLUGIN == 1
#undef INT
#undef UINT
#undef DWORD
#undef FLOAT
#endif // #if V6_UE4_PLUGIN == 1

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#pragma warning( pop )
