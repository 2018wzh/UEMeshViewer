#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ObjViewerGameMode.generated.h"

/** Game mode that spawns a spectator pawn and loads an OBJ at startup. */
UCLASS()
class UEMESHVIEWER_API AObjViewerGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AObjViewerGameMode();

protected:
    virtual void BeginPlay() override;

private:
    FString ResolveObjPath() const;
    void PositionSpectator(const FBox &Bounds) const;
};
