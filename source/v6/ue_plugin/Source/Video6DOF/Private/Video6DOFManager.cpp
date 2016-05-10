// Copyright 2016 Video6DOF.  All rights reserved.

#include <v6/core/common.h>
#include "Video6DOFPrivatePCH.h"

FVideo6DOFManager::FVideo6DOFManager()
	: m_screenshotCommand(
		TEXT( "V6.Screenshot" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_ScreenShot", "Takes a V6 screenshot" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::Screenshot ) )
	, m_addViewCommand(
		TEXT( "V6.AddView" ),
		*NSLOCTEXT( "Video6DOF", "CommandText_AddView", "Add a V6 view" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::AddView ) )
	, m_capturer( nullptr )
{
}

void FVideo6DOFManager::CreateCapturer()
{
	if (m_capturer)
	{
		m_capturer->RemoveFromRoot();
		m_capturer = nullptr;
	}

	m_capturer = NewObject< UVideo6DOFCapturer >(UVideo6DOFCapturer::StaticClass());
	m_capturer->AddToRoot();
}

void FVideo6DOFManager::AddView( const TArray<FString>& Args )
{
	CreateCapturer();
	
	m_capturer->AddView();

	UE_LOG( LogVideo6DOF, Log, TEXT( "Add View..." ) );
}

void FVideo6DOFManager::Screenshot( const TArray<FString>& Args )
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

	m_capturer->Capture( position, rotation.Quaternion() );

	UE_LOG( LogVideo6DOF, Log, TEXT( "Screenshot..." ) );
}
