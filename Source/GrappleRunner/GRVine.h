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
	UPROPERTY(EditAnywhere, Category = "Vine", Meta = (MakeEditWidget = true)) FVector EndPoint = FVector(1000.f, 0.f, 0.f);
	// How far the middle hangs below the straight line between the ends (cm).
	UPROPERTY(EditAnywhere, Category = "Vine") float Sag = 200.f;
	// How many bent pieces the vine is made of: more = smoother curve, a little more cost.
	UPROPERTY(EditAnywhere, Category = "Vine", Meta = (ClampMin = 2, ClampMax = 32)) int32 Segments = 8;
	// Thickness multiplier (1 = the mesh's own thickness).
	UPROPERTY(EditAnywhere, Category = "Vine") float Thickness = 1.f;
	// The mesh bent along the curve, and which of its axes runs along its length.
	UPROPERTY(EditAnywhere, Category = "Vine") TSoftObjectPtr<UStaticMesh> VineMesh =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Brushify/Meshes/FloatingIslands/Dressing/Vines/S_Vines.S_Vines")));
	UPROPERTY(EditAnywhere, Category = "Vine") TEnumAsByte<ESplineMeshAxis::Type> LengthAxis = ESplineMeshAxis::X;
	// Vines further away than this aren't drawn (cm), to keep the frame rate up in VR.
	UPROPERTY(EditAnywhere, Category = "Vine") float DrawDistance = 20000.f;

	// Runs whenever the vine is placed or a setting changes: rebuilds the pieces.
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY() TArray<TObjectPtr<USplineMeshComponent>> Pieces;
	FVector PointAt(float T) const;   // a point along the sagging curve, T from 0 (start) to 1 (end)
};
