// =====================================================================================
//  GRPlanets.cpp  -  what the planets DO (see GRPlanets.h for the idea)
// =====================================================================================
//
//  BeginPlay: make a ball (and maybe a ring) per planet, each with its own copy of M_Planet.
//  Tick, every frame:
//    1. where is the camera? put each planet Distance away from it, in its own direction
//    2. turn each planet a little (spin) and tilt its ring
//    3. which way is the sun? tell every planet, so its lit side faces the sun
//    4. how far through the day are we? fade the planets in for the night
// =====================================================================================
#include "GRPlanets.h"
#include "GRDayNight.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AGRPlanets::AGRPlanets()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera has moved this frame
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));   // 100 cm wide
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));      // 100 cm square
	SphereMesh = Sphere.Object;
	PlaneMesh = Plane.Object;

	// The four worlds of Aeloria's sky (pictures from Art/Planets/make_planets.py).
	auto Tex = [](const TCHAR* Name)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/GrappleRunner/Sky/Planets/%s.%s"), Name, Name)));
	};
	FGRPlanet Vael;   // the big ringed gas giant, low over the route ahead
	Vael.Name = TEXT("Vael");
	Vael.Surface = Tex(TEXT("T_Planet_Vael"));
	Vael.Ring = Tex(TEXT("T_Planet_Vael_Ring"));
	Vael.Yaw = 20.f;
	Vael.Pitch = 16.f;
	Vael.Size = 22.f;
	Vael.Tilt = FRotator(14.f, 0.f, -18.f);
	Vael.SpinDegreesPerMinute = 3.f;
	Planets.Add(Vael);

	FGRPlanet Lun;    // a pale moon, high and off to the left
	Lun.Name = TEXT("Lun");
	Lun.Surface = Tex(TEXT("T_Planet_Lun"));
	Lun.Yaw = -55.f;
	Lun.Pitch = 38.f;
	Lun.Size = 5.f;
	Lun.SpinDegreesPerMinute = 0.5f;
	Planets.Add(Lun);

	FGRPlanet Iskra;  // small ice world, behind you on the right
	Iskra.Name = TEXT("Iskra");
	Iskra.Surface = Tex(TEXT("T_Planet_Iskra"));
	Iskra.Yaw = 140.f;
	Iskra.Pitch = 24.f;
	Iskra.Size = 3.5f;
	Planets.Add(Iskra);

	FGRPlanet Merrow; // far ocean world, low on the left horizon
	Merrow.Name = TEXT("Merrow");
	Merrow.Surface = Tex(TEXT("T_Planet_Merrow"));
	Merrow.Yaw = -120.f;
	Merrow.Pitch = 9.f;
	Merrow.Size = 8.f;
	Merrow.Tilt = FRotator(0.f, 0.f, 10.f);
	Planets.Add(Merrow);
}

void AGRPlanets::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Base = PlanetMaterial.LoadSynchronous();
	if (!Base)
	{
		UE_LOG(LogTemp, Warning, TEXT("Grapple Runner planets: M_Planet missing - run Scripts/add_planets.py"));
		return;
	}
	auto MakePart = [this](UStaticMesh* Mesh, UMaterialInterface* Mat) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetUsingAbsoluteLocation(true);   // we place them in the world ourselves each frame
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->bNeverDistanceCull = true;        // never hidden for being "too far away"
		C->SetBoundsScale(10.f);
		C->RegisterComponent();
		C->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		C->SetMaterial(0, Mat);
		return C;
	};
	// The sky is often a giant ball mesh (a "sky dome"). Planets must sit INSIDE it or the dome
	// hides them, so if there's one smaller than our distance, move them in to 80% of its size.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->GetClass()->GetName().Contains(TEXT("Sky")) && !It->GetActorNameOrLabel().Contains(TEXT("Sky")))
		{
			continue;
		}
		FVector Origin, Extent;
		It->GetActorBounds(false, Origin, Extent);
		const float Radius = Extent.GetMin();
		if (Radius > 50000.f && Radius * 0.8f < Distance)
		{
			Distance = Radius * 0.8f;
			UE_LOG(LogTemp, Log, TEXT("Grapple Runner planets: inside the sky dome %s, distance %.0f m"), *It->GetActorNameOrLabel(), Distance / 100.f);
		}
	}
	for (const FGRPlanet& P : Planets)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		if (UTexture2D* T = P.Surface.LoadSynchronous())
		{
			M->SetTextureParameterValue(TEXT("Surface"), T);
		}
		M->SetScalarParameterValue(TEXT("Shade"), 1.f);
		Balls.Add(MakePart(SphereMesh, M));
		Materials.Add(M);
	}
	for (const FGRPlanet& P : Planets)
	{
		UTexture2D* T = P.Ring.IsNull() ? nullptr : P.Ring.LoadSynchronous();
		if (!T)
		{
			Rings.Add(nullptr);
			continue;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetTextureParameterValue(TEXT("Surface"), T);
		M->SetScalarParameterValue(TEXT("Shade"), 0.f);   // rings aren't shaded like a ball
		Rings.Add(MakePart(PlaneMesh, M));
		Materials.Add(M);
	}
}

void AGRPlanets::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam || Balls.Num() != Planets.Num())
	{
		return;
	}
	const FVector Eye = Cam->GetCameraLocation();
	Spin += DeltaSeconds / 60.f;

	// ---- Sun direction: the opposite of the way the sunlight travels ----
	FVector ToSun = FVector(0.f, 0.f, 1.f);
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		ToSun = -It->GetLightComponent()->GetForwardVector();
		break;
	}
	// ---- Night factor: 0 by day, 1 at night (the clock's night starts about two thirds through) ----
	float Night = 0.f;
	for (TActorIterator<AGRDayNight> It(GetWorld()); It; ++It)
	{
		const float T = It->GetDayProgress();
		Night = FMath::Clamp((T - 0.6f) / 0.12f, 0.f, 1.f) + FMath::Clamp((0.08f - T) / 0.08f, 0.f, 1.f);   // dusk -> night, and the dark before dawn
		Night = FMath::Clamp(Night, 0.f, 1.f);
		break;
	}
	const float Glow = FMath::Lerp(DayGlow, NightGlow, Night);
	const float Opacity = FMath::Lerp(DayOpacity, 1.f, Night);
	for (UMaterialInstanceDynamic* M : Materials)
	{
		M->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(ToSun.X, ToSun.Y, ToSun.Z, 0.f));
		M->SetScalarParameterValue(TEXT("Glow"), Glow);
		M->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}

	for (int32 i = 0; i < Planets.Num(); ++i)
	{
		const FGRPlanet& P = Planets[i];
		// Direction from Yaw/Pitch, then the planet sits Distance away in that direction.
		const FVector Dir = FRotator(P.Pitch, P.Yaw, 0.f).Vector();
		const FVector At = Eye + Dir * Distance;
		// Size in degrees -> how wide the ball must be at that distance (simple trigonometry).
		const float Width = 2.f * Distance * FMath::Tan(FMath::DegreesToRadians(P.Size * 0.5f));
		const FQuat Tilt = P.Tilt.Quaternion();
		const FQuat Turn = Tilt * FQuat(FVector::UpVector, FMath::DegreesToRadians(Spin * P.SpinDegreesPerMinute));
		Balls[i]->SetWorldLocationAndRotation(At, Turn);
		Balls[i]->SetWorldScale3D(FVector(Width / 100.f));   // the engine sphere is 100 cm wide
		if (Rings.IsValidIndex(i) && Rings[i])
		{
			Rings[i]->SetWorldLocationAndRotation(At, Tilt);   // flat around the equator, tilted with the planet
			Rings[i]->SetWorldScale3D(FVector(Width * P.RingSize / 100.f, Width * P.RingSize / 100.f, 1.f));
		}
	}
}
