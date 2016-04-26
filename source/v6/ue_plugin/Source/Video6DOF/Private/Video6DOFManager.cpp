// Copyright 2016 Video6DOF.  All rights reserved.

#include "Video6DOFPrivatePCH.h"

FVideo6DOFManager::FVideo6DOFManager()
	: m_screenshotCommand(
		TEXT( "V6.Screenshot" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_ScreenShot", "Takes a V6 screenshot" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::Screenshot ) )
	, m_capturer( nullptr )
{
}

void FVideo6DOFManager::Screenshot( const TArray<FString>& Args )
{
	APlayerController* playerController = UGameplayStatics::GetPlayerController( GWorld, 0 );

	if ( playerController == nullptr )
	{
		UE_LOG( LogVideo6DOF, Warning, TEXT( "No player controller" ) );
		return;
	}

	if ( m_capturer )
	{
		m_capturer->RemoveFromRoot();
		m_capturer = nullptr;
	}

	m_capturer = NewObject< UVideo6DOFCapturer >( UVideo6DOFCapturer::StaticClass() );
	m_capturer->AddToRoot();

	FVector position;
	FRotator rotation;
	playerController->GetPlayerViewPoint( position, rotation );

	m_capturer->Capture( position, rotation.Quaternion() );

	UE_LOG( LogVideo6DOF, Log, TEXT( "Screenshot..." ) );
}
