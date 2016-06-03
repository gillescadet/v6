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

#include <v6/core/vec3.h>
#include <v6/graphic/capture.h>
#include <v6/graphic/capture_shared.h>
#include <v6/graphic/gpu.h>

static const uint32			s_targetFPS			= 75;
static const uint32			s_sampleCount		= 9;
static const uint32			s_gridMacroShift	= 7;
static const float			s_gridMinScale		= 50;
static const float			s_gridMaxScale		= 5000;
static const uint32			s_renderTargetSize	= 1 << (s_gridMacroShift + 2);

enum CaptureState_e
{
	CAPTURE_STATE_BEGIN,
	CAPTURE_STATE_MIDDLE,
	CAPTURE_STATE_END,
};

FDynamicRHI*				s_dynamicRHIOriginal = nullptr;
FDynamicRHIWrap				s_dynamicRHIWrap;
FRHICommandContextWrap		s_rhiCommandContextWrap;
v6::CaptureContext_s		s_captureContext;
CaptureState_e				s_captureState;
v6::u32						s_captureFrameID;
v6::Vec3					s_captureOrigin;
v6::Vec3					s_captureSamplePos;
v6::Vec3					s_captureFaceBasis[3];
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
		s_capturedSampleCount = v6::CaptureContext_AddSamplesFromCubeFace( &s_captureContext, &s_captureSamplePos, s_captureFaceBasis, s_colorRenderTarget->GetShaderResourceView(), s_depthRenderTarget->GetShaderResourceView() );
	}

	if ( s_captureState == CAPTURE_STATE_END )
	{
		v6::CaptureContext_End( &s_captureContext );
		
		FString path = FString::Printf( TEXT( "%s_%06u.v6f" ), TEXT( "d:/tmp/v6/ue" ), s_captureFrameID );

		v6::CFileWriter fileWriter;
		if ( !fileWriter.Open( TCHAR_TO_ANSI( *path ) ) )
		{
			UE_LOG( LogVideo6DOF, Error, TEXT( "Unable to create file." ) );
		}
		else
		{
			v6::CodecRawFrameDesc_s frameDesc = {};
			frameDesc.gridOrigin = s_captureOrigin;
			frameDesc.gridBasis[0] = v6::Vec3_Make( 1, 0, 0 );
			frameDesc.gridBasis[1] = v6::Vec3_Make( 0, 0, 1 );
			frameDesc.gridBasis[2] = v6::Vec3_Make( 0, 1, 0 );
			frameDesc.frameID = s_captureFrameID;
			frameDesc.frameRate = s_targetFPS;
			frameDesc.sampleCount = s_sampleCount;
			frameDesc.gridMacroShift = s_gridMacroShift;
			frameDesc.gridScaleMin = s_gridMinScale;
			frameDesc.gridScaleMax = s_gridMaxScale;
			
			v6::CodecRawFrameData_s frameData = {};

			{
				v6::CaptureContext_MapBlocksForRead( &s_captureContext, frameDesc.blockCounts, &frameData.blockPos, &frameData.blockData );
				v6::Codec_WriteRawFrame( &fileWriter, &frameDesc, &frameData );
				v6::CaptureContext_UnmapBlocksForRead( &s_captureContext );
			}
		}
	}

	v6::GPUEvent_End();

	v6::GPUShaderState_Restore( &gpuShaderState );
	v6::GPURenderTargetState_Restore( &gpuRenderTargetState );
}

static void Scene_Begin( FSceneViewFamily* viewFamily )
{
	v6::GPUEvent_Begin( "Video6DOF Capture" );
	s_dynamicRHIOriginal->RHIBindDebugLabelName( viewFamily->RenderTarget->GetRenderTargetTexture(), TEXT( "SceneColorVideo6DOF" ) );
	s_captureRenderTarget = __GetD3D11TextureFromRHITexture( viewFamily->RenderTarget->GetRenderTargetTexture() );
	s_colorRenderTarget = nullptr;
	s_depthRenderTarget = nullptr;
	s_capturedSampleCount = 0;
	s_rhiCommandContextWrap.m_endSceneCallback = Scene_End;
	s_rhiCommandContextWrap.m_setRenderTargetCallback = Scene_SetRenderTarget;

	if ( s_captureState == CAPTURE_STATE_BEGIN )
		v6::CaptureContext_Begin( &s_captureContext, &s_captureOrigin );
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

static void ComputeOrthoBasisFromFace( uint32 faceID, FVector* right, FVector* up, FVector* lookAt )
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

	*lookAt = lookAts[faceID];
	*up = ups[faceID];
	*right = *up ^ *lookAt;
}

static void Scene_CaptureFace( FTextureRenderTargetResource* renderTargetResource, const FVector& origin, const FVector& samplePos, uint32 frameID, uint32 faceID, CaptureState_e captureState )
{
	FEngineShowFlags showFlags( ESFIM_Game );
	showFlags.SetMotionBlur( false ); // motion blur doesn't work correctly with scene captures.
	showFlags.SetSeparateTranslucency( false );
	showFlags.SetHMDDistortion( false );
	showFlags.SetAntiAliasing( false );
	showFlags.SetTranslucency( false );
	showFlags.SetCameraImperfections( false );
	showFlags.SetDepthOfField( false );
	showFlags.SetGrain( false );
	
	FSceneViewFamilyContext viewFamily( FSceneViewFamily::ConstructionValues( renderTargetResource, GWorld->Scene, showFlags ) );

	FVector right, up, lookAt;
	ComputeOrthoBasisFromFace( faceID, &right, &up, &lookAt );

	FSceneViewInitOptions viewInitOptions;
	viewInitOptions.SetViewRectangle( FIntRect( 0, 0, renderTargetResource->GetSizeXY().X, renderTargetResource->GetSizeXY().Y ) );
	viewInitOptions.ViewFamily = &viewFamily;
	viewInitOptions.ViewOrigin = samplePos;
	viewInitOptions.ViewRotationMatrix = FBasisVectorMatrix( right, up, lookAt, FVector::ZeroVector );
	viewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix( v6::DegToRad( 90.0f ) * 0.5f, 1.0f, 1.0f, GNearClippingPlane );

	FSceneView* newView = new FSceneView( viewInitOptions );
	newView->bIsSceneCapture = true;
	viewFamily.Views.Add( newView );
	viewFamily.ViewExtensions.Add( MakeShareable( new FSceneViewExtension ) );

	s_captureState = captureState;
	s_captureFrameID = frameID;
	s_captureSamplePos = v6::Vec3_Make( samplePos.X, samplePos.Y, samplePos.Z );
	s_captureFaceBasis[0] = v6::Vec3_Make( right.X, right.Y, right.Z );
	s_captureFaceBasis[1] = v6::Vec3_Make( up.X, up.Y, up.Z );
	s_captureFaceBasis[2] = v6::Vec3_Make( lookAt.X, lookAt.Y, lookAt.Z );
	s_captureOrigin = v6::Vec3_Make( origin.X, origin.Y, origin.Z );
	s_captureRenderTarget = nullptr;

	FCanvas canvas( renderTargetResource, nullptr, GWorld, GWorld->Scene->GetFeatureLevel() );
	canvas.Clear( FLinearColor::Transparent );
	GetRendererModule().BeginRenderingViewFamily( &canvas, &viewFamily );

	FlushRenderingCommands();
}

static void Scene_CaptureCube( uint32 size, const FVector& origin, uint32 frameID )
{
	UTextureRenderTarget2D* renderTargetTexture = NewObject< UTextureRenderTarget2D >();
	renderTargetTexture->AddToRoot();
	renderTargetTexture->ClearColor = FLinearColor::Black;
	renderTargetTexture->InitCustomFormat( size, size, PF_B8G8R8A8, false );
	
	FTextureRenderTargetResource* renderTargetResource = renderTargetTexture->GameThread_GetRenderTargetResource();

	v6::CaptureDesc_s captureDesc;
	captureDesc.sampleCount = s_sampleCount;
	captureDesc.gridMacroShift = s_gridMacroShift;
	captureDesc.gridScaleMin = s_gridMinScale;
	captureDesc.gridScaleMax = s_gridMaxScale;
	captureDesc.depthLinearScale = 1.0f / GNearClippingPlane;
	captureDesc.depthLinearBias = 0.0f;
	captureDesc.logReadBack = false;
	v6::CaptureContext_Create( &s_captureContext, &captureDesc );

	for ( uint32 sampleID = 0; sampleID < s_sampleCount; ++sampleID )
	{
		const v6::Vec3 gridOrigin = v6::Vec3_Make( origin.X, origin.Y, origin.Z );
		const v6::Vec3 samplePos = v6::CaptureContext_ComputeSamplePos( &s_captureContext, &gridOrigin, sampleID );

		for ( uint32 faceID = 0; faceID < 6; ++faceID )
		{
			CaptureState_e captureState;
			if ( sampleID == 0 && faceID == 0)
				captureState = CAPTURE_STATE_BEGIN;
			else if ( sampleID == s_sampleCount-1 && faceID == 5 )
				captureState = CAPTURE_STATE_END;
			else
				captureState = CAPTURE_STATE_MIDDLE;

			Scene_CaptureFace( renderTargetResource, origin, FVector( samplePos.x, samplePos.y, samplePos.z ), frameID, faceID, captureState );
		}
	}

	v6::CaptureContext_Release( &s_captureContext );

	renderTargetTexture->RemoveFromRoot();
	renderTargetTexture = nullptr;
}

static void Device_Override()
{
	check( s_dynamicRHIOriginal == nullptr );

	FD3D11DynamicRHI* d3d11DynamicRHI = static_cast< FD3D11DynamicRHI* >( GDynamicRHI );
	v6::GPUDevice_Set( d3d11DynamicRHI->GetDevice() );
	
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

	v6::GPUDevice_Release();

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
	m_state = EVideo6DOFCapturerState::NONE;
}

void UVideo6DOFCapturer::Capture( const FVector& position, const FQuat& orientation, uint32 frameCount )
{
#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
	if( !FParse::Param( FCommandLine::Get(), TEXT( "NoTextureStreaming" ) ) )
		UE_LOG( LogVideo6DOF, Warning, TEXT( "Texture streaming should be disable while capturing. Add \"-notexturestreaming\" on the command line and restart tthe editor." ) );
#endif // #if PLATFORM_SUPPORTS_TEXTURE_STREAMING
	m_capturePosition = position;
	m_captureOrientation = orientation;
	m_captureFrameID = 0;
	m_captureFrameCount = frameCount;
	m_state = EVideo6DOFCapturerState::CAPTURE;
	FApp::SetFixedDeltaTime( 1.0f / s_targetFPS );
	FApp::SetUseFixedTimeStep( true );
}

void UVideo6DOFCapturer::Stop()
{
	m_state = EVideo6DOFCapturerState::STOP;
}

void UVideo6DOFCapturer::Tick( float DeltaTime )
{
	switch( m_state )
	{
	case EVideo6DOFCapturerState::NONE:
		return;
	
	case EVideo6DOFCapturerState::CAPTURE:
		Scene_CaptureCube( s_renderTargetSize, m_capturePosition, m_captureFrameID );
		++m_captureFrameID;
		UE_LOG( LogVideo6DOF, Log, TEXT( "Captured frame %d/%d" ), m_captureFrameID, m_captureFrameCount );

		if ( m_captureFrameID == m_captureFrameCount )
			m_state = EVideo6DOFCapturerState::DONE;
		break;
	
	case EVideo6DOFCapturerState::STOP:
		UE_LOG( LogVideo6DOF, Log, TEXT( "Capture stopped by user." ) );
		m_state = EVideo6DOFCapturerState::DONE;
		break;
	
	case EVideo6DOFCapturerState::DONE:
		FApp::SetUseFixedTimeStep( false );
		if ( m_captureFrameID == m_captureFrameCount )
			UE_LOG( LogVideo6DOF, Log, TEXT( "Capture done." ) );
		m_state = EVideo6DOFCapturerState::NONE;
		break;
	}
}
