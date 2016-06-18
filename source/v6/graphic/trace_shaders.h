/*V6*/

#ifndef __V6_HLSL_TRACE_SHADERS_H__
#define __V6_HLSL_TRACE_SHADERS_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#include <v6/graphic/block_cull_x4_cs_bytecode.h>
#include <v6/graphic/block_cull_x8_cs_bytecode.h>
#include <v6/graphic/block_cull_x16_cs_bytecode.h>
#include <v6/graphic/block_cull_x32_cs_bytecode.h>
#include <v6/graphic/block_cull_x64_cs_bytecode.h>

#include <v6/graphic/block_cull_stats_x4_cs_bytecode.h>
#include <v6/graphic/block_cull_stats_x8_cs_bytecode.h>
#include <v6/graphic/block_cull_stats_x16_cs_bytecode.h>
#include <v6/graphic/block_cull_stats_x32_cs_bytecode.h>
#include <v6/graphic/block_cull_stats_x64_cs_bytecode.h>

static const u8* g_main_block_cull_cs[2][HLSL_BUCKET_COUNT] = 
{
	{ 
		g_main_block_cull_x4_cs,
		g_main_block_cull_x8_cs,
		g_main_block_cull_x16_cs,
		g_main_block_cull_x32_cs,
		g_main_block_cull_x64_cs,
	},
	{ 
		g_main_block_cull_stats_x4_cs,
		g_main_block_cull_stats_x8_cs,
		g_main_block_cull_stats_x16_cs,
		g_main_block_cull_stats_x32_cs,
		g_main_block_cull_stats_x64_cs,
	},
};

static const u32 g_sizeof_block_cull_cs[2][HLSL_BUCKET_COUNT] = 
{
	{ 
		sizeof( g_main_block_cull_x4_cs ),
		sizeof( g_main_block_cull_x8_cs ),
		sizeof( g_main_block_cull_x16_cs ),
		sizeof( g_main_block_cull_x32_cs ),
		sizeof( g_main_block_cull_x64_cs ),
	},
	{ 
		sizeof( g_main_block_cull_stats_x4_cs ),
		sizeof( g_main_block_cull_stats_x8_cs ),
		sizeof( g_main_block_cull_stats_x16_cs ),
		sizeof( g_main_block_cull_stats_x32_cs ),
		sizeof( g_main_block_cull_stats_x64_cs ),
	},
};

#include <v6/graphic/block_trace_debug_x4_cs_bytecode.h>
#include <v6/graphic/block_trace_debug_x8_cs_bytecode.h>
#include <v6/graphic/block_trace_debug_x16_cs_bytecode.h>
#include <v6/graphic/block_trace_debug_x32_cs_bytecode.h>
#include <v6/graphic/block_trace_debug_x64_cs_bytecode.h>


#include <v6/graphic/block_trace_init_cs_bytecode.h>

#include <v6/graphic/block_trace_x4_cs_bytecode.h>
#include <v6/graphic/block_trace_x8_cs_bytecode.h>
#include <v6/graphic/block_trace_x32_cs_bytecode.h>
#include <v6/graphic/block_trace_x16_cs_bytecode.h>
#include <v6/graphic/block_trace_x64_cs_bytecode.h>

static const u8* g_main_block_trace_cs[2][HLSL_BUCKET_COUNT] = 
{
	{ 
		g_main_block_trace_x4_cs,
		g_main_block_trace_x8_cs,
		g_main_block_trace_x16_cs,
		g_main_block_trace_x32_cs,
		g_main_block_trace_x64_cs,
	},
	{ 
		g_main_block_trace_debug_x4_cs,
		g_main_block_trace_debug_x8_cs,
		g_main_block_trace_debug_x16_cs,
		g_main_block_trace_debug_x32_cs,
		g_main_block_trace_debug_x64_cs,
	},
};

static const u32 g_sizeof_block_trace_cs[2][HLSL_BUCKET_COUNT] = 
{
	{ 
		sizeof( g_main_block_trace_x4_cs ),
		sizeof( g_main_block_trace_x8_cs ),
		sizeof( g_main_block_trace_x16_cs ),
		sizeof( g_main_block_trace_x32_cs ),
		sizeof( g_main_block_trace_x64_cs ),
	},
	{ 
		sizeof( g_main_block_trace_debug_x4_cs ),
		sizeof( g_main_block_trace_debug_x8_cs ),
		sizeof( g_main_block_trace_debug_x16_cs ),
		sizeof( g_main_block_trace_debug_x32_cs ),
		sizeof( g_main_block_trace_debug_x64_cs ),
	},
};

#include <v6/graphic/pixel_blend_cs_bytecode.h>
#include <v6/graphic/pixel_blend_overdraw_cs_bytecode.h>

static const u8* g_main_pixel_blend_cs_options[2] = 
{
	g_main_pixel_blend_cs,
	g_main_pixel_blend_overdraw_cs,
};

static const u32 g_sizeof_pixel_blend_cs_options[2] = 
{
	sizeof( g_main_pixel_blend_cs ),
	sizeof( g_main_pixel_blend_overdraw_cs ),
};

#include <v6/graphic/pixel_filter_cs_bytecode.h>

static const u32 g_sizeof_pixel_filter_cs = { sizeof( g_main_pixel_filter_cs ) };

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_TRACE_SHADERS_H__
