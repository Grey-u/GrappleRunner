// =====================================================================================
//  GREndlessSpawner.h  -  Endless Runner mode: islands keep coming, forever
// =====================================================================================
//
//  WHAT THIS IS
//  An actor placed once in the level. In Endless mode it builds new "chunks" of world ahead
//  of the player as they swing along +X, and removes chunks far behind them, so the
//  run never ends and memory stays flat however far you go.
//
//  ONE CHUNK =
//    - a floating island, 18-40 m wide, 45-70 m after the last one, a little higher or lower
//    - grapple rocks hanging 11-21 m above the line between the islands, 7-16 m apart
//      (the spacing tested in the 2D bench and T-01 to T-14, so every gap can be swung)
//    - trees, ferns and an overgrown ruin on the island's top, and a vine (that snaps)
//  Distance score: how far past the start you've swung, and your best run this session.
//
//  WHEN IT RUNS
//  Only when the menu picked Endless (the level was opened with "?mode=endless"), or when
//  you press Play in the editor without the menu (handy for testing). Team modes leave the
//  fixed level as it is. Fall into the mist and the run resets: chunks are cleared and the
//  next run starts again from the start.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = can be changed in the Details panel without code.
//  - Distances are centimetres (100 = 1 metre).
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GREndlessSpawner.generated.h"

class AGrappleRig;
class UStaticMesh;

UCLASS()
class GRAPPLERUNNER_API AGREndlessSpawner : public AActor
{
	GENERATED_BODY()

public:
	AGREndlessSpawner();
	virtual void Tick(float DeltaSeconds) override;

	// ---- When ----
	UPROPERTY(EditAnywhere, Category = "Endless") bool bRunInEditorPlay = true;   // also run when Play is pressed in the editor (no menu)

	// ---- How far ahead / behind ----
	UPROPERTY(EditAnywhere, Category = "Endless") float BuildAhead = 30000.f;    // keep 300 m of world built in front of you
	UPROPERTY(EditAnywhere, Category = "Endless") float RemoveBehind = 15000.f;  // remove chunks 150 m behind you

	// ---- Shape of the route (same numbers as the hand-built route) ----
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D IslandGap = FVector2D(4500.f, 7000.f);      // cm between islands
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D IslandWidth = FVector2D(1800.f, 4000.f);
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D Rise = FVector2D(-600.f, 900.f);            // height change per island
	UPROPERTY(EditAnywhere, Category = "Endless") float SideDrift = 2500.f;                              // sideways wander
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D AnchorHeight = FVector2D(1100.f, 2100.f);
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D AnchorSpacing = FVector2D(700.f, 1600.f);
	UPROPERTY(EditAnywhere, Category = "Endless") int32 TreesPerIsland = 4;
	UPROPERTY(EditAnywhere, Category = "Endless") int32 FernsPerIsland = 10;

	// ---- What it's built from (all from the packs already in the project) ----
	UPROPERTY(EditAnywhere, Category = "Endless|Assets") TArray<TSoftClassPtr<AActor>> IslandTypes;
	UPROPERTY(EditAnywhere, Category = "Endless|Assets") TArray<TSoftObjectPtr<UStaticMesh>> RockMeshes;
	UPROPERTY(EditAnywhere, Category = "Endless|Assets") TArray<TSoftObjectPtr<UStaticMesh>> TreeMeshes;
	UPROPERTY(EditAnywhere, Category = "Endless|Assets") TArray<TSoftObjectPtr<UStaticMesh>> FernMeshes;
	UPROPERTY(EditAnywhere, Category = "Endless|Assets") TArray<TSoftObjectPtr<UStaticMesh>> RuinMeshes;
	UPROPERTY(EditAnywhere, Category = "Endless|Assets") TArray<TSoftObjectPtr<UStaticMesh>> RootMeshes;

	// Distance (metres) of this run, and the best run this session.
	float GetDistance() const { return Distance; }
	float GetBest() const { return Best; }

protected:
	virtual void BeginPlay() override;

private:
	struct FChunk
	{
		float X = 0.f;
		TArray<TWeakObjectPtr<AActor>> Actors;
	};
	TArray<FChunk> Chunks;
	TWeakObjectPtr<AGrappleRig> Rig;
	bool bActive = false;
	FVector StartTop = FVector::ZeroVector;     // top of the last hand-built island: the endless route starts after it
	FVector LastTop = FVector::ZeroVector;      // top of the last island built
	float StartX = 0.f;
	float Distance = 0.f;
	float Best = 0.f;
	float FurthestX = 0.f;
	FRandomStream Rng;

	void BuildChunk();
	void ClearChunks();
	AActor* SpawnMesh(UStaticMesh* Mesh, const FVector& At, const FRotator& Turn, float Scale, bool bSolid, FChunk& Into);
	bool GroundOn(AActor* Island, float X, float Y, FVector& OutPoint) const;
	UStaticMesh* Pick(const TArray<TSoftObjectPtr<UStaticMesh>>& List);   // one of the list, at random
};
