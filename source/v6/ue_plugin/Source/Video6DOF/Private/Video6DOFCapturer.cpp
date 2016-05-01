// Copyright 2016 Video6DOF.  All rights reserved.

#include "Video6DOFPrivatePCH.h"

#include "D3D11RHIPrivate.h"
#include "SceneViewExtension.h"
#include "ScenePrivate.h"
#include "Video6DOFWrapper.h"

FDynamicRHIWrap s_dynamicRHIWrap;
FRHICommandContextWrap s_rhiCommandContextWrap;

#define DELEGATE0( R, F )			virtual void F() final override { return g_dynamicRHI->F(); }
#define DELEGATE1( R, F, T1 )		virtual R F( T1 arg1 ) final override { return g_dynamicRHI->F( arg1 ); }

static void UpdateScene_RenderThread( FRHICommandListImmediate& rhiCmdList, FSceneRenderer* sceneRenderer, FTextureRenderTargetResource* textureRenderTarget )
{
	FMemMark memStackMark( FMemStack::Get() );

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources( rhiCmdList );

	{
		SCOPED_DRAW_EVENT( rhiCmdList, UpdateScene_RenderThread );

		check( !IsMobileHDR() );
		check( !RHINeedsToSwitchVerticalAxis(GMaxRHIShaderPlatform) );

		FViewInfo& view = sceneRenderer->Views[0];
		FIntRect viewRect = view.ViewRect;
		FIntRect uynconstrainedViewRect = view.UnconstrainedViewRect;
		SetRenderTarget( rhiCmdList, textureRenderTarget->GetRenderTargetTexture(), nullptr, true );
		rhiCmdList.Clear( true, FLinearColor::Black, false, (float)ERHIZBuffer::FarPlane, false, 0, viewRect );

		// Render the scene normally
		{
			SCOPED_DRAW_EVENT( rhiCmdList, RenderScene );

			sceneRenderer->Render( rhiCmdList );
		}

		rhiCmdList.CopyToResolveTarget( textureRenderTarget->GetRenderTargetTexture(), textureRenderTarget->TextureRHI, false, FResolveParams() );
	}

	// FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer( rhiCmdList, sceneRenderer );
}

class FSceneViewExtension : public ISceneViewExtension
{
public:
	/**
	* Called on game thread when creating the view family.
	*/
	void SetupViewFamily( FSceneViewFamily& InViewFamily ) override
	{
		static bool displayed = false;
		if ( !displayed )
		{
			UE_LOG( LogVideo6DOF, Log, TEXT( "SetupViewFamily" ) );
			displayed = true;
		}
	}

	/**
	* Called on game thread when creating the view.
	*/
	void SetupView( FSceneViewFamily& InViewFamily, FSceneView& InView ) override
	{
		static bool displayed = false;
		if ( !displayed )
		{
			UE_LOG( LogVideo6DOF, Log, TEXT( "SetupView" ) );
			displayed = true;
		}
	}

	/**
	* Called on game thread when view family is about to be rendered.
	*/
	void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{
		static bool displayed = false;
		if ( !displayed )
		{
			UE_LOG( LogVideo6DOF, Log, TEXT( "BeginRenderViewFamily" ) );
			displayed = true;
		}
	}

	/**
	* Called on render thread at the start of rendering.
	*/
	void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
	{
		static bool displayed = false;
		if ( !displayed )
		{
			UE_LOG( LogVideo6DOF, Log, TEXT( "PreRenderViewFamily_RenderThread" ) );
			displayed = true;
		}
	}

	/**
	* Called on render thread at the start of rendering, for each view, after PreRenderViewFamily_RenderThread call.
	*/
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
	{
		static bool displayed = false;
		if ( !displayed )
		{
			UE_LOG( LogVideo6DOF, Log, TEXT( "PreRenderView_RenderThread" ) );
			displayed = true;
		}
	}
};

UVideo6DOFCapturer::UVideo6DOFCapturer()
{
	Init();
}

UVideo6DOFCapturer::UVideo6DOFCapturer( FVTableHelper& Helper )
	: Super(Helper)
{
	Init();
}

void UVideo6DOFCapturer::UpdateScene()
{
	const float fov = 90.0f * (float)PI / 360.0f;
	
	UTextureRenderTarget2D* textureTarget = NewObject< UTextureRenderTarget2D >( this, TEXT( "V6") );
	textureTarget->InitCustomFormat( 1024, 1024, PF_B8G8R8A8, false );
	textureTarget->ClearColor = FLinearColor::Black;

	FIntPoint captureSize( textureTarget->GetSurfaceWidth(), textureTarget->GetSurfaceHeight());

	FSceneInterface* sceneInterface = nullptr;
	FEngineShowFlags showFlags;

	FTextureRenderTargetResource* targetResource = textureTarget->GameThread_GetRenderTargetResource();
	FSceneViewFamilyContext viewFamily( FSceneViewFamily::ConstructionValues( targetResource, sceneInterface, showFlags ).SetResolveScene( false ) );

	FSceneViewStateInterface* viewStateInterface = nullptr;

	FSceneViewInitOptions viewInitOptions;
	viewInitOptions.SetViewRectangle( FIntRect(0, 0, textureTarget->GetSurfaceWidth(), textureTarget->GetSurfaceHeight() ) );
	viewInitOptions.ViewFamily = &viewFamily;
	viewInitOptions.ViewOrigin = m_capturePosition;
	viewInitOptions.BackgroundColor = FLinearColor::Black;
	viewInitOptions.OverrideFarClippingPlaneDistance = 0.0f;
	viewInitOptions.SceneViewStateInterface = viewStateInterface;
    viewInitOptions.StereoPass = eSSP_FULL;

	const FTransform transform( m_captureOrientation );
	viewInitOptions.ViewRotationMatrix = transform.ToInverseMatrixWithScale();

	// swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
	viewInitOptions.ViewRotationMatrix = viewInitOptions.ViewRotationMatrix * FMatrix(
		FPlane( 0, 0, 1, 0),
		FPlane( 1, 0, 0, 0),
		FPlane( 0, 1, 0, 0),
		FPlane( 0, 0, 0, 1) );

	if ( (int32)ERHIZBuffer::IsInverted != 0)
		viewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix( fov, fov, 1.0f, 1.0f, GNearClippingPlane, GNearClippingPlane );
	else
		viewInitOptions.ProjectionMatrix = FPerspectiveMatrix( fov,fov, 1.0f, 1.0f, GNearClippingPlane, GNearClippingPlane );

	FSceneView* view = new FSceneView( viewInitOptions );
	view->bIsSceneCapture = true;
	viewFamily.Views.Add( view );

	FPostProcessSettings postProcessSettings;

	view->StartFinalPostprocessSettings( m_capturePosition );
	view->OverridePostProcessSettings( postProcessSettings, 1.0f );
	view->EndFinalPostprocessSettings( viewInitOptions );

	FSceneRenderer* sceneRenderer = nullptr; //FSceneRenderer::CreateSceneRenderer( &viewFamily, nullptr );
#if 0
	if ( viewFamily.Scene->ShouldUseDeferredRenderer() )
		sceneRenderer = new FDeferredShadingSceneRenderer( &viewFamily, nullptr );
	else
		sceneRenderer = new FForwardShadingSceneRenderer( &viewFamily, nullptr );
#endif

	FTextureRenderTargetResource* textureRenderTarget = textureTarget->GameThread_GetRenderTargetResource();

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		CaptureCommand,
		FSceneRenderer*, sceneRenderer, sceneRenderer,
		FTextureRenderTargetResource*, textureRenderTarget, textureRenderTarget,
	{
		UpdateScene_RenderThread( RHICmdList, sceneRenderer, textureRenderTarget );
	});
}

void UVideo6DOFCapturer::Init()
{
	m_imageWrapperModule = &FModuleManager::LoadModuleChecked< IImageWrapperModule >( FName("ImageWrapper") );

	m_captureComponent = CreateDefaultSubobject< USceneCaptureComponent2D >( TEXT( "captureComponent" ) );
	m_captureComponent->SetVisibility(true);
	m_captureComponent->SetHiddenInGame(false);
	m_captureComponent->CaptureStereoPass = eSSP_FULL;
	m_captureComponent->FOVAngle = 90.0f;
	m_captureComponent->bCaptureEveryFrame = false;
	m_captureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	const FName TargetName = MakeUniqueObjectName( this, UTextureRenderTarget2D::StaticClass(), TEXT( "SceneCaptureTextureTarget" ) );
	m_captureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>( this, TargetName );
	m_captureComponent->TextureTarget->InitCustomFormat( 1024, 1024, PF_B8G8R8A8, false );
	m_captureComponent->TextureTarget->ClearColor = FLinearColor::Black;

#if 1
	const FName DepthTargetName = MakeUniqueObjectName(this, UTextureRenderTarget2D::StaticClass(), TEXT("SceneCaptureDepthTarget"));
	m_captureComponent->DepthTarget = NewObject<UTextureRenderTarget2D>(this, DepthTargetName);
	m_captureComponent->DepthTarget->InitCustomFormat(1024, 1024, PF_B8G8R8A8, false);
	m_captureComponent->DepthTarget->ClearColor = FLinearColor::White;
#endif

	m_captureComponent->RegisterComponentWithWorld( GWorld );

	m_captureComponent->AddToRoot();

	m_state = EVideo6DOFCapturerState::NONE;
}

void UVideo6DOFCapturer::Capture( const FVector& position, const FQuat& orientation )
{
	m_capturePosition = position;
	m_captureOrientation = orientation;
	m_state = EVideo6DOFCapturerState::CAPTURE;
}

void UVideo6DOFCapturer::AddView()
{
	//m_viewExtension = MakeShareable( new FSceneViewExtension );
	//GEngine->ViewExtensions.AddUnique( m_viewExtension );
	if ( GDynamicRHI != &s_dynamicRHIWrap )
	{
		FD3D11DynamicRHI* d3d11DynamicRHI = static_cast< FD3D11DynamicRHI* >( GDynamicRHI );
		s_rhiCommandContextWrap.m_wrapped = d3d11DynamicRHI;
		s_dynamicRHIWrap.m_wrapped = d3d11DynamicRHI;
		s_dynamicRHIWrap.m_commandContext = &s_rhiCommandContextWrap;
		GDynamicRHI = &s_dynamicRHIWrap;
	}
}

void UVideo6DOFCapturer::Tick( float DeltaTime )
{
	switch( m_state )
	{
	case EVideo6DOFCapturerState::NONE:
		return;
	case EVideo6DOFCapturerState::CAPTURE:
		{
			m_captureComponent->SetWorldLocationAndRotation( m_capturePosition, m_captureOrientation );
			m_captureComponent->UpdateContent();
			m_state = EVideo6DOFCapturerState::READBACK;
		}
		break;
	case EVideo6DOFCapturerState::READBACK:
		{
			{
				FTextureRenderTargetResource* renderTarget = m_captureComponent->TextureTarget->GameThread_GetRenderTargetResource();
				renderTarget->ReadPixels( m_colors, FReadSurfaceDataFlags(), FIntRect( 0, 0, 0, 0 ) );
				m_state = EVideo6DOFCapturerState::DONE;

				IImageWrapperPtr imageWrapper = m_imageWrapperModule->CreateImageWrapper( EImageFormat::PNG );
				imageWrapper->SetRaw( m_colors.GetData(), m_colors.GetAllocatedSize(), renderTarget->GetSizeXY().X, renderTarget->GetSizeXY().Y, ERGBFormat::BGRA, 8 );
				const TArray< uint8 >& pngData = imageWrapper->GetCompressed( 100 );
				FFileHelper::SaveArrayToFile( pngData, TEXT( "d:/tmp/ue_color.png" ) );
				imageWrapper.Reset();
			}

#if 1
			{
				FTextureRenderTargetResource* renderTarget = m_captureComponent->DepthTarget->GameThread_GetRenderTargetResource();
				renderTarget->ReadPixels(m_colors, FReadSurfaceDataFlags(), FIntRect(0, 0, 0, 0));
				m_state = EVideo6DOFCapturerState::DONE;

#if DEPTH_32_BIT_CONVERSION
				// DXGI_FORMAT_R32G8X24_TYPELESS
#else
				// DXGI_FORMAT_R24G8_TYPELESS
#endif

				IImageWrapperPtr imageWrapper = m_imageWrapperModule->CreateImageWrapper(EImageFormat::PNG);
				imageWrapper->SetRaw(m_colors.GetData(), m_colors.GetAllocatedSize(), renderTarget->GetSizeXY().X, renderTarget->GetSizeXY().Y, ERGBFormat::BGRA, 8);
				const TArray< uint8 >& pngData = imageWrapper->GetCompressed(100);
				FFileHelper::SaveArrayToFile(pngData, TEXT("d:/tmp/ue_depth.png"));
				imageWrapper.Reset();
			}
#endif

			UE_LOG( LogVideo6DOF, Log, TEXT( "Captured %d pixels" ), m_colors.Num() );
		}
		break;
	case EVideo6DOFCapturerState::DONE:
		break;
	}
}
