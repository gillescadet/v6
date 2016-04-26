// Copyright 2016 Video6DOF.  All rights reserved.

#include "Video6DOFPrivatePCH.h"

UVideo6DOFCapturer::UVideo6DOFCapturer()
{
	Init();
}

UVideo6DOFCapturer::UVideo6DOFCapturer( FVTableHelper& Helper )
	: Super(Helper)
{
	Init();
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
			FTextureRenderTargetResource* renderTarget = m_captureComponent->TextureTarget->GameThread_GetRenderTargetResource();
			renderTarget->ReadPixels( m_colors, FReadSurfaceDataFlags(), FIntRect( 0, 0, 0, 0 ) );
			m_state = EVideo6DOFCapturerState::DONE;

			IImageWrapperPtr imageWrapper = m_imageWrapperModule->CreateImageWrapper( EImageFormat::PNG );
			imageWrapper->SetRaw( m_colors.GetData(), m_colors.GetAllocatedSize(), renderTarget->GetSizeXY().X, renderTarget->GetSizeXY().Y, ERGBFormat::BGRA, 8 );
			const TArray< uint8 >& pngData = imageWrapper->GetCompressed( 100 );
			FFileHelper::SaveArrayToFile( pngData, TEXT( "d:/tmp/ue.png" ) );
			imageWrapper.Reset();

			UE_LOG( LogVideo6DOF, Log, TEXT( "Captured %d pixels" ), m_colors.Num() );
		}
		break;
	case EVideo6DOFCapturerState::DONE:
		break;
	}
}
