// =====================================================================================
//  GRGasPod.h  -  a glowing gas canister floating in the air: slice it to refuel
// =====================================================================================
//
//  WHAT THIS IS
//  The endless run's fuel. Canisters float along the route, bobbing and turning, with a
//  thin beam of light above them so you can spot them from far away. Slice one with a
//  blade and it bursts into a spray of light in the colour your gas gauge shows right now
//  (green, orange or red), and your tank refills by Refill.
//
//  HOW THE SLICE IS DETECTED
//  Each frame of a sword swing, the arsenal (GRArsenal) asks every canister "did this blade
//  pass through you?" (TrySlice). The answer is a distance check: how close does the line
//  from the blade's base to its tip come to the canister's centre? Closer than Radius = hit.
//
//  THE BURST (no particle editor needed)
//  On a hit the canister hides and throws out a few dozen tiny glowing balls. They fly
//  outward, slow down, shrink and vanish in about 0.8 s, while a light flashes and fades.
//  Then the canister removes itself.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = can be changed in the Details panel without code.
//  - Distances are centimetres (100 = 1 metre).
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRGasPod.generated.h"

class AGrappleRig;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class GRAPPLERUNNER_API AGRGasPod : public AActor
{
	GENERATED_BODY()

public:
	AGRGasPod();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Gas Pod") float Refill = 40.f;      // gas added (the tank holds 100)
	UPROPERTY(EditAnywhere, Category = "Gas Pod") float Radius = 75.f;      // how close the blade must pass (cm): generous on purpose
	UPROPERTY(EditAnywhere, Category = "Gas Pod") int32 Sparks = 36;        // how many bits of light in the burst
	UPROPERTY(EditAnywhere, Category = "Gas Pod") FLinearColor Glow = FLinearColor(0.35f, 0.95f, 0.35f);   // the canister's own colour

	// Did the blade (a line from Base to Tip) pass through? If so: burst and refill. Returns true on a hit.
	bool TrySlice(const FVector& Base, const FVector& Tip, AGrappleRig* Rig);
	bool IsSpent() const { return bBurst; }

	// Number of canisters sliced this run (for the partner's chatter and the score screen).
	static int32 SlicedThisRun;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;   // the glowing canister
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CapTop;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CapBottom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Beam;   // thin light beam above it
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Bits;          // the burst's bits of light
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BurstMat;
	TArray<FVector> BitSpeed;
	FVector Home;
	float Clock = 0.f;
	float Age = -1.f;         // seconds since the burst (-1 = not burst yet)
	bool bBurst = false;
	float LightStart = 0.f;
	void Burst(const FLinearColor& Colour);
};
