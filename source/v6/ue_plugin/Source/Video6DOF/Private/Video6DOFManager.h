// Copyright 2016 Video6DOF.  All rights reserved.

#pragma once

class FVideo6DOFManager
{
public:

							FVideo6DOFManager();
	virtual					~FVideo6DOFManager();

public:
	void					CreateCapturer();
	void					CaptureScreenshot( const TArray<FString>& Args );
	void					CaptureSequence( const TArray<FString>& Args );
	void					CaptureStop( const TArray<FString>& Args );

public:
	UPROPERTY()
	class					UVideo6DOFCapturer*	m_capturer;

private:
	void					Capture( uint32 frameCount );
private:

	FAutoConsoleCommand		m_screenshotCommand;
	FAutoConsoleCommand		m_sequenceCommand;
	FAutoConsoleCommand		m_stopCommand;
};
