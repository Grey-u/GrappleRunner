// =====================================================================================
//  GRCutter.cpp  -  how the cutting works (see GRCutter.h for the idea)
// =====================================================================================
//
//  THE STEPS
//    ScanWorld   (first frame)  write down every static object that can be cut, in a grid
//    AddActor    (any frame)    a new island chunk appeared: write its objects down too
//    BladePass   (sword swing)  which objects did the blade pass through? hit each once
//    Cut         (out of HP)    copy -> hide original -> slice -> top piece falls
//    Tick                       tidy up: old fallen pieces disappear
// =====================================================================================
#include "GRCutter.h"
#include "GrappleRig.h"
#include "GRArsenal.h"
#include "GRAmbientLife.h"
#include "GRPlanets.h"
#include "GRDayNight.h"
#include "GRVine.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"

AGRCutter::AGRCutter()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AGRCutter::BeginPlay()
{
	Super::BeginPlay();
	// Whenever anything is spawned (the endless spawner makes new island chunks), note it down.
	// We wait one frame before reading it, because the spawner sets its model AFTER spawning.
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateWeakLambda(this,
		[this](AActor* A) { NewActors.Add(A); }));
}

void AGRCutter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld())
	{
		GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Super::EndPlay(Reason);
}

FIntVector AGRCutter::CellOf(const FVector& P) const
{
	return FIntVector(FMath::FloorToInt(P.X / CellSize), FMath::FloorToInt(P.Y / CellSize), FMath::FloorToInt(P.Z / CellSize));
}

// =====================================================================================
//  Writing things down
// =====================================================================================
void AGRCutter::ScanWorld()
{
	bScanned = true;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AddActor(*It);
	}
	UE_LOG(LogTemp, Log, TEXT("GRCutter: %d cuttable things in the level"), Entries.Num());
}

void AGRCutter::AddActor(AActor* A)
{
	// Never our own machinery, the player, the sky or the vines (vines snap in their own way).
	if (!A || A->ActorHasTag(TEXT("NoCut")) || A->IsA<APawn>() || A->IsA<AGrappleRig>() || A->IsA<AGRArsenal>()
		|| A->IsA<AGRAmbientLife>() || A->IsA<AGRPlanets>() || A->IsA<AGRDayNight>() || A->IsA<AGRVine>() || A == this)
	{
		return;
	}
	TArray<UStaticMeshComponent*> Comps;
	A->GetComponents<UStaticMeshComponent>(Comps);
	for (UStaticMeshComponent* C : Comps)
	{
		AddComponent(C);
	}
}

void AGRCutter::AddComponent(UStaticMeshComponent* C)
{
	if (!C || !C->GetStaticMesh() || Known.Contains(C) || C->ComponentHasTag(TEXT("NoCut")) || !C->IsVisible() || C->bHiddenInGame)
	{
		return;
	}
	Known.Add(C);
	if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(C))
	{
		// Instanced meshes are many copies in one component: each copy is its own entry.
		for (int32 i = 0; i < ISM->GetInstanceCount(); ++i)
		{
			FTransform T;
			if (ISM->GetInstanceTransform(i, T, true))
			{
				AddEntry(C, i, T);
			}
		}
		return;
	}
	AddEntry(C, INDEX_NONE, C->GetComponentTransform());
}

void AGRCutter::AddEntry(UStaticMeshComponent* C, int32 Item, const FTransform& World)
{
	FEntry E;
	E.Comp = C;
	E.Item = Item;
	E.World = World;
	E.Local = C->GetStaticMesh()->GetBoundingBox();
	const FBox WorldBox = E.Local.TransformBy(World);
	const FVector Size = WorldBox.GetSize();
	const float Height = Size.Z;
	const float Width = FMath::Max(Size.X, Size.Y);
	const float Biggest = Size.GetMax();
	if (Biggest < 5.f || Biggest > MaxCutSize)
	{
		return;   // tiny specks and giant things (islands) are not cuttable
	}
	E.bTree = Height >= TreeTallness * Width && Height > SmallSize;
	if (!E.bTree && Biggest > MaxBlobSize)
	{
		return;   // a cliff or a wall: too big and chunky to slice
	}
	E.bSoft = C->GetCollisionEnabled() == ECollisionEnabled::NoCollision;   // plants have no collision
	// How many hacks it takes.
	if (E.bTree)
	{
		E.Health = FMath::Clamp(FMath::CeilToFloat(Height / (MetresPerTreeHack * 100.f)), float(MinTreeHacks), float(MaxTreeHacks));
	}
	else
	{
		E.Health = Biggest < SmallSize ? 1.f : 2.f;
	}
	const int32 Index = Entries.Add(E);
	// Put it in every grid square its box touches.
	const FIntVector Lo = CellOf(WorldBox.Min);
	const FIntVector Hi = CellOf(WorldBox.Max);
	for (int32 X = Lo.X; X <= Hi.X; ++X)
	{
		for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
		{
			for (int32 Z = Lo.Z; Z <= Hi.Z; ++Z)
			{
				Grid.FindOrAdd(FIntVector(X, Y, Z)).Add(Index);
			}
		}
	}
}

// =====================================================================================
//  Did the line A-B pass through this thing? (OutPoint = where)
//  Trees: only the TRUNK counts (a thick upright line in the middle of the tree's box), so
//  brushing the leaves doesn't hack the tree. Everything else: its box, shrunk a little,
//  because plant boxes include a lot of air around the leaves.
// =====================================================================================
bool AGRCutter::Touches(const FEntry& E, const FVector& A, const FVector& B, FVector& OutPoint) const
{
	const FVector LA = E.World.InverseTransformPosition(A);   // the line in the object's own space
	const FVector LB = E.World.InverseTransformPosition(B);
	const FVector C = E.Local.GetCenter();
	const FVector X = E.Local.GetExtent();
	if (E.bTree)
	{
		const float Radius = FMath::Clamp(0.18f * FMath::Min(X.X, X.Y), 10.f, 70.f);
		const FVector Bottom(C.X, C.Y, E.Local.Min.Z);
		const FVector Top(C.X, C.Y, E.Local.Min.Z + (E.Local.Max.Z - E.Local.Min.Z) * 0.6f);   // lower 60% = trunk
		FVector OnLine, OnTrunk;
		FMath::SegmentDistToSegmentSafe(LA, LB, Bottom, Top, OnLine, OnTrunk);
		// Compare in world centimetres (the tree may be scaled).
		if (FVector::Dist(E.World.TransformPosition(OnLine), E.World.TransformPosition(OnTrunk))
			> Radius * E.World.GetScale3D().GetAbsMax())
		{
			return false;
		}
		OutPoint = E.World.TransformPosition(OnLine);
		return true;
	}
	const FBox Shrunk(C - X * 0.8f, C + X * 0.8f);
	if (!FMath::LineBoxIntersection(Shrunk, LA, LB, LB - LA))
	{
		return false;
	}
	OutPoint = E.World.TransformPosition(FMath::ClosestPointOnSegment(C, LA, LB));
	return true;
}

// =====================================================================================
//  One frame of a blade's movement
// =====================================================================================
void AGRCutter::BladePass(const FVector& FromA, const FVector& FromB, const FVector& ToA, const FVector& ToB,
	float Damage, int32 SwingId, TArray<FGRCutResult>& Out)
{
	if (!bScanned)
	{
		ScanWorld();
	}
	// The flat plane the blade swept through this frame: it contains the blade and its motion.
	const FVector Along = (ToB - ToA).GetSafeNormal();
	const FVector Motion = ((ToA + ToB) - (FromA + FromB)) * 0.5f;
	FVector Normal = FVector::CrossProduct(Along, Motion.GetSafeNormal()).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;   // blade barely moved: cut flat
	}
	const FVector SwingDir = Motion.GetSafeNormal();

	// The lines to test: five points along the blade from last frame to this frame, plus the
	// blade itself as it is now (catches things it is already inside).
	TArray<TPair<FVector, FVector>> Lines;
	for (float T : { 0.15f, 0.4f, 0.65f, 0.85f, 1.f })
	{
		Lines.Emplace(FMath::Lerp(FromA, FromB, T), FMath::Lerp(ToA, ToB, T));
	}
	Lines.Emplace(ToA, ToB);

	// Which grid squares could hold something? Everything near the swept area.
	FBox Area(ForceInit);
	for (const FVector& P : { FromA, FromB, ToA, ToB })
	{
		Area += P;
	}
	const FIntVector Lo = CellOf(Area.Min);
	const FIntVector Hi = CellOf(Area.Max);
	TSet<int32> Candidates;
	for (int32 X = Lo.X; X <= Hi.X; ++X)
	{
		for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
		{
			for (int32 Z = Lo.Z; Z <= Hi.Z; ++Z)
			{
				if (const TArray<int32>* In = Grid.Find(FIntVector(X, Y, Z)))
				{
					Candidates.Append(*In);
				}
			}
		}
	}

	for (int32 Index : Candidates)
	{
		FEntry& E = Entries[Index];
		if (E.bGone || E.LastSwing == SwingId || !E.Comp.IsValid())
		{
			continue;
		}
		FVector Point;
		bool bHit = false;
		for (const TPair<FVector, FVector>& L : Lines)
		{
			if (Touches(E, L.Key, L.Value, Point))
			{
				bHit = true;
				break;
			}
		}
		if (!bHit)
		{
			continue;
		}
		E.LastSwing = SwingId;   // one hit per swing
		E.Health -= Damage * BladeDamage;
		FGRCutResult R;
		R.Point = Point;
		R.bTree = E.bTree;
		R.bSoft = E.bSoft;
		R.HitsLeft = FMath::Max(0, FMath::CeilToInt(E.Health - KINDA_SMALL_NUMBER));
		if (E.Health <= 0.f)
		{
			R.bCut = true;
			Cut(E, Point, Normal, SwingDir);
		}
		Out.Add(R);
	}
}

// =====================================================================================
//  The cut itself
// =====================================================================================
void AGRCutter::Cut(FEntry& E, const FVector& PlanePoint, FVector PlaneNormal, const FVector& SwingDir)
{
	E.bGone = true;
	UStaticMeshComponent* S = E.Comp.Get();
	if (!S || !S->GetStaticMesh())
	{
		return;
	}
	AActor* Owner = S->GetOwner();

	// 1. A procedural copy, in the same place. It belongs to the same actor, so if an island
	//    chunk is removed later, its cut pieces go with it.
	UProceduralMeshComponent* Piece = NewObject<UProceduralMeshComponent>(Owner ? static_cast<UObject*>(Owner) : this);
	Piece->SetUsingAbsoluteLocation(true);
	Piece->SetUsingAbsoluteRotation(true);
	Piece->SetUsingAbsoluteScale(true);
	Piece->bUseComplexAsSimpleCollision = true;
	Piece->RegisterComponent();
	Piece->SetWorldTransform(E.World);
	UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(S, 0, Piece, true);
	Piece->SetOverlayMaterial(S->GetOverlayMaterial());   // keeps the night glow on glowing plants
	if (Piece->GetNumSections() == 0)
	{
		Piece->DestroyComponent();   // couldn't read the model (see the note on "Allow CPU Access" in NOW.md)
		E.bGone = false;
		return;
	}

	// 2. Hide the original.
	if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(S); ISM && E.Item != INDEX_NONE)
	{
		FTransform Hidden = E.World;
		Hidden.SetScale3D(FVector(KINDA_SMALL_NUMBER));   // shrink that one copy to nothing (keeps the others' numbering)
		ISM->UpdateInstanceTransform(E.Item, Hidden, true, true, true);
	}
	else
	{
		S->SetVisibility(false);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	// The stump keeps the original's collision (none for plants, solid for the rest).
	if (E.bSoft)
	{
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		Piece->SetCollisionProfileName(S->GetCollisionProfileName());
	}

	// 3. Trees are cut nearly flat (a tilted trunk cut looks wrong and the tree must fall),
	//    and never right at the roots.
	if (E.bTree)
	{
		if (PlaneNormal.Z < 0.f)
		{
			PlaneNormal = -PlaneNormal;
		}
		PlaneNormal = (PlaneNormal * 0.25f + FVector::UpVector * 0.75f).GetSafeNormal();
	}
	FVector At = PlanePoint;
	const float GroundZ = E.Local.TransformBy(E.World).Min.Z;
	At.Z = FMath::Max(At.Z, GroundZ + 25.f);
	UProceduralMeshComponent* Other = nullptr;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(Piece, At, PlaneNormal, true, Other,
		EProcMeshSliceCapOption::CreateNewSectionForCap, Piece->GetMaterial(0));   // cap = same surface as the outside

	// 4. Which half falls? The one whose middle is higher up.
	auto Middle = [](UProceduralMeshComponent* M)
	{
		FVector Sum = FVector::ZeroVector;
		int32 N = 0;
		for (int32 s = 0; M && s < M->GetNumSections(); ++s)
		{
			if (const FProcMeshSection* Sec = M->GetProcMeshSection(s))
			{
				for (int32 v = 0; v < Sec->ProcVertexBuffer.Num(); v += 7)   // every 7th point is plenty
				{
					Sum += Sec->ProcVertexBuffer[v].Position;
					++N;
				}
			}
		}
		return N && M ? M->GetComponentTransform().TransformPosition(Sum / N) : FVector(0.f, 0.f, -1e9f);
	};
	if (Other)
	{
		Other->SetOverlayMaterial(S->GetOverlayMaterial());
	}
	UProceduralMeshComponent* Falling = Other;
	if (!Other || Middle(Piece).Z > Middle(Other).Z)
	{
		Falling = Piece;   // the slice kept the top in Piece (or missed: the whole thing falls)
	}
	if (!Falling)
	{
		return;
	}
	if (Falling == Piece && Other)
	{
		// The stump is Other now: give it the stump's collision.
		if (E.bSoft) { Other->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		else { Other->SetCollisionProfileName(S->GetCollisionProfileName()); }
	}

	// 5. The falling piece: a simple wrapped-up shape ("convex hull") for physics, a weight,
	//    and a push in the direction the blade was moving so it topples away from you.
	TArray<FVector> Hull;
	FBox Box(ForceInit);
	for (int32 s = 0; s < Falling->GetNumSections(); ++s)
	{
		if (const FProcMeshSection* Sec = Falling->GetProcMeshSection(s))
		{
			const int32 Step = FMath::Max(1, Sec->ProcVertexBuffer.Num() / 200);
			for (int32 v = 0; v < Sec->ProcVertexBuffer.Num(); v += Step)
			{
				Hull.Add(Sec->ProcVertexBuffer[v].Position);
				Box += Sec->ProcVertexBuffer[v].Position;
			}
		}
	}
	if (Hull.Num() < 4)
	{
		return;
	}
	Falling->bUseComplexAsSimpleCollision = false;
	Falling->ClearCollisionConvexMeshes();
	Falling->AddCollisionConvexMesh(Hull);
	Falling->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Falling->SetCollisionObjectType(ECC_PhysicsBody);
	Falling->SetCollisionResponseToAllChannels(ECR_Block);
	Falling->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);         // never blocks the player's swing
	Falling->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);   // hooks and bullets pass through
	Falling->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Falling->SetSimulatePhysics(true);
	const FVector Size = Box.GetSize() * E.World.GetScale3D().GetAbs();
	const float Mass = FMath::Clamp(Size.X * Size.Y * Size.Z * Density * (E.bSoft ? 0.15f : 1.f), 2.f, MaxPieceMass);
	Falling->SetMassOverrideInKg(NAME_None, Mass, true);
	const FVector Push = (SwingDir.IsNearlyZero() ? FVector::ForwardVector : SwingDir) * PushSpeed;
	// Push high up on the piece so a tree tips over instead of sliding.
	const FVector High = Falling->GetComponentTransform().TransformPosition(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z));
	Falling->AddImpulseAtLocation(Push * Mass * (E.bTree ? 0.6f : 1.f), E.bTree ? High : At);
	Falling->AddImpulse(FVector(0.f, 0.f, E.bTree ? 0.f : 150.f) * Mass);   // small things hop up a little

	Pieces.Add({ Falling, static_cast<float>(GetWorld()->GetTimeSeconds()) });
	while (Pieces.Num() > MaxPieces)
	{
		if (UProceduralMeshComponent* Old = Pieces[0].Mesh.Get())
		{
			Old->DestroyComponent();
		}
		Pieces.RemoveAt(0);
	}
}

void AGRCutter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bScanned)
	{
		ScanWorld();
	}
	// New actors from last frame: write them down now that their models are set.
	if (NewActors.Num() > 0)
	{
		TArray<TWeakObjectPtr<AActor>> Batch = MoveTemp(NewActors);
		NewActors.Reset();
		for (const TWeakObjectPtr<AActor>& A : Batch)
		{
			AddActor(A.Get());
		}
	}
	// Old fallen pieces disappear.
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	for (int32 i = Pieces.Num() - 1; i >= 0; --i)
	{
		UProceduralMeshComponent* M = Pieces[i].Mesh.Get();
		if (!M || Now - Pieces[i].Born > PieceLifeSeconds)
		{
			if (M)
			{
				M->DestroyComponent();
			}
			Pieces.RemoveAt(i);
		}
	}
}
