// Copyright 2016 Video6DOF.  All rights reserved.

#pragma once

#include "Video6DOFCapturer.generated.h"

class IImageWrapperModule;

UENUM()
enum EVideo6DOFCapturerState
{
	NONE,
	CAPTURE,
	STOP,
	DONE,

	COUNT
};

USTRUCT()
struct FVideo6DOFCaptureSettings
{
	GENERATED_BODY()

	uint32	m_targetFPS;
	uint32	m_sampleCount;
	int32	m_sampleID;
	uint32	m_samplingWidth;
	uint32	m_gridWidth;
	float	m_gridMinScale;
	float	m_gridMaxScale;
	uint32	m_compressionQuality;
	bool	m_movingPointOfView;
	bool	m_useToneMapping;
	bool	m_dumpRenderTarget;
	bool	m_lockCameraForLighting;
};

UCLASS()
class UVideo6DOFCapturer
	: public UObject
	, public FTickableGameObject
{
	GENERATED_BODY()

public:

	UVideo6DOFCapturer();
	UVideo6DOFCapturer( FVTableHelper& Helper );

public:
	static void													Startup();
	static void													Shutdown();

public:

	//~ FTickableGameObject interface

	virtual TStatId												GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT( UVideo6DOFCapturer, STATGROUP_Tickables ); }
	virtual UWorld*												GetTickableGameObjectWorld() const override { return GWorld; }
	virtual bool												IsTickable() const { return true; }
	virtual bool												IsTickableWhenPaused() const { return false; }
	virtual void												Tick( float DeltaTime ) override;

public:
	
	void														Capture( const FVector& position, const FQuat& orientation, uint32 frameCount, const FVideo6DOFCaptureSettings* settings );
	void														Stop();

private:
	
	void														Init();

private:

	IImageWrapperModule*										m_imageWrapperModule;
	FVideo6DOFCaptureSettings									m_captureSettings;
	EVideo6DOFCapturerState										m_state;
	FVector														m_capturePosition;
	FQuat														m_captureOrientation;
	uint32														m_captureFrameID;
	uint32														m_captureFrameEncodedCount;
	uint32														m_captureFrameTotalCount;
	TArray< FColor >											m_colors;
};
