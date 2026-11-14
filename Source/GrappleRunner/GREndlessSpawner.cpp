// =====================================================================================
//  GREndlessSpawner.cpp  -  what the endless spawner DOES (see the .h for the overview)
// =====================================================================================
#include "GREndlessSpawner.h"
#include "GRCompanion.h"   // the AI partner (Endless + AI partner mode)
#include "GRGasPod.h"      // gas canisters to slice
#include "GRStylizedNature.h"   // anime grass and flowers on the new rocks and islands

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
	// Caves are built from Brushify's cliffs and rocks (solid all the way round).
	// (Only rocks that are solid all the way round. The Granite and Nordic "cliff" scans are
	// open at the back, so from behind they look like paper: Grey, 14 Nov. Not used.)
	// (anime_world.py measured them: these Brushify cliffs and rocks have no open edges. The
	// Megascans scans, including the mossy rock formation, are open shells.)
	const TCHAR* CaveParts[] = {
		TEXT("/Game/Brushify/Meshes/Cliffs/Meshes/Cliff_Spiky_01/S_Cliff_Spiky_01.S_Cliff_Spiky_01"),
		TEXT("/Game/Brushify/Meshes/Cliffs/Meshes/Cliff_Spiky_02/S_Cliff_Spiky_02.S_Cliff_Spiky_02"),
		TEXT("/Game/Brushify/Meshes/Cliffs/Meshes/Cliff_Large_02/S_Cliff_Large_02.S_Cliff_Large_02"),
		TEXT("/Game/Brushify/Materials/Landscape/ProceduralMeshes/Rocks/Rock_01/S_Rock_01.S_Rock_01") };
	for (const TCHAR* Path : CaveParts)
	{
		CaveMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path)));
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
	// Pressing Play in the editor (no menu, so no mode): run only in the endless level itself.
	// The floating islands are the multiplayer map now, so they stay as built.
	const bool bEndlessMap = GetWorld()->GetMapName().Contains(TEXT("Endless"));
	bActive = Mode.Equals(TEXT("endless"), ESearchCase::IgnoreCase) || (Mode.IsEmpty() && bRunInEditorPlay && bEndlessMap);
	UE_LOG(LogTemp, Log, TEXT("Endless spawner: map %s, mode '%s', active %d"), *GetWorld()->GetMapName(), *Mode, bActive ? 1 : 0);
	if (!bActive)
	{
		SetActorTickEnabled(false);   // a team mode: leave the hand-built level alone
		return;
	}

	// Endless rules: no free refill on the ground (only canisters refill), and with the
	// "AI partner" option a second soldier flies with you.
	AGRGasPod::SlicedThisRun = 0;
	const FString Partner = GM ? UGameplayStatics::ParseOption(GM->OptionsString, TEXT("partner")) : FString();
	bPartnerWanted = Partner == TEXT("1") || (Mode.IsEmpty() && bPartnerInEditorPlay);

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
	if (Rig.IsValid())
	{
		Rig->UseEndlessRules();
	}
	if (bPartnerWanted)
	{
		GetWorld()->SpawnActor<AGRCompanion>();
	}

	// Never build caves from the open-backed Megascans scans (they look like paper from behind),
	// even if an older copy of this spawner saved in a level still lists them.
	CaveMeshes.RemoveAll([](const TSoftObjectPtr<UStaticMesh>& M) { return M.ToSoftObjectPath().ToString().Contains(TEXT("/Fab/Megascans/")); });

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
		AGRGasPod::SlicedThisRun = 0;
		CaveCentres.Reset();
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
		GEngine->AddOnScreenDebugMessage(4205, 0.f, FColor(255, 214, 160),   // 4205: its own line (4202 is the weapon status)
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
	if (AGRStylizedNature* Nature = AGRStylizedNature::Get())
	{
		Nature->DressRock(Island, 2500);   // a meadow on top of the island
	}
	const FVector Top(X, Y, Z);

	// ---- grapple rocks along the gap, 11-21 m above the line between the islands
	//      (every CaveEvery-th gap is a floating cave instead of open rocks)
	const float Span = static_cast<float>(Top.X - LastTop.X);
	++ChunkCount;
	const bool bCave = CaveEvery > 0 && ChunkCount % CaveEvery == 0 && CaveMeshes.Num() > 0;
	const float CaveFrom = 0.25f, CaveTo = 0.75f;   // the cave fills the middle half of the gap
	for (float T = Rng.FRandRange(AnchorSpacing.X, AnchorSpacing.Y); T < Span - 600.f; T += Rng.FRandRange(AnchorSpacing.X, AnchorSpacing.Y))
	{
		if (bCave && T > Span * CaveFrom - 300.f && T < Span * CaveTo + 300.f)
		{
			continue;   // the cave's ceiling gives the hooks here
		}
		const FVector Line = FMath::Lerp(LastTop, Top, T / Span);
		const FVector At = Line + FVector(0.f, Rng.FRandRange(-500.f, 500.f), Rng.FRandRange(AnchorHeight.X, AnchorHeight.Y));
		AddAnchorRock(Pick(RockMeshes), At, Rng.FRandRange(AnchorSize.X, AnchorSize.Y), C);
		if (Rng.FRand() < SideRockChance)   // an extra rock off to one side: a choice of line
		{
			const float SideWays = (Rng.FRand() < 0.5f ? -1.f : 1.f) * Rng.FRandRange(900.f, 1400.f);
			AddAnchorRock(Pick(RockMeshes), At + FVector(Rng.FRandRange(-200.f, 200.f), SideWays, Rng.FRandRange(-300.f, 300.f)),
				Rng.FRandRange(AnchorSize.X, AnchorSize.Y) * 0.85f, C);
		}
	}
	if (bCave)
	{
		BuildCave(FMath::Lerp(LastTop, Top, CaveFrom), FMath::Lerp(LastTop, Top, CaveTo), C);
	}

	// ---- a gas canister to slice, at swinging height somewhere along the gap
	if (Rng.FRand() < PodChance || bCave)
	{
		const float T = bCave ? 0.5f : Rng.FRandRange(0.3f, 0.75f);
		const FVector At = FMath::Lerp(LastTop, Top, T) + FVector(0.f, Rng.FRandRange(-350.f, 350.f),
			bCave ? 700.f : Rng.FRandRange(PodHeight.X, PodHeight.Y));
		if (AGRGasPod* Pod = GetWorld()->SpawnActor<AGRGasPod>(At, FRotator::ZeroRotator))
		{
			C.Actors.Add(Pod);
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

// One floating grapple rock: sized by width, its middle on At, tagged so hooks and the aim
// assist find it.
void AGREndlessSpawner::AddAnchorRock(UStaticMesh* Mesh, const FVector& At, float Width, FChunk& Into)
{
	AActor* Rock = SpawnMesh(Mesh, At, FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), 0.f), -Width, true, Into);
	if (!Rock)
	{
		return;
	}
	FVector RO, RE;                                         // rocks float: put the rock's middle on the point
	Rock->GetActorBounds(false, RO, RE);
	Rock->AddActorWorldOffset(At - RO);
	Rock->Tags.Add(FName(TEXT("GrappleAnchor")));
	if (Rig.IsValid())
	{
		Rig->AddAnchor(Rock);
	}
	if (AGRStylizedNature* Nature = AGRStylizedNature::Get())
	{
		Nature->DressRock(Rock);   // a tuft of anime grass and flowers on top
	}
}

// A FLOATING CAVE: a tube of cliff rock from From to To that you swing through.
//   - rings of big rocks every ~5 m along the tube, each ring a circle of 9 rocks
//     (the bottom of the circle left open, so a wrong move drops you out, not into a wall)
//   - the ceiling rocks are grapple anchors: you hook the roof as you fly through
//   - mossy roots hang inside, and a few rocks stick out on the outside so it reads as a
//     lump of floating mountain, not a pipe
void AGREndlessSpawner::BuildCave(const FVector& From, const FVector& To, FChunk& Into)
{
	const FVector Mid = (From + To) * 0.5f + FVector(0.f, 0.f, 900.f);   // the tube runs ~9 m above the line between islands
	const FVector Dir = (To - From).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
	const float Length = static_cast<float>(FVector::Dist2D(From, To));
	const float Radius = Rng.FRandRange(900.f, 1200.f);                  // 18-24 m across inside
	CaveCentres.Add(Mid);
	for (float S = -Length * 0.5f; S <= Length * 0.5f; S += 520.f)
	{
		const FVector RingCentre = Mid + Dir * S;
		for (int32 k = 0; k < 9; ++k)
		{
			// Angle round the ring: 0 = straight up, going round both sides; skip the bottom gap.
			const float A = FMath::DegreesToRadians(-150.f + k * 37.5f);
			const FVector Out = FVector::UpVector * FMath::Cos(A) + Right * FMath::Sin(A);
			const float Size = Rng.FRandRange(900.f, 1400.f);
			const FVector At = RingCentre + Out * (Radius + Size * 0.35f);
			AActor* Part = SpawnMesh(Pick(CaveMeshes), At, FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f)), -Size, true, Into);
			if (!Part)
			{
				continue;
			}
			FVector PO, PE;
			Part->GetActorBounds(false, PO, PE);
			Part->AddActorWorldOffset(At - PO);
			if (FMath::Abs(k - 4) <= 2)   // the 5 rocks across the top of the ring = the ceiling
			{
				Part->Tags.Add(FName(TEXT("GrappleAnchor")));
				if (Rig.IsValid())
				{
					Rig->AddAnchor(Part);
				}
			}
		}
		// Roots hanging from the roof.
		if (Rng.FRand() < 0.6f)
		{
			SpawnMesh(Pick(RootMeshes), RingCentre + FVector(0.f, 0.f, Radius - 250.f), FRotator(180.f, Rng.FRandRange(0.f, 360.f), 0.f),
				-Rng.FRandRange(500.f, 800.f), false, Into);
		}
	}
	// A canister deep inside, as a reward for going through.
	if (AGRGasPod* Pod = GetWorld()->SpawnActor<AGRGasPod>(Mid + FVector(0.f, 0.f, -200.f), FRotator::ZeroRotator))
	{
		Into.Actors.Add(Pod);
	}
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
