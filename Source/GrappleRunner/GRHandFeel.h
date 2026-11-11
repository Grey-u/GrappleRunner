// =====================================================================================
//  GRHandFeel.h  -  feel what your fingers touch (LucidGloves / OpenGloves force feedback)
// =====================================================================================
//
//  WHAT THIS IS
//  The bridge between the game and haptic gloves. Every frame it asks the headset where each
//  fingertip is (hand tracking), checks whether that fingertip is touching something solid,
//  and if so tells the glove to resist that finger curling: your finger "meets" the rock.
//
//  THE GLOVES: LucidGloves (open-source DIY gloves by LucidVR) run on the OpenGloves SteamVR
//  driver. The driver tracks finger curl and accepts force feedback through a "named pipe"
//  (a little text-free mailbox between two programs on the same PC):
//      \\.\pipe\vrapplication\ffb\curl\left   and   ...\right
//  The message is five numbers, thumb to pinky, each 0..1000: 0 = finger free, 1000 = finger
//  fully blocked from curling. (Source: the opengloves-driver wiki, "Integrating Force Feedback".)
//
//  "SLIGHTLY FEEL" = FIRMNESS
//  Not everything should stop your finger dead. Rock and ruins push back fully; plants, vines
//  and leaves only a little, so brushing through a fern feels soft. Firmness is decided from
//  the name of what you touch (see SoftWords), so new assets work without extra setup.
//
//  THE EXIT SUIT
//  The same touch events are where the suit plugs in later (body haptics, tendon fingers):
//  see OnTouch() in the .cpp. For now it also gives a tiny controller buzz on first contact,
//  so it works with ordinary controllers too.
//
//  WHEN IT RUNS
//  Only in VR with hand tracking (SteamVR turns glove/Index finger data into OpenXR hand
//  tracking; the OpenXRHandTracking plugin is enabled in the project). On desktop it sleeps.
//  If no glove driver is running, the pipe just isn't there: it retries every 2 seconds and
//  the game carries on normally.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = change it in the Details panel without code.
//  - Distances are centimetres.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRHandFeel.generated.h"

UCLASS()
class GRAPPLERUNNER_API AGRHandFeel : public AActor
{
	GENERATED_BODY()

public:
	AGRHandFeel();
	virtual void Tick(float DeltaSeconds) override;

	// How big a fingertip is for touching (cm). Real fingertips are about 1.6 cm across.
	UPROPERTY(EditAnywhere, Category = "Hand Feel") float FingertipRadius = 0.9f;
	// Words in an object's name that mean "soft" (plants, vines, leaves, water).
	UPROPERTY(EditAnywhere, Category = "Hand Feel") TArray<FString> SoftWords = { TEXT("Plant"), TEXT("Fern"), TEXT("Grass"), TEXT("Leaf"), TEXT("Vine"), TEXT("Bush"), TEXT("Flower"), TEXT("Water"), TEXT("Moss") };
	UPROPERTY(EditAnywhere, Category = "Hand Feel") float SoftFirmness = 0.25f;   // 0..1: how hard soft things push back
	UPROPERTY(EditAnywhere, Category = "Hand Feel") float HardFirmness = 1.0f;    // rock, ruins, metal
	UPROPERTY(EditAnywhere, Category = "Hand Feel") float TouchBuzz = 0.2f;       // controller buzz on first contact (0 = off)

	// What the glove was last told, per hand (0 left, 1 right), thumb..pinky, 0..1000. For debugging.
	int16 Sent[2][5] = { {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0} };

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void* Pipe[2] = { nullptr, nullptr };   // Windows handle to each glove's mailbox (nullptr = not connected)
	float RetryIn[2] = { 0.f, 0.f };
	bool bTouching[2][5] = { {false}, {false} };

	bool FingerFirmness(const FVector& Tip, float& OutFirmness) const;   // is this fingertip touching something? how firm?
	void Send(int32 Hand, const int16 Values[5]);                         // write the five numbers to the glove
	void OnTouch(int32 Hand, int32 Finger, float Firmness);               // first contact: buzz now, suit later
	void ClosePipes();
};
