// =====================================================================================
//  GRPlanets.h  -  planets hanging in Aeloria's sky
// =====================================================================================
//
//  WHAT THIS IS
//  A few big painted balls (and one ring) placed very far away in fixed directions, so they
//  look like planets and moons. The grapple rig spawns this when you press Play.
//
//  HOW "VERY FAR AWAY" WORKS
//  Real planets are so far that walking (or swinging) never changes where they are in the
//  sky. So every frame each planet is put at the same distance in the same DIRECTION from
//  the camera: you can swing 10 km and the gas giant still sits over the same horizon.
//
//  DAY AND NIGHT, AND HAZE
//  The planets read the day/night clock (GRDayNight): faint by day, bright at night. Their
//  lit side faces the sun, so you see crescents at dawn and dusk. They are also blended toward
//  the fog's colour, so they sit far off behind the mist (the "sepia haze" look). This actor
//  also draws the sun's disc: a small bright circle where the sunlight comes from.
//
//  PICTURES AND MATERIAL
//  Painted by Art/Planets/make_planets.py, imported by Scripts/add_planets.py (M_Planet).
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = change it in the Details panel without code.
//  - Direction: Yaw = compass direction (0 = along +X, the way the route runs), Pitch = how
//    high above the horizon. Size = how wide it looks, in degrees (the real Moon is about 0.5).
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRPlanets.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

USTRUCT(BlueprintType)
struct FGRPlanet
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Planet") FString Name;
	UPROPERTY(EditAnywhere, Category = "Planet") TSoftObjectPtr<UTexture2D> Surface;
	UPROPERTY(EditAnywhere, Category = "Planet") float Yaw = 0.f;          // compass direction (degrees)
	UPROPERTY(EditAnywhere, Category = "Planet") float Pitch = 20.f;       // height above the horizon (degrees)
	UPROPERTY(EditAnywhere, Category = "Planet") float Size = 10.f;        // how wide it looks (degrees)
	UPROPERTY(EditAnywhere, Category = "Planet") FRotator Tilt = FRotator::ZeroRotator;   // axis tilt (rings tilt with it)
	UPROPERTY(EditAnywhere, Category = "Planet") float SpinDegreesPerMinute = 2.f;         // slow turn, so the storms drift
	UPROPERTY(EditAnywhere, Category = "Planet") TSoftObjectPtr<UTexture2D> Ring;          // optional ring picture
	UPROPERTY(EditAnywhere, Category = "Planet") float RingSize = 2.4f;                    // ring width / planet width
};

UCLASS()
class GRAPPLERUNNER_API AGRPlanets : public AActor
{
	GENERATED_BODY()

public:
	AGRPlanets();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Planets") TArray<FGRPlanet> Planets;
	UPROPERTY(EditAnywhere, Category = "Planets") float Distance = 600000.f;   // 6 km: past the islands, inside the sky
	UPROPERTY(EditAnywhere, Category = "Planets") float DayGlow = 0.5f;        // brightness by day...
	UPROPERTY(EditAnywhere, Category = "Planets") float NightGlow = 1.6f;      // ...and at night
	UPROPERTY(EditAnywhere, Category = "Planets") float DayOpacity = 0.45f;    // a bright sky washes them out a little
	UPROPERTY(EditAnywhere, Category = "Planets") float DayHaze = 0.6f;        // how much they fade into the fog colour by day...
	UPROPERTY(EditAnywhere, Category = "Planets") float NightHaze = 0.3f;      // ...and at night (clearer)
	UPROPERTY(EditAnywhere, Category = "Planets") float SunSize = 1.8f;        // the sun's disc, degrees wide
	UPROPERTY(EditAnywhere, Category = "Planets") float SunGlow = 14.f;        // how bright the disc is (bloom makes the halo)
	UPROPERTY(EditAnywhere, Category = "Planets") TSoftObjectPtr<UMaterialInterface> PlanetMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Sky/Planets/M_Planet.M_Planet")));

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> PlaneMesh;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Balls;   // one per planet
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Rings;   // one per planet (empty if no ring)
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;   // balls first, then rings
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SunDisc;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SunMaterial;
	float Spin = 0.f;   // minutes of spin so far
};
