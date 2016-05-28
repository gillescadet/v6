// Copyright 2016 Video6DOF.  All rights reserved.

// command line:
// -usefixedtimestep
// -fps=25
// -notexturestreaming
// Edit -> EditorPreferences -> (General)Miscellaneous called "Use Less CPU when in Background"

#include <v6/core/common.h>
#include "Video6DOFPrivatePCH.h"

#include "D3D11RHIPrivate.h"
#include "EngineModule.h"
#include "RHIDefinitions.h"
#include "SceneViewExtension.h"
#include "ScenePrivate.h"
#include "Video6DOFWrapper.h"

#include <v6/graphic/capture.h>
#include <v6/graphic/capture_shared.h>
#include <v6/graphic/gpu.h>

static const uint32			s_gridMacroShift	= 8;
static const float			s_gridMinScale		= 50;
static const float			s_gridMaxScale		= 5000;
static const uint32			s_renderTargetSize	= 1 << (s_gridMacroShift + 2);

FDynamicRHI*				s_dynamicRHIOriginal = nullptr;
FDynamicRHIWrap				s_dynamicRHIWrap;
FRHICommandContextWrap		s_rhiCommandContextWrap;
v6::CaptureContext_s		s_captureContext;
v6::u32						s_captureFaceID;
v6::Vec3					s_captureOrigin;
FD3D11TextureBase*			s_captureRenderTarget;
FD3D11TextureBase*			s_colorRenderTarget;
FD3D11TextureBase*			s_depthRenderTarget;
v6::u32						s_capturedSampleCount;

extern ID3D11Device*		v6::g_device;
extern ID3D11DeviceContext*	v6::g_deviceContext;

static void Scene_SetRenderTarget( FD3D11TextureBase* color, FD3D11TextureBase* depth )
{
	if ( s_captureRenderTarget == color )
		s_colorRenderTarget = color;

	if ( depth != nullptr )
		s_depthRenderTarget = depth;
}

static void Scene_End()
{
	v6::GPURenderTargetState_s gpuRenderTargetState;
	v6::GPUShaderState_s gpuShaderState;
	v6::GPURenderTargetState_Save( &gpuRenderTargetState );
	v6::GPUShaderState_Save( &gpuShaderState );

	if ( s_colorRenderTarget == nullptr || s_depthRenderTarget == nullptr )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Unable to find the color and depth render targets" ) );
	}
	else
	{
		v6::g_deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );
		s_capturedSampleCount = v6::Capture_AddSamplesFromCubeFace( &s_captureContext, &s_captureOrigin, &s_captureOrigin, s_captureFaceID, s_colorRenderTarget->GetShaderResourceView(), s_depthRenderTarget->GetShaderResourceView() );
		UE_LOG( LogVideo6DOF, Log, TEXT( "Captured %d samples" ), s_capturedSampleCount );
	}

	if ( s_captureFaceID == 5 )
	{
		v6::Capture_End( &s_captureContext );
		
		const uint32 frameID = 0;

		FString path = FString::Printf( TEXT( "%s_%06u.v6f" ), TEXT( "d:/tmp/v6/ue" ), frameID );

		v6::CFileWriter fileWriter;
		if ( !fileWriter.Open( TCHAR_TO_ANSI( *path ) ) )
		{
			UE_LOG( LogVideo6DOF, Error, TEXT( "Unable to create file." ) );
		}
		else
		{
			v6::CodecRawFrameDesc_s frameDesc = {};
			frameDesc.origin = s_captureOrigin;
			frameDesc.frameID = frameID;
			frameDesc.sampleCount = 1;
			frameDesc.gridMacroShift = s_gridMacroShift;
			frameDesc.gridScaleMin = s_gridMinScale;
			frameDesc.gridScaleMax = s_gridMaxScale;
			
			v6::CodecRawFrameData_s frameData = {};

			{
				v6::Capture_MapBlocksForRead( &s_captureContext, frameDesc.blockCounts, &frameData.blockPos, &frameData.blockData );
				v6::Codec_WriteRawFrame( &fileWriter, &frameDesc, &frameData );
				v6::Capture_UnmapBlocksForRead( &s_captureContext );
			}

			UE_LOG( LogVideo6DOF, Log, TEXT( "Stream saved to file." ) );
		}
	}

	v6::GPU_EndEvent();

	v6::GPUShaderState_Restore( &gpuShaderState );
	v6::GPURenderTargetState_Restore( &gpuRenderTargetState );
}

static void Scene_Begin( FSceneViewFamily* viewFamily )
{
	v6::GPU_BeginEvent( "Video6DOF Capture" );
	s_dynamicRHIOriginal->RHIBindDebugLabelName( viewFamily->RenderTarget->GetRenderTargetTexture(), TEXT( "SceneColorVideo6DOF" ) );
	s_captureRenderTarget = __GetD3D11TextureFromRHITexture( viewFamily->RenderTarget->GetRenderTargetTexture() );
	s_colorRenderTarget = nullptr;
	s_depthRenderTarget = nullptr;
	s_capturedSampleCount = 0;
	s_rhiCommandContextWrap.m_endSceneCallback = Scene_End;
	s_rhiCommandContextWrap.m_setRenderTargetCallback = Scene_SetRenderTarget;

	if ( s_captureFaceID == 0 )
		v6::Capture_Begin( &s_captureContext );
}

class FSceneViewExtension : public ISceneViewExtension
{
public:
    void SetupViewFamily( FSceneViewFamily& InViewFamily )
	{
	}

	void SetupView( FSceneViewFamily& InViewFamily, FSceneView& InView )
	{
	}

    void BeginRenderViewFamily( FSceneViewFamily& InViewFamily )
	{
	}

    void PreRenderViewFamily_RenderThread( FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily )
	{
		Scene_Begin( &InViewFamily );
	}
    
	void PreRenderView_RenderThread( FRHICommandListImmediate& RHICmdList, FSceneView& InView )
	{
	}
};

static FMatrix ComputeOrientationMatrixFromFace( uint32 faceID )
{
	static const FVector lookAts[6] = 
	{
		FVector(  1.0f,  0.0f,  0.0f ),
		FVector( -1.0f,  0.0f,  0.0f ),
		FVector(  0.0f,  1.0f,  0.0f ),
		FVector(  0.0f, -1.0f,  0.0f ),
		FVector(  0.0f,  0.0f,  1.0f ),
		FVector(  0.0f,  0.0f, -1.0f )
	};

	static const FVector ups[6] =
	{
		FVector( 0.0f,  1.0f,  0.0f ),
		FVector( 0.0f , 1.0f,  0.0f ),
		FVector( 0.0f,  0.0f, -1.0f ),
		FVector( 0.0f,  0.0f,  1.0f ),
		FVector( 0.0f,  1.0f,  0.0f ),
		FVector( 0.0f,  1.0f,  0.0f )
	};

	const FVector& lookAt = lookAts[faceID];
	const FVector& up = ups[faceID];
	const FVector right = up ^ lookAt;
	return FBasisVectorMatrix( right, up, lookAt, FVector::ZeroVector );
}

static void Scene_CaptureFace( TArray< FColor >& colors, uint32 size, const FVector& origin, uint32 faceID )
{
	UTextureRenderTarget2D* renderTargetTexture = NewObject< UTextureRenderTarget2D >();
	renderTargetTexture->AddToRoot();
	renderTargetTexture->ClearColor = FLinearColor::Black;
	renderTargetTexture->InitCustomFormat( size, size, PF_B8G8R8A8, false );
	
	FTextureRenderTargetResource* renderTargetResource = renderTargetTexture->GameThread_GetRenderTargetResource();

	FEngineShowFlags showFlags( ESFIM_Game );
	showFlags.SetMotionBlur( 0 ); // motion blur doesn't work correctly with scene captures.
	showFlags.SetSeparateTranslucency( 0 );
	showFlags.SetHMDDistortion( 0 );
	showFlags.SetAntiAliasing( false );
	showFlags.SetTranslucency( false );
	
	FSceneViewFamilyContext viewFamily( FSceneViewFamily::ConstructionValues( renderTargetResource, GWorld->Scene, showFlags ) );

	//static_assert( ERHIZBuffer::IsInverted );

	FSceneViewInitOptions viewInitOptions;
	viewInitOptions.SetViewRectangle( FIntRect( 0, 0, size, size ) );
	viewInitOptions.ViewFamily = &viewFamily;
	viewInitOptions.ViewOrigin = origin;
	viewInitOptions.ViewRotationMatrix = ComputeOrientationMatrixFromFace( faceID );
	viewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix( v6::DegToRad( 90.0f ) * 0.5f, 1.0f, 1.0f, GNearClippingPlane );

	FSceneView* newView = new FSceneView( viewInitOptions );
	newView->bIsSceneCapture = true;
	viewFamily.Views.Add( newView );
	viewFamily.ViewExtensions.Add( MakeShareable( new FSceneViewExtension ) );

	s_captureFaceID = faceID;
	s_captureOrigin = v6::Vec3_Make( origin.X, origin.Y, origin.Z );
	s_captureRenderTarget = nullptr;

	FCanvas canvas( renderTargetResource, nullptr, GWorld, GWorld->Scene->GetFeatureLevel() );
	canvas.Clear( FLinearColor::Transparent );
	GetRendererModule().BeginRenderingViewFamily( &canvas, &viewFamily );

#if 0
	renderTargetResource->ReadPixels( colors, FReadSurfaceDataFlags(), FIntRect( 0, 0, 0, 0 ) );
#endif
	FlushRenderingCommands();

	renderTargetTexture->RemoveFromRoot();
	renderTargetTexture = nullptr;
}

static void Scene_CaptureCube( uint32 size, const FVector& origin, IImageWrapperModule* imageWrapperModule )
{
	IImageWrapperPtr imageWrapper = imageWrapperModule->CreateImageWrapper( EImageFormat::PNG );
	TArray< FColor > colors;

	v6::CaptureDesc_s captureDesc;
	captureDesc.appWorldToV6World.m_row0 = Vec3_Make( 1.0f, 0.0f, 0.0f );
	captureDesc.appWorldToV6World.m_row1 = Vec3_Make( 0.0f, 0.0f, 1.0f );
	captureDesc.appWorldToV6World.m_row2 = Vec3_Make( 0.0f, 1.0f, 0.0f );
	captureDesc.gridMacroShift = s_gridMacroShift;
	captureDesc.gridScaleMin = s_gridMinScale;
	captureDesc.gridScaleMax = s_gridMaxScale;
	captureDesc.depthLinearScale = 1.0f / GNearClippingPlane;
	captureDesc.depthLinearBias = 0.0f;
	captureDesc.logReadBack = false;
	v6::Capture_Create( &s_captureContext, &captureDesc );
	
	for ( uint32 faceID = 0; faceID < 6; ++faceID )
	{
		Scene_CaptureFace( colors, size, origin, faceID );
#if 0
		imageWrapper->SetRaw( colors.GetData(), colors.GetAllocatedSize(), size, size, ERGBFormat::BGRA, 8 );
		const TArray< uint8 >& pngData = imageWrapper->GetCompressed( 100 );
		const FString filename = TEXT( "d:/tmp/ue_color" ) + FString::FromInt( faceID ) + TEXT( ".png" );
		FFileHelper::SaveArrayToFile( pngData, *filename );
		UE_LOG( LogVideo6DOF, Log, TEXT( "Captured %d pixels" ), colors.Num() );
#else
		UE_LOG( LogVideo6DOF, Log, TEXT( "Captured face %d" ), faceID );
#endif
	}

	v6::Capture_Release( &s_captureContext );

	imageWrapper.Reset();
}

static void Device_Override()
{
	check( s_dynamicRHIOriginal == nullptr );

	FD3D11DynamicRHI* d3d11DynamicRHI = static_cast< FD3D11DynamicRHI* >( GDynamicRHI );
	v6::GPU_SetDevice( d3d11DynamicRHI->GetDevice() );
	
	s_dynamicRHIOriginal = GDynamicRHI;
	s_rhiCommandContextWrap.m_wrapped = d3d11DynamicRHI->RHIGetDefaultContext();
	s_rhiCommandContextWrap.m_endSceneCallback = nullptr;
	s_rhiCommandContextWrap.m_setRenderTargetCallback = nullptr;
	s_dynamicRHIWrap.m_commandContext = &s_rhiCommandContextWrap;
	s_dynamicRHIWrap.m_wrapped = GDynamicRHI;
	GDynamicRHI = &s_dynamicRHIWrap;

	GEmitDrawEvents = true;
}

static void Device_Restore()
{
	check( s_dynamicRHIOriginal != nullptr );

	GDynamicRHI = s_dynamicRHIOriginal;
	s_dynamicRHIOriginal = nullptr;
}

UVideo6DOFCapturer::UVideo6DOFCapturer()
{
	Init();
}

UVideo6DOFCapturer::UVideo6DOFCapturer( FVTableHelper& Helper )
	: Super(Helper)
{
	Init();
}

void UVideo6DOFCapturer::Startup()
{
	Device_Override();
}

void UVideo6DOFCapturer::Shutdown()
{
	Device_Restore();
}

void UVideo6DOFCapturer::Init()
{
	m_imageWrapperModule = &FModuleManager::LoadModuleChecked< IImageWrapperModule >( FName("ImageWrapper") );

	m_state = EVideo6DOFCapturerState::NONE;
}

void UVideo6DOFCapturer::Capture( const FVector& position, const FQuat& orientation )
{
	m_capturePosition = position;
	m_captureOrientation = orientation;
	m_state = EVideo6DOFCapturerState::CAPTURE;
}

void UVideo6DOFCapturer::Tick( float DeltaTime )
{
	switch( m_state )
	{
	case EVideo6DOFCapturerState::NONE:
		return;
	case EVideo6DOFCapturerState::CAPTURE:
		Scene_CaptureCube( s_renderTargetSize, m_capturePosition, m_imageWrapperModule );
		m_state = EVideo6DOFCapturerState::DONE;
		break;
	case EVideo6DOFCapturerState::DONE:
		break;
	}
}
