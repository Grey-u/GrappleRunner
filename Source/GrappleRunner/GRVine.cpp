// =====================================================================================
//  GRVine.cpp  -  what the hanging vine DOES (see GRVine.h for the overview)
// =====================================================================================
#include "GRVine.h"

#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"

AGRVine::AGRVine()
{
	PrimaryActorTick.bCanEverTick = true;          // ticks ONLY while a snapped vine is swinging
	PrimaryActorTick.bStartWithTickEnabled = false;
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
		// Movable: the pieces must be able to bend while playing (snapping), and they hang from a
		// movable root. (Static pieces can't be attached to a movable root: they'd lose their place.)
		Piece->SetMobility(EComponentMobility::Movable);
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

// -------------------------------------------------------------------------------------
//  SNAPPING
// -------------------------------------------------------------------------------------
// The player's body moved from From to To this frame, as a ball of the given radius. We
// walk along the vine in short straight pieces and ask: did that path come within Radius of
// any of them? (Two line segments' closest distance: FMath does the geometry.)
bool AGRVine::TrySnap(const FVector& From, const FVector& To, float Radius, const FVector& Velocity)
{
	if (!bCanSnap || bSnapped || Pieces.Num() < 2 || Velocity.Size() < MinSnapSpeed)
	{
		return false;   // can't snap, already snapped, or the player is just drifting past
	}
	const FTransform& Here = GetActorTransform();
	const int32 Samples = Pieces.Num() * 2;
	for (int32 i = 0; i < Samples; ++i)
	{
		const float T0 = float(i) / Samples;
		const float T1 = float(i + 1) / Samples;
		FVector OnPath, OnVine;
		FMath::SegmentDistToSegmentSafe(From, To, Here.TransformPosition(PointAt(T0)), Here.TransformPosition(PointAt(T1)), OnPath, OnVine);
		if (FVector::DistSquared(OnPath, OnVine) > FMath::Square(Radius))
		{
			continue;
		}
		// Hit. Break between the two pieces nearest the hit, never right at an end.
		const int32 N = Pieces.Num();
		CutPiece = FMath::Clamp(FMath::RoundToInt((T0 + T1) * 0.5f * N), 1, N - 1);
		const FVector Cut = PointAt(float(CutPiece) / N);
		const FVector Ends[2] = { PointAt(0.f), PointAt(1.f) };
		const float Lengths[2] = { static_cast<float>(FVector::Dist(Ends[0], Cut)) * 1.1f, static_cast<float>(FVector::Dist(Ends[1], Cut)) * 1.1f };
		const FVector Push = Here.InverseTransformVectorNoScale(Velocity).GetSafeNormal2D();
		const float Strength = FMath::Clamp(static_cast<float>(Velocity.Size()) / 2000.f, 0.3f, 1.f);   // faster hit, bigger swing
		for (int32 h = 0; h < 2; ++h)
		{
			// Where the loose end settles: almost straight down from its anchor, leaning a little
			// toward where the vine used to go. The player's push throws it out at first.
			const FVector Toward = (Cut - Ends[h]).GetSafeNormal2D();
			RestEnd[h] = Ends[h] + Toward * Lengths[h] * 0.15f - FVector(0.f, 0.f, Lengths[h] * 0.95f);
			SwingKick[h] = Push * Lengths[h] * 0.45f * Strength;
		}
		bSnapped = true;
		SnapAge = 0.f;
		SetActorTickEnabled(true);
		PoseHalves();
		return true;
	}
	return false;
}

void AGRVine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSnapped)
	{
		SetActorTickEnabled(false);
		return;
	}
	SnapAge += DeltaSeconds;
	PoseHalves();
	if (SnapAge > SwingSettleSeconds * 5.f)
	{
		SetActorTickEnabled(false);   // it has stopped swinging: stop spending time on it
	}
}

// Bends each half into a hanging strand: from its anchor to its loose end. The loose end
// starts where the vine broke, falls, and swings like a pendulum whose swing dies away
// (cos for the swing, exp for the dying away). Its swing time follows the pendulum rule:
// period = 2 pi * square root of (length / gravity).
void AGRVine::PoseHalves()
{
	const int32 N = Pieces.Num();
	const FVector Ends[2] = { PointAt(0.f), PointAt(1.f) };
	const FVector Cut = PointAt(float(CutPiece) / N);
	const float Fall = FMath::SmoothStep(0.f, 1.f, FMath::Min(1.f, SnapAge / 0.35f));   // 0.35 s to drop
	const float Decay = FMath::Exp(-SnapAge / FMath::Max(0.1f, SwingSettleSeconds));
	for (int32 h = 0; h < 2; ++h)
	{
		const float Length = FMath::Max(50.f, static_cast<float>(FVector::Dist(Ends[h], RestEnd[h])));
		const float Period = 2.f * PI * FMath::Sqrt(Length / 980.f);
		const FVector Swing = RestEnd[h] + SwingKick[h] * Decay * FMath::Cos(2.f * PI * SnapAge / Period);
		const FVector Loose = FMath::Lerp(Cut, Swing, Fall);
		const int32 First = (h == 0) ? 0 : CutPiece;
		const int32 Count = (h == 0) ? CutPiece : N - CutPiece;
		auto Strand = [&](float U)   // a gently curved line from the anchor to the loose end
		{
			return FMath::Lerp(Ends[h], Loose, U) - FVector(0.f, 0.f, Length * 0.06f * 4.f * U * (1.f - U));
		};
		for (int32 k = 0; k < Count; ++k)
		{
			USplineMeshComponent* Piece = Pieces[First + k];
			if (!IsValid(Piece))
			{
				continue;
			}
			const float U0 = float(k) / Count;
			const float U1 = float(k + 1) / Count;
			const FVector P0 = Strand(U0);
			const FVector P1 = Strand(U1);
			const FVector D = (P1 - P0);
			Piece->SetStartAndEnd(P0, D, P1, D, true);
		}
	}
}
