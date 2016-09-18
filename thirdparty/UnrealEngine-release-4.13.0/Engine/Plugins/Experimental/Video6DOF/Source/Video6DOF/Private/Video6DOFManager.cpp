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
	, m_sampleIDCommand(
		TEXT( "V6.sampleID" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_SampleID", "Set the unique sample ID used for the capture of a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureSampleID ) )
	, m_samplingWidthCommand(
		TEXT( "V6.samplingWidth" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_SamplingWidth", "Set the resolution of the V6 capture" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureSamplingWidth ) )
	, m_gridWidthCommand(
		TEXT( "V6.gridWidth" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_GridWidth", "Set the resolution of the grid used for the capture of a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureGridWidth ) )
	, m_gridMinScaleCommand(
		TEXT( "V6.gridMinScale" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_MinScale", "Set the minimum distance from the camera to capture a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureGridMinScale ) )
	, m_gridMaxScaleCommand(
		TEXT( "V6.gridMaxScale" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_MaxScale", "Set the maximum distance from the camera to capture a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureGridMaxScale ) )
	, m_compressionQualityCommand(
		TEXT( "V6.compressionQuality" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_CompressionQuality", "Set the compression quality used to encode the V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureCompressionQuality ) )
	, m_movingPointOfViewCommand(
		TEXT( "V6.movingPointOfView" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_MovingPointOfView", "Allows the point of view to move while capturing a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureMovingPointOfView ) )
	, m_useToneMappingCommand(
		TEXT( "V6.useToneMapping" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_UseToneMapping", "Use tone mapping for the capture a V6 sequence" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureUseToneMapping ) )
	, m_dumpRenderTargetCommand(
		TEXT( "V6.dumpRenderTarget" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_DumpRenderTarget", "Dump each captured render target" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureDumpRenderTarget ) )
	, m_lockCameraForLightingCommand(
		TEXT( "V6.lockCameraForLighting" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_LockCameraForLighting", "Lock the camera for lighting" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::SetCaptureLockCameraForLighting ) )
	, m_capturer( nullptr )
	, m_fps( 75 )
	, m_sampleCount( 17 )
	, m_sampleID( -1 )
	, m_samplingWidth( 2048 )
	, m_gridWidth( 1200 )
	, m_gridMinScale( 20.0f )
	, m_gridMaxScale( 100000.0f )
	, m_compressionQuality( 1 )
	, m_movingPointOfView( false )
	, m_useToneMapping( true )
	, m_dumpRenderTarget( false )
	, m_lockCameraForLighting( true )
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

bool FVideo6DOFManager::Capture( uint32 frameCount )
{
	APlayerController* playerController = UGameplayStatics::GetPlayerController( GWorld, 0 );

	if ( playerController == nullptr )
	{
		UE_LOG( LogVideo6DOF, Warning, TEXT( "No player controller" ) );
		return false;
	}

	if ( m_dumpRenderTarget && frameCount > 1 )
	{
		UE_LOG( LogVideo6DOF, Warning, TEXT( "Dump render target could only be used for one frame" ) );
		return false;
	}

	if ( m_sampleID == -2 && frameCount > 1 )
	{
		UE_LOG( LogVideo6DOF, Warning, TEXT( "Sample ID -2 could only be used for one frame" ) );
		return false;
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
	settings.m_sampleID = m_sampleID;
	settings.m_samplingWidth = m_samplingWidth;
	settings.m_gridWidth = m_gridWidth;
	settings.m_gridMinScale = m_gridMinScale;
	settings.m_gridMaxScale = m_gridMaxScale;
	settings.m_compressionQuality = m_compressionQuality;
	settings.m_movingPointOfView = m_movingPointOfView;
	settings.m_useToneMapping = m_useToneMapping;
	settings.m_dumpRenderTarget = m_dumpRenderTarget;
	settings.m_lockCameraForLighting = m_lockCameraForLighting;

	m_capturer->Capture( position, rotation.Quaternion(), frameCount, &settings );

	return true;
}

void FVideo6DOFManager::CaptureScreenshot( const TArray<FString>& Args )
{
	if ( Capture( 1 ) )
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

	if ( Capture( frameCount ) )
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

void FVideo6DOFManager::SetCaptureSampleID( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need sample ID as argument." ) );
		return;
	}

	const int32 sampleID = FCString::Atoi( *Args[0] );

	if ( sampleID < -2 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Sample ID should be -2 or -1 or >= 0." ) );
		return;
	}

	m_sampleID = sampleID;
}

void FVideo6DOFManager::SetCaptureSamplingWidth( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need sampling width as argument." ) );
		return;
	}

	const int32 samplingWidth = FCString::Atoi( *Args[0] );

	if ( samplingWidth <= 0 || (samplingWidth & 7) != 0 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Sampling width should be >= 0 and a multiple of 8." ) );
		return;
	}

	m_samplingWidth = samplingWidth;
}

void FVideo6DOFManager::SetCaptureGridWidth( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need grid width as argument." ) );
		return;
	}

	const int32 gridWidth = FCString::Atoi( *Args[0] );

	if ( gridWidth <= 0 || (gridWidth & 7) != 0 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Grid width should be >= 0 and a multiple of 8." ) );
		return;
	}

	m_gridWidth = gridWidth;
}

void FVideo6DOFManager::SetCaptureGridMinScale( const TArray<FString>& Args )
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

void FVideo6DOFManager::SetCaptureGridMaxScale( const TArray<FString>& Args )
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

void FVideo6DOFManager::SetCaptureCompressionQuality( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need 0 or 1 as argument." ) );
		return;
	}

	const uint32 compressionQuality = FCString::Atoi( *Args[0] ) != 0;

	m_compressionQuality = compressionQuality;
}

void FVideo6DOFManager::SetCaptureMovingPointOfView( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need 0 or 1 as argument." ) );
		return;
	}

	const bool movingPointOfView = FCString::Atoi( *Args[0] ) != 0;

	m_movingPointOfView = movingPointOfView;
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

void FVideo6DOFManager::SetCaptureDumpRenderTarget( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need 0 or 1 as argument." ) );
		return;
	}

	const bool dumpRenderTarget = FCString::Atoi( *Args[0] ) != 0;

	m_dumpRenderTarget = dumpRenderTarget;
}

void FVideo6DOFManager::SetCaptureLockCameraForLighting( const TArray<FString>& Args )
{
	if ( Args.Num() < 1 )
	{
		UE_LOG( LogVideo6DOF, Error, TEXT( "Need 0 or 1 as argument." ) );
		return;
	}

	const bool lockCameraForLighting = FCString::Atoi( *Args[0] ) != 0;

	m_lockCameraForLighting = lockCameraForLighting;
}
