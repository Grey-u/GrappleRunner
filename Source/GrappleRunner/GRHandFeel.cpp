// =====================================================================================
//  GRHandFeel.cpp  -  what the hand-feel bridge DOES (see GRHandFeel.h for the idea)
// =====================================================================================
//
//  EVERY FRAME, FOR EACH HAND:
//    1. ask the headset for this hand's joints (OpenXR hand tracking), in world space
//    2. for each fingertip: is a small ball at the tip overlapping anything solid?
//    3. touching -> that finger's number = firmness x 1000 (rock 1000, fern 250); else 0
//    4. if any number changed, send the five numbers down the glove's pipe
// =====================================================================================
#include "GRHandFeel.h"
#include "GrappleRig.h"

#include "OverlapResult.h"   // FOverlapResult: one thing a fingertip overlaps
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "HeadMountedDisplayTypes.h"   // FXRHandTrackingState, EHandKeypoint
#include "IXRTrackingSystem.h"         // the headset system (GEngine->XRSystem)
#include "Kismet/GameplayStatics.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	// The five fingertips, in the order the glove expects (thumb first).
	const EHandKeypoint Tips[5] = { EHandKeypoint::ThumbTip, EHandKeypoint::IndexTip, EHandKeypoint::MiddleTip,
		EHandKeypoint::RingTip, EHandKeypoint::LittleTip };
	const TCHAR* PipeNames[2] = { TEXT("\\\\.\\pipe\\vrapplication\\ffb\\curl\\left"), TEXT("\\\\.\\pipe\\vrapplication\\ffb\\curl\\right") };
}

AGRHandFeel::AGRHandFeel()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AGRHandFeel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GEngine || !GEngine->XRSystem.IsValid() || !GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		return;   // desktop: nothing to feel with
	}
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		// ---- 1. where are this hand's joints? ----
		FXRHandTrackingState State;
		const EControllerHand Which = Hand == 0 ? EControllerHand::Left : EControllerHand::Right;
		GEngine->XRSystem->GetHandTrackingState(this, EXRSpaceType::UnrealWorldSpace, Which, State);
		if (!State.bValid || State.HandKeyLocations.Num() < EHandKeypointCount)
		{
			continue;   // this hand isn't tracked right now
		}

		// ---- 2 + 3. each fingertip: touching? how firm? ----
		int16 Values[5];
		for (int32 F = 0; F < 5; ++F)
		{
			const FVector Tip = State.HandKeyLocations[static_cast<int32>(Tips[F])];
			float Firmness = 0.f;
			const bool bNow = FingerFirmness(Tip, Firmness);
			if (bNow && !bTouching[Hand][F])
			{
				OnTouch(Hand, F, Firmness);
			}
			bTouching[Hand][F] = bNow;
			Values[F] = static_cast<int16>(FMath::RoundToInt(FMath::Clamp(Firmness, 0.f, 1.f) * 1000.f));
		}

		// ---- 4. tell the glove, but only when something changed ----
		if (FMemory::Memcmp(Values, Sent[Hand], sizeof(Values)) != 0)
		{
			Send(Hand, Values);
		}
	}
}

bool AGRHandFeel::FingerFirmness(const FVector& Tip, float& OutFirmness) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GRFingertip), false, this);
	if (APlayerController* P = UGameplayStatics::GetPlayerController(this, 0))
	{
		Query.AddIgnoredActor(P->GetPawn());   // never "touch" your own body
	}
	for (TActorIterator<AGrappleRig> It(GetWorld()); It; ++It)
	{
		Query.AddIgnoredActor(*It);            // or your own gear and ropes
	}
	TArray<FOverlapResult> Hits;
	if (!GetWorld()->OverlapMultiByChannel(Hits, Tip, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(FingertipRadius), Query))
	{
		return false;
	}
	// Touching several things: the firmest one wins (a fern in front of a rock still lets the rock push).
	OutFirmness = 0.f;
	for (const FOverlapResult& Hit : Hits)
	{
		FString Name = Hit.GetActor() ? Hit.GetActor()->GetName() : FString();
		if (const UPrimitiveComponent* C = Hit.GetComponent())
		{
			Name += C->GetName() + C->GetDetailedInfo();   // the mesh's name is the best clue for plants
		}
		bool bSoft = false;
		for (const FString& W : SoftWords)
		{
			if (Name.Contains(W))
			{
				bSoft = true;
				break;
			}
		}
		OutFirmness = FMath::Max(OutFirmness, bSoft ? SoftFirmness : HardFirmness);
	}
	return true;
}

void AGRHandFeel::OnTouch(int32 Hand, int32 Finger, float Firmness)
{
	// Controllers (no gloves): a tiny tick of vibration on first contact.
	if (TouchBuzz > 0.f)
	{
		if (APlayerController* P = UGameplayStatics::GetPlayerController(this, 0))
		{
			P->SetHapticsByValue(0.8f, TouchBuzz * Firmness, Hand == 0 ? EControllerHand::Left : EControllerHand::Right);
			FTimerHandle Stop;
			TWeakObjectPtr<APlayerController> Weak = P;
			const EControllerHand Which = Hand == 0 ? EControllerHand::Left : EControllerHand::Right;
			GetWorldTimerManager().SetTimer(Stop, [Weak, Which]()
			{
				if (Weak.IsValid())
				{
					Weak->SetHapticsByValue(0.f, 0.f, Which);
				}
			}, 0.04f, false);
		}
	}
	// EXIT SUIT (later): this is the moment to send "finger F of hand H touched something of
	// this firmness" to the suit's own driver, alongside the glove.
}

void AGRHandFeel::Send(int32 Hand, const int16 Values[5])
{
#if PLATFORM_WINDOWS
	// Not connected yet? Try to open the glove's pipe (at most every 2 seconds).
	if (!Pipe[Hand])
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now < RetryIn[Hand])
		{
			return;
		}
		RetryIn[Hand] = Now + 2.f;
		HANDLE H = CreateFileW(PipeNames[Hand], GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (H == INVALID_HANDLE_VALUE)
		{
			return;   // no glove driver running: fine, try again later
		}
		Pipe[Hand] = H;
		UE_LOG(LogTemp, Log, TEXT("Grapple Runner hand feel: connected to the %s glove"), Hand == 0 ? TEXT("left") : TEXT("right"));
	}
	// The message: five 16-bit numbers, thumb to pinky (the glove driver's VRFFBData).
	DWORD Written = 0;
	if (!WriteFile(static_cast<HANDLE>(Pipe[Hand]), Values, sizeof(int16) * 5, &Written, nullptr))
	{
		CloseHandle(static_cast<HANDLE>(Pipe[Hand]));   // the driver went away: reconnect later
		Pipe[Hand] = nullptr;
		return;
	}
#endif
	FMemory::Memcpy(Sent[Hand], Values, sizeof(int16) * 5);
}

void AGRHandFeel::ClosePipes()
{
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		if (Pipe[Hand])
		{
			const int16 Free[5] = { 0, 0, 0, 0, 0 };
			Send(Hand, Free);   // let every finger go before we leave
#if PLATFORM_WINDOWS
			if (Pipe[Hand])
			{
				CloseHandle(static_cast<HANDLE>(Pipe[Hand]));
			}
#endif
			Pipe[Hand] = nullptr;
		}
	}
}

void AGRHandFeel::EndPlay(const EEndPlayReason::Type Reason)
{
	ClosePipes();
	Super::EndPlay(Reason);
}
