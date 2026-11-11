// =====================================================================================
//  GRAmbientLife.cpp  -  what the drifting specks DO (see GRAmbientLife.h for the idea)
// =====================================================================================
//
//  EVERY FRAME:
//    1. where is the camera (your eyes)? that's the centre of the cloud
//    2. each speck moves: wind + its own slow wander (which slowly changes direction)
//    3. a speck further than Radius from you is put back somewhere random near you
//    4. day/night: read the clock, blend colour and brightness from pollen to spores,
//       and make the specks twinkle (size pulses) more at night
// =====================================================================================
#include "GRAmbientLife.h"
#include "GRDayNight.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AGRAmbientLife::AGRAmbientLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera has moved
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));   // 100 cm ball
	Ball = Sphere.Object;
}

FVector AGRAmbientLife::RandomSpotNear(const FVector& Centre) const
{
	// Anywhere inside a ball of Radius around you, a bit flattened (more specks at eye level).
	FVector Offset = FMath::VRand() * FMath::FRandRange(150.f, Radius);
	Offset.Z *= 0.5f;
	return Centre + Offset;
}

void AGRAmbientLife::BeginPlay()
{
	Super::BeginPlay();
	Specks = NewObject<UInstancedStaticMeshComponent>(this);
	Specks->SetStaticMesh(Ball);
	Specks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Specks->SetCastShadow(false);
	Specks->SetUsingAbsoluteLocation(true);
	Specks->SetUsingAbsoluteRotation(true);
	Specks->SetUsingAbsoluteScale(true);
	Specks->SetMobility(EComponentMobility::Movable);
	Specks->RegisterComponent();
	Specks->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	if (UMaterialInterface* Base = GlowMaterial.LoadSynchronous())
	{
		Material = UMaterialInstanceDynamic::Create(Base, this);
		Specks->SetMaterial(0, Material);
	}
	const FVector Start = GetActorLocation();
	for (int32 i = 0; i < Count; ++i)
	{
		Pos.Add(RandomSpotNear(Start));
		Vel.Add(FMath::VRand() * Wander);
		Phase.Add(FMath::FRandRange(0.f, 2.f * PI));
		Specks->AddInstance(FTransform(FQuat::Identity, Pos.Last(), FVector(SpeckSize / 100.f)), true);
	}
}

void AGRAmbientLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam || !Specks || Pos.Num() == 0)
	{
		return;
	}
	Time += DeltaSeconds;
	const FVector Eye = Cam->GetCameraLocation();

	// ---- 4 (first, so it's ready): how dark is it? ----
	float Night = 0.f;
	for (TActorIterator<AGRDayNight> It(GetWorld()); It; ++It)
	{
		const float T = It->GetDayProgress();
		Night = FMath::Clamp(FMath::Clamp((T - 0.6f) / 0.12f, 0.f, 1.f) + FMath::Clamp((0.08f - T) / 0.08f, 0.f, 1.f), 0.f, 1.f);
		break;
	}
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Colour"), FMath::Lerp(DayColour, NightColour, Night));
		Material->SetScalarParameterValue(TEXT("Strength"), FMath::Lerp(DayGlow, NightGlow, Night));
	}

	// ---- 1-3. move every speck ----
	TArray<FTransform> Moved;
	Moved.SetNum(Pos.Num());
	const float RadiusSq = Radius * Radius;
	for (int32 i = 0; i < Pos.Num(); ++i)
	{
		Vel[i] = (Vel[i] + FMath::VRand() * Wander * 0.6f * DeltaSeconds).GetClampedToMaxSize(Wander);   // the wander slowly turns
		Pos[i] += (Wind + Vel[i]) * DeltaSeconds;
		if (FVector::DistSquared(Pos[i], Eye) > RadiusSq)
		{
			Pos[i] = RandomSpotNear(Eye);   // drifted away: reappear near you
		}
		// Twinkle: night spores pulse in size; day dust barely does.
		const float Pulse = 1.f + FMath::Sin(Time * 2.3f + Phase[i]) * FMath::Lerp(0.15f, 0.6f, Night);
		Moved[i] = FTransform(FQuat::Identity, Pos[i], FVector(SpeckSize * Pulse / 100.f));
	}
	Specks->BatchUpdateInstancesTransforms(0, Moved, true, true, true);
}
