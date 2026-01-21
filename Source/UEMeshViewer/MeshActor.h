// Simple OBJ loader backed by a ProceduralMeshComponent.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeshActor.generated.h"

class UProceduralMeshComponent;

UCLASS()
class UEMESHVIEWER_API AMeshActor : public AActor
{
	GENERATED_BODY()

public:
	AMeshActor();

	// Load an OBJ file from disk and build a procedural mesh. Returns false on failure.
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	bool LoadFromObjFile(const FString &FilePath);

	// Load a GLB file from disk and build a procedural mesh. Returns false on failure.
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	bool LoadFromGlbFile(const FString &FilePath);

	// Remove any existing mesh section.
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	void ClearMesh();

	const FBox &GetMeshBounds() const { return MeshBounds; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent *MeshComponent;

private:
	struct FObjIndex
	{
		int32 Vertex = INDEX_NONE;
		int32 UV = INDEX_NONE;
		int32 Normal = INDEX_NONE;
	};

	bool ParseFaceVertex(const FString &Token, FObjIndex &OutIndex, int32 VertexCount, int32 UVCount, int32 NormalCount) const;
	bool BuildMesh(const TArray<FVector> &Positions, const TArray<FVector2D> &UVs, const TArray<FVector> &Normals, const TArray<TArray<FObjIndex>> &Faces);
	static int32 ResolveIndex(int32 Index, int32 Count);

	FBox MeshBounds;
};
