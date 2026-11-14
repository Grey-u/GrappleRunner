// =====================================================================================
//  GRStylizedNature.h  -  the anime-style world: meadows of grass and flowers, a sea of
//                         clouds below the islands, flocks of birds, butterflies
// =====================================================================================
//
//  WHAT THIS IS
//  The Genshin-inspired look for the world itself (M_GenshinToon, from genshin_look.py,
//  does the characters).
//  When a level starts it:
//    1. GRASS: looks at every solid thing in the level from above (a downward "line trace"
//       every ~1.1 m) and wherever it finds a fairly flat top (an island, a rock, a ledge),
//       it plants a clump: mostly short grass, with patches of tall grass, flower meadows
//       (pink, white, yellow, blue), little bushes and mossy stones. The rocks the endless
//       mode spawns get dressed the same way (DressRock, called by the spawner).
//    2. CLOUDS: big puffy white clouds with soft lilac undersides float below and around
//       you, drifting with the wind. Ones you leave behind reappear somewhere ahead.
//    3. BIRDS: a few flocks circle in the distance, flapping (the flapping is done by the
//       material, see make_stylized_meshes.py: the wing tips are marked to move).
//    4. BUTTERFLIES: flutter over the grass near you.
//
//  The meshes are our own, made from scratch by Scripts/make_stylized_meshes.py (Blender)
//  and brought into Unreal by Scripts/import_stylized.py, which also makes their materials.
//
//  HOW IT STAYS FAST (VR needs 90 frames a second)
//  Every kind of mesh is ONE "instanced" component: the graphics card draws thousands of
//  grass clumps in one go. Grass further than GrassCullEnd away isn't drawn at all, and
//  grass casts no shadows. Lower GrassSpacing = denser grass = more work.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = change it in the Details panel without code.
//  - Distances are centimetres (100 = 1 m), speeds centimetres per second.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRStylizedNature.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPrimitiveComponent;
class UStaticMesh;

UCLASS()
class GRAPPLERUNNER_API AGRStylizedNature : public AActor
{
	GENERATED_BODY()

public:
	AGRStylizedNature();
	virtual void Tick(float DeltaSeconds) override;

	// The one in the level (so the endless spawner can find it quickly). Null if none.
	static AGRStylizedNature* Get();

	// Plant grass and flowers on top of one rock or island (used for things spawned while
	// playing). The grass is attached to it, so it disappears when it does.
	// MaxOnIt = most clumps on this one actor (rocks ~220, islands more).
	void DressRock(AActor* Rock, int32 MaxOnIt = 220);

	// ---- grass ----
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") float GrassSpacing = 110.f;      // gap between clumps (smaller = denser)
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") float MinFlatness = 0.72f;       // 1 = only perfectly flat ground, 0 = walls too
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") float FlowerChance = 0.14f;      // in flower meadows: chance a clump is flowers
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") float TallGrassChance = 0.16f;
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") float BushChance = 0.025f;
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") float StoneChance = 0.02f;
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") int32 MaxClumps = 60000;         // safety cap on the whole level
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") float GrassCullStart = 5500.f;   // grass starts fading out here...
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") float GrassCullEnd = 8000.f;     // ...and is gone here
	UPROPERTY(EditAnywhere, Category = "Nature|Grass") FVector2D ClumpScale = FVector2D(0.8f, 1.5f);

	// ---- clouds ----
	UPROPERTY(EditAnywhere, Category = "Nature|Clouds") int32 CloudCount = 60;
	UPROPERTY(EditAnywhere, Category = "Nature|Clouds") float CloudNear = 5000.f;      // closest a new cloud appears (horizontally)
	UPROPERTY(EditAnywhere, Category = "Nature|Clouds") float CloudFar = 28000.f;       // clouds further than this are moved ahead
	UPROPERTY(EditAnywhere, Category = "Nature|Clouds") FVector2D CloudHeight = FVector2D(-9000.f, 3500.f);   // relative to you
	UPROPERTY(EditAnywhere, Category = "Nature|Clouds") FVector2D CloudScale = FVector2D(1.5f, 4.f);
	UPROPERTY(EditAnywhere, Category = "Nature|Clouds") FVector Wind = FVector(60.f, 25.f, 0.f);

	// ---- birds ----
	UPROPERTY(EditAnywhere, Category = "Nature|Birds") int32 Flocks = 4;
	UPROPERTY(EditAnywhere, Category = "Nature|Birds") int32 BirdsPerFlock = 6;
	UPROPERTY(EditAnywhere, Category = "Nature|Birds") float BirdScale = 1.3f;
	UPROPERTY(EditAnywhere, Category = "Nature|Birds") float BirdYawOffset = 0.f;        // if birds fly sideways, try 90 or -90

	// ---- butterflies ----
	UPROPERTY(EditAnywhere, Category = "Nature|Butterflies") int32 Butterflies = 30;
	UPROPERTY(EditAnywhere, Category = "Nature|Butterflies") float ButterflyRange = 3000.f;   // how far from you they live
	UPROPERTY(EditAnywhere, Category = "Nature|Butterflies") float ButterflyScale = 1.6f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	// What one grass "spot" becomes.
	enum class EKind : uint8 { Grass, Tall, FlowerPink, FlowerWhite, FlowerYellow, FlowerBlue, Bush, StoneA, StoneB, Count };

	UStaticMesh* MeshFor(EKind Kind) const;
	EKind PickKind(const FVector& At);
	FTransform ClumpTransform(const FVector& At, const FVector& Normal);
	UInstancedStaticMeshComponent* MakeInstancer(UStaticMesh* Mesh, USceneComponent* AttachTo, bool bHierarchical, bool bShadows, float CullStart, float CullEnd);
	void ScatterOnLevel();
	FVector Viewer() const;
	void PlaceCloud(int32 i, const FVector& Near, bool bFar);

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;                          // by EKind
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> LevelGrass;     // by EKind
	TArray<FVector> Spots;                                            // where grass was planted (butterflies visit)
	int32 Planted = 0;
	FRandomStream Rng;

	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> CloudSets;      // one per cloud shape
	struct FCloud { int32 Set = 0; int32 Index = 0; FVector Pos; float Scale = 1.f; float Yaw = 0.f; };
	TArray<FCloud> Clouds;

	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> BirdSet;
	struct FFlock { FVector Offset; float Radius = 5000.f; float Angle = 0.f; float Speed = 0.1f; };
	TArray<FFlock> FlockList;
	TArray<FVector> BirdLocal;                                        // each bird's place in its flock
	FVector FlockFollow = FVector::ZeroVector;                        // a slowly-following copy of your position

	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> ButterflySets;  // one per colour
	struct FButterfly { int32 Set = 0; int32 Index = 0; FVector Home; FVector Pos; float Phase = 0.f; float NextMove = 0.f; bool bHidden = true; };
	TArray<FButterfly> ButterflyList;
	float Clock = 0.f;

	static TWeakObjectPtr<AGRStylizedNature> Instance;
};
