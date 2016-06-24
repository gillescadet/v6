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

BEGIN_V6_NAMESPACE

struct GPUFontResources_s
{
	GPUConstantBuffer_s		cb;
	GPUBuffer_s				characters;
	GPUTexture2D_s			fontBitmap;
	GPUShader_s				shader;
	ID3D11Buffer*			indices;
	ID3D11SamplerState*		trilinearSamplerState;
};

extern ID3D11Device*		g_device;
extern ID3D11DeviceContext*	g_deviceContext;

static GPUFontResources_s	s_gpuFontResources;
static bool					s_gpuFontResourcesCreated = false;
static const u32			s_gpuCharacterMaxCount = 20000;
static hlsl::Character		s_gpuCharacters[s_gpuCharacterMaxCount];
static const u32			s_fontTextBufferMaxSize = s_gpuCharacterMaxCount + (s_gpuCharacterMaxCount / 8) * sizeof( FontText_s );
static u8					s_fontTextBuffer[s_fontTextBufferMaxSize];

void FontContext_Create( FontContext_s* fontContext )
{
	memset( fontContext, 0, sizeof( *fontContext ) );
	
	V6_ASSERT( s_gpuFontResourcesCreated == false );
	GPUFontResources_s* res = &s_gpuFontResources;

	fontContext->textBuffer = s_fontTextBuffer;
	fontContext->textBufferSize = 0;
	fontContext->firstText = nullptr;
	fontContext->characters = s_gpuCharacters;
	fontContext->characterCount = 0;

	GPUConstantBuffer_Create( &res->cb, sizeof( v6::hlsl::CBFont ), "font" );
	GPUBuffer_CreateStructured( &res->characters, sizeof( hlsl::Character ), s_gpuCharacterMaxCount, GPUBUFFER_CREATION_FLAG_DYNAMIC, "fontCharacters" );

	GPUTexture2D_CreateCompressed( &res->fontBitmap, g_fontBitmapSize.x, g_fontBitmapSize.y, (void*)(g_fontBitmap + 128), true, "fontColors" );

	GPUShader_CreateFromSource( &res->shader, hlsl::g_main_font_vs, sizeof( hlsl::g_main_font_vs ), hlsl::g_main_font_ps, sizeof( hlsl::g_main_font_ps ), 0 );
	
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

	fontContext->res = res;
	s_gpuFontResourcesCreated = true;
}

void FontContext_AddText( FontContext_s* fontContext, u32 x, u32 y, Color_s color, const char* str )
{
	if ( !str || !*str || color.a == 0 )
		return;

	const u32 strSize = (u32)strlen( str ) + 1;
	const u32 fontTextSize = sizeof( FontText_s ) + strSize;

	if ( fontContext->textBufferSize + fontTextSize > s_fontTextBufferMaxSize || fontContext->characterCount + strSize > s_gpuCharacterMaxCount )
		return;

	FontTextEx_s* fontText = (FontTextEx_s*)(fontContext->textBuffer + fontContext->textBufferSize);
	fontText->x = (u16)x;
	fontText->y = (u16)y;
	fontText->color = color;
	fontText->next = fontContext->firstText;
	memcpy( fontText->str, str, strSize );

	fontContext->firstText = fontText;

	fontContext->textBufferSize += fontTextSize;
	fontContext->characterCount += strSize;
}

void FontContext_Draw( FontContext_s* fontContext, GPURenderTargetSet_s* renderTargetSet )
{
	hlsl::Character* gpuCharacters = (hlsl::Character*)fontContext->characters;
	u32 gpuCharacterCount = 0;

	{
		hlsl::Character* gpuCharacter = gpuCharacters;
		for ( FontTextEx_s* fontText = (FontTextEx_s*)fontContext->firstText; fontText; fontText = (FontTextEx_s*)fontText->next )
		{
			u32 posX = fontText->x;
			const u32 posY = fontText->y;
			const u32 rgba = (fontText->color.r << 24) | (fontText->color.g << 16) | (fontText->color.b << 8) | (fontText->color.a << 0);
			for ( char* c = fontText->str; *c; ++c )
			{
				const FontCharacter_s* fontCharacter = &g_fontCharacters[*c];
				if ( fontCharacter->xadvance == 0 )
					continue;

				gpuCharacter->posx16_posy16 = ((posX + fontCharacter->xoffset) << 16) | (posY + fontCharacter->yoffset);
				gpuCharacter->x8_y8_w8_h8 = (fontCharacter->x << 24) | (fontCharacter->y << 16) | (fontCharacter->width << 8) | (fontCharacter->height << 0);
				gpuCharacter->rgba = rgba;
				++gpuCharacter;

				posX += fontCharacter->xadvance;
			}
		}

		gpuCharacterCount = (u32)(gpuCharacter - gpuCharacters);
		V6_ASSERT( gpuCharacterCount <= s_gpuCharacterMaxCount );

		fontContext->textBufferSize = 0;
		fontContext->firstText = nullptr;
		fontContext->characterCount = 0;
	}

	if ( gpuCharacterCount == 0 )
		return;

	GPURenderTargetSetBindingDesc_s renderTargetSetBindingDesc = {};
	renderTargetSetBindingDesc.noZ = true;
	renderTargetSetBindingDesc.blendMode = GPU_BLEND_MODE_ADDITIF;

	GPUEvent_Begin( "Font");

	GPUFontResources_s* fontRes = fontContext->res;

	GPURenderTargetSet_Bind( renderTargetSet, &renderTargetSetBindingDesc, 0 );

	// update

	{
		v6::hlsl::CBFont* cbFont = (v6::hlsl::CBFont*)GPUConstantBuffer_MapWrite( &fontRes->cb );
		cbFont->c_fontInvFrameSize = 1.0f / Vec2_Make( (float)renderTargetSet->width, (float)renderTargetSet->height );
		cbFont->c_fontInvBitmapSize = 1.0f / Vec2_Make( (float)g_fontBitmapSize.x, (float)g_fontBitmapSize.y );
		GPUConstantBuffer_UnmapWrite( &fontRes->cb );
	}

	GPUBuffer_Update( &fontRes->characters, 0, gpuCharacters, gpuCharacterCount );
	
	// set

	g_deviceContext->VSSetConstantBuffers( v6::hlsl::CBFontSlot, 1, &fontRes->cb.buf );

	g_deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &fontRes->trilinearSamplerState );

	g_deviceContext->VSSetShaderResources( HLSL_FONT_CHARACTER_SLOT, 1, &fontRes->characters.srv );
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

	GPURenderTargetSet_Unbind( renderTargetSet );

	GPUEvent_End();
}

u32 FontContext_GetLineHeight( const FontContext_s* fontContext )
{
	return g_fontLineHeight;
}

void FontContext_Release( FontContext_s* fontContext )
{
	V6_ASSERT( s_gpuFontResourcesCreated == true );
	GPUFontResources_s* res = &s_gpuFontResources;

	GPUBuffer_Release( &res->characters );
	GPUTexture2D_Release( &res->fontBitmap );
	GPUShader_Release( &res->shader );
	V6_RELEASE_D3D11( res->indices );
	V6_RELEASE_D3D11( res->trilinearSamplerState );

	s_gpuFontResourcesCreated = false;
}

END_V6_NAMESPACE
