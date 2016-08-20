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
	virtual bool												IsTickable() const { return true; }
	virtual bool												IsTickableWhenPaused() const { return false; }
	virtual void												Tick( float DeltaTime ) override;

public:
	
	void														Capture( const FVector& position, const FQuat& orientation, uint32 frameCount );
	void														Stop();

private:
	
	void														Init();

private:

	IImageWrapperModule*										m_imageWrapperModule;
	EVideo6DOFCapturerState										m_state;
	FVector														m_capturePosition;
	FQuat														m_captureOrientation;
	uint32														m_captureFrameID;
	uint32														m_captureFrameEncodedCount;
	uint32														m_captureFrameTotalCount;
	TArray< FColor >											m_colors;
};
