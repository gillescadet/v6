// Copyright 2016 Vide6DOF.  All rights reserved.

#pragma once

class FVideo6DOFManager;

class FVideo6DOFModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:

	static TSharedPtr< FVideo6DOFManager > Get();
};
