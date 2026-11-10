// =====================================================================================
//  GRVine.h  -  a hanging vine between two points (tree to tree, ruin to island...)
// =====================================================================================
//
//  WHAT THIS IS
//  An actor that draws a vine from where it stands to EndPoint, sagging in the middle like
//  a real hanging plant (a "catenary": the curve a rope or chain makes under its own weight).
//  The vine is made of several bent copies of a vine mesh (Brushify's S_Vines), each one
//  stretched along a piece of the curve. That's a "spline mesh": a mesh bent along a curve.
//  Scripts/expand_world.py places these between trees automatically.
//
//  VINES ARE NOT SOLID: no collision, so they never snag the player or a rope.
//  But they SNAP: fly through one faster than MinSnapSpeed and it breaks where your body hit
//  it. The two halves drop and swing from their anchors, the swing dying away over a few
//  seconds, then they hang still. (The GrappleRig checks your path through the vines every
//  frame and calls TrySnap; see AGrappleRig::SnapVines.)
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = shows in the Details panel; change it and the vine redraws.
//  - "Meta = (MakeEditWidget)" puts a little diamond in the level you can drag to move the end.
//  - Distances are centimetres.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"   // ESplineMeshAxis (which way the mesh runs)
#include "GameFramework/Actor.h"
#include "GRVine.generated.h"

class UStaticMesh;

UCLASS()
class GRAPPLERUNNER_API AGRVine : public AActor
{
	GENERATED_BODY()

public:
	AGRVine();

	// Where the vine ends, measured from this actor (drag the diamond in the level to move it).
	UPROPERTY(EditAnywhere, Category = "Vine", Meta = (MakeEditWidget = true)) FVector EndPoint = FVector::ZeroVector;
	// (Starts at zero = draws nothing, so a vine placed by a script is built ONCE, when its end is set.)
	// How far the middle hangs below the straight line between the ends (cm).
	UPROPERTY(EditAnywhere, Category = "Vine") float Sag = 200.f;
	// How many bent pieces the vine is made of: more = smoother curve, a little more cost.
	UPROPERTY(EditAnywhere, Category = "Vine", Meta = (ClampMin = 2, ClampMax = 32)) int32 Segments = 5;
	// Thickness multiplier (1 = the mesh's own thickness).
	UPROPERTY(EditAnywhere, Category = "Vine") float Thickness = 1.f;
	// The mesh bent along the curve, and which of its axes runs along its length.
	UPROPERTY(EditAnywhere, Category = "Vine") TSoftObjectPtr<UStaticMesh> VineMesh =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Vines/SM_VineStem.SM_VineStem")));
	// (Our own light stem with leaves, Art/Vines/make_vine.py. The pack's vine mesh used far too much memory.)
	UPROPERTY(EditAnywhere, Category = "Vine") TEnumAsByte<ESplineMeshAxis::Type> LengthAxis = ESplineMeshAxis::X;
	// Vines further away than this aren't drawn (cm), to keep the frame rate up in VR.
	UPROPERTY(EditAnywhere, Category = "Vine") float DrawDistance = 20000.f;

	// Snapping: the slowest speed (cm/s) that breaks the vine, and how fast the halves settle.
	UPROPERTY(EditAnywhere, Category = "Vine") bool bCanSnap = true;
	UPROPERTY(EditAnywhere, Category = "Vine") float MinSnapSpeed = 400.f;
	UPROPERTY(EditAnywhere, Category = "Vine") float SwingSettleSeconds = 1.6f;

	// Runs whenever the vine is placed or a setting changes: rebuilds the pieces.
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;   // only runs while snapped halves are swinging

	// Called by the GrappleRig each frame with the path the player's body took (From -> To),
	// the body's radius and its velocity. Snaps the vine if that path crossed it. True = snapped.
	bool TrySnap(const FVector& From, const FVector& To, float Radius, const FVector& Velocity);
	bool IsSnapped() const { return bSnapped; }
	// A sphere that holds the whole vine (world space), for a quick "is it anywhere near?" test.
	FVector BoundsCentre() const { return GetActorLocation() + EndPoint * 0.5f - FVector(0.f, 0.f, Sag * 0.5f); }
	float BoundsRadius() const { return EndPoint.Size() * 0.5f + Sag; }

private:
	UPROPERTY() TArray<TObjectPtr<USplineMeshComponent>> Pieces;
	FVector PointAt(float T) const;   // a point along the sagging curve, T from 0 (start) to 1 (end)

	// After a snap: pieces [0, CutPiece) hang from the start, [CutPiece, N) from the end.
	bool bSnapped = false;
	int32 CutPiece = 0;
	float SnapAge = 0.f;
	FVector RestEnd[2];        // where each half's loose end comes to rest (relative to the actor)
	FVector SwingKick[2];      // how far each loose end is thrown at first (the player's push)
	void PoseHalves();         // bend the pieces of both halves into their hanging curves
};
