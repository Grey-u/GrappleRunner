// =====================================================================================
//  GRGasGauge.h  -  the gas gauge you can see while you swing
// =====================================================================================
//
//  WHAT THIS IS
//  Your ODM gear runs on gas. Reeling the rope in, boosting and firing hooks all burn it.
//  When the tank is empty the rope can't pull you any more, so you swing down and fall.
//  This gauge shows how much is left.
//
//  WHERE IT SITS (VR comfort rule: no HUD stuck to your eyes)
//  It is a small panel IN THE WORLD, held low in front of your body, like a gauge on the
//  gear's handle. It turns with your body (left/right), not with your head, so it never
//  swims around when you look about. Glance down a little and it's there.
//  In VR it sits on your left wrist instead, so you look at it like a watch.
//
//  COLOURS
//  Green when more than half full, orange below half, red below a fifth (and it pulses).
//  A gas canister you slice bursts in the gauge's colour at that moment (ColourFor).
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = a value you can change in the Details panel without code.
//  - Distances are centimetres (100 = 1 metre).
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRGasGauge.generated.h"

class AGrappleRig;
class UWidgetComponent;
class SWidget;

UCLASS()
class GRAPPLERUNNER_API AGRGasGauge : public AActor
{
	GENERATED_BODY()

public:
	AGRGasGauge();
	virtual void Tick(float DeltaSeconds) override;

	// Where it floats on desktop, measured from your eyes (turning with your body):
	UPROPERTY(EditAnywhere, Category = "Gauge") float Ahead = 60.f;      // in front of you
	UPROPERTY(EditAnywhere, Category = "Gauge") float Side = 0.f;        // centred, between your hands (minus = left)
	UPROPERTY(EditAnywhere, Category = "Gauge") float Down = 14.f;       // below eye level: just under the centre of your view (Naomi: must be easy to see)
	UPROPERTY(EditAnywhere, Category = "Gauge") float WidthCm = 16.f;    // how wide it is in the world

	// The gauge colour for a tank this full (0 = empty, 1 = full). Used for the burst too.
	static FLinearColor ColourFor(float Fraction);

	// A quick bright flash (called when you refill).
	void Flash() { FlashLeft = 0.5f; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> Panel;
	TWeakObjectPtr<AGrappleRig> Rig;
	TSharedPtr<SWidget> Root;
	float Shown = 1.f;        // the fraction the bar shows (eases toward the real value)
	float FlashLeft = 0.f;    // seconds of refill flash left
	float Clock = 0.f;        // for the low-gas pulse
	float LastGas = -1.f;
	TSharedRef<SWidget> Build();
};
