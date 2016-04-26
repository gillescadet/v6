// Copyright 2016 Vide6DOF.  All rights reserved.

#pragma once

#include "GameFramework/Pawn.h"
#include "LatentActions.h"
#include "Video6DOFPawn.generated.h"

UCLASS( config = Game, Blueprintable, BlueprintType )
class AVideo6DOFPawn
	: public ADefaultPawn
{
    GENERATED_BODY()

public:

    UPROPERTY( VisibleAnywhere, BlueprintReadOnly, Category = Pawn )
	UTexture2D* OutputImage;
};
