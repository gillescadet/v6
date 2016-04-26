// Copyright 2016 Video6DOF.  All rights reserved.

#pragma once

class FVideo6DOFManager
{
public:

	FVideo6DOFManager();

public:

	void Screenshot( const TArray<FString>& Args );

public:
	UPROPERTY()
	class UVideo6DOFCapturer*	m_capturer;

private:

	FAutoConsoleCommand			m_screenshotCommand;
};
