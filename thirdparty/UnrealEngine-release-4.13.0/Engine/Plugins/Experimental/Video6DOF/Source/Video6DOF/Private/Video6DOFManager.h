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
	void					SetCaptureSamplingWidth( const TArray<FString>& Args );
	void					SetCaptureGridWidth( const TArray<FString>& Args );
	void					SetCaptureGridMinScale( const TArray<FString>& Args );
	void					SetCaptureGridMaxScale( const TArray<FString>& Args );
	void					SetCaptureCompressionQuality( const TArray<FString>& Args );
	void					SetCaptureMovingPointOfView( const TArray<FString>& Args );
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
	FAutoConsoleCommand		m_samplingWidthCommand;
	FAutoConsoleCommand		m_gridWidthCommand;
	FAutoConsoleCommand		m_gridMinScaleCommand;
	FAutoConsoleCommand		m_gridMaxScaleCommand;
	FAutoConsoleCommand		m_compressionQualityCommand;
	FAutoConsoleCommand		m_movingPointOfViewCommand;
	FAutoConsoleCommand		m_useToneMappingCommand;
	FAutoConsoleCommand		m_dumpRenderTargetCommand;
	FAutoConsoleCommand		m_lockCameraForLightingCommand;

	uint32					m_fps;
	uint32					m_sampleCount;
	int32					m_sampleID;
	uint32					m_samplingWidth;
	uint32					m_gridWidth;
	float					m_gridMinScale;
	float					m_gridMaxScale;
	uint32					m_compressionQuality;
	bool					m_movingPointOfView;
	bool					m_useToneMapping;
	bool					m_dumpRenderTarget;
	bool					m_lockCameraForLighting;
};
