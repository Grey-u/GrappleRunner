// =====================================================================================
//  GRStylizedNature.cpp  -  what the stylized nature DOES (see the .h for the overview)
// =====================================================================================
#include "GRStylizedNature.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

TWeakObjectPtr<AGRStylizedNature> AGRStylizedNature::Instance;

namespace
{
	// What each kind of plant is, in EKind order: the StyleHex Stylized Forest pack first, our
	// own mesh as the fallback, and how tall it should end up (cm; negative = keep its size).
	// Grey: grass is FOOT height and very dense, not knee-high.
	struct FKindSource { const TCHAR* Pack; const TCHAR* Ours; float Height; };
	#define GR_SH(N) TEXT("/Game/StyleHex_Studio/Free_Packs/FREE_Stylized_Forest_Sample/Meshes/") TEXT(N)
	const FKindSource KindSources[] = {
		{ GR_SH("Foliage/SM_Fol_Grass_Clump_1"), TEXT("SM_GR_Grass"), 25.f },
		{ GR_SH("Foliage/SM_Fol_Grass_Clump_3"), TEXT("SM_GR_GrassTall"), 38.f },
		{ GR_SH("Foliage/SM_Fol_Flower_Clump_1"), TEXT("SM_GR_Flowers_Pink"), 30.f },
		{ GR_SH("Foliage/SM_Fol_Flower_Clump_2"), TEXT("SM_GR_Flowers_White"), 30.f },
		{ GR_SH("Foliage/SM_Fol_Flower_Clump_3"), TEXT("SM_GR_Flowers_Yellow"), 30.f },
		{ nullptr, TEXT("SM_GR_Flowers_Blue"), 30.f },
		{ GR_SH("Foliage/SM_Fol_Bush_1"), TEXT("SM_GR_Bush"), 55.f },
		{ GR_SH("Stones/SM_Stone_1"), TEXT("SM_GR_Rock_A"), 35.f },
		{ GR_SH("Stones/SM_Stone_2"), TEXT("SM_GR_Rock_B"), 45.f },
		{ nullptr, TEXT("SM_GR_Moss_A"), -1.f },
		{ nullptr, TEXT("SM_GR_Moss_B"), -1.f },
		{ GR_SH("Foliage/SM_Fol_Grass_Clump_2"), TEXT("SM_GR_Grass"), 25.f },
		{ GR_SH("Foliage/SM_Fol_Grass_Clump_4"), TEXT("SM_GR_Grass"), 28.f },
		{ GR_SH("Foliage/SM_Fol_Grass_Clump_5"), TEXT("SM_GR_Grass"), 22.f },
		{ GR_SH("Foliage/SM_Fol_Grass_Clump_3"), TEXT("SM_GR_Grass"), 26.f },   // the tall clump, kept short
	};
	#undef GR_SH

	// Plant words: a mesh or actor with one of these in its name is a plant (tree, fern, bush,
	// vine...), never ground. The grass trace looks straight through plants.
	bool HasPlantWord(const FString& S)
	{
		static const TCHAR* Words[] = { TEXT("Tree"), TEXT("Fern"), TEXT("Plant"), TEXT("Foliage"), TEXT("Beech"), TEXT("Bush"),
			TEXT("Grass"), TEXT("Vine"), TEXT("Root"), TEXT("Leaf"), TEXT("Leaves"), TEXT("Ivy"), TEXT("Flower"), TEXT("Banana"),
			TEXT("Pandanus"), TEXT("Palm"), TEXT("Fol_"), TEXT("PN_"), TEXT("Moss") };
		for (const TCHAR* W : Words)
		{
			if (S.Contains(W))
			{
				return true;
			}
		}
		return false;
	}

	// A whole actor that is a plant (by its class or its name in the Outliner). NOT by what's
	// on it: a Brushify island carries ferns and bushes but is still ground (that mistake
	// made the start island get no grass, 14 Nov).
	bool IsPlant(const AActor* A)
	{
		if (HasPlantWord(A->GetClass()->GetName()))
		{
			return true;
		}
#if WITH_EDITOR
		if (HasPlantWord(A->GetActorLabel(false)))   // the name shown in the editor's Outliner
		{
			return true;
		}
#endif
		return false;
	}

	// One piece (a fern on an island, a tree) is a plant if its mesh's name says so.
	bool IsPlantPiece(const UPrimitiveComponent* C)
	{
		const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
		return SM && SM->GetStaticMesh() && HasPlantWord(SM->GetStaticMesh()->GetPathName());
	}

	// Trace down to the GROUND: if the first thing hit is a plant piece, look through it and
	// keep going (up to 8 layers of leaves).
	bool GroundTrace(UWorld* World, const FVector& From, const FVector& To, FCollisionQueryParams& Params, FHitResult& Hit)
	{
		for (int32 Layer = 0; Layer < 8; ++Layer)
		{
			if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
			{
				return false;
			}
			if (!IsPlantPiece(Hit.GetComponent()))
			{
				return true;
			}
			Params.AddIgnoredComponent(Hit.GetComponent());   // see through this plant from now on
		}
		return false;
	}

	UStaticMesh* LoadPackMesh(const TCHAR* Path)
	{
		if (!Path)
		{
			return nullptr;
		}
		const FString P(Path);
		const FString Name = FPaths::GetBaseFilename(P);
		return LoadObject<UStaticMesh>(nullptr, *(P + TEXT(".") + Name));
	}
	const TCHAR* CloudNames[] = { TEXT("SM_GR_Cloud_A"), TEXT("SM_GR_Cloud_B"), TEXT("SM_GR_Cloud_C") };
	const TCHAR* ButterflyNames[] = { TEXT("SM_GR_Butterfly_Pink"), TEXT("SM_GR_Butterfly_Blue"), TEXT("SM_GR_Butterfly_Yellow") };

	UStaticMesh* LoadNatureMesh(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/GrappleRunner/Nature/%s.%s"), Name, Name);
		return LoadObject<UStaticMesh>(nullptr, *Path);   // null if import_stylized.py hasn't been run yet
	}
}

AGRStylizedNature::AGRStylizedNature()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);   // so baked (static) grass can hang off it
}

AGRStylizedNature* AGRStylizedNature::Get()
{
	return Instance.Get();
}

// Where the player's eyes are (the camera position; it doesn't decide anything about gameplay).
FVector AGRStylizedNature::Viewer() const
{
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		return Cam->GetCameraLocation();
	}
	return GetActorLocation();
}

// One instanced component: draws many copies of one mesh cheaply.
//   bHierarchical = the "HISM" kind, which can skip whole groups of far-away copies (best for
//   thousands of grass clumps that never move). Birds, clouds and butterflies move every
//   frame, so they use the plain kind.
UInstancedStaticMeshComponent* AGRStylizedNature::MakeInstancer(UStaticMesh* Mesh, USceneComponent* AttachTo, bool bHierarchical, bool bShadows, float CullStart, float CullEnd, bool bForSaving)
{
	if (!Mesh || !AttachTo)
	{
		return nullptr;
	}
	AActor* Holder = AttachTo->GetOwner();
	const EObjectFlags Flags = bForSaving ? RF_Transactional : RF_NoFlags;   // saved with the level when baked in the editor
	UInstancedStaticMeshComponent* C = bHierarchical
		? NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder, NAME_None, Flags)
		: NewObject<UInstancedStaticMeshComponent>(Holder, NAME_None, Flags);
	if (bForSaving)
	{
		C->CreationMethod = EComponentCreationMethod::Instance;
	}
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // you swing through grass and clouds
	C->SetCastShadow(bShadows);
	C->SetMobility(bForSaving ? EComponentMobility::Static : EComponentMobility::Movable);
	C->SetUsingAbsoluteLocation(true);                        // instances are given in world space
	C->SetUsingAbsoluteRotation(true);
	C->SetUsingAbsoluteScale(true);
	if (CullEnd > 0.f)
	{
		C->SetCullDistances(static_cast<int32>(CullStart), static_cast<int32>(CullEnd));
	}
	C->RegisterComponent();
	C->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepWorldTransform);
	Holder->AddInstanceComponent(C);
	return C;
}

UStaticMesh* AGRStylizedNature::MeshFor(EKind Kind) const
{
	const int32 i = static_cast<int32>(Kind);
	return Meshes.IsValidIndex(i) ? Meshes[i].Get() : nullptr;
}

// What grows here? Smooth "noise" (a pattern of gentle random hills) is used instead of plain
// dice, so flowers come in meadows of one colour and tall grass in patches, like real fields.
AGRStylizedNature::EKind AGRStylizedNature::PickKind(const FVector& At)
{
	const float Meadow = FMath::PerlinNoise2D(FVector2D(At.X, At.Y) * 0.0009f);          // -1..1, changes every ~10 m
	const float Colour = FMath::PerlinNoise2D(FVector2D(At.Y, At.X) * 0.0006f + 17.3f);
	const float Tall = FMath::PerlinNoise2D(FVector2D(At.X, At.Y) * 0.0013f + 41.f);
	const float Roll = Rng.FRand();
	if (Roll < StoneChance)
	{
		return Rng.FRand() < 0.5f ? EKind::StoneA : EKind::StoneB;
	}
	if (Roll < StoneChance + BushChance)
	{
		return EKind::Bush;
	}
	if (Meadow > 0.1f && Rng.FRand() < FlowerChance * 2.5f)
	{
		return Colour < -0.25f ? EKind::FlowerBlue : Colour < 0.f ? EKind::FlowerWhite : Colour < 0.25f ? EKind::FlowerPink : EKind::FlowerYellow;
	}
	if (Rng.FRand() < FlowerChance * 0.25f)                                                  // the odd stray flower anywhere
	{
		return static_cast<EKind>(static_cast<int32>(EKind::FlowerPink) + Rng.RandRange(0, 3));
	}
	if (Tall > 0.2f && Rng.FRand() < TallGrassChance * 3.f)
	{
		return EKind::Tall;
	}
	const int32 G = Rng.RandRange(0, 4);   // one of the five grass clumps
	return G == 0 ? EKind::Grass : static_cast<EKind>(static_cast<int32>(EKind::Grass2) + G - 1);
}

// Random turn and size; tilted halfway towards the slope so clumps hug the ground.
FTransform AGRStylizedNature::ClumpTransform(const FVector& At, const FVector& Normal, EKind Kind)
{
	const FVector Up = (FVector::UpVector + Normal).GetSafeNormal();
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Up);
	const FQuat Spin(FVector::UpVector, Rng.FRandRange(0.f, 2.f * PI));
	const int32 k = static_cast<int32>(Kind);
	const float S = (KindScale.IsValidIndex(k) ? KindScale[k] : 1.f) * Rng.FRandRange(0.75f, 1.3f);   // right height, give or take
	return FTransform(Tilt * Spin, At - Normal * 3.f, FVector(S));   // sink 3 cm so no roots float
}

// Moss lies fully along the surface (tops, slopes and sides), unlike grass which grows up.
FTransform AGRStylizedNature::MossTransform(const FVector& At, const FVector& Normal)
{
	const FQuat Lie = FQuat::FindBetweenNormals(FVector::UpVector, Normal);
	const FQuat Spin(FVector::UpVector, Rng.FRandRange(0.f, 2.f * PI));
	const float S = Rng.FRandRange(1.0f, 2.2f);
	return FTransform(Lie * Spin, At - Normal * 2.f, FVector(S));
}

// Load the meshes for each kind (StyleHex first, ours as fallback) and how much to scale each.
bool AGRStylizedNature::LoadMeshes()
{
	Meshes.Reset();
	KindScale.Reset();
	for (const FKindSource& K : KindSources)
	{
		UStaticMesh* M = LoadPackMesh(K.Pack);
		if (!M)
		{
			M = LoadNatureMesh(K.Ours);   // the pack isn't there: use our own
		}
		Meshes.Add(M);
		float Scale = 1.f;
		if (M && K.Height > 0.f)
		{
			const float NativeHeight = FMath::Max(1.f, static_cast<float>(M->GetBounds().BoxExtent.Z) * 2.f);
			Scale = K.Height / NativeHeight;
		}
		KindScale.Add(Scale);
	}
	return Meshes.Num() > 0 && Meshes[0] != nullptr;
}

// Make one instanced component per kind of plant, then plant the whole level.
//   bForSaving = called in the editor (PlantGrassNow): the components are made so they're
//   saved with the level, and don't move (cheaper).
void AGRStylizedNature::PlantLevelGrass(bool bForSaving)
{
	for (UInstancedStaticMeshComponent* Old : LevelGrass)
	{
		if (Old)
		{
			RemoveInstanceComponent(Old);
			Old->DestroyComponent();
		}
	}
	LevelGrass.Reset();
	Spots.Reset();
	Planted = 0;
	Rng.Initialize(1234);   // same grass every time on the same level
	for (int32 k = 0; k < static_cast<int32>(EKind::Count); ++k)
	{
		const bool bSmall = k <= static_cast<int32>(EKind::FlowerBlue) || k >= static_cast<int32>(EKind::MossA);   // grass, flowers, moss
		UInstancedStaticMeshComponent* C = MakeInstancer(Meshes[k], RootComponent, true, !bSmall,
			bSmall ? GrassCullStart : GrassCullStart * 3.f, bSmall ? GrassCullEnd : GrassCullEnd * 3.f, bForSaving);
		LevelGrass.Add(C);
	}
	ScatterOnLevel();
}

void AGRStylizedNature::PlantGrassNow()
{
	if (!LoadMeshes())
	{
		UE_LOG(LogTemp, Warning, TEXT("Stylized nature: meshes not found. Run Scripts/import_stylized.py first."));
		return;
	}
	Modify();
	PlantLevelGrass(true);
	bBaked = true;
	UE_LOG(LogTemp, Log, TEXT("Stylized nature: baked %d clumps into the level"), Planted);
}

void AGRStylizedNature::BeginPlay()
{
	Super::BeginPlay();
	Instance = this;
	Rng.Initialize(1234);   // same grass every time on the same level

	if (!LoadMeshes())
	{
		UE_LOG(LogTemp, Warning, TEXT("Stylized nature: meshes not found. Run Scripts/import_stylized.py in the editor once."));
		SetActorTickEnabled(false);
		return;
	}

	// ---- 1. the level's grass ----
	if (bBaked)
	{
		// Planted in the editor already (PlantGrassNow, run by bake_grass.py): it's saved in the
		// level, so just note where it is, for the butterflies.
		for (UInstancedStaticMeshComponent* Set : LevelGrass)
		{
			for (int32 i = 0; Set && i < Set->GetInstanceCount() && Spots.Num() < 40000; i += 3)
			{
				FTransform T;
				Set->GetInstanceTransform(i, T, true);
				Spots.Add(T.GetLocation());
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Stylized nature: using the grass baked into the level"));
	}
	else
	{
		PlantLevelGrass(false);
	}

	const FVector Eye = Viewer();
	FlockFollow = Eye;

	// ---- 2. clouds ----
	for (const TCHAR* Name : CloudNames)
	{
		CloudSets.Add(MakeInstancer(LoadNatureMesh(Name), RootComponent, false, false, 0.f, 0.f));
	}
	for (int32 i = 0; i < CloudCount; ++i)
	{
		FCloud C;
		C.Set = i % CloudSets.Num();
		if (!CloudSets[C.Set])
		{
			continue;
		}
		C.Index = CloudSets[C.Set]->AddInstance(FTransform::Identity, true);
		Clouds.Add(C);
		PlaceCloud(Clouds.Num() - 1, Eye, false);
	}

	// ---- 3. birds: each flock circles a point out in the sky ----
	BirdSet = MakeInstancer(LoadNatureMesh(TEXT("SM_GR_Bird")), RootComponent, false, false, 0.f, 0.f);
	if (BirdSet)
	{
		for (int32 f = 0; f < Flocks; ++f)
		{
			FFlock F;
			const float A = Rng.FRandRange(0.f, 2.f * PI);
			F.Offset = FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Rng.FRandRange(4000.f, 12000.f) + FVector(0.f, 0.f, Rng.FRandRange(-1500.f, 4000.f));
			F.Radius = Rng.FRandRange(2500.f, 6000.f);
			F.Angle = Rng.FRandRange(0.f, 2.f * PI);
			F.Speed = (Rng.FRand() < 0.5f ? -1.f : 1.f) * Rng.FRandRange(900.f, 1400.f) / F.Radius;   // ~10-14 m/s along the circle
			FlockList.Add(F);
			for (int32 b = 0; b < BirdsPerFlock; ++b)
			{
				// a loose V: the leader in front, the others behind and to the sides
				const float Side = (b % 2 == 0 ? 1.f : -1.f) * ((b + 1) / 2) * 260.f;
				BirdLocal.Add(FVector(-FMath::Abs(Side) * 0.9f + Rng.FRandRange(-60.f, 60.f), Side, Rng.FRandRange(-80.f, 80.f)));
				BirdSet->AddInstance(FTransform::Identity, true);
			}
		}
	}

	// ---- 4. butterflies ----
	for (const TCHAR* Name : ButterflyNames)
	{
		ButterflySets.Add(MakeInstancer(LoadNatureMesh(Name), RootComponent, false, false, 0.f, 0.f));
	}
	for (int32 i = 0; i < Butterflies; ++i)
	{
		FButterfly B;
		B.Set = i % ButterflySets.Num();
		if (!ButterflySets[B.Set])
		{
			continue;
		}
		B.Index = ButterflySets[B.Set]->AddInstance(FTransform(FQuat::Identity, Eye, FVector(0.001f)), true);   // hidden until it has a home
		B.Phase = Rng.FRandRange(0.f, 2.f * PI);
		B.Pos = Eye;
		ButterflyList.Add(B);
	}
	UE_LOG(LogTemp, Log, TEXT("Stylized nature: %d grass clumps, %d clouds, %d birds, %d butterflies"),
		Planted, Clouds.Num(), BirdLocal.Num(), ButterflyList.Num());
}

void AGRStylizedNature::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Instance.Get() == this)
	{
		Instance.Reset();
	}
	Super::EndPlay(Reason);
}

// Look down on every solid, non-moving, VISIBLE thing in the level and plant grass on its flat
// tops. Nearest first (from where you start), and each mesh gets a fair share, so one huge
// floor far below can't use up all the planting before your islands get any.
void AGRStylizedNature::ScatterOnLevel()
{
	TArray<TArray<FTransform>> ByKind;   // collected first, then added in one go per kind (much faster)
	ByKind.SetNum(static_cast<int32>(EKind::Count));

	// Where to start: the player start (editor) or you (playing).
	FVector From = GetActorLocation();
	if (const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		From = Me->GetActorLocation();
	}
	else
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			From = It->GetActorLocation();
			break;
		}
	}

	// Every visible, solid, still actor you could stand on: islands (also the ones built from
	// instanced pieces, which the first version skipped), rocks, ruins, ledges.
	struct FTarget { AActor* Actor; FBox Box; double Dist; };
	TArray<FTarget> Targets;
	FCollisionQueryParams Params(TEXT("GRGrass"), true);   // true = trace the real shape, not a rough box
	Params.AddIgnoredActor(this);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A == this || A->IsHidden() || A->IsA<APawn>() || !A->GetRootComponent())
		{
			continue;
		}
		if (IsPlant(A))
		{
			Params.AddIgnoredActor(A);   // see through trees and plants down to the ground
			continue;
		}
		if (A->ActorHasTag(FName(TEXT("NoGrass"))) || A->GetRootComponent()->Mobility == EComponentMobility::Movable)
		{
			continue;
		}
		bool bSolid = false;
		TArray<UPrimitiveComponent*> Prims;
		A->GetComponents(Prims);
		for (const UPrimitiveComponent* P : Prims)
		{
			if (P && P->IsVisible() && P->IsCollisionEnabled() && P->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block)
			{
				bSolid = true;
				break;
			}
		}
		if (!bSolid)
		{
			continue;
		}
		FVector O, E;
		A->GetActorBounds(true, O, E);
		if (E.X < 75.f || E.Y < 75.f || E.X > 60000.f || E.Y > 60000.f)
		{
			continue;   // too small to stand on, or a giant floor / sky dome
		}
		Targets.Add({ A, FBox(O - E, O + E), FVector::Dist(O, From) });
	}
	Targets.Sort([](const FTarget& L, const FTarget& R) { return L.Dist < R.Dist; });

	int32 Traces = 0;
	const int32 MaxTraces = 2500000;      // baked in the editor, so it can take its time
	const int32 MaxPerActor = 60000;      // traces any one actor may use (huge ones get wider spacing)
	for (const FTarget& T : Targets)
	{
		if (Planted >= MaxClumps || Traces >= MaxTraces)
		{
			break;
		}
		const FBox& Box = T.Box;
		const FVector Size = Box.GetSize();
		const float Gap = FMath::Max(GrassSpacing, FMath::Sqrt(static_cast<float>(Size.X * Size.Y) / MaxPerActor));
		for (float X = Box.Min.X; X < Box.Max.X && Traces < MaxTraces; X += Gap)
		{
			for (float Y = Box.Min.Y; Y < Box.Max.Y && Traces < MaxTraces; Y += Gap)
			{
				const FVector Top = FVector(X + Rng.FRandRange(-0.45f, 0.45f) * Gap, Y + Rng.FRandRange(-0.45f, 0.45f) * Gap, Box.Max.Z + 50.f);
				FHitResult Hit;
				++Traces;
				// The first thing hit from above must be part of THIS actor, and flat enough.
				if (!GroundTrace(GetWorld(), Top, FVector(Top.X, Top.Y, Box.Min.Z - 50.f), Params, Hit)
					|| Hit.GetActor() != T.Actor || Hit.ImpactNormal.Z < MinFlatness)
				{
					continue;
				}
				if (FMath::PerlinNoise2D(FVector2D(Hit.ImpactPoint.X, Hit.ImpactPoint.Y) * 0.0021f) < -0.55f)
				{
					continue;   // the odd bare patch, so it looks natural, not carpeted
				}
				const EKind K = PickKind(Hit.ImpactPoint);
				if (LevelGrass[static_cast<int32>(K)])
				{
					ByKind[static_cast<int32>(K)].Add(ClumpTransform(Hit.ImpactPoint, Hit.ImpactNormal, K));
					Spots.Add(Hit.ImpactPoint);
					++Planted;
				}
				if (Rng.FRand() < MossUnderGrass)
				{
					const int32 M = static_cast<int32>(Rng.FRand() < 0.5f ? EKind::MossA : EKind::MossB);
					ByKind[M].Add(MossTransform(Hit.ImpactPoint, Hit.ImpactNormal));
				}
			}
		}
	}
	for (int32 k = 0; k < ByKind.Num(); ++k)
	{
		if (LevelGrass[k] && ByKind[k].Num() > 0)
		{
			LevelGrass[k]->AddInstances(ByKind[k], false, true);   // (transforms, don't need the numbers back, world space)
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Stylized nature: %d actors looked at, %d traces, %d clumps"), Targets.Num(), Traces, Planted);
}

// Rocks and islands spawned while playing (endless mode): same idea, but only that actor,
// and the grass is attached to it so it goes when the endless run clears it away.
void AGRStylizedNature::DressRock(AActor* Rock, int32 MaxOnIt)
{
	if (!Rock || !Rock->GetRootComponent() || !Meshes.IsValidIndex(0) || !Meshes[0])
	{
		return;
	}
	if (Spots.Num() > 90000)
	{
		Spots.RemoveAt(0, 30000);   // forget the oldest spots (long behind you) so the list stays small
	}
	// Look down on the whole actor from above. Any of its parts counts as ground (Brushify's
	// islands are built from instanced pieces, which the first version skipped: no grass).
	FVector Origin, Extent;
	Rock->GetActorBounds(true, Origin, Extent);
	const FBox Box(Origin - Extent, Origin + Extent);
	FCollisionQueryParams Params(TEXT("GRGrassRock"), true);
	// (The spawner adds an island's trees AFTER this, so nothing blocks the ground here.)
	const float Gap = FMath::Max(GrassSpacing * 0.85f, FMath::Sqrt(static_cast<float>(4.f * Extent.X * Extent.Y) / (MaxOnIt * 1.6f)));
	TMap<int32, UInstancedStaticMeshComponent*> Sets;
	auto Plant = [&](EKind K, const FTransform& Xf)
	{
		UInstancedStaticMeshComponent*& Set = Sets.FindOrAdd(static_cast<int32>(K));
		if (!Set)
		{
			Set = MakeInstancer(MeshFor(K), Rock->GetRootComponent(), true, false, GrassCullStart, GrassCullEnd);
		}
		if (Set)
		{
			Set->AddInstance(Xf, true);
		}
	};
	int32 Count = 0;
	for (float X = Box.Min.X; X < Box.Max.X && Count < MaxOnIt; X += Gap)
	{
		for (float Y = Box.Min.Y; Y < Box.Max.Y && Count < MaxOnIt; Y += Gap)
		{
			const FVector Top(X + Rng.FRandRange(-0.4f, 0.4f) * Gap, Y + Rng.FRandRange(-0.4f, 0.4f) * Gap, Box.Max.Z + 50.f);
			FHitResult Hit;
			if (!GroundTrace(GetWorld(), Top, FVector(Top.X, Top.Y, Box.Min.Z - 50.f), Params, Hit)
				|| Hit.GetActor() != Rock || Hit.ImpactNormal.Z < MinFlatness)
			{
				continue;
			}
			EKind K = PickKind(Hit.ImpactPoint);
			if (MaxOnIt < 1000 && (K == EKind::StoneA || K == EKind::StoneB || K == EKind::Bush))
			{
				K = EKind::Grass;   // keep the swing rocks clean of extra lumps
			}
			Plant(K, ClumpTransform(Hit.ImpactPoint, Hit.ImpactNormal, K));
			Spots.Add(Hit.ImpactPoint);
			++Count;
			if (Rng.FRand() < MossUnderGrass)
			{
				Plant(Rng.FRand() < 0.5f ? EKind::MossA : EKind::MossB, MossTransform(Hit.ImpactPoint, Hit.ImpactNormal));
			}
		}
	}

	// Moss down the sides: look at the rock from four sides and cushion every wall and slope
	// that isn't facing the ground (undersides stay bare rock).
	int32 Moss = 0;
	const FVector Sides[4] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
	const float MossGap = Gap * 1.1f;
	for (const FVector& Dir : Sides)
	{
		if (MossOnSides <= 0.f)
		{
			break;   // side moss is off (Grey didn't like it, 14 Nov)
		}
		const FVector Across(-Dir.Y, Dir.X, 0.f);
		const float Wide = static_cast<float>(FMath::Abs(Across.X) * Extent.X + FMath::Abs(Across.Y) * Extent.Y);
		const float Reach = static_cast<float>(FMath::Abs(Dir.X) * Extent.X + FMath::Abs(Dir.Y) * Extent.Y) + 50.f;
		for (float U = -Wide; U < Wide && Moss < MaxOnIt; U += MossGap)
		{
			for (float Z = -static_cast<float>(Extent.Z); Z < Extent.Z && Moss < MaxOnIt; Z += MossGap)
			{
				const FVector Start = Origin - Dir * Reach + Across * (U + Rng.FRandRange(-0.4f, 0.4f) * MossGap) + FVector(0.f, 0.f, Z + Rng.FRandRange(-0.4f, 0.4f) * MossGap);
				FHitResult Hit;
				if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * (Reach * 2.f), ECC_Visibility, Params)
					|| Hit.GetActor() != Rock || Hit.ImpactNormal.Z < -0.25f || Rng.FRand() > MossOnSides)
				{
					continue;
				}
				Plant(Rng.FRand() < 0.5f ? EKind::MossA : EKind::MossB, MossTransform(Hit.ImpactPoint, Hit.ImpactNormal));
				++Moss;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Stylized nature: dressed %s with %d clumps and %d moss"), *Rock->GetName(), Count, Moss);
}

// Put cloud i somewhere around Near: anywhere in the ring, or (bFar) out at the edge, so a
// cloud you've left behind reappears in the distance rather than popping up beside you.
void AGRStylizedNature::PlaceCloud(int32 i, const FVector& Near, bool bFar)
{
	FCloud& C = Clouds[i];
	const float A = Rng.FRandRange(0.f, 2.f * PI);
	const float D = bFar ? Rng.FRandRange(CloudFar * 0.75f, CloudFar * 0.95f) : Rng.FRandRange(CloudNear, CloudFar * 0.9f);
	// Clouds sit below you: a sea of clouds under the islands (never in your way).
	const float Z = Rng.FRandRange(CloudHeight.X, CloudHeight.Y);
	C.Pos = Near + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Z);
	C.Scale = Rng.FRandRange(CloudScale.X, CloudScale.Y);
	C.Yaw = Rng.FRandRange(0.f, 360.f);
}

void AGRStylizedNature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	const FVector Eye = Viewer();

	// ---- clouds drift; far-behind ones move to the distance ----
	TArray<TArray<FTransform>> CloudXf;
	CloudXf.SetNum(CloudSets.Num());
	for (int32 i = 0; i < Clouds.Num(); ++i)
	{
		FCloud& C = Clouds[i];
		C.Pos += Wind * DeltaSeconds;
		if (FVector::Dist2D(C.Pos, Eye) > CloudFar)
		{
			PlaceCloud(i, Eye, true);
		}
		TArray<FTransform>& T = CloudXf[C.Set];
		if (T.Num() <= C.Index)
		{
			T.SetNum(C.Index + 1);
		}
		T[C.Index] = FTransform(FRotator(0.f, C.Yaw, 0.f), C.Pos, FVector(C.Scale));
	}
	for (int32 s = 0; s < CloudSets.Num(); ++s)
	{
		if (CloudSets[s] && CloudXf[s].Num() > 0)
		{
			CloudSets[s]->BatchUpdateInstancesTransforms(0, CloudXf[s], true, true, true);
		}
	}

	// ---- birds circle their flock's point, which slowly follows you ----
	if (BirdSet && FlockList.Num() > 0)
	{
		FlockFollow = FMath::VInterpTo(FlockFollow, Eye, DeltaSeconds, 0.25f);
		TArray<FTransform> BirdXf;
		BirdXf.Reserve(BirdLocal.Num());
		for (int32 f = 0; f < FlockList.Num(); ++f)
		{
			FFlock& F = FlockList[f];
			F.Angle += F.Speed * DeltaSeconds;
			const FVector Centre = FlockFollow + F.Offset;
			const FVector Round(FMath::Cos(F.Angle), FMath::Sin(F.Angle), 0.f);
			const FVector Ahead = FVector(-Round.Y, Round.X, 0.f) * FMath::Sign(F.Speed);   // direction of travel
			const FRotator Facing(-6.f * FMath::Sin(Clock * 0.7f + f), Ahead.Rotation().Yaw + BirdYawOffset, -18.f * FMath::Sign(F.Speed));   // bank into the turn
			const FVector Lead = Centre + Round * F.Radius + FVector(0.f, 0.f, 300.f * FMath::Sin(Clock * 0.3f + f * 1.7f));
			for (int32 b = 0; b < BirdsPerFlock; ++b)
			{
				const int32 i = f * BirdsPerFlock + b;
				if (!BirdLocal.IsValidIndex(i))
				{
					break;
				}
				const FVector Bob(0.f, 0.f, 40.f * FMath::Sin(Clock * 1.3f + i));
				BirdXf.Add(FTransform(Facing, Lead + Facing.RotateVector(BirdLocal[i] * BirdScale) + Bob, FVector(BirdScale)));
			}
		}
		BirdSet->BatchUpdateInstancesTransforms(0, BirdXf, true, true, true);
	}

	// ---- butterflies flutter around a grass spot near you ----
	if (ButterflyList.Num() > 0)
	{
		TArray<TArray<FTransform>> Xf;
		Xf.SetNum(ButterflySets.Num());
		for (FButterfly& B : ButterflyList)
		{
			// Find a new home now and then, or when you've moved away. Try a few random grass
			// spots (half from the newest, which are the ones the endless run just made).
			if (Clock > B.NextMove || FVector::Dist(B.Home, Eye) > ButterflyRange * 1.3f)
			{
				float Best = ButterflyRange;
				bool bFound = false;
				for (int32 t = 0; t < 24 && Spots.Num() > 0; ++t)
				{
					const int32 Newest = FMath::Max(0, Spots.Num() - 3000);
					const int32 Pick = t % 2 == 0 ? Rng.RandRange(Newest, Spots.Num() - 1) : Rng.RandRange(0, Spots.Num() - 1);
					const float D = FVector::Dist(Spots[Pick], Eye);
					if (D < Best && D > 250.f)
					{
						Best = D;
						B.Home = Spots[Pick];
						bFound = true;
					}
				}
				if (bFound)
				{
					if (B.bHidden)
					{
						B.Pos = B.Home + FVector(0.f, 0.f, 80.f);
					}
					B.bHidden = false;
					B.NextMove = Clock + Rng.FRandRange(6.f, 14.f);
				}
				else if (FVector::Dist(B.Home, Eye) > ButterflyRange * 1.3f)
				{
					B.bHidden = true;   // nowhere green nearby (mid-air): tuck it away
					B.NextMove = Clock + 0.5f;
				}
			}
			// Wobbly loops a little above the grass.
			const float T = Clock * 0.9f + B.Phase;
			const FVector Target = B.Home + FVector(FMath::Sin(T * 1.3f) * 140.f, FMath::Cos(T * 0.9f) * 140.f, 70.f + 45.f * FMath::Sin(T * 2.1f));
			const FVector Old = B.Pos;
			B.Pos = FMath::VInterpTo(B.Pos, Target, DeltaSeconds, 1.6f);
			const FVector Move = B.Pos - Old;
			const float Yaw = Move.SizeSquared2D() > 0.01f ? Move.Rotation().Yaw : 0.f;
			const float S = B.bHidden ? 0.001f : ButterflyScale;
			TArray<FTransform>& T2 = Xf[B.Set];
			if (T2.Num() <= B.Index)
			{
				T2.SetNum(B.Index + 1);
			}
			T2[B.Index] = FTransform(FRotator(0.f, Yaw, 0.f), B.Pos, FVector(S));
		}
		for (int32 s = 0; s < ButterflySets.Num(); ++s)
		{
			if (ButterflySets[s] && Xf[s].Num() > 0)
			{
				ButterflySets[s]->BatchUpdateInstancesTransforms(0, Xf[s], true, true, true);
			}
		}
	}
}
