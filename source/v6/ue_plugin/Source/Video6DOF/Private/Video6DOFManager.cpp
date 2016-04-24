// Copyright 2016 Video6DOF.  All rights reserved.

#include "Video6DOFPrivatePCH.h"

FVideo6DOFManager::FVideo6DOFManager()
	: ScreenshotCommand(
			TEXT( "V6.Screenshot" ),
			*NSLOCTEXT( "Video6DOF", "CommandText_ScreenShot", "Takes a V6 screenshot" ).ToString(),
			FConsoleCommandWithArgsDelegate::CreateRaw( this, &FVideo6DOFManager::Screenshot ) )
{
}

void FVideo6DOFManager::Screenshot( const TArray<FString>& Args )
{
	UE_LOG( LogVideo6DOF, Log, TEXT( "V6 Screenshot" ) );
}
