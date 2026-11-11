// =====================================================================================
//  GRAmbientLife.h  -  the air is alive: drifting pollen and dust by day, glowing spores at night
// =====================================================================================
//
//  WHAT THIS IS
//  A cloud of tiny glowing specks that always surrounds the player (about 25 m around you).
//  Each speck drifts with the wind and wanders a little, like pollen, seeds and dust in
//  sunlight. As night falls (the day/night clock's "Night" value) they turn into bright
//  blue-green spores and fireflies, like a bioluminescent forest.
//
//  HOW IT'S CHEAP
//  All specks are ONE "instanced" mesh: the graphics card draws hundreds of copies of a tiny
//  ball in a single go. A speck that drifts too far from you is moved back to a fresh random
//  spot near you, so there are never more than Count of them, however far you swing.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = change it in the Details panel without code.
//  - Distances are centimetres, speeds are centimetres per second.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRAmbientLife.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

UCLASS()
class GRAPPLERUNNER_API AGRAmbientLife : public AActor
{
	GENERATED_BODY()

public:
	AGRAmbientLife();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Ambient Life") int32 Count = 350;              // how many specks
	UPROPERTY(EditAnywhere, Category = "Ambient Life") float Radius = 2500.f;           // how far around you they live
	UPROPERTY(EditAnywhere, Category = "Ambient Life") float SpeckSize = 0.6f;          // cm across
	UPROPERTY(EditAnywhere, Category = "Ambient Life") FVector Wind = FVector(40.f, 15.f, 6.f);   // the breeze they ride
	UPROPERTY(EditAnywhere, Category = "Ambient Life") float Wander = 25.f;             // how much each one drifts on its own
	UPROPERTY(EditAnywhere, Category = "Ambient Life") FLinearColor DayColour = FLinearColor(1.f, 0.95f, 0.8f);    // sunlit pollen and dust
	UPROPERTY(EditAnywhere, Category = "Ambient Life") FLinearColor NightColour = FLinearColor(0.25f, 1.f, 0.85f); // glowing spores
	UPROPERTY(EditAnywhere, Category = "Ambient Life") float DayGlow = 2.f;
	UPROPERTY(EditAnywhere, Category = "Ambient Life") float NightGlow = 45.f;
	UPROPERTY(EditAnywhere, Category = "Ambient Life") TSoftObjectPtr<UMaterialInterface> GlowMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Weapons/FX/M_WeaponGlow.M_WeaponGlow")));

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Specks;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY() TObjectPtr<UStaticMesh> Ball;
	TArray<FVector> Pos;     // where each speck is
	TArray<FVector> Vel;     // its own wandering drift
	TArray<float> Phase;     // so they don't all twinkle together
	float Time = 0.f;
	FVector RandomSpotNear(const FVector& Centre) const;
};
