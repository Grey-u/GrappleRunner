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
#include "Kismet/GameplayStatics.h"

TWeakObjectPtr<AGRStylizedNature> AGRStylizedNature::Instance;

namespace
{
	// Where import_stylized.py puts the meshes, in EKind order.
	const TCHAR* KindMeshNames[] = { TEXT("SM_GR_Grass"), TEXT("SM_GR_GrassTall"), TEXT("SM_GR_Flowers_Pink"),
		TEXT("SM_GR_Flowers_White"), TEXT("SM_GR_Flowers_Yellow"), TEXT("SM_GR_Flowers_Blue"), TEXT("SM_GR_Bush"),
		TEXT("SM_GR_Rock_A"), TEXT("SM_GR_Rock_B") };
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
UInstancedStaticMeshComponent* AGRStylizedNature::MakeInstancer(UStaticMesh* Mesh, USceneComponent* AttachTo, bool bHierarchical, bool bShadows, float CullStart, float CullEnd)
{
	if (!Mesh || !AttachTo)
	{
		return nullptr;
	}
	AActor* Holder = AttachTo->GetOwner();
	UInstancedStaticMeshComponent* C = bHierarchical
		? NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder)
		: NewObject<UInstancedStaticMeshComponent>(Holder);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // you swing through grass and clouds
	C->SetCastShadow(bShadows);
	C->SetMobility(EComponentMobility::Movable);
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
	return EKind::Grass;
}

// Random turn and size; tilted halfway towards the slope so clumps hug the ground.
FTransform AGRStylizedNature::ClumpTransform(const FVector& At, const FVector& Normal)
{
	const FVector Up = (FVector::UpVector + Normal).GetSafeNormal();
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Up);
	const FQuat Spin(FVector::UpVector, Rng.FRandRange(0.f, 2.f * PI));
	const float S = Rng.FRandRange(ClumpScale.X, ClumpScale.Y);
	return FTransform(Tilt * Spin, At - Normal * 3.f, FVector(S));   // sink 3 cm so no roots float
}

void AGRStylizedNature::BeginPlay()
{
	Super::BeginPlay();
	Instance = this;
	Rng.Initialize(1234);   // same grass every time on the same level

	for (const TCHAR* Name : KindMeshNames)
	{
		Meshes.Add(LoadNatureMesh(Name));
	}
	if (!Meshes[0])
	{
		UE_LOG(LogTemp, Warning, TEXT("Stylized nature: meshes not found. Run Scripts/import_stylized.py in the editor once."));
		SetActorTickEnabled(false);
		return;
	}

	// ---- 1. the level's grass ----
	for (int32 k = 0; k < static_cast<int32>(EKind::Count); ++k)
	{
		const bool bSmall = k <= static_cast<int32>(EKind::FlowerBlue);
		LevelGrass.Add(MakeInstancer(Meshes[k], RootComponent, true, !bSmall,
			bSmall ? GrassCullStart : GrassCullStart * 3.f, bSmall ? GrassCullEnd : GrassCullEnd * 3.f));
	}
	ScatterOnLevel();

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

	const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector From = Me ? Me->GetActorLocation() : Viewer();
	struct FTarget { UStaticMeshComponent* Comp; double Dist; };
	TArray<FTarget> Targets;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A == this || A->IsHidden() || A->ActorHasTag(FName(TEXT("NoGrass"))))
		{
			continue;
		}
		TArray<UStaticMeshComponent*> Comps;
		A->GetComponents(Comps);
		for (UStaticMeshComponent* C : Comps)
		{
			if (!C || C->Mobility == EComponentMobility::Movable || !C->IsVisible() || C->bHiddenInGame || !C->IsCollisionEnabled()
				|| C->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block || C->IsA<UInstancedStaticMeshComponent>())
			{
				continue;   // moving things, invisible things, things you can't stand on, and other foliage get no grass
			}
			const FVector Size = C->Bounds.GetBox().GetSize();
			if (Size.X < 150.f || Size.Y < 150.f || Size.X > 120000.f || Size.Y > 120000.f)
			{
				continue;   // too small to stand on, or a giant floor / sky dome
			}
			Targets.Add({ C, FVector::Dist(C->Bounds.Origin, From) });
		}
	}
	Targets.Sort([](const FTarget& L, const FTarget& R) { return L.Dist < R.Dist; });

	int32 Traces = 0;
	const int32 MaxTraces = 400000;       // about half a second of work when the level loads
	const int32 MaxPerMesh = 6000;        // traces any one mesh may use (big ones get wider spacing)
	FCollisionQueryParams Params(TEXT("GRGrass"), true);   // true = trace the real shape, not a rough box
	for (const FTarget& T : Targets)
	{
		if (Planted >= MaxClumps || Traces >= MaxTraces)
		{
			break;
		}
		UStaticMeshComponent* C = T.Comp;
		const FBox Box = C->Bounds.GetBox();
		const FVector Size = Box.GetSize();
		const float Gap = FMath::Max(GrassSpacing, FMath::Sqrt(static_cast<float>(Size.X * Size.Y) / MaxPerMesh));
		for (float X = Box.Min.X; X < Box.Max.X && Traces < MaxTraces; X += Gap)
		{
			for (float Y = Box.Min.Y; Y < Box.Max.Y && Traces < MaxTraces; Y += Gap)
			{
				const FVector Jitter(Rng.FRandRange(-0.45f, 0.45f) * Gap, Rng.FRandRange(-0.45f, 0.45f) * Gap, 0.f);
				const FVector Top = FVector(X, Y, Box.Max.Z + 50.f) + Jitter;
				const FVector Bottom = FVector(Top.X, Top.Y, Box.Min.Z - 50.f);
				FHitResult Hit;
				++Traces;
				// The first thing hit from above must be THIS mesh (so grass isn't planted
				// under another rock), and the ground must be flat enough.
				if (!GetWorld()->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_Visibility, Params)
					|| Hit.GetComponent() != C || Hit.ImpactNormal.Z < MinFlatness)
				{
					continue;
				}
				if (FMath::PerlinNoise2D(FVector2D(Hit.ImpactPoint.X, Hit.ImpactPoint.Y) * 0.0021f) < -0.5f)
				{
					continue;   // the odd bare patch, so it looks natural, not carpeted
				}
				const EKind K = PickKind(Hit.ImpactPoint);
				if (LevelGrass[static_cast<int32>(K)])
				{
					ByKind[static_cast<int32>(K)].Add(ClumpTransform(Hit.ImpactPoint, Hit.ImpactNormal));
					Spots.Add(Hit.ImpactPoint);
					++Planted;
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
	UE_LOG(LogTemp, Log, TEXT("Stylized nature: %d meshes looked at, %d traces"), Targets.Num(), Traces);
}

// Rocks and islands spawned while playing (endless mode): same idea, but only that actor,
// and the grass is attached to it so it goes when the endless run clears it away.
void AGRStylizedNature::DressRock(AActor* Rock, int32 MaxOnIt)
{
	if (!Rock || !Meshes.IsValidIndex(0) || !Meshes[0])
	{
		return;
	}
	if (Spots.Num() > 90000)
	{
		Spots.RemoveAt(0, 30000);   // forget the oldest spots (long behind you) so the list stays small
	}
	TArray<UStaticMeshComponent*> Comps;
	Rock->GetComponents(Comps);
	FCollisionQueryParams Params(TEXT("GRGrassRock"), true);
	const float Gap = GrassSpacing * 1.15f;
	int32 Count = 0;
	for (UStaticMeshComponent* C : Comps)
	{
		if (!C || C->IsA<UInstancedStaticMeshComponent>() || !C->IsCollisionEnabled())
		{
			continue;
		}
		const FBox Box = C->Bounds.GetBox();
		TMap<int32, UInstancedStaticMeshComponent*> Sets;
		for (float X = Box.Min.X; X < Box.Max.X && Count < MaxOnIt; X += Gap)
		{
			for (float Y = Box.Min.Y; Y < Box.Max.Y && Count < MaxOnIt; Y += Gap)
			{
				const FVector Top(X + Rng.FRandRange(-0.4f, 0.4f) * Gap, Y + Rng.FRandRange(-0.4f, 0.4f) * Gap, Box.Max.Z + 50.f);
				FHitResult Hit;
				if (!C->LineTraceComponent(Hit, Top, FVector(Top.X, Top.Y, Box.Min.Z - 50.f), Params) || Hit.ImpactNormal.Z < MinFlatness)
				{
					continue;
				}
				EKind K = PickKind(Hit.ImpactPoint);
				if (MaxOnIt < 500 && (K == EKind::StoneA || K == EKind::StoneB || K == EKind::Bush))
				{
					K = EKind::Grass;   // keep the swing rocks clean of extra lumps
				}
				UInstancedStaticMeshComponent*& Set = Sets.FindOrAdd(static_cast<int32>(K));
				if (!Set)
				{
					Set = MakeInstancer(MeshFor(K), C, true, false, GrassCullStart, GrassCullEnd);
				}
				if (Set)
				{
					Set->AddInstance(ClumpTransform(Hit.ImpactPoint, Hit.ImpactNormal), true);
					Spots.Add(Hit.ImpactPoint);
					++Count;
				}
			}
		}
	}
}

// Put cloud i somewhere around Near: anywhere in the ring, or (bFar) out at the edge, so a
// cloud you've left behind reappears in the distance rather than popping up beside you.
void AGRStylizedNature::PlaceCloud(int32 i, const FVector& Near, bool bFar)
{
	FCloud& C = Clouds[i];
	const float A = Rng.FRandRange(0.f, 2.f * PI);
	const float D = bFar ? Rng.FRandRange(CloudFar * 0.75f, CloudFar * 0.95f) : Rng.FRandRange(CloudNear, CloudFar * 0.9f);
	// Most clouds sit below you (a sea of clouds under the islands); a few are at your height.
	const float Z = Rng.FRand() < 0.7f ? Rng.FRandRange(CloudHeight.X, CloudHeight.X * 0.3f) : Rng.FRandRange(CloudHeight.X * 0.3f, CloudHeight.Y);
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
