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
	, m_capturer( nullptr )
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

	m_capturer->Capture( position, rotation.Quaternion(), frameCount );
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
