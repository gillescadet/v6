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
	void					SetCaptureFPS( const TArray<FString>& Args );
	void					SetCaptureSampleCount( const TArray<FString>& Args );
	void					SetCaptureSampleID( const TArray<FString>& Args );
	void					SetCaptureSetGridMacroShift( const TArray<FString>& Args );
	void					SetCaptureSetGridMinScale( const TArray<FString>& Args );
	void					SetCaptureSetGridMaxScale( const TArray<FString>& Args );
	void					SetCaptureUseToneMapping( const TArray<FString>& Args );
	void					SetCaptureDumpRenderTarget( const TArray<FString>& Args );
	void					SetCaptureLockCameraForLighting( const TArray<FString>& Args );

public:
	UPROPERTY()
	class					UVideo6DOFCapturer*	m_capturer;

private:
	bool					Capture( uint32 frameCount );
private:

	FAutoConsoleCommand		m_screenshotCommand;
	FAutoConsoleCommand		m_sequenceCommand;
	FAutoConsoleCommand		m_stopCommand;
	FAutoConsoleCommand		m_fpsCommand;
	FAutoConsoleCommand		m_sampleCountCommand;
	FAutoConsoleCommand		m_sampleIDCommand;
	FAutoConsoleCommand		m_gridMacroShiftCommand;
	FAutoConsoleCommand		m_gridMinScaleCommand;
	FAutoConsoleCommand		m_gridMaxScaleCommand;
	FAutoConsoleCommand		m_useToneMappingCommand;
	FAutoConsoleCommand		m_dumpRenderTargetCommand;
	FAutoConsoleCommand		m_lockCameraForLightingCommand;

	uint32					m_fps;
	uint32					m_sampleCount;
	int32					m_sampleID;
	uint32					m_gridMacroShift;
	float					m_gridMinScale;
	float					m_gridMaxScale;
	bool					m_useToneMapping;
	bool					m_dumpRenderTarget;
	bool					m_lockCameraForLighting;
};
