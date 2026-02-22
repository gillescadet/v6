/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/core/memory.h>
#include <v6/graphic/font.h>
#include <v6/graphic/font_data.h>
#include <v6/graphic/font_shaders.h>
#include <v6/graphic/font_shared.h>
#include <v6/graphic/view.h>

BEGIN_V6_NAMESPACE

struct GPUFontResources_s
{
	GPUConstantBuffer_s		cb;
	GPUBuffer_s				characters;
	GPUTexture2D_s			fontBitmap;
	GPUShader_s				shader;
	GPUShader_s				shaderBackground;
	ID3D11Buffer*			indices;
	ID3D11SamplerState*		trilinearSamplerState;
};

extern ID3D11Device*		g_device;
extern ID3D11DeviceContext*	g_deviceContext;

static const GPUEventID_t	s_gpuEventFontContext = GPUEvent_Register( "FontContext", false );
static const GPUEventID_t	s_gpuEventFontObject = GPUEvent_Register( "FontObject", false );

static GPUFontResources_s	s_gpuFontResources;
static bool					s_gpuFontResourcesCreated = false;
static const u32			s_gpuCharacterMaxCount = 20000;
static hlsl::Character		s_gpuCharacters[s_gpuCharacterMaxCount];
static const u32			s_fontTextBufferMaxSize = s_gpuCharacterMaxCount + (s_gpuCharacterMaxCount / 8) * sizeof( FontText_s );
static u8					s_fontTextBuffer[s_fontTextBufferMaxSize];


static u32 EncodeTextToGPUCHaracters( hlsl::Character* gpuCharacters, u32 gpuCharacterMaxCount, Vec2u* bbMin, Vec2u* bbMax, u16 x, u16 y, Color_s color, const char* str )
{
	u32 gpuCharacterCount = 0;

	u32 posX = x;
	const u32 posY = y;
	const u32 rgba = (color.r << 24) | (color.g << 16) | (color.b << 8) | (color.a << 0);
	for ( const char* c = str; *c; ++c )
	{
		const FontCharacter_s* fontCharacter = &g_fontCharacters[*c];
		if ( fontCharacter->xadvance == 0 )
			continue;

		V6_ASSERT( gpuCharacterCount < gpuCharacterMaxCount );

		hlsl::Character* gpuCharacter = &gpuCharacters[gpuCharacterCount];
		gpuCharacter->posx16_posy16 = ((posX + fontCharacter->xoffset) << 16) | (posY + g_fontLineHeight - fontCharacter->yoffset);
		gpuCharacter->x8_y8_w8_h8 = (fontCharacter->x << 24) | (fontCharacter->y << 16) | (fontCharacter->width << 8) | (fontCharacter->height << 0);
		gpuCharacter->rgba = rgba;
		++gpuCharacterCount;

		if ( bbMin && bbMax )
		{
			const Vec2u pos = Vec2u_Make( posX, posY );
			*bbMin = Min( *bbMin, pos );
			*bbMax = Max( *bbMax, pos );
		}

		posX += fontCharacter->xadvance;
	}

	return gpuCharacterCount;
}

void FontSystem_Create()
{
	V6_ASSERT( s_gpuFontResourcesCreated == false );
	GPUFontResources_s* res = &s_gpuFontResources;

	GPUConstantBuffer_Create( &res->cb, sizeof( v6::hlsl::CBFont ), "font" );
	GPUBuffer_CreateStructured( &res->characters, sizeof( hlsl::Character ), s_gpuCharacterMaxCount, GPUBUFFER_CREATION_FLAG_MAP_DISCARD, "fontCharacters" );

	GPUTexture2D_CreateCompressed( &res->fontBitmap, g_fontBitmapSize.x, g_fontBitmapSize.y, (void*)g_fontBitmap, true, "fontColors" );

	GPUShader_CreateFromSource( &res->shader, hlsl::g_main_font_vs, sizeof( hlsl::g_main_font_vs ), hlsl::g_main_font_ps, sizeof( hlsl::g_main_font_ps ), 0 );
	GPUShader_CreateFromSource( &res->shaderBackground, hlsl::g_main_font_background_vs, sizeof( hlsl::g_main_font_background_vs ), hlsl::g_main_font_background_ps, sizeof( hlsl::g_main_font_background_ps ), 0 );
	
	{
		const u16 indices[] = { 0, 1, 2, 3 };

		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = sizeof( indices );
		bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = indices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufDesc, &data, &res->indices ) );
		GPUResource_LogMemory( "IndexBuffer", bufDesc.ByteWidth, "font" );
	}

	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = D3D11_REQ_MAXANISOTROPY;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0;
		samplerDesc.BorderColor[1] = 0;
		samplerDesc.BorderColor[2] = 0;
		samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		V6_ASSERT_D3D11( g_device->CreateSamplerState( &samplerDesc, &res->trilinearSamplerState ) );
	}

	s_gpuFontResourcesCreated = true;
}

void FontSystem_Release()
{
	V6_ASSERT( s_gpuFontResourcesCreated == true );
	GPUFontResources_s* res = &s_gpuFontResources;

	GPUBuffer_Release( &res->characters );
	GPUTexture2D_Release( &res->fontBitmap );
	GPUShader_Release( &res->shader );
	GPUShader_Release( &res->shaderBackground );
	V6_RELEASE_D3D11( res->indices );
	V6_RELEASE_D3D11( res->trilinearSamplerState );

	s_gpuFontResourcesCreated = false;
}

void FontContext_Create( FontContext_s* fontContext )
{
	V6_ASSERT( s_gpuFontResourcesCreated == true );

	memset( fontContext, 0, sizeof( *fontContext ) );

	fontContext->textBuffer = s_fontTextBuffer;
	fontContext->textBufferSize = 0;
	fontContext->firstText = nullptr;
	fontContext->characters = s_gpuCharacters;
	fontContext->characterCount = 0;

	fontContext->res = &s_gpuFontResources;
}

void FontContext_AddLineWithSize( FontContext_s* fontContext, u32 x, u32 y, Color_s color, const char* str, u32 strSize, u32 anchors )
{
	V6_ASSERT( str && *str && color.a > 0 && strSize > 0 );

	const u32 characterCount = strSize + 1;
	const u32 fontTextSize = sizeof( FontText_s ) + characterCount;

	if ( fontContext->textBufferSize + fontTextSize > s_fontTextBufferMaxSize || fontContext->characterCount + characterCount > s_gpuCharacterMaxCount )
		return;

	if ( anchors & FONT_ANCHOR_HORIZONTAL_CENTER )
	{
		const u32 textHalfWidth = (g_fontCharacterWidth * strSize) / 2;
		x = Max( 0, (int)(x - textHalfWidth) );
	}

	FontTextEx_s* fontText = (FontTextEx_s*)(fontContext->textBuffer + fontContext->textBufferSize);
	fontText->x = (u16)x;
	fontText->y = (u16)y;
	fontText->color = color;
	fontText->next = fontContext->firstText;
	memcpy( fontText->str, str, strSize );
	fontText->str[strSize] = 0;

	fontContext->firstText = fontText;

	fontContext->textBufferSize += fontTextSize;
	fontContext->characterCount += characterCount;
}

void FontContext_AddLine( FontContext_s* fontContext, u32 x, u32 y, Color_s color, const char* str, u32 anchors )
{
	if ( !str || !*str || color.a == 0 )
		return;

	FontContext_AddLineWithSize( fontContext, x, y, color, str, (u32)strlen( str ), anchors );
}

u32 FontContext_AddText( FontContext_s* fontContext, u32 x, u32 y, u32 lineHeight, Color_s color, const char* text )
{
	if ( !text || !*text || color.a == 0 )
		return y;

	for (;;)
	{
		const char* lineBegin = text;
		while ( *text && *text != '\n' )
			++text;

		if ( text != lineBegin )
			FontContext_AddLineWithSize( fontContext, x, y, color, lineBegin, (u32)(text - lineBegin), FONT_ANCHOR_NONE );

		if ( *text == 0 )
			break;
		
		++text;
		y -= lineHeight;
	}

	return y;
}

void FontContext_Draw( FontContext_s* fontContext, GPURenderTargetSet_s* renderTargetSet, const View_s* leftView, const View_s* rightView, Color_s backgroundColor )
{
	hlsl::Character* gpuCharacters = (hlsl::Character*)fontContext->characters;
	u32 gpuCharacterCount = 0;

	Vec2u bbMin = Vec2u_Make( 0xFFFFFFFF, 0xFFFFFFFF );
	Vec2u bbMax = Vec2u_Make( 0, 0 );

	for ( FontTextEx_s* fontText = (FontTextEx_s*)fontContext->firstText; fontText; fontText = (FontTextEx_s*)fontText->next )
	{
		gpuCharacterCount += EncodeTextToGPUCHaracters( gpuCharacters + gpuCharacterCount, s_gpuCharacterMaxCount - gpuCharacterCount, &bbMin, &bbMax, fontText->x, fontText->y, fontText->color, fontText->str );
		V6_ASSERT( gpuCharacterCount <= s_gpuCharacterMaxCount );
	}

	fontContext->textBufferSize = 0;
	fontContext->firstText = nullptr;
	fontContext->characterCount = 0;

	if ( gpuCharacterCount == 0 )
		return;

	GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
	renderTargetSetBindingDesc.noZ = true;
	renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_ADDITIF;

	GPUEvent_Begin( s_gpuEventFontContext );

	GPUFontResources_s* fontRes = fontContext->res;

	// update

	GPUBuffer_Update( &fontRes->characters, 0, gpuCharacters, gpuCharacterCount );

	Vec4 backGroundQuad;
	backGroundQuad.x = (float)(bbMin.x - g_fontCharacterWidth);
	backGroundQuad.y = (float)(bbMin.y - 0.5f * g_fontLineHeight);
	backGroundQuad.z = (float)(bbMax.x - bbMin.x) + 3.0f * g_fontCharacterWidth;
	backGroundQuad.w = (float)(bbMax.y - bbMin.y) + 2.0f * g_fontLineHeight;

	for ( u32 eye = 0; eye < 2; ++eye )
	{
		const View_s* view = eye == 0 ? leftView : rightView;

		if ( !view )
			continue;

		const float inv255 = 1.0f / 255.0f;

		{
			Mat4x4 objectToProjMatrix;
			Mat4x4_Mul( &objectToProjMatrix, view->projMatrix, view->viewMatrix );

			v6::hlsl::CBFont* cbFont = (v6::hlsl::CBFont*)GPUConstantBuffer_MapWrite( &fontRes->cb );
			cbFont->c_fontMatRow0 = objectToProjMatrix.m_row0;
			cbFont->c_fontMatRow1 = objectToProjMatrix.m_row1;
			cbFont->c_fontMatRow2 = objectToProjMatrix.m_row2;
			cbFont->c_fontMatRow3 = objectToProjMatrix.m_row3;
			cbFont->c_fontBackgroundQuad = backGroundQuad;
			cbFont->c_fontBackgroundColor = Vec4_Make( backgroundColor.r * inv255, backgroundColor.g * inv255, backgroundColor.b * inv255, backgroundColor.a * inv255 );
			cbFont->c_fontInvBitmapSize = 1.0f / Vec2_Make( (float)g_fontBitmapSize.x, (float)g_fontBitmapSize.y );
			GPUConstantBuffer_UnmapWrite( &fontRes->cb );
		}

		GPURenderTargetSet_Bind( renderTargetSet, &renderTargetSetBindingDesc, eye );

		// set

		g_deviceContext->VSSetConstantBuffers( v6::hlsl::CBFontSlot, 1, &fontRes->cb.buf );

		g_deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &fontRes->trilinearSamplerState );

		g_deviceContext->VSSetShaderResources( HLSL_FONT_CHARACTER_SLOT, 1, &fontRes->characters.srv );
		g_deviceContext->PSSetShaderResources( HLSL_FONT_TEXTURE_SLOT, 1, &fontRes->fontBitmap.srv );

		// draw

		g_deviceContext->IASetInputLayout( fontRes->shader.m_inputLayout );

		u32 stride = 0;
		u32 offset = 0;
		g_deviceContext->IASetVertexBuffers( 0, 0, nullptr, &stride, &offset );
		g_deviceContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
		g_deviceContext->IASetIndexBuffer( fontRes->indices, DXGI_FORMAT_R16_UINT, 0 );

		if ( backgroundColor.a > 0 )
		{
			g_deviceContext->VSSetShader( fontRes->shaderBackground.m_vertexShader, nullptr, 0 );
			g_deviceContext->PSSetShader( fontRes->shaderBackground.m_pixelShader, nullptr, 0 );
			
			g_deviceContext->DrawIndexed( 4, 0, 0 );
		}

		g_deviceContext->VSSetShader( fontRes->shader.m_vertexShader, nullptr, 0 );
		g_deviceContext->PSSetShader( fontRes->shader.m_pixelShader, nullptr, 0 );

		g_deviceContext->DrawIndexedInstanced( 4, gpuCharacterCount, 0, 0, 0 );

		// unset
	
		static const void* nulls[8] = {};
		g_deviceContext->VSSetShaderResources( HLSL_FONT_CHARACTER_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
		g_deviceContext->PSSetShaderResources( HLSL_FONT_TEXTURE_SLOT, 1, (ID3D11ShaderResourceView**)nulls );

		GPURenderTargetSet_Unbind( renderTargetSet );
	}

	GPUEvent_End();
}

u32 FontContext_GetLineHeight( const FontContext_s* fontContext )
{
	return g_fontLineHeight;
}

u32 FontContext_GetLineWidth( const FontContext_s* fontContext, const char* str )
{
	return (u32)strlen( str ) * g_fontCharacterWidth;
}

void FontContext_Release( FontContext_s* fontContext )
{
	V6_ASSERT( s_gpuFontResourcesCreated == true );

	memset( fontContext, 0, sizeof( *fontContext ) );
}

void FontObject_Create( FontObject_s* fontObject, u32 characterMaxCount )
{
	V6_ASSERT( characterMaxCount < FontObject_s::CHARACTER_MAX_COUNT );
	memset( fontObject, 0, sizeof( *fontObject ) );
	GPUBuffer_CreateStructured( &fontObject->gpuCharacters, sizeof( hlsl::Character ), characterMaxCount, GPUBUFFER_CREATION_FLAG_MAP_DISCARD, "fontObjectCharacters" );
}

void FontObject_Release( FontObject_s* fontObject)
{
	GPUBuffer_Release( &fontObject->gpuCharacters );
	memset( fontObject, 0, sizeof( *fontObject ) );
}

u32 FontObject_GetTextHeight( FontObject_s* fontObject, const char* str )
{
	return g_fontLineHeight;
}

u32 FontObject_GetTextWidth( FontObject_s* fontObject, const char* str )
{
	return (u32)strlen( str ) * g_fontCharacterWidth;
}

void FontObject_Draw( FontObject_s* fontObject, const Mat4x4* viewProj, u16 x, u16 y, Color_s color, const char* str )
{
	hlsl::Character gpuCharacters[FontObject_s::CHARACTER_MAX_COUNT];
	
	const u32 gpuCharacterCount = EncodeTextToGPUCHaracters( gpuCharacters, FontObject_s::CHARACTER_MAX_COUNT, nullptr, nullptr, x, y, color, str );

	if ( gpuCharacterCount == 0 )
		return;

	GPUEvent_Begin( s_gpuEventFontObject );

	GPUFontResources_s* fontRes = &s_gpuFontResources;

	// update

	{
		v6::hlsl::CBFont* cbFont = (v6::hlsl::CBFont*)GPUConstantBuffer_MapWrite( &fontRes->cb );
		cbFont->c_fontMatRow0 = viewProj->m_row0;
		cbFont->c_fontMatRow1 = viewProj->m_row1;
		cbFont->c_fontMatRow2 = viewProj->m_row2;
		cbFont->c_fontMatRow3 = viewProj->m_row3;
		cbFont->c_fontInvBitmapSize = 1.0f / Vec2_Make( (float)g_fontBitmapSize.x, (float)g_fontBitmapSize.y );
		GPUConstantBuffer_UnmapWrite( &fontRes->cb );
	}

	GPUBuffer_Update( &fontObject->gpuCharacters, 0, gpuCharacters, gpuCharacterCount );

	// set

	g_deviceContext->VSSetConstantBuffers( v6::hlsl::CBFontSlot, 1, &fontRes->cb.buf );

	g_deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &fontRes->trilinearSamplerState );

	g_deviceContext->VSSetShaderResources( HLSL_FONT_CHARACTER_SLOT, 1, &fontObject->gpuCharacters.srv );
	g_deviceContext->PSSetShaderResources( HLSL_FONT_TEXTURE_SLOT, 1, &fontRes->fontBitmap.srv );

	// draw
	
	g_deviceContext->IASetInputLayout( fontRes->shader.m_inputLayout );
	g_deviceContext->VSSetShader( fontRes->shader.m_vertexShader, nullptr, 0 );
	g_deviceContext->PSSetShader( fontRes->shader.m_pixelShader, nullptr, 0 );

	u32 stride = 0;
	u32 offset = 0;
	g_deviceContext->IASetVertexBuffers( 0, 0, nullptr, &stride, &offset );
	g_deviceContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP   );
	g_deviceContext->IASetIndexBuffer( fontRes->indices, DXGI_FORMAT_R16_UINT, 0 );

	g_deviceContext->DrawIndexedInstanced( 4, gpuCharacterCount, 0, 0, 0 );
	
	// unset

	static const void* nulls[8] = {};
	g_deviceContext->VSSetShaderResources( HLSL_FONT_CHARACTER_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->PSSetShaderResources( HLSL_FONT_TEXTURE_SLOT, 1, (ID3D11ShaderResourceView**)nulls );

	GPUEvent_End();
}

END_V6_NAMESPACE
