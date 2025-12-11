#include "ObjViewerGameMode.h"

#include "ObjMeshActor.h"
#include "UEMeshViewer.h"

#include "DesktopPlatformModule.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

AObjViewerGameMode::AObjViewerGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	bStartPlayersAsSpectators = true;
}

void AObjViewerGameMode::BeginPlay()
{
	Super::BeginPlay();

	const FString ObjPath = ResolveObjPath();
	if (ObjPath.IsEmpty())
	{
		UE_LOG(LogUEMeshViewer, Warning, TEXT("No OBJ selected; viewer will remain empty."));
		return;
	}

	UWorld *World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AObjMeshActor *MeshActor = World->SpawnActor<AObjMeshActor>(AObjMeshActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!MeshActor)
	{
		UE_LOG(LogUEMeshViewer, Error, TEXT("Failed to spawn OBJ mesh actor."));
		return;
	}

	if (!MeshActor->LoadFromObjFile(ObjPath))
	{
		UE_LOG(LogUEMeshViewer, Error, TEXT("Failed to load OBJ at %s"), *ObjPath);
		return;
	}

	PositionSpectator(MeshActor->GetMeshBounds());
}

FString AObjViewerGameMode::ResolveObjPath() const
{
	IDesktopPlatform *DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		UE_LOG(LogUEMeshViewer, Warning, TEXT("DesktopPlatform module unavailable; cannot open file dialog."));
		return FString();
	}

	const void *SlateHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		SlateHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	}
	void *ParentWindowHandle = const_cast<void *>(SlateHandle);

	const FString DialogTitle = TEXT("Select OBJ file");
	const FString DefaultPath = FPaths::ProjectDir();
	const FString FileTypes = TEXT("OBJ Files|*.obj|All Files|*.*");
	TArray<FString> OutFiles;

	const bool bPicked = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		DialogTitle,
		DefaultPath,
		TEXT(""),
		FileTypes,
		EFileDialogFlags::None,
		OutFiles);

	if (bPicked && OutFiles.Num() > 0)
	{
		return OutFiles[0];
	}

	UE_LOG(LogUEMeshViewer, Warning, TEXT("OBJ selection canceled or failed."));
	return FString();
}

void AObjViewerGameMode::PositionSpectator(const FBox &Bounds) const
{
	APlayerController *PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	APawn *Pawn = PC->GetPawn();
	if (!Pawn)
	{
		Pawn = PC->GetPawnOrSpectator();
	}

	const FVector Center = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();
	const float Radius = FMath::Max3(Extent.X, Extent.Y, Extent.Z);

	const FVector Offset(Radius * 0.5f, Radius * 1.25f, Radius * 0.75f + 50.0f);
	const FVector DesiredLocation = Center + Offset;

	if (Pawn)
	{
		Pawn->SetActorLocation(DesiredLocation);
	}

	const FRotator ViewRotation = (Center - DesiredLocation).Rotation();
	PC->SetControlRotation(ViewRotation);
}
