// Copyright 2016 Video6DOF.  All rights reserved.

#pragma once

class FDynamicRHIWrap : public FDynamicRHI
{
public:

        virtual void Init(  ) final override
        {
                m_wrapped->Init(  );
        }

        virtual void PostInit(  ) final override
        {
                m_wrapped->PostInit(  );
        }

        virtual void Shutdown(  ) final override
        {
                m_wrapped->Shutdown(  );
        }

        virtual FSamplerStateRHIRef RHICreateSamplerState( const FSamplerStateInitializerRHI& Initializer ) final override
        {
                return m_wrapped->RHICreateSamplerState( Initializer );
        }

        virtual FRasterizerStateRHIRef RHICreateRasterizerState( const FRasterizerStateInitializerRHI& Initializer ) final override
        {
                return m_wrapped->RHICreateRasterizerState( Initializer );
        }

        virtual FDepthStencilStateRHIRef RHICreateDepthStencilState( const FDepthStencilStateInitializerRHI& Initializer ) final override
        {
                return m_wrapped->RHICreateDepthStencilState( Initializer );
        }

        virtual FBlendStateRHIRef RHICreateBlendState( const FBlendStateInitializerRHI& Initializer ) final override
        {
                return m_wrapped->RHICreateBlendState( Initializer );
        }

        virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration( const FVertexDeclarationElementList& Elements ) final override
        {
                return m_wrapped->RHICreateVertexDeclaration( Elements );
        }

        virtual FPixelShaderRHIRef RHICreatePixelShader( const TArray<uint8>& Code ) final override
        {
                return m_wrapped->RHICreatePixelShader( Code );
        }

        virtual FVertexShaderRHIRef RHICreateVertexShader( const TArray<uint8>& Code ) final override
        {
                return m_wrapped->RHICreateVertexShader( Code );
        }

        virtual FHullShaderRHIRef RHICreateHullShader( const TArray<uint8>& Code ) final override
        {
                return m_wrapped->RHICreateHullShader( Code );
        }

        virtual FDomainShaderRHIRef RHICreateDomainShader( const TArray<uint8>& Code ) final override
        {
                return m_wrapped->RHICreateDomainShader( Code );
        }

        virtual FGeometryShaderRHIRef RHICreateGeometryShader( const TArray<uint8>& Code ) final override
        {
                return m_wrapped->RHICreateGeometryShader( Code );
        }

        virtual FGeometryShaderRHIRef RHICreateGeometryShaderWithStreamOutput( const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream ) final override
        {
                return m_wrapped->RHICreateGeometryShaderWithStreamOutput( Code, ElementList, NumStrides, Strides, RasterizedStream );
        }

        virtual FComputeShaderRHIRef RHICreateComputeShader( const TArray<uint8>& Code ) final override
        {
                return m_wrapped->RHICreateComputeShader( Code );
        }

        virtual FComputeFenceRHIRef RHICreateComputeFence( const FName& Name ) final override
        {
                return m_wrapped->RHICreateComputeFence( Name );
        }

        virtual FBoundShaderStateRHIRef RHICreateBoundShaderState( FVertexDeclarationRHIParamRef VertexDeclaration, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader ) final override
        {
                return m_wrapped->RHICreateBoundShaderState( VertexDeclaration, VertexShader, HullShader, DomainShader, PixelShader, GeometryShader );
        }

        virtual FUniformBufferRHIRef RHICreateUniformBuffer( const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage ) final override
        {
                return m_wrapped->RHICreateUniformBuffer( Contents, Layout, Usage );
        }

        virtual FIndexBufferRHIRef RHICreateIndexBuffer( uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateIndexBuffer( Stride, Size, InUsage, CreateInfo );
        }

        virtual void* RHILockIndexBuffer( FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode ) final override
        {
                return m_wrapped->RHILockIndexBuffer( IndexBuffer, Offset, Size, LockMode );
        }

        virtual void RHIUnlockIndexBuffer( FIndexBufferRHIParamRef IndexBuffer ) final override
        {
                m_wrapped->RHIUnlockIndexBuffer( IndexBuffer );
        }

        virtual FVertexBufferRHIRef RHICreateVertexBuffer( uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateVertexBuffer( Size, InUsage, CreateInfo );
        }

        virtual void* RHILockVertexBuffer( FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode ) final override
        {
                return m_wrapped->RHILockVertexBuffer( VertexBuffer, Offset, SizeRHI, LockMode );
        }

        virtual void RHIUnlockVertexBuffer( FVertexBufferRHIParamRef VertexBuffer ) final override
        {
                m_wrapped->RHIUnlockVertexBuffer( VertexBuffer );
        }

        virtual void RHICopyVertexBuffer( FVertexBufferRHIParamRef SourceBuffer, FVertexBufferRHIParamRef DestBuffer ) final override
        {
                m_wrapped->RHICopyVertexBuffer( SourceBuffer, DestBuffer );
        }

        virtual FStructuredBufferRHIRef RHICreateStructuredBuffer( uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateStructuredBuffer( Stride, Size, InUsage, CreateInfo );
        }

        virtual void* RHILockStructuredBuffer( FStructuredBufferRHIParamRef StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode ) final override
        {
                return m_wrapped->RHILockStructuredBuffer( StructuredBuffer, Offset, SizeRHI, LockMode );
        }

        virtual void RHIUnlockStructuredBuffer( FStructuredBufferRHIParamRef StructuredBuffer ) final override
        {
                m_wrapped->RHIUnlockStructuredBuffer( StructuredBuffer );
        }

        virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView( FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer ) final override
        {
                return m_wrapped->RHICreateUnorderedAccessView( StructuredBuffer, bUseUAVCounter, bAppendBuffer );
        }

        virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView( FTextureRHIParamRef Texture, uint32 MipLevel ) final override
        {
                return m_wrapped->RHICreateUnorderedAccessView( Texture, MipLevel );
        }

        virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView( FVertexBufferRHIParamRef VertexBuffer, uint8 Format ) final override
        {
                return m_wrapped->RHICreateUnorderedAccessView( VertexBuffer, Format );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView( FStructuredBufferRHIParamRef StructuredBuffer ) final override
        {
                return m_wrapped->RHICreateShaderResourceView( StructuredBuffer );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView( FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format ) final override
        {
                return m_wrapped->RHICreateShaderResourceView( VertexBuffer, Stride, Format );
        }

        virtual uint64 RHICalcTexture2DPlatformSize( uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign ) final override
        {
                return m_wrapped->RHICalcTexture2DPlatformSize( SizeX, SizeY, Format, NumMips, NumSamples, Flags, OutAlign );
        }

        virtual uint64 RHICalcTexture3DPlatformSize( uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign ) final override
        {
                return m_wrapped->RHICalcTexture3DPlatformSize( SizeX, SizeY, SizeZ, Format, NumMips, Flags, OutAlign );
        }

        virtual uint64 RHICalcTextureCubePlatformSize( uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign ) final override
        {
                return m_wrapped->RHICalcTextureCubePlatformSize( Size, Format, NumMips, Flags, OutAlign );
        }

        virtual void RHIGetTextureMemoryStats( FTextureMemoryStats& OutStats ) final override
        {
                m_wrapped->RHIGetTextureMemoryStats( OutStats );
        }

        virtual bool RHIGetTextureMemoryVisualizeData( FColor* TextureData, int32 SizeX, int32 SizeY, int32 Pitch, int32 PixelSize ) final override
        {
                return m_wrapped->RHIGetTextureMemoryVisualizeData( TextureData, SizeX, SizeY, Pitch, PixelSize );
        }

        virtual FTextureReferenceRHIRef RHICreateTextureReference( FLastRenderTimeContainer* LastRenderTime ) final override
        {
                return m_wrapped->RHICreateTextureReference( LastRenderTime );
        }

        virtual FTexture2DRHIRef RHICreateTexture2D( uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTexture2D( SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo );
        }

        virtual FTexture2DRHIRef RHIAsyncCreateTexture2D( uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips ) final override
        {
                return m_wrapped->RHIAsyncCreateTexture2D( SizeX, SizeY, Format, NumMips, Flags, InitialMipData, NumInitialMips );
        }

        virtual void RHICopySharedMips( FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D ) final override
        {
                m_wrapped->RHICopySharedMips( DestTexture2D, SrcTexture2D );
        }

        virtual FTexture2DArrayRHIRef RHICreateTexture2DArray( uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTexture2DArray( SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo );
        }

        virtual FTexture3DRHIRef RHICreateTexture3D( uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTexture3D( SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo );
        }

        virtual void RHIGetResourceInfo( FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo ) final override
        {
                m_wrapped->RHIGetResourceInfo( Ref, OutInfo );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView( FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel ) final override
        {
                return m_wrapped->RHICreateShaderResourceView( Texture2DRHI, MipLevel );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView( FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format ) final override
        {
                return m_wrapped->RHICreateShaderResourceView( Texture2DRHI, MipLevel, NumMipLevels, Format );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView( FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel ) final override
        {
                return m_wrapped->RHICreateShaderResourceView( Texture3DRHI, MipLevel );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView( FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel ) final override
        {
                return m_wrapped->RHICreateShaderResourceView( Texture2DArrayRHI, MipLevel );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView( FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel ) final override
        {
                return m_wrapped->RHICreateShaderResourceView( TextureCubeRHI, MipLevel );
        }

        virtual void RHIGenerateMips( FTextureRHIParamRef Texture ) final override
        {
                m_wrapped->RHIGenerateMips( Texture );
        }

        virtual uint32 RHIComputeMemorySize( FTextureRHIParamRef TextureRHI ) final override
        {
                return m_wrapped->RHIComputeMemorySize( TextureRHI );
        }

        virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus ) final override
        {
                return m_wrapped->RHIAsyncReallocateTexture2D( Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus );
        }

        virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted ) final override
        {
                return m_wrapped->RHIFinalizeAsyncReallocateTexture2D( Texture2D, bBlockUntilCompleted );
        }

        virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted ) final override
        {
                return m_wrapped->RHICancelAsyncReallocateTexture2D( Texture2D, bBlockUntilCompleted );
        }

        virtual void* RHILockTexture2D( FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail ) final override
        {
                return m_wrapped->RHILockTexture2D( Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail );
        }

        virtual void RHIUnlockTexture2D( FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail ) final override
        {
                m_wrapped->RHIUnlockTexture2D( Texture, MipIndex, bLockWithinMiptail );
        }

        virtual void* RHILockTexture2DArray( FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail ) final override
        {
                return m_wrapped->RHILockTexture2DArray( Texture, TextureIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail );
        }

        virtual void RHIUnlockTexture2DArray( FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail ) final override
        {
                m_wrapped->RHIUnlockTexture2DArray( Texture, TextureIndex, MipIndex, bLockWithinMiptail );
        }

        virtual void RHIUpdateTexture2D( FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData ) final override
        {
                m_wrapped->RHIUpdateTexture2D( Texture, MipIndex, UpdateRegion, SourcePitch, SourceData );
        }

        virtual void RHIUpdateTexture3D( FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData ) final override
        {
                m_wrapped->RHIUpdateTexture3D( Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData );
        }

        virtual FTextureCubeRHIRef RHICreateTextureCube( uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTextureCube( Size, Format, NumMips, Flags, CreateInfo );
        }

        virtual FTextureCubeRHIRef RHICreateTextureCubeArray( uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTextureCubeArray( Size, ArraySize, Format, NumMips, Flags, CreateInfo );
        }

        virtual void* RHILockTextureCubeFace( FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail ) final override
        {
                return m_wrapped->RHILockTextureCubeFace( Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail );
        }

        virtual void RHIUnlockTextureCubeFace( FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail ) final override
        {
                m_wrapped->RHIUnlockTextureCubeFace( Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail );
        }

        virtual void RHIBindDebugLabelName( FTextureRHIParamRef Texture, const TCHAR* Name ) final override
        {
                m_wrapped->RHIBindDebugLabelName( Texture, Name );
        }

        virtual void RHIReadSurfaceData( FTextureRHIParamRef Texture, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags ) final override
        {
                m_wrapped->RHIReadSurfaceData( Texture, Rect, OutData, InFlags );
        }

        virtual void RHIMapStagingSurface( FTextureRHIParamRef Texture, void*& OutData, int32& OutWidth, int32& OutHeight ) final override
        {
                m_wrapped->RHIMapStagingSurface( Texture, OutData, OutWidth, OutHeight );
        }

        virtual void RHIUnmapStagingSurface( FTextureRHIParamRef Texture ) final override
        {
                m_wrapped->RHIUnmapStagingSurface( Texture );
        }

        virtual void RHIReadSurfaceFloatData( FTextureRHIParamRef Texture, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace, int32 ArrayIndex, int32 MipIndex ) final override
        {
                m_wrapped->RHIReadSurfaceFloatData( Texture, Rect, OutData, CubeFace, ArrayIndex, MipIndex );
        }

        virtual void RHIRead3DSurfaceFloatData( FTextureRHIParamRef Texture, FIntRect Rect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData ) final override
        {
                m_wrapped->RHIRead3DSurfaceFloatData( Texture, Rect, ZMinMax, OutData );
        }

        virtual FRenderQueryRHIRef RHICreateRenderQuery( ERenderQueryType QueryType ) final override
        {
                return m_wrapped->RHICreateRenderQuery( QueryType );
        }

        virtual bool RHIGetRenderQueryResult( FRenderQueryRHIParamRef RenderQuery, uint64& OutResult, bool bWait ) final override
        {
                return m_wrapped->RHIGetRenderQueryResult( RenderQuery, OutResult, bWait );
        }

        virtual FTexture2DRHIRef RHIGetViewportBackBuffer( FViewportRHIParamRef Viewport ) final override
        {
                return m_wrapped->RHIGetViewportBackBuffer( Viewport );
        }

        virtual void RHIAdvanceFrameForGetViewportBackBuffer(  ) final override
        {
                m_wrapped->RHIAdvanceFrameForGetViewportBackBuffer(  );
        }

        virtual void RHIAcquireThreadOwnership(  ) final override
        {
                m_wrapped->RHIAcquireThreadOwnership(  );
        }

        virtual void RHIReleaseThreadOwnership(  ) final override
        {
                m_wrapped->RHIReleaseThreadOwnership(  );
        }

        virtual void RHIFlushResources(  ) final override
        {
                m_wrapped->RHIFlushResources(  );
        }

        virtual uint32 RHIGetGPUFrameCycles(  ) final override
        {
                return m_wrapped->RHIGetGPUFrameCycles(  );
        }

        virtual FViewportRHIRef RHICreateViewport( void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat ) final override
        {
                return m_wrapped->RHICreateViewport( WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat );
        }

        virtual void RHIResizeViewport( FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen ) final override
        {
                m_wrapped->RHIResizeViewport( Viewport, SizeX, SizeY, bIsFullscreen );
        }

        virtual void RHITick( float DeltaTime ) final override
        {
                m_wrapped->RHITick( DeltaTime );
        }

        virtual void RHISetStreamOutTargets( uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets ) final override
        {
                m_wrapped->RHISetStreamOutTargets( NumTargets, VertexBuffers, Offsets );
        }

        virtual void RHIDiscardRenderTargets( bool Depth, bool Stencil, uint32 ColorBitMask ) final override
        {
                m_wrapped->RHIDiscardRenderTargets( Depth, Stencil, ColorBitMask );
        }

        virtual void RHIBlockUntilGPUIdle(  ) final override
        {
                m_wrapped->RHIBlockUntilGPUIdle(  );
        }

        virtual void RHISuspendRendering(  ) final override
        {
                m_wrapped->RHISuspendRendering(  );
        }

        virtual void RHIResumeRendering(  ) final override
        {
                m_wrapped->RHIResumeRendering(  );
        }

        virtual bool RHIIsRenderingSuspended(  ) final override
        {
                return m_wrapped->RHIIsRenderingSuspended(  );
        }

        virtual bool RHIGetAvailableResolutions( FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate ) final override
        {
                return m_wrapped->RHIGetAvailableResolutions( Resolutions, bIgnoreRefreshRate );
        }

        virtual void RHIGetSupportedResolution( uint32& Width, uint32& Height ) final override
        {
                m_wrapped->RHIGetSupportedResolution( Width, Height );
        }

        virtual void RHIVirtualTextureSetFirstMipInMemory( FTexture2DRHIParamRef Texture, uint32 FirstMip ) final override
        {
                m_wrapped->RHIVirtualTextureSetFirstMipInMemory( Texture, FirstMip );
        }

        virtual void RHIVirtualTextureSetFirstMipVisible( FTexture2DRHIParamRef Texture, uint32 FirstMip ) final override
        {
                m_wrapped->RHIVirtualTextureSetFirstMipVisible( Texture, FirstMip );
        }

        virtual void RHIExecuteCommandList( FRHICommandList* CmdList ) final override
        {
                m_wrapped->RHIExecuteCommandList( CmdList );
        }

        virtual void* RHIGetNativeDevice(  ) final override
        {
                return m_wrapped->RHIGetNativeDevice(  );
        }

        virtual IRHICommandContext* RHIGetDefaultContext(  ) final override
        {
                return m_commandContext;
        }

        virtual IRHIComputeContext* RHIGetDefaultAsyncComputeContext(  ) final override
        {
                return m_wrapped->RHIGetDefaultAsyncComputeContext(  );
        }

        virtual class IRHICommandContextContainer* RHIGetCommandContextContainer(  ) final override
        {
                return m_wrapped->RHIGetCommandContextContainer(  );
        }

        virtual FVertexBufferRHIRef CreateAndLockVertexBuffer_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer ) final override
        {
                return m_wrapped->CreateAndLockVertexBuffer_RenderThread( RHICmdList, Size, InUsage, CreateInfo, OutDataBuffer );
        }

        virtual FIndexBufferRHIRef CreateAndLockIndexBuffer_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer ) final override
        {
                return m_wrapped->CreateAndLockIndexBuffer_RenderThread( RHICmdList, Stride, Size, InUsage, CreateInfo, OutDataBuffer );
        }

        virtual FVertexBufferRHIRef CreateVertexBuffer_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->CreateVertexBuffer_RenderThread( RHICmdList, Size, InUsage, CreateInfo );
        }

        virtual FShaderResourceViewRHIRef CreateShaderResourceView_RenderThread( class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format ) final override
        {
                return m_wrapped->CreateShaderResourceView_RenderThread( RHICmdList, VertexBuffer, Stride, Format );
        }

        virtual void* LockVertexBuffer_RenderThread( class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode ) final override
        {
                return m_wrapped->LockVertexBuffer_RenderThread( RHICmdList, VertexBuffer, Offset, SizeRHI, LockMode );
        }

        virtual void UnlockVertexBuffer_RenderThread( class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer ) final override
        {
                m_wrapped->UnlockVertexBuffer_RenderThread( RHICmdList, VertexBuffer );
        }

        virtual FTexture2DRHIRef AsyncReallocateTexture2D_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus ) final override
        {
                return m_wrapped->AsyncReallocateTexture2D_RenderThread( RHICmdList, Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus );
        }

        virtual ETextureReallocationStatus FinalizeAsyncReallocateTexture2D_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted ) final override
        {
                return m_wrapped->FinalizeAsyncReallocateTexture2D_RenderThread( RHICmdList, Texture2D, bBlockUntilCompleted );
        }

        virtual ETextureReallocationStatus CancelAsyncReallocateTexture2D_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted ) final override
        {
                return m_wrapped->CancelAsyncReallocateTexture2D_RenderThread( RHICmdList, Texture2D, bBlockUntilCompleted );
        }

        virtual FIndexBufferRHIRef CreateIndexBuffer_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->CreateIndexBuffer_RenderThread( RHICmdList, Stride, Size, InUsage, CreateInfo );
        }

        virtual void* LockIndexBuffer_RenderThread( class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode ) final override
        {
                return m_wrapped->LockIndexBuffer_RenderThread( RHICmdList, IndexBuffer, Offset, SizeRHI, LockMode );
        }

        virtual void UnlockIndexBuffer_RenderThread( class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBuffer ) final override
        {
                m_wrapped->UnlockIndexBuffer_RenderThread( RHICmdList, IndexBuffer );
        }

        virtual FVertexDeclarationRHIRef CreateVertexDeclaration_RenderThread( class FRHICommandListImmediate& RHICmdList, const FVertexDeclarationElementList& Elements ) final override
        {
                return m_wrapped->CreateVertexDeclaration_RenderThread( RHICmdList, Elements );
        }

        virtual void UpdateTexture2D_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData ) final override
        {
                m_wrapped->UpdateTexture2D_RenderThread( RHICmdList, Texture, MipIndex, UpdateRegion, SourcePitch, SourceData );
        }

        virtual void* LockTexture2D_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true ) final override
        {
                return m_wrapped->LockTexture2D_RenderThread( RHICmdList, Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, true );
        }

        virtual void UnlockTexture2D_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true ) final override
        {
                m_wrapped->UnlockTexture2D_RenderThread( RHICmdList, Texture, MipIndex, bLockWithinMiptail, true );
        }

        virtual FTexture2DRHIRef RHICreateTexture2D_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTexture2D_RenderThread( RHICmdList, SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo );
        }

        virtual FTexture2DArrayRHIRef RHICreateTexture2DArray_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTexture2DArray_RenderThread( RHICmdList, SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo );
        }

        virtual FTexture3DRHIRef RHICreateTexture3D_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTexture3D_RenderThread( RHICmdList, SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo );
        }

        virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread( class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer ) final override
        {
                return m_wrapped->RHICreateUnorderedAccessView_RenderThread( RHICmdList, StructuredBuffer, bUseUAVCounter, bAppendBuffer );
        }

        virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread( class FRHICommandListImmediate& RHICmdList, FTextureRHIParamRef Texture, uint32 MipLevel ) final override
        {
                return m_wrapped->RHICreateUnorderedAccessView_RenderThread( RHICmdList, Texture, MipLevel );
        }

        virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread( class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint8 Format ) final override
        {
                return m_wrapped->RHICreateUnorderedAccessView_RenderThread( RHICmdList, VertexBuffer, Format );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel ) final override
        {
                return m_wrapped->RHICreateShaderResourceView_RenderThread( RHICmdList, Texture2DRHI, MipLevel );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format ) final override
        {
                return m_wrapped->RHICreateShaderResourceView_RenderThread( RHICmdList, Texture2DRHI, MipLevel, NumMipLevels, Format );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel ) final override
        {
                return m_wrapped->RHICreateShaderResourceView_RenderThread( RHICmdList, Texture3DRHI, MipLevel );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread( class FRHICommandListImmediate& RHICmdList, FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel ) final override
        {
                return m_wrapped->RHICreateShaderResourceView_RenderThread( RHICmdList, Texture2DArrayRHI, MipLevel );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread( class FRHICommandListImmediate& RHICmdList, FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel ) final override
        {
                return m_wrapped->RHICreateShaderResourceView_RenderThread( RHICmdList, TextureCubeRHI, MipLevel );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread( class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format ) final override
        {
                return m_wrapped->RHICreateShaderResourceView_RenderThread( RHICmdList, VertexBuffer, Stride, Format );
        }

        virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread( class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer ) final override
        {
                return m_wrapped->RHICreateShaderResourceView_RenderThread( RHICmdList, StructuredBuffer );
        }

        virtual FTextureCubeRHIRef RHICreateTextureCube_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTextureCube_RenderThread( RHICmdList, Size, Format, NumMips, Flags, CreateInfo );
        }

        virtual FTextureCubeRHIRef RHICreateTextureCubeArray_RenderThread( class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo ) final override
        {
                return m_wrapped->RHICreateTextureCubeArray_RenderThread( RHICmdList, Size, ArraySize, Format, NumMips, Flags, CreateInfo );
        }

        virtual FRenderQueryRHIRef RHICreateRenderQuery_RenderThread( class FRHICommandListImmediate& RHICmdList, ERenderQueryType QueryType ) final override
        {
                return m_wrapped->RHICreateRenderQuery_RenderThread( RHICmdList, QueryType );
        }

public:

        FDynamicRHI* m_wrapped;
		IRHICommandContext* m_commandContext;
};

class FRHICommandContextWrap : public IRHICommandContext
{
public:

        virtual void RHIWaitComputeFence( FComputeFenceRHIParamRef InFence ) final override
        {
                m_wrapped->RHIWaitComputeFence( InFence );
        }

        virtual void RHISetComputeShader( FComputeShaderRHIParamRef ComputeShader ) final override
        {
                m_wrapped->RHISetComputeShader( ComputeShader );
        }

        virtual void RHIDispatchComputeShader( uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ ) final override
        {
                m_wrapped->RHIDispatchComputeShader( ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ );
        }

        virtual void RHIDispatchIndirectComputeShader( FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset ) final override
        {
                m_wrapped->RHIDispatchIndirectComputeShader( ArgumentBuffer, ArgumentOffset );
        }

        virtual void RHIAutomaticCacheFlushAfterComputeShader( bool bEnable ) final override
        {
                m_wrapped->RHIAutomaticCacheFlushAfterComputeShader( bEnable );
        }

        virtual void RHIFlushComputeShaderCache(  ) final override
        {
                m_wrapped->RHIFlushComputeShaderCache(  );
        }

        virtual void RHISetMultipleViewports( uint32 Count, const FViewportBounds* Data ) final override
        {
                m_wrapped->RHISetMultipleViewports( Count, Data );
        }

        virtual void RHIClearUAV( FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values ) final override
        {
                m_wrapped->RHIClearUAV( UnorderedAccessViewRHI, Values );
        }

        virtual void RHICopyToResolveTarget( FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, bool bKeepOriginalSurface, const FResolveParams& ResolveParams ) final override
        {
                m_wrapped->RHICopyToResolveTarget( SourceTexture, DestTexture, bKeepOriginalSurface, ResolveParams );
        }

        virtual void RHITransitionResources( EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures ) final override
        {
                m_wrapped->RHITransitionResources( TransitionType, InTextures, NumTextures );
        }

        virtual void RHITransitionResources( EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence ) final override
        {
                m_wrapped->RHITransitionResources( TransitionType, TransitionPipeline, InUAVs, NumUAVs, WriteComputeFence );
        }

        virtual void RHIBeginRenderQuery( FRenderQueryRHIParamRef RenderQuery ) final override
        {
                m_wrapped->RHIBeginRenderQuery( RenderQuery );
        }

        virtual void RHIEndRenderQuery( FRenderQueryRHIParamRef RenderQuery ) final override
        {
                m_wrapped->RHIEndRenderQuery( RenderQuery );
        }

        virtual void RHIBeginOcclusionQueryBatch(  ) final override
        {
                m_wrapped->RHIBeginOcclusionQueryBatch(  );
        }

        virtual void RHIEndOcclusionQueryBatch(  ) final override
        {
                m_wrapped->RHIEndOcclusionQueryBatch(  );
        }

        virtual void RHISubmitCommandsHint(  ) final override
        {
                m_wrapped->RHISubmitCommandsHint(  );
        }

        virtual void RHIBeginDrawingViewport( FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI ) final override
        {
                m_wrapped->RHIBeginDrawingViewport( Viewport, RenderTargetRHI );
        }

        virtual void RHIEndDrawingViewport( FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync ) final override
        {
                m_wrapped->RHIEndDrawingViewport( Viewport, bPresent, bLockToVsync );
        }

        virtual void RHIBeginFrame(  ) final override
        {
                m_wrapped->RHIBeginFrame(  );
        }

        virtual void RHIEndFrame(  ) final override
        {
                m_wrapped->RHIEndFrame(  );
        }

        virtual void RHIBeginScene(  ) final override
        {
                m_wrapped->RHIBeginScene(  );
        }

        virtual void RHIEndScene(  ) final override
        {
                m_wrapped->RHIEndScene(  );
        }

        virtual void RHISetStreamSource( uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset ) final override
        {
                m_wrapped->RHISetStreamSource( StreamIndex, VertexBuffer, Stride, Offset );
        }

        virtual void RHISetRasterizerState( FRasterizerStateRHIParamRef NewState ) final override
        {
                m_wrapped->RHISetRasterizerState( NewState );
        }

        virtual void RHISetViewport( uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ ) final override
        {
                m_wrapped->RHISetViewport( MinX, MinY, MinZ, MaxX, MaxY, MaxZ );
        }

        virtual void RHISetScissorRect( bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY ) final override
        {
                m_wrapped->RHISetScissorRect( bEnable, MinX, MinY, MaxX, MaxY );
        }

        virtual void RHISetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderState ) final override
        {
                m_wrapped->RHISetBoundShaderState( BoundShaderState );
        }

        virtual void RHISetShaderTexture( FVertexShaderRHIParamRef VertexShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture ) final override
        {
                m_wrapped->RHISetShaderTexture( VertexShader, TextureIndex, NewTexture );
        }

        virtual void RHISetShaderTexture( FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture ) final override
        {
                m_wrapped->RHISetShaderTexture( HullShader, TextureIndex, NewTexture );
        }

        virtual void RHISetShaderTexture( FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture ) final override
        {
                m_wrapped->RHISetShaderTexture( DomainShader, TextureIndex, NewTexture );
        }

        virtual void RHISetShaderTexture( FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture ) final override
        {
                m_wrapped->RHISetShaderTexture( GeometryShader, TextureIndex, NewTexture );
        }

        virtual void RHISetShaderTexture( FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture ) final override
        {
                m_wrapped->RHISetShaderTexture( PixelShader, TextureIndex, NewTexture );
        }

        virtual void RHISetShaderTexture( FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture ) final override
        {
                m_wrapped->RHISetShaderTexture( PixelShader, TextureIndex, NewTexture );
        }

        virtual void RHISetShaderSampler( FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState ) final override
        {
                m_wrapped->RHISetShaderSampler( ComputeShader, SamplerIndex, NewState );
        }

        virtual void RHISetShaderSampler( FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState ) final override
        {
                m_wrapped->RHISetShaderSampler( VertexShader, SamplerIndex, NewState );
        }

        virtual void RHISetShaderSampler( FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState ) final override
        {
                m_wrapped->RHISetShaderSampler( GeometryShader, SamplerIndex, NewState );
        }

        virtual void RHISetShaderSampler( FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState ) final override
        {
                m_wrapped->RHISetShaderSampler( DomainShader, SamplerIndex, NewState );
        }

        virtual void RHISetShaderSampler( FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState ) final override
        {
                m_wrapped->RHISetShaderSampler( HullShader, SamplerIndex, NewState );
        }

        virtual void RHISetShaderSampler( FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState ) final override
        {
                m_wrapped->RHISetShaderSampler( PixelShader, SamplerIndex, NewState );
        }

        virtual void RHISetUAVParameter( FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV ) final override
        {
                m_wrapped->RHISetUAVParameter( ComputeShader, UAVIndex, UAV );
        }

        virtual void RHISetUAVParameter( FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount ) final override
        {
                m_wrapped->RHISetUAVParameter( ComputeShader, UAVIndex, UAV, InitialCount );
        }

        virtual void RHISetShaderResourceViewParameter( FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV ) final override
        {
                m_wrapped->RHISetShaderResourceViewParameter( PixelShader, SamplerIndex, SRV );
        }

        virtual void RHISetShaderResourceViewParameter( FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV ) final override
        {
                m_wrapped->RHISetShaderResourceViewParameter( VertexShader, SamplerIndex, SRV );
        }

        virtual void RHISetShaderResourceViewParameter( FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV ) final override
        {
                m_wrapped->RHISetShaderResourceViewParameter( ComputeShader, SamplerIndex, SRV );
        }

        virtual void RHISetShaderResourceViewParameter( FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV ) final override
        {
                m_wrapped->RHISetShaderResourceViewParameter( HullShader, SamplerIndex, SRV );
        }

        virtual void RHISetShaderResourceViewParameter( FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV ) final override
        {
                m_wrapped->RHISetShaderResourceViewParameter( DomainShader, SamplerIndex, SRV );
        }

        virtual void RHISetShaderResourceViewParameter( FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV ) final override
        {
                m_wrapped->RHISetShaderResourceViewParameter( GeometryShader, SamplerIndex, SRV );
        }

        virtual void RHISetShaderUniformBuffer( FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer ) final override
        {
                m_wrapped->RHISetShaderUniformBuffer( VertexShader, BufferIndex, Buffer );
        }

        virtual void RHISetShaderUniformBuffer( FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer ) final override
        {
                m_wrapped->RHISetShaderUniformBuffer( HullShader, BufferIndex, Buffer );
        }

        virtual void RHISetShaderUniformBuffer( FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer ) final override
        {
                m_wrapped->RHISetShaderUniformBuffer( DomainShader, BufferIndex, Buffer );
        }

        virtual void RHISetShaderUniformBuffer( FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer ) final override
        {
                m_wrapped->RHISetShaderUniformBuffer( GeometryShader, BufferIndex, Buffer );
        }

        virtual void RHISetShaderUniformBuffer( FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer ) final override
        {
                m_wrapped->RHISetShaderUniformBuffer( PixelShader, BufferIndex, Buffer );
        }

        virtual void RHISetShaderUniformBuffer( FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer ) final override
        {
                m_wrapped->RHISetShaderUniformBuffer( ComputeShader, BufferIndex, Buffer );
        }

        virtual void RHISetShaderParameter( FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue ) final override
        {
                m_wrapped->RHISetShaderParameter( VertexShader, BufferIndex, BaseIndex, NumBytes, NewValue );
        }

        virtual void RHISetShaderParameter( FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue ) final override
        {
                m_wrapped->RHISetShaderParameter( PixelShader, BufferIndex, BaseIndex, NumBytes, NewValue );
        }

        virtual void RHISetShaderParameter( FHullShaderRHIParamRef HullShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue ) final override
        {
                m_wrapped->RHISetShaderParameter( HullShader, BufferIndex, BaseIndex, NumBytes, NewValue );
        }

        virtual void RHISetShaderParameter( FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue ) final override
        {
                m_wrapped->RHISetShaderParameter( DomainShader, BufferIndex, BaseIndex, NumBytes, NewValue );
        }

        virtual void RHISetShaderParameter( FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue ) final override
        {
                m_wrapped->RHISetShaderParameter( GeometryShader, BufferIndex, BaseIndex, NumBytes, NewValue );
        }

        virtual void RHISetShaderParameter( FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue ) final override
        {
                m_wrapped->RHISetShaderParameter( ComputeShader, BufferIndex, BaseIndex, NumBytes, NewValue );
        }

        virtual void RHISetDepthStencilState( FDepthStencilStateRHIParamRef NewState, uint32 StencilRef ) final override
        {
                m_wrapped->RHISetDepthStencilState( NewState, StencilRef );
        }

        virtual void RHISetBlendState( FBlendStateRHIParamRef NewState, const FLinearColor& BlendFactor ) final override
        {
                m_wrapped->RHISetBlendState( NewState, BlendFactor );
        }

        virtual void RHISetRenderTargets( uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs ) final override
        {
                m_wrapped->RHISetRenderTargets( NumSimultaneousRenderTargets, NewRenderTargets, NewDepthStencilTarget, NumUAVs, UAVs );
        }

        virtual void RHISetRenderTargetsAndClear( const FRHISetRenderTargetsInfo& RenderTargetsInfo ) final override
        {
                m_wrapped->RHISetRenderTargetsAndClear( RenderTargetsInfo );
        }

        virtual void RHIBindClearMRTValues( bool bClearColor, bool bClearDepth, bool bClearStencil ) final override
        {
                m_wrapped->RHIBindClearMRTValues( bClearColor, bClearDepth, bClearStencil );
        }

        virtual void RHIDrawPrimitive( uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances ) final override
        {
                m_wrapped->RHIDrawPrimitive( PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances );
        }

        virtual void RHIDrawPrimitiveIndirect( uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset ) final override
        {
                m_wrapped->RHIDrawPrimitiveIndirect( PrimitiveType, ArgumentBuffer, ArgumentOffset );
        }

        virtual void RHIDrawIndexedIndirect( FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances ) final override
        {
                m_wrapped->RHIDrawIndexedIndirect( IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances );
        }

        virtual void RHIDrawIndexedPrimitive( FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances ) final override
        {
                m_wrapped->RHIDrawIndexedPrimitive( IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances );
        }

        virtual void RHIDrawIndexedPrimitiveIndirect( uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset ) final override
        {
                m_wrapped->RHIDrawIndexedPrimitiveIndirect( PrimitiveType, IndexBuffer, ArgumentBuffer, ArgumentOffset );
        }

        virtual void RHIBeginDrawPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData ) final override
        {
                m_wrapped->RHIBeginDrawPrimitiveUP( PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData );
        }

        virtual void RHIEndDrawPrimitiveUP(  ) final override
        {
                m_wrapped->RHIEndDrawPrimitiveUP(  );
        }

        virtual void RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData ) final override
        {
                m_wrapped->RHIBeginDrawIndexedPrimitiveUP( PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData, MinVertexIndex, NumIndices, IndexDataStride, OutIndexData );
        }

        virtual void RHIEndDrawIndexedPrimitiveUP(  ) final override
        {
                m_wrapped->RHIEndDrawIndexedPrimitiveUP(  );
        }

        virtual void RHIClear( bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect ) final override
        {
                m_wrapped->RHIClear( bClearColor, Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect );
        }

        virtual void RHIClearMRT( bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect ) final override
        {
                m_wrapped->RHIClearMRT( bClearColor, NumClearColors, ColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect );
        }

        virtual void RHIEnableDepthBoundsTest( bool bEnable, float MinDepth, float MaxDepth ) final override
        {
                m_wrapped->RHIEnableDepthBoundsTest( bEnable, MinDepth, MaxDepth );
        }

        virtual void RHIPushEvent( const TCHAR* Name ) final override
        {
                m_wrapped->RHIPushEvent( Name );
        }

        virtual void RHIPopEvent(  ) final override
        {
                m_wrapped->RHIPopEvent(  );
        }

        virtual void RHIUpdateTextureReference( FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture ) final override
        {
                m_wrapped->RHIUpdateTextureReference( TextureRef, NewTexture );
        }

        virtual void RHIBeginAsyncComputeJob_DrawThread( EAsyncComputePriority Priority ) final override
        {
                m_wrapped->RHIBeginAsyncComputeJob_DrawThread( Priority );
        }

        virtual void RHIEndAsyncComputeJob_DrawThread( uint32 FenceIndex ) final override
        {
                m_wrapped->RHIEndAsyncComputeJob_DrawThread( FenceIndex );
        }

        virtual void RHIGraphicsWaitOnAsyncComputeJob( uint32 FenceIndex ) final override
        {
                m_wrapped->RHIGraphicsWaitOnAsyncComputeJob( FenceIndex );
        }

public:

        IRHICommandContext* m_wrapped;
};
