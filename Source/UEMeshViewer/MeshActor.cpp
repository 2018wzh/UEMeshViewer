#include "MeshActor.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include "KismetProceduralMeshLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UEMeshViewer.h"

AMeshActor::AMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);

	MeshComponent->bUseAsyncCooking = true;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	MeshComponent->SetGenerateOverlapEvents(false);
}

void AMeshActor::ClearMesh()
{
	MeshComponent->ClearAllMeshSections();
	MeshBounds = FBox(EForceInit::ForceInit);
}

bool AMeshActor::LoadFromObjFile(const FString &FilePath)
{
	ClearMesh();

	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogUEMeshViewer, Error, TEXT("OBJ file not found: %s"), *FilePath);
		return false;
	}

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		UE_LOG(LogUEMeshViewer, Error, TEXT("Failed to read OBJ: %s"), *FilePath);
		return false;
	}

	TArray<FVector> Positions;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<TArray<FObjIndex>> Faces;

	for (const FString &RawLine : Lines)
	{
		FString Line = RawLine.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		TArray<FString> Tokens;
		Line.ParseIntoArray(Tokens, TEXT(" "), true);
		if (Tokens.Num() == 0)
		{
			continue;
		}

		const FString &Head = Tokens[0];
		if (Head == TEXT("v"))
		{
			if (Tokens.Num() < 4)
			{
				UE_LOG(LogUEMeshViewer, Warning, TEXT("Malformed vertex line: %s"), *Line);
				continue;
			}

			const float X = FCString::Atof(*Tokens[1]);
			const float Y = FCString::Atof(*Tokens[2]);
			const float Z = FCString::Atof(*Tokens[3]);
			Positions.Add(FVector(X, Y, Z));
		}
		else if (Head == TEXT("vt"))
		{
			if (Tokens.Num() < 3)
			{
				UE_LOG(LogUEMeshViewer, Warning, TEXT("Malformed UV line: %s"), *Line);
				continue;
			}

			const float U = FCString::Atof(*Tokens[1]);
			const float V = FCString::Atof(*Tokens[2]);
			UVs.Add(FVector2D(U, 1.0f - V));
		}
		else if (Head == TEXT("vn"))
		{
			if (Tokens.Num() < 4)
			{
				UE_LOG(LogUEMeshViewer, Warning, TEXT("Malformed normal line: %s"), *Line);
				continue;
			}

			const float X = FCString::Atof(*Tokens[1]);
			const float Y = FCString::Atof(*Tokens[2]);
			const float Z = FCString::Atof(*Tokens[3]);
			Normals.Add(FVector(X, Y, Z));
		}
		else if (Head == TEXT("f"))
		{
			if (Tokens.Num() < 4)
			{
				UE_LOG(LogUEMeshViewer, Warning, TEXT("Malformed face line: %s"), *Line);
				continue;
			}

			TArray<FObjIndex> Face;
			for (int32 i = 1; i < Tokens.Num(); ++i)
			{
				FObjIndex Index;
				if (ParseFaceVertex(Tokens[i], Index, Positions.Num(), UVs.Num(), Normals.Num()))
				{
					Face.Add(Index);
				}
			}

			if (Face.Num() >= 3)
			{
				Faces.Add(Face);
			}
		}
	}

	if (Positions.Num() == 0)
	{
		UE_LOG(LogUEMeshViewer, Error, TEXT("OBJ contains no vertex data: %s"), *FilePath);
		return false;
	}

	return BuildMesh(Positions, UVs, Normals, Faces);
}

bool AMeshActor::LoadFromGlbFile(const FString &FilePath)
{
	ClearMesh();

	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogUEMeshViewer, Error, TEXT("GLB file not found: %s"), *FilePath);
		return false;
	}

	tinygltf::Model Model;
	tinygltf::TinyGLTF Loader;
	std::string Err;
	std::string Warn;

	bool Ret = Loader.LoadBinaryFromFile(&Model, &Err, &Warn, TCHAR_TO_UTF8(*FilePath));

	if (!Warn.empty())
	{
		UE_LOG(LogUEMeshViewer, Warning, TEXT("GLB Warning: %s"), UTF8_TO_TCHAR(Warn.c_str()));
	}

	if (!Err.empty())
	{
		UE_LOG(LogUEMeshViewer, Error, TEXT("GLB Error: %s"), UTF8_TO_TCHAR(Err.c_str()));
	}

	if (!Ret)
	{
		return false;
	}

	int32 SectionIndex = 0;
	FBox TotalBounds(EForceInit::ForceInit);

	for (const auto &Mesh : Model.meshes)
	{
		for (const auto &Primitive : Mesh.primitives)
		{
			if (Primitive.mode != TINYGLTF_MODE_TRIANGLES)
			{
				continue;
			}

			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FProcMeshTangent> Tangents;

			// Extract Positions
			if (Primitive.attributes.count("POSITION") > 0)
			{
				const tinygltf::Accessor &Accessor = Model.accessors[Primitive.attributes.at("POSITION")];
				const tinygltf::BufferView &BufferView = Model.bufferViews[Accessor.bufferView];
				const tinygltf::Buffer &Buffer = Model.buffers[BufferView.buffer];
				const float *Data = reinterpret_cast<const float *>(&Buffer.data[BufferView.byteOffset + Accessor.byteOffset]);

				for (size_t i = 0; i < Accessor.count; ++i)
				{
					// GLTF: X(right), Y(up), Z(forward) -> UE: X(forward), Y(right), Z(up)
					// Conversion: UE.X = -GLTF.Z, UE.Y = GLTF.X, UE.Z = GLTF.Y
					Vertices.Add(FVector(-Data[i * 3 + 2] * 100.0f, Data[i * 3] * 100.0f, Data[i * 3 + 1] * 100.0f));
				}
			}

			// Extract Normals
			if (Primitive.attributes.count("NORMAL") > 0)
			{
				const tinygltf::Accessor &Accessor = Model.accessors[Primitive.attributes.at("NORMAL")];
				const tinygltf::BufferView &BufferView = Model.bufferViews[Accessor.bufferView];
				const tinygltf::Buffer &Buffer = Model.buffers[BufferView.buffer];
				const float *Data = reinterpret_cast<const float *>(&Buffer.data[BufferView.byteOffset + Accessor.byteOffset]);

				for (size_t i = 0; i < Accessor.count; ++i)
				{
					Normals.Add(FVector(-Data[i * 3 + 2], Data[i * 3], Data[i * 3 + 1]));
				}
			}

			// Extract UVs
			if (Primitive.attributes.count("TEXCOORD_0") > 0)
			{
				const tinygltf::Accessor &Accessor = Model.accessors[Primitive.attributes.at("TEXCOORD_0")];
				const tinygltf::BufferView &BufferView = Model.bufferViews[Accessor.bufferView];
				const tinygltf::Buffer &Buffer = Model.buffers[BufferView.buffer];
				const float *Data = reinterpret_cast<const float *>(&Buffer.data[BufferView.byteOffset + Accessor.byteOffset]);

				for (size_t i = 0; i < Accessor.count; ++i)
				{
					UVs.Add(FVector2D(Data[i * 2], Data[i * 2 + 1]));
				}
			}

			// Extract Indices
			if (Primitive.indices >= 0)
			{
				const tinygltf::Accessor &Accessor = Model.accessors[Primitive.indices];
				const tinygltf::BufferView &BufferView = Model.bufferViews[Accessor.bufferView];
				const tinygltf::Buffer &Buffer = Model.buffers[BufferView.buffer];

				if (Accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					const uint32 *Data = reinterpret_cast<const uint32 *>(&Buffer.data[BufferView.byteOffset + Accessor.byteOffset]);
					for (size_t i = 0; i < Accessor.count; ++i)
					{
						Triangles.Add(Data[i]);
					}
				}
				else if (Accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					const uint16 *Data = reinterpret_cast<const uint16 *>(&Buffer.data[BufferView.byteOffset + Accessor.byteOffset]);
					for (size_t i = 0; i < Accessor.count; ++i)
					{
						Triangles.Add(Data[i]);
					}
				}
				else if (Accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					const uint8 *Data = reinterpret_cast<const uint8 *>(&Buffer.data[BufferView.byteOffset + Accessor.byteOffset]);
					for (size_t i = 0; i < Accessor.count; ++i)
					{
						Triangles.Add(Data[i]);
					}
				}
			}

			if (Vertices.Num() > 0)
			{
				if (Normals.Num() == 0)
				{
					Normals.Init(FVector::ZeroVector, Vertices.Num());
					UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
				}

				MeshComponent->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, {}, Tangents, true);
				MeshComponent->SetMeshSectionVisible(SectionIndex, true);

				for (const FVector &V : Vertices)
				{
					TotalBounds += V;
				}

				SectionIndex++;
			}
		}
	}

	MeshBounds = TotalBounds;
	return SectionIndex > 0;
}

bool AMeshActor::ParseFaceVertex(const FString &Token, FObjIndex &OutIndex, int32 VertexCount, int32 UVCount, int32 NormalCount) const
{
	TArray<FString> Parts;
	Token.ParseIntoArray(Parts, TEXT("/"), false);

	if (Parts.Num() < 1 || Parts.Num() > 3)
	{
		UE_LOG(LogUEMeshViewer, Warning, TEXT("Unsupported face token: %s"), *Token);
		return false;
	}

	const int32 V = FCString::Atoi(*Parts[0]);
	OutIndex.Vertex = ResolveIndex(V, VertexCount);

	if (Parts.Num() >= 2 && !Parts[1].IsEmpty())
	{
		const int32 VT = FCString::Atoi(*Parts[1]);
		OutIndex.UV = ResolveIndex(VT, UVCount);
	}

	if (Parts.Num() == 3 && !Parts[2].IsEmpty())
	{
		const int32 VN = FCString::Atoi(*Parts[2]);
		OutIndex.Normal = ResolveIndex(VN, NormalCount);
	}

	return OutIndex.Vertex != INDEX_NONE;
}

int32 AMeshActor::ResolveIndex(int32 Index, int32 Count)
{
	if (Index > 0)
	{
		return Index - 1;
	}

	if (Index < 0)
	{
		return Count + Index;
	}

	return INDEX_NONE;
}

bool AMeshActor::BuildMesh(const TArray<FVector> &Positions, const TArray<FVector2D> &UVs, const TArray<FVector> &Normals, const TArray<TArray<FObjIndex>> &Faces)
{
	TArray<FVector> FinalVertices;
	TArray<int32> Triangles;
	TArray<FVector> FinalNormals;
	TArray<FVector2D> FinalUVs;
	TArray<FProcMeshTangent> Tangents;

	FinalVertices.Reserve(Faces.Num() * 3);
	FinalNormals.Reserve(Faces.Num() * 3);
	FinalUVs.Reserve(Faces.Num() * 3);

	auto AddVertex = [&](const FObjIndex &Idx) -> int32
	{
		const int32 V = Positions.IsValidIndex(Idx.Vertex) ? Idx.Vertex : INDEX_NONE;
		if (V == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		FinalVertices.Add(Positions[V]);

		const FVector2D UV = (Idx.UV != INDEX_NONE && UVs.IsValidIndex(Idx.UV)) ? UVs[Idx.UV] : FVector2D::ZeroVector;
		FinalUVs.Add(UV);

		const FVector Normal = (Idx.Normal != INDEX_NONE && Normals.IsValidIndex(Idx.Normal)) ? Normals[Idx.Normal] : FVector::ZeroVector;
		FinalNormals.Add(Normal);

		return FinalVertices.Num() - 1;
	};

	for (const TArray<FObjIndex> &Face : Faces)
	{
		if (Face.Num() < 3)
		{
			continue;
		}

		const FObjIndex &A = Face[0];
		for (int32 i = 1; i < Face.Num() - 1; ++i)
		{
			const FObjIndex &B = Face[i];
			const FObjIndex &C = Face[i + 1];

			const int32 IA = AddVertex(A);
			const int32 IB = AddVertex(B);
			const int32 IC = AddVertex(C);

			if (IA == INDEX_NONE || IB == INDEX_NONE || IC == INDEX_NONE)
			{
				UE_LOG(LogUEMeshViewer, Warning, TEXT("Skipped degenerate face while building mesh."));
				continue;
			}

			Triangles.Add(IA);
			Triangles.Add(IB);
			Triangles.Add(IC);
		}
	}

	if (FinalVertices.Num() == 0)
	{
		UE_LOG(LogUEMeshViewer, Error, TEXT("OBJ contained no triangles."));
		return false;
	}

	const bool bNormalsMissing = FinalNormals.ContainsByPredicate([](const FVector &N)
																  { return N.IsNearlyZero(); });

	if (bNormalsMissing)
	{
		FinalNormals.Init(FVector::ZeroVector, FinalVertices.Num());
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(FinalVertices, Triangles, FinalUVs, FinalNormals, Tangents);
	}

	MeshComponent->CreateMeshSection(0, FinalVertices, Triangles, FinalNormals, FinalUVs, {}, Tangents, true);
	MeshComponent->SetMeshSectionVisible(0, true);
	MeshBounds = FBox(FinalVertices.GetData(), FinalVertices.Num());

	return true;
}
