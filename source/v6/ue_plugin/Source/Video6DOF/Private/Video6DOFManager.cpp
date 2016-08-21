// Copyright 2016 Video6DOF.  All rights reserved.

#include <v6/core/common.h>
#include "Video6DOFPrivatePCH.h"

FVideo6DOFManager::FVideo6DOFManager()
	: m_screenshotCommand(
		TEXT( "V6.screenshot" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_ScreenShot", "Captures a V6 screenshot" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::CaptureScreenshot ) )
	, m_sequenceCommand(
		TEXT( "V6.sequence" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_Sequence", "Captures a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::CaptureSequence ) )
	, m_stopCommand(
		TEXT( "V6.stop" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_Stop", "Stops a V6 capture" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::CaptureStop ) )
	, m_fpsCommand(
		TEXT( "V6.fps" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_FPS", "Set the FPS for the capture of a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureFPS ) )
	, m_sampleCountCommand(
		TEXT( "V6.sampleCount" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_SampleCount", "Set the number of samples used for the capture of a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureSampleCount ) )
	, m_gridMacroShiftCommand(
		TEXT( "V6.gridMacroShift" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_SampleCount", "Set the power of 2 of the resolution of the macro grid used for the capture of a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureSetGridMacroShift ) )
	, m_gridMinScaleCommand(
		TEXT( "V6.gridMinScale" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_SampleCount", "Set the minimum distance from the camera to capture a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureSetGridMinScale ) )
	, m_gridMaxScaleCommand(
		TEXT( "V6.gridMaxScale" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_SampleCount", "Set the maximum distance from the camera to capture a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureSetGridMaxScale ) )
	, m_useToneMappingCommand(
		TEXT( "V6.useToneMapping" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_UseToneMapping", "Use tone mapping for the capture a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureUseToneMapping ) )
	, m_capturer( nullptr )
	, m_fps( 75 )
	, m_sampleCount( 17 )
	, m_gridMacroShift( 9 )
	, m_gridMinScale( 50.0f )
	, m_gridMaxScale( 2000.0f )
	, m_useToneMapping( true )
{
	UVideo6DOFCapturer::Startup();
}

FVideo6DOFManager::~FVideo6DOFManager()
{
	UVideo6DOFCapturer::Shutdown();
}

void FVideo6DOFManager::CreateCapturer()
{
	if ( m_capturer )
	{
		m_capturer->RemoveFromRoot();
		m_capturer = nullptr;
	}

	m_capturer = NewObject< UVideo6DOFCapturer >(UVideo6DOFCapturer::StaticClass());
	m_capturer->AddToRoot();
}

void FVideo6DOFManager::Capture( uint32 frameCount )
{
	APlayerController* playerController = UGameplayStatics::GetPlayerController( GWorld, 0 );

	if ( playerController == nullptr )
	{
		UE_LOG( LogVideo6DOF, Warning, TEXT( "No player controller" ) );
		return;
	}
	
	CreateCapturer();

	FVector position;
	FRotator rotation;
	playerController->GetPlayerViewPoint( position, rotation );

	/*
	FPostProcessSettings postProcessSettings;
	
	uint32 pawnCount = 0;
	for ( TActorIterator< AVideo6DOFPawn > actorItr( playerController->GetWorld() ); actorItr; ++actorItr, ++pawnCount )
	{
		if ( pawnCount == 0 )
		{
			AVideo6DOFPawn* pawn = *actorItr;
			postProcessSettings = pawn->PostProcessSettings;
		}
		else
		{
			UE_LOG( LogVideo6DOF, Warning, TEXT( "Only one Video6DOF pawn should be instanced." ) );
			break;
		}
	}
	*/

	FVideo6DOFCaptureSettings settings = {};
	settings.m_targetFPS = m_fps;
	settings.m_sampleCount = m_sampleCount;
	settings.m_gridMacroShift = m_gridMacroShift;
	settings.m_gridMinScale = m_gridMinScale;
	settings.m_gridMaxScale = m_gridMaxScale;
	settings.m_useToneMapping = m_useToneMapping;

	m_capturer->Capture( position, rotation.Quaternion(), frameCount, &settings );
}

void FVideo6DOFManager::CaptureScreenshot( const TArray<FString>& Args )
{
	Capture( 1 );
	UE_LOG( LogVideo6DOF, Log, TEXT( "Screenshot..." ) );
}

void FVideo6DOFManager::CaptureSequence( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need frame count as argument." ) );
		return;
	}

	const int32 frameCount = FCString::Atoi( *Args[0] );

	if ( frameCount < 0 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Frame count should greater than 0." ) );
		return;
	}

	Capture( frameCount );
	UE_LOG( LogVideo6DOF, Log, TEXT( "Sequence..." ) );
}

void FVideo6DOFManager::CaptureStop(  const TArray<FString>& Args )
{
	m_capturer->Stop();

	UE_LOG( LogVideo6DOF, Log, TEXT( "Stop..." ) );
}

void FVideo6DOFManager::SetCaptureFPS( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need FPS as argument." ) );
		return;
	}

	const int32 fps = FCString::Atoi( *Args[0] );

	if ( fps != 75 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "FPS should be 75." ) );
		return;
	}

	m_fps = fps;
}

void FVideo6DOFManager::SetCaptureSampleCount( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need sample count as argument." ) );
		return;
	}

	const int32 sampleCount = FCString::Atoi( *Args[0] );

	if ( sampleCount < 1 || sampleCount > 64 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Sample count should be >= 1 and <= 64." ) );
		return;
	}

	m_sampleCount = sampleCount;
}

void FVideo6DOFManager::SetCaptureSetGridMacroShift( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need grid macro shift as argument." ) );
		return;
	}

	const int32 gridMacroShift = FCString::Atoi( *Args[0] );

	if ( gridMacroShift < 1 || gridMacroShift > 9 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Grid macro shift should be >= 1 and <= 9." ) );
		return;
	}

	m_gridMacroShift = gridMacroShift;
}

void FVideo6DOFManager::SetCaptureSetGridMinScale( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need grid min scale as argument." ) );
		return;
	}

	const float gridMinScale = FCString::Atof( *Args[0] );

	if ( gridMinScale <= 0.0f || gridMinScale >= m_gridMaxScale )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Grid min scale should be > 0 and <= grid max scale" ) );
		return;
	}

	m_gridMinScale = gridMinScale;
}

void FVideo6DOFManager::SetCaptureSetGridMaxScale( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need grid max scale as argument." ) );
		return;
	}

	const float gridMaxScale = FCString::Atof( *Args[0] );

	if ( gridMaxScale <= m_gridMinScale )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Grid max scale should be > grid min scale" ) );
		return;
	}

	m_gridMaxScale = gridMaxScale;
}

void FVideo6DOFManager::SetCaptureUseToneMapping( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need 0 or 1 as argument." ) );
		return;
	}

	const bool useToneMapping = FCString::Atoi( *Args[0] ) != 0;

	m_useToneMapping = useToneMapping;
}
