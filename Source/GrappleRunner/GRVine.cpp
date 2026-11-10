// =====================================================================================
//  GRVine.cpp  -  what the hanging vine DOES (see GRVine.h for the overview)
// =====================================================================================
#include "GRVine.h"

#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"

AGRVine::AGRVine()
{
	PrimaryActorTick.bCanEverTick = false;   // a vine never needs to think every frame
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

// The sagging curve. A straight line from start to end, pulled down in the middle by
// Sag * 4 * T * (1 - T): that's 0 at both ends and exactly Sag halfway along.
// (A true catenary uses cosh, but at these sags this parabola is within a few cm of it.)
FVector AGRVine::PointAt(float T) const
{
	return EndPoint * T - FVector(0.f, 0.f, Sag * 4.f * T * (1.f - T));
}

void AGRVine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Remove the pieces from the last time (settings may have changed).
	// (Unreal usually clears construction-made pieces itself; this catches any left over.)
	for (USplineMeshComponent* Old : Pieces)
	{
		if (IsValid(Old))
		{
			Old->DestroyComponent();
		}
	}
	Pieces.Reset();

	UStaticMesh* Mesh = VineMesh.LoadSynchronous();
	if (!Mesh || EndPoint.IsNearlyZero())
	{
		return;
	}
	const int32 N = FMath::Clamp(Segments, 2, 32);
	for (int32 i = 0; i < N; ++i)
	{
		const float T0 = float(i) / N;
		const float T1 = float(i + 1) / N;
		const FVector P0 = PointAt(T0);
		const FVector P1 = PointAt(T1);
		// The direction of the curve at each end of this piece (its "tangent"), so neighbouring
		// pieces meet smoothly instead of kinking. Scaled to the piece's length.
		const float Len = FVector::Dist(P0, P1);
		const FVector D0 = (PointAt(FMath::Min(T0 + 0.01f, 1.f)) - PointAt(FMath::Max(T0 - 0.01f, 0.f))).GetSafeNormal() * Len;
		const FVector D1 = (PointAt(FMath::Min(T1 + 0.01f, 1.f)) - PointAt(FMath::Max(T1 - 0.01f, 0.f))).GetSafeNormal() * Len;

		USplineMeshComponent* Piece = NewObject<USplineMeshComponent>(this);
		Piece->CreationMethod = EComponentCreationMethod::UserConstructionScript;   // rebuilt with the vine
		Piece->SetMobility(EComponentMobility::Static);
		Piece->SetStaticMesh(Mesh);
		Piece->SetForwardAxis(LengthAxis, false);
		Piece->SetStartAndEnd(P0, D0, P1, D1, false);
		Piece->SetStartScale(FVector2D(Thickness), false);
		Piece->SetEndScale(FVector2D(Thickness), false);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // never snags the player or a rope
		Piece->SetCastShadow(false);                                   // thin, and cheaper for VR
		Piece->SetCullDistance(DrawDistance);
		Piece->SetupAttachment(RootComponent);
		Piece->RegisterComponent();
		Piece->UpdateMesh();
		Pieces.Add(Piece);
	}
}
