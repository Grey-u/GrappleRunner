// =====================================================================================
//  GRCutter.h  -  the blades can cut (almost) anything: plants, props, pillars, trees
// =====================================================================================
//
//  WHAT THIS IS
//  One invisible actor that knows every "cuttable" thing in the level and does the cutting.
//  The swords (GRArsenal) ask it, every frame of a swing: "did my blade just pass through
//  anything?" If yes, that thing takes a hit:
//    - small things (plants, crates, lamps)  : one hit and they are sliced in two
//    - medium things (boulders, statues)     : two hits
//    - trees and tall pillars                 : several hacks (2 to 5, taller = more), then
//                                               the trunk is cut and the top topples over
//    - huge things (islands, cliffs, walls)   : can't be cut; the blade just throws sparks
//
//  HOW A REAL CUT WORKS (Unreal's "procedural mesh" tools)
//    1. Copy the object's 3D model into a "procedural mesh": a model the game can edit
//       while playing (an ordinary model is locked once it's built).
//    2. Hide the original.
//    3. Slice the copy along the flat plane the blade swept through. Unreal splits it into
//       two pieces and fills the open faces ("caps") so they don't look hollow.
//    4. The piece above the cut gets physics: it falls, rolls and lands. The piece below
//       (the stump) stays where it was.
//    Falling pieces vanish after PieceLifeSeconds so the world doesn't fill up with logs.
//
//  WHY IT KEEPS ITS OWN LIST (instead of asking the physics engine)
//  Most plants have collision switched OFF (so they never snag a grapple swing), which means
//  the physics engine can't "see" them. So when the level starts, this actor writes down
//  every object's box in a grid of 8 m squares. A blade then only checks the few objects in
//  the squares it passes through, which is fast even with 10,000 plants. New island chunks
//  from the endless spawner are added as they appear.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = a value you can change in the Details panel.
//  - Distances are centimetres (100 = 1 m), masses kilograms, times seconds.
//  - To stop something being cuttable: give its actor (or component) the tag  NoCut .
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRCutter.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;

// What one blade pass did to one thing (the swords use it for sparks, chips and buzz).
struct FGRCutResult
{
	FVector Point = FVector::ZeroVector;   // where the blade met it
	bool bCut = false;                     // true = it was sliced through this time
	bool bTree = false;                    // a tree / tall thing (chips fly, it takes several hacks)
	bool bSoft = false;                    // a plant (green bits) rather than wood or stone
	int32 HitsLeft = 0;                    // hacks still needed (0 once cut)
};

UCLASS()
class GRAPPLERUNNER_API AGRCutter : public AActor
{
	GENERATED_BODY()

public:
	AGRCutter();
	virtual void Tick(float DeltaSeconds) override;

	// The blade moved from (FromA, FromB) last frame to (ToA, ToB) this frame
	// (A = near the handle, B = the tip). Every cuttable thing it passed through takes one hit,
	// but only once per swing: pass the same SwingId for the whole swing.
	// SwingDir = which way the blade is moving. Results are added to Out.
	void BladePass(const FVector& FromA, const FVector& FromB, const FVector& ToA, const FVector& ToB,
		float Damage, int32 SwingId, TArray<FGRCutResult>& Out);

	// ---- What can be cut ----
	UPROPERTY(EditAnywhere, Category = "Cutting") float MaxCutSize = 3000.f;       // anything bigger than 30 m is never cut
	UPROPERTY(EditAnywhere, Category = "Cutting") float MaxBlobSize = 450.f;       // chunky things wider than 4.5 m (cliffs, walls) can't be cut...
	UPROPERTY(EditAnywhere, Category = "Cutting") float TreeTallness = 1.8f;       // ...unless they are this many times taller than wide (trees, pillars)
	UPROPERTY(EditAnywhere, Category = "Cutting") float SmallSize = 250.f;         // smaller than this: one hit
	UPROPERTY(EditAnywhere, Category = "Cutting") float MetresPerTreeHack = 7.f;   // a tree needs one hack per this many metres of height...
	UPROPERTY(EditAnywhere, Category = "Cutting") int32 MinTreeHacks = 2;          // ...at least this many...
	UPROPERTY(EditAnywhere, Category = "Cutting") int32 MaxTreeHacks = 5;          // ...and at most this many
	UPROPERTY(EditAnywhere, Category = "Cutting") float BladeDamage = 1.f;         // one normal hit = 1 hack (a heavy cross-cut counts 1.5)

	// ---- What the pieces do ----
	UPROPERTY(EditAnywhere, Category = "Cutting|Pieces") float PieceLifeSeconds = 30.f;   // fallen pieces disappear after this
	UPROPERTY(EditAnywhere, Category = "Cutting|Pieces") int32 MaxPieces = 40;            // oldest pieces go first beyond this
	UPROPERTY(EditAnywhere, Category = "Cutting|Pieces") float Density = 0.00035f;        // kg per cubic cm (rough wood)
	UPROPERTY(EditAnywhere, Category = "Cutting|Pieces") float MaxPieceMass = 900.f;      // kg
	UPROPERTY(EditAnywhere, Category = "Cutting|Pieces") float PushSpeed = 260.f;         // cm/s the top piece is pushed in the swing direction
	UPROPERTY(EditAnywhere, Category = "Cutting|Pieces") float CellSize = 800.f;          // the grid squares (cm)

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	// One cuttable thing (a component, or one copy inside an instanced component).
	struct FEntry
	{
		TWeakObjectPtr<UStaticMeshComponent> Comp;
		int32 Item = INDEX_NONE;      // which instance, for instanced meshes; INDEX_NONE otherwise
		FTransform World;             // where it is
		FBox Local;                   // its box in its own space
		bool bTree = false;
		bool bSoft = false;           // had no collision = a plant
		float Health = 1.f;           // hacks left
		int32 LastSwing = -1;         // the swing that last hit it (one hit per swing)
		bool bGone = false;           // already cut
	};
	TArray<FEntry> Entries;
	TMap<FIntVector, TArray<int32>> Grid;      // grid square -> entries touching it
	TSet<TWeakObjectPtr<UStaticMeshComponent>> Known;   // components already written down
	TArray<TWeakObjectPtr<AActor>> NewActors;           // spawned since last frame; written down next frame
	bool bScanned = false;
	FDelegateHandle SpawnHandle;

	// Falling pieces and when they were made
	struct FPiece { TWeakObjectPtr<UProceduralMeshComponent> Mesh; float Born = 0.f; };
	TArray<FPiece> Pieces;

	void ScanWorld();
	void AddActor(AActor* Actor);
	void AddComponent(UStaticMeshComponent* Comp);
	void AddEntry(UStaticMeshComponent* Comp, int32 Item, const FTransform& World);
	bool Touches(const FEntry& E, const FVector& A, const FVector& B, FVector& OutPoint) const;
	void Cut(FEntry& E, const FVector& PlanePoint, FVector PlaneNormal, const FVector& SwingDir);
	FIntVector CellOf(const FVector& P) const;
};
