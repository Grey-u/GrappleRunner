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
//    - grapple rocks hanging 11-21 m above the line between the islands, 4.5-9.5 m apart and
//      5-8.5 m wide (closer and bigger since 14 Nov), sometimes an extra one off to the side
//    - usually a glowing gas canister to slice (your fuel), and every 3rd gap a floating
//      cave: a tube of cliff rock you swing through, hooking its ceiling
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
	UPROPERTY(EditAnywhere, Category = "Endless") bool bPartnerInEditorPlay = true;   // Play in the editor (no menu): bring the AI partner, for testing

	// ---- How far ahead / behind ----
	UPROPERTY(EditAnywhere, Category = "Endless") float BuildAhead = 30000.f;    // keep 300 m of world built in front of you
	UPROPERTY(EditAnywhere, Category = "Endless") float RemoveBehind = 15000.f;  // remove chunks 150 m behind you

	// ---- Shape of the route (same numbers as the hand-built route) ----
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D IslandGap = FVector2D(4500.f, 7000.f);      // cm between islands
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D IslandWidth = FVector2D(1800.f, 4000.f);
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D Rise = FVector2D(-600.f, 900.f);            // height change per island
	UPROPERTY(EditAnywhere, Category = "Endless") float SideDrift = 2500.f;                              // sideways wander
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D AnchorHeight = FVector2D(1100.f, 2100.f);
	// 14 Nov (Naomi): more rocks, closer together, big enough to aim at easily.
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D AnchorSpacing = FVector2D(450.f, 950.f);   // was 7-16 m apart
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D AnchorSize = FVector2D(500.f, 850.f);      // rock width in cm (was 2.5-4.5 m)
	UPROPERTY(EditAnywhere, Category = "Endless") float SideRockChance = 0.5f;   // chance of an extra rock off to one side, for choice
	// Gas canisters: the fuel you slice to keep going.
	UPROPERTY(EditAnywhere, Category = "Endless") float PodChance = 0.8f;        // chance of a canister in each gap
	UPROPERTY(EditAnywhere, Category = "Endless") FVector2D PodHeight = FVector2D(500.f, 1100.f);   // above the island-to-island line
	// Floating caves and tunnels: every few gaps, a tube of rock to swing through.
	UPROPERTY(EditAnywhere, Category = "Endless") int32 CaveEvery = 3;          // one every 3 gaps
	UPROPERTY(EditAnywhere, Category = "Endless|Assets") TArray<TSoftObjectPtr<UStaticMesh>> CaveMeshes;
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
	// Middles of the caves built so far (the AI partner calls them out as you approach).
	const TArray<FVector>& GetCaveCentres() const { return CaveCentres; }

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
	bool bPartnerWanted = false;   // "?partner=1": spawn the AI partner
	FVector StartTop = FVector::ZeroVector;     // top of the last hand-built island: the endless route starts after it
	FVector LastTop = FVector::ZeroVector;      // top of the last island built
	float StartX = 0.f;
	float Distance = 0.f;
	float Best = 0.f;
	float FurthestX = 0.f;
	FRandomStream Rng;
	int32 ChunkCount = 0;
	TArray<FVector> CaveCentres;
	void BuildCave(const FVector& From, const FVector& To, FChunk& Into);
	void AddAnchorRock(UStaticMesh* Mesh, const FVector& At, float Width, FChunk& Into);

	void BuildChunk();
	void ClearChunks();
	AActor* SpawnMesh(UStaticMesh* Mesh, const FVector& At, const FRotator& Turn, float Scale, bool bSolid, FChunk& Into);
	bool GroundOn(AActor* Island, float X, float Y, FVector& OutPoint) const;
	UStaticMesh* Pick(const TArray<TSoftObjectPtr<UStaticMesh>>& List);   // one of the list, at random
};
