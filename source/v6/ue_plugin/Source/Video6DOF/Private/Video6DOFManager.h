// Copyright 2016 Video6DOF.  All rights reserved.

#pragma once

class FVideo6DOFManager
{
public:

	FVideo6DOFManager();

public:

	void Screenshot( const TArray<FString>& Args );

private:

	FAutoConsoleCommand ScreenshotCommand;
};
