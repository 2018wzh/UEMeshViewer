#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MeshViewerGameMode.generated.h"

/** Game mode that spawns a spectator pawn and loads an OBJ at startup. */
UCLASS()
class UEMESHVIEWER_API AMeshViewerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMeshViewerGameMode();

protected:
	virtual void BeginPlay() override;

private:
	FString ResolveObjPath() const;
	void PositionSpectator(const FBox &Bounds) const;
};
