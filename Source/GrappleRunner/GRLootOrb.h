// =====================================================================================
//  GRLootOrb.h  -  a glowing loot orb: flies to you when you're close, gives a little gas
// =====================================================================================
//
//  WHAT THIS IS
//  The loot of the endless run. Orbs come from two places:
//    - a gas canister you slice bursts and DROPS a few (they pop out, then fly to you)
//    - trails of orbs hang along some swing paths, rewarding a good line
//  Each orb is one of the MayuOrbs particle effects Grey added (a glowing Niagara orb). When
//  your body comes within MagnetRadius, the orb speeds towards you like a magnet; touching
//  it collects it: a little gas back, and it counts towards this run's orbs.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = change it in the Details panel without code.
//  - Distances are centimetres, speeds centimetres per second.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRLootOrb.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class AGrappleRig;

UCLASS()
class GRAPPLERUNNER_API AGRLootOrb : public AActor
{
	GENERATED_BODY()

public:
	AGRLootOrb();
	virtual void Tick(float DeltaSeconds) override;

	// Throw it out of a burst: it flies off in this direction, slows, then homes in on you.
	void Launch(const FVector& Velocity);

	UPROPERTY(EditAnywhere, Category = "Loot") float MagnetRadius = 900.f;   // starts flying to you inside 9 m
	UPROPERTY(EditAnywhere, Category = "Loot") float CollectRadius = 140.f;
	UPROPERTY(EditAnywhere, Category = "Loot") float GasPerOrb = 4.f;
	UPROPERTY(EditAnywhere, Category = "Loot") float LifeSeconds = 0.f;      // 0 = forever (trail orbs); drops use 25 s

	static int32 CollectedThisRun;   // shown on the distance line

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Effect;
	TWeakObjectPtr<AGrappleRig> Rig;
	FVector Velocity = FVector::ZeroVector;
	FVector Home = FVector::ZeroVector;
	float Age = 0.f;
	float Phase = 0.f;
	bool bHoming = false;
};
