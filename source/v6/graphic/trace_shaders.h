/*V6*/

#ifndef __V6_HLSL_TRACE_SHADERS_H__
#define __V6_HLSL_TRACE_SHADERS_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#include <v6/graphic/block_cull_optim_mip_cs_bytecode.h>
#include <v6/graphic/block_cull_optim_onion_cs_bytecode.h>
#include <v6/graphic/block_cull_stats_mip_cs_bytecode.h>
#include <v6/graphic/block_cull_stats_onion_cs_bytecode.h>

static const u8* g_main_block_cull_cs[4] = 
{
	g_main_block_cull_optim_mip_cs,
	g_main_block_cull_optim_onion_cs,
	g_main_block_cull_stats_mip_cs,
	g_main_block_cull_stats_onion_cs,
};

static const u32 g_sizeof_block_cull_cs[4] = 
{
	sizeof( g_main_block_cull_optim_mip_cs ),
	sizeof( g_main_block_cull_optim_onion_cs ),
	sizeof( g_main_block_cull_stats_mip_cs ),
	sizeof( g_main_block_cull_stats_onion_cs ),
};

#include <v6/graphic/block_cull_post_cs_bytecode.h>

#include <v6/graphic/block_project_optim_mip_cs_bytecode.h>
#include <v6/graphic/block_project_optim_onion_cs_bytecode.h>
#include <v6/graphic/block_project_stats_mip_cs_bytecode.h>
#include <v6/graphic/block_project_stats_onion_cs_bytecode.h>

static const u8* g_main_block_project_cs[4] = 
{
	g_main_block_project_optim_mip_cs,
	g_main_block_project_optim_onion_cs,
	g_main_block_project_stats_mip_cs,
	g_main_block_project_stats_onion_cs,
};

static const u32 g_sizeof_block_project_cs[4] = 
{
	sizeof( g_main_block_project_optim_mip_cs ),
	sizeof( g_main_block_project_optim_onion_cs ),
	sizeof( g_main_block_project_stats_mip_cs ),
	sizeof( g_main_block_project_stats_onion_cs ),
};

#include <v6/graphic/block_trace_debug_mip_cs_bytecode.h>
#include <v6/graphic/block_trace_debug_onion_cs_bytecode.h>
#include <v6/graphic/block_trace_optim_mip_cs_bytecode.h>
#include <v6/graphic/block_trace_optim_onion_cs_bytecode.h>

static const u8* g_main_block_trace_cs[4] = 
{
	g_main_block_trace_optim_mip_cs,
	g_main_block_trace_optim_onion_cs,
	g_main_block_trace_debug_mip_cs,
	g_main_block_trace_debug_onion_cs,
};

static const u32 g_sizeof_block_trace_cs[4] = 
{
	sizeof( g_main_block_trace_optim_mip_cs ),
	sizeof( g_main_block_trace_optim_onion_cs ),
	sizeof( g_main_block_trace_debug_mip_cs ),
	sizeof( g_main_block_trace_debug_onion_cs ),
};

#include <v6/graphic/pixel_tsaa_cs_bytecode.h>
#include <v6/graphic/pixel_sharpen_cs_bytecode.h>

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_TRACE_SHADERS_H__
