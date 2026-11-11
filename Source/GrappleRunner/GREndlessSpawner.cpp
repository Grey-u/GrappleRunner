// =====================================================================================
//  GREndlessSpawner.cpp  -  what the endless spawner DOES (see the .h for the overview)
// =====================================================================================
#include "GREndlessSpawner.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GRVine.h"
#include "GrappleRig.h"
#include "Kismet/GameplayStatics.h"

AGREndlessSpawner::AGREndlessSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The asset packs already in the project. (Paths can be changed in the Details panel.)
	for (int32 i = 1; i <= 4; ++i)
	{
		IslandTypes.Add(TSoftClassPtr<AActor>(FSoftObjectPath(FString::Printf(
			TEXT("/Game/Brushify/Meshes/FloatingIslands/FloatingIsland_0%d/BP_FloatingIsland_0%d.BP_FloatingIsland_0%d_C"), i, i, i))));
		TreeMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(
			TEXT("/Game/EuropeanBeech/Geometry/SimpleWind/SM_EuropeanBeech_Forest_0%d.SM_EuropeanBeech_Forest_0%d"), i, i))));
	}
	for (int32 i = 1; i <= 3; ++i)
	{
		FernMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(
			TEXT("/Game/KiteDemo/Environments/Foliage/Ferns/SM_Fern_0%d.SM_Fern_0%d"), i, i))));
		RootMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(
			TEXT("/Game/Brushify/Meshes/FloatingIslands/Dressing/Roots_0%d/S_Roots_0%d.S_Roots_0%d"), i, i, i))));
	}
	const TCHAR* Ruins[] = { TEXT("SM_Plains_Column_Large_Broken_01"), TEXT("SM_Plains_Column_Large_Broken_02"),
		TEXT("SM_Plains_CastleWall_Ruin_02"), TEXT("SM_Plains_CastleArch_Ruin_01") };
	for (const TCHAR* R : Ruins)
	{
		RuinMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(
			TEXT("/Game/InfinityBladeGrassLands/Environments/Plains/Env_Plains_Ruins/StaticMesh/%s.%s"), R, R))));
	}
	// RockMeshes is filled in BeginPlay from the grapple rocks already in the level, so the new
	// ones look the same.
}

void AGREndlessSpawner::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(FDateTime::Now().GetMillisecond() + 7919 * FDateTime::Now().GetSecond());

	// Which mode did the menu pick? ("?mode=endless" travels with the level.)
	const AGameModeBase* GM = GetWorld()->GetAuthGameMode();
	const FString Mode = GM ? UGameplayStatics::ParseOption(GM->OptionsString, TEXT("mode")) : FString();
	bActive = Mode.Equals(TEXT("endless"), ESearchCase::IgnoreCase) || (Mode.IsEmpty() && bRunInEditorPlay);
	if (!bActive)
	{
		SetActorTickEnabled(false);   // a team mode: leave the hand-built level alone
		return;
	}

	for (TActorIterator<AGrappleRig> It(GetWorld()); It; ++It)
	{
		Rig = *It;
		break;
	}

	// The endless route starts after the last island of the hand-built route: the landing
	// island furthest along +X that's near the route's centre line (side islands don't count).
	TArray<AActor*> Islands;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("LandingIsland")), Islands);
	float BestX = -1e9f;
	for (AActor* A : Islands)
	{
		FVector O, E;
		A->GetActorBounds(false, O, E);
		if (FMath::Abs(O.Y) < 4000.f && O.X > BestX)
		{
			BestX = O.X;
			StartTop = FVector(O.X, O.Y, O.Z + E.Z);
		}
	}
	LastTop = StartTop;

	// Use the same rock as the existing grapple rocks.
	if (RockMeshes.Num() == 0)
	{
		TArray<AActor*> Rocks;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("GrappleAnchor")), Rocks);
		for (AActor* A : Rocks)
		{
			if (UStaticMeshComponent* C = A->FindComponentByClass<UStaticMeshComponent>())
			{
				if (C->GetStaticMesh())
				{
					RockMeshes.Add(C->GetStaticMesh());
					break;
				}
			}
		}
	}
}

void AGREndlessSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive || !Rig.IsValid())
	{
		return;
	}
	const FVector P = Rig->GetBodyPosition();
	if (FurthestX == 0.f)
	{
		StartX = P.X;          // the run starts wherever the player first stands
		FurthestX = P.X;
	}
	// Fell into the mist and respawned at the start? That run is over: keep the best, start again.
	if (P.X < FurthestX - 20000.f)
	{
		Best = FMath::Max(Best, Distance);
		ClearChunks();
		LastTop = StartTop;
		FurthestX = P.X;
		Distance = 0.f;
	}
	FurthestX = FMath::Max(FurthestX, static_cast<float>(P.X));
	Distance = FMath::Max(0.f, (FurthestX - StartX) / 100.f);

	// Build ahead (a few chunks a frame at most, to avoid a hitch), remove far behind.
	for (int32 Built = 0; LastTop.X < P.X + BuildAhead && Built < 2; ++Built)
	{
		BuildChunk();
	}
	for (int32 i = Chunks.Num() - 1; i >= 0; --i)
	{
		if (Chunks[i].X < P.X - RemoveBehind)
		{
			for (TWeakObjectPtr<AActor>& A : Chunks[i].Actors)
			{
				if (A.IsValid())
				{
					A->Destroy();
				}
			}
			Chunks.RemoveAt(i);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(4202, 0.f, FColor(255, 214, 160),
			FString::Printf(TEXT("ENDLESS   %d m      best %d m"), FMath::RoundToInt(Distance), FMath::RoundToInt(FMath::Max(Best, Distance))));
	}
}

UStaticMesh* AGREndlessSpawner::Pick(const TArray<TSoftObjectPtr<UStaticMesh>>& List)
{
	return List.Num() ? List[Rng.RandRange(0, List.Num() - 1)].LoadSynchronous() : nullptr;
}

// Places a mesh, sized by its height in cm (Size > 0) or by its width (Size < 0), with its
// bottom on At. Solid = blocks the player and catches hooks.
AActor* AGREndlessSpawner::SpawnMesh(UStaticMesh* Mesh, const FVector& At, const FRotator& Turn, float Size, bool bSolid, FChunk& Into)
{
	if (!Mesh)
	{
		return nullptr;
	}
	AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(At, Turn);
	if (!A)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = A->GetStaticMeshComponent();
	C->SetMobility(EComponentMobility::Movable);           // made while playing, so it can't be "static"
	C->SetStaticMesh(Mesh);
	const FBoxSphereBounds B = Mesh->GetBounds();
	const float Native = Size > 0.f ? static_cast<float>(B.BoxExtent.Z) * 2.f : static_cast<float>(FMath::Max(B.BoxExtent.X, B.BoxExtent.Y)) * 2.f;
	const float Scale = FMath::Clamp(FMath::Abs(Size) / FMath::Max(1.f, Native), 0.05f, 20.f);
	A->SetActorScale3D(FVector(Scale));
	A->AddActorWorldOffset(FVector(0.f, 0.f, -static_cast<float>(B.Origin.Z - B.BoxExtent.Z) * Scale - 8.f));   // bottom on the ground
	if (bSolid)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
	}
	Into.Actors.Add(A);
	return A;
}

// A point on the island's top under (X, Y), if there is one there.
bool AGREndlessSpawner::GroundOn(AActor* Island, float X, float Y, FVector& OutPoint) const
{
	FVector O, E;
	Island->GetActorBounds(false, O, E);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, FVector(X, Y, O.Z + E.Z + 3000.f), FVector(X, Y, O.Z - E.Z - 100.f), ECC_Visibility);
	if (!bHit || Hit.GetActor() != Island || Hit.ImpactNormal.Z < 0.75f)
	{
		return false;
	}
	OutPoint = Hit.ImpactPoint;
	return true;
}

void AGREndlessSpawner::BuildChunk()
{
	FChunk C;
	// ---- where the next island goes
	const float X = LastTop.X + Rng.FRandRange(IslandGap.X, IslandGap.Y);
	const float Y = FMath::Clamp(static_cast<float>(LastTop.Y) + Rng.FRandRange(-1500.f, 1500.f), static_cast<float>(StartTop.Y) - SideDrift, static_cast<float>(StartTop.Y) + SideDrift);
	const float Z = FMath::Max(static_cast<float>(LastTop.Z) + Rng.FRandRange(Rise.X, Rise.Y), static_cast<float>(StartTop.Z) - 1500.f);
	C.X = X;

	AActor* Island = nullptr;
	if (IslandTypes.Num())
	{
		if (UClass* Cls = IslandTypes[Rng.RandRange(0, IslandTypes.Num() - 1)].LoadSynchronous())
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Island = GetWorld()->SpawnActor<AActor>(Cls, FVector(X, Y, Z), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), SP);
		}
	}
	if (!Island)
	{
		return;
	}
	// size it, then lift or lower it so its top is at Z
	FVector O, E;
	Island->GetActorBounds(false, O, E);
	const float S = Rng.FRandRange(IslandWidth.X, IslandWidth.Y) / FMath::Max(1.f, 2.f * static_cast<float>(FMath::Max(E.X, E.Y)));
	Island->SetActorScale3D(Island->GetActorScale3D() * S);
	Island->GetActorBounds(false, O, E);
	Island->AddActorWorldOffset(FVector(0.f, 0.f, Z - static_cast<float>(O.Z + E.Z)));
	Island->Tags.Add(FName(TEXT("LandingIsland")));
	C.Actors.Add(Island);
	const FVector Top(X, Y, Z);

	// ---- grapple rocks along the gap, 11-21 m above the line between the islands
	const float Span = static_cast<float>(Top.X - LastTop.X);
	for (float T = Rng.FRandRange(AnchorSpacing.X, AnchorSpacing.Y); T < Span - 600.f; T += Rng.FRandRange(AnchorSpacing.X, AnchorSpacing.Y))
	{
		const FVector Line = FMath::Lerp(LastTop, Top, T / Span);
		const FVector At = Line + FVector(0.f, Rng.FRandRange(-500.f, 500.f), Rng.FRandRange(AnchorHeight.X, AnchorHeight.Y));
		if (AActor* Rock = SpawnMesh(Pick(RockMeshes), At, FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), 0.f),
			-Rng.FRandRange(250.f, 450.f), true, C))
		{
			FVector RO, RE;                                         // rocks float: put the rock's middle on the point
			Rock->GetActorBounds(false, RO, RE);
			Rock->AddActorWorldOffset(At - RO);
			Rock->Tags.Add(FName(TEXT("GrappleAnchor")));
			if (Rig.IsValid())
			{
				Rig->AddAnchor(Rock);
			}
		}
	}

	// ---- the island's top: trees, ferns, one overgrown ruin, and a vine between two trees
	TArray<FVector> TreeSpots;
	auto RandomTop = [&](FVector& Out)
	{
		for (int32 Try = 0; Try < 12; ++Try)
		{
			if (GroundOn(Island, X + Rng.FRandRange(-0.6f, 0.6f) * static_cast<float>(E.X), Y + Rng.FRandRange(-0.6f, 0.6f) * static_cast<float>(E.Y), Out))
			{
				return true;
			}
		}
		return false;
	};
	FVector Spot;
	for (int32 i = 0; i < TreesPerIsland; ++i)
	{
		if (RandomTop(Spot) && SpawnMesh(Pick(TreeMeshes), Spot, FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Rng.FRandRange(1300.f, 1700.f), true, C))
		{
			TreeSpots.Add(Spot);
		}
	}
	for (int32 i = 0; i < FernsPerIsland; ++i)
	{
		if (RandomTop(Spot))
		{
			SpawnMesh(Pick(FernMeshes), Spot, FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Rng.FRandRange(80.f, 140.f), false, C);
		}
	}
	if (RandomTop(Spot))
	{
		if (AActor* Ruin = SpawnMesh(Pick(RuinMeshes), Spot, FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Rng.FRandRange(450.f, 700.f), true, C))
		{
			FVector RO, RE;
			Ruin->GetActorBounds(false, RO, RE);
			SpawnMesh(Pick(RootMeshes), FVector(RO.X, RO.Y, RO.Z + RE.Z * 0.7f), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f),
				-2.3f * static_cast<float>(FMath::Max(RE.X, RE.Y)), false, C);
		}
	}
	if (TreeSpots.Num() >= 2)
	{
		const FVector A = TreeSpots[0] + FVector(0.f, 0.f, 900.f);
		const FVector B = TreeSpots[1] + FVector(0.f, 0.f, 850.f);
		// "Deferred" spawn: set the vine's end BEFORE it's built, so it's built just once.
		AGRVine* Vine = GetWorld()->SpawnActorDeferred<AGRVine>(AGRVine::StaticClass(), FTransform(A));
		if (Vine)
		{
			Vine->EndPoint = B - A;
			Vine->Sag = static_cast<float>((B - A).Size()) * 0.18f;
			Vine->FinishSpawning(FTransform(A));
			C.Actors.Add(Vine);
			if (Rig.IsValid())
			{
				Rig->AddVine(Vine);
			}
		}
	}

	LastTop = Top;
	Chunks.Add(MoveTemp(C));
}

void AGREndlessSpawner::ClearChunks()
{
	for (FChunk& C : Chunks)
	{
		for (TWeakObjectPtr<AActor>& A : C.Actors)
		{
			if (A.IsValid())
			{
				A->Destroy();
			}
		}
	}
	Chunks.Reset();
}
