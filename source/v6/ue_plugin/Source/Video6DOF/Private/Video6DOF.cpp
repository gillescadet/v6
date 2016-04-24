// Copyright 2016 Video6DOF.  All rights reserved.

#include "Video6DOFPrivatePCH.h"

TSharedPtr< FVideo6DOFManager > Video6DOFManager;

void FVideo6DOFModule::StartupModule()
{
	Video6DOFManager = MakeShareable( new FVideo6DOFManager() );
}

void FVideo6DOFModule::ShutdownModule()
{
	if ( Video6DOFManager.IsValid() )
		Video6DOFManager.Reset();
}

TSharedPtr< FVideo6DOFManager > FVideo6DOFModule::Get()
{
	check( Video6DOFManager.IsValid() );
	return Video6DOFManager;
}

IMPLEMENT_MODULE( FVideo6DOFModule, Video6DOF )
DEFINE_LOG_CATEGORY(LogVideo6DOF);
