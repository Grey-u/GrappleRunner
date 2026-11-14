// =====================================================================================
//  GRCompanion.h  -  the AI partner who swings with you in "Endless Runner + AI partner"
// =====================================================================================
//
//  WHAT THIS IS
//  A second soldier who flies beside you through the endless run. They:
//    - keep up with you, a few metres to the side and a little ahead, on their own rope;
//    - help: when your tank is nearly empty they throw you some of their gas (once in a while);
//    - TALK: short back-and-forth conversations between them and you (your character answers),
//      shown as subtitles floating in the world, never stuck to the screen.
//
//  THE LINES ARE NOT IN THE CODE
//  Every line lives in Content/GrappleRunner/Dialogue/CompanionLines.csv, a plain table you
//  can open in Excel or Notepad. Columns: Event, Convo, Speaker, Line.
//    Event   when it can be said: Start, Pod, LowGas, Empty, ShareGas, Milestone100, Milestone250,
//            Milestone500, Milestone1000, Fell, Cave, Idle
//    Convo   lines with the same Event and Convo number form one conversation, in order
//    Speaker PARTNER or YOU
//  A writer can rewrite or add conversations without touching code (Naomi's film friend).
//
//  HOW IT MOVES (kept simple on purpose)
//  It isn't a full physics swing: it follows a moving target point beside you with a soft
//  spring, leans into the motion, and draws a rope to the nearest grapple rock above it.
//  That reads as swinging from a distance and can't fall behind or get stuck.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = can be changed in the Details panel without code.
//  - Distances are centimetres (100 = 1 metre).
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRCompanion.generated.h"

class AGrappleRig;
class AGREndlessSpawner;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class SWidget;

UCLASS()
class GRAPPLERUNNER_API AGRCompanion : public AActor
{
	GENERATED_BODY()

public:
	AGRCompanion();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Partner") FString PartnerName = TEXT("KAI");   // placeholder name until the story is written
	UPROPERTY(EditAnywhere, Category = "Partner") FVector Offset = FVector(600.f, 650.f, 250.f);   // ahead, to the side, above you
	UPROPERTY(EditAnywhere, Category = "Partner") float ShareGasAmount = 25.f;     // gas thrown to you when you're nearly empty
	UPROPERTY(EditAnywhere, Category = "Partner") float ShareGasCooldown = 45.f;   // seconds between gifts
	UPROPERTY(EditAnywhere, Category = "Partner") float QuietSeconds = 25.f;       // after this long without talking, some idle banter

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rope;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> PartnerCaption;   // above the partner's head
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> YouCaption;       // low in front of you, above the gas gauge

	TWeakObjectPtr<AGrappleRig> Rig;
	TWeakObjectPtr<AGREndlessSpawner> Spawner;
	FVector Velocity = FVector::ZeroVector;
	FVector Side = FVector(0.f, 1.f, 0.f);

	// ---- talking ----
	struct FLine { bool bPartner = true; FString Text; };
	TMap<FString, TArray<TArray<FLine>>> Convos;    // Event -> its conversations -> their lines
	TArray<FLine> Queue;                             // the conversation being spoken now
	FString PartnerText, YouText;
	float LineLeft = 0.f;                            // seconds the current line stays up
	float SinceTalk = 0.f;
	bool bChatter = true;
	TSet<FString> SaidOnce;                          // milestone / start events fire once per run

	// ---- what it watches ----
	float ShareCooldown = 10.f;
	float LastDistance = 0.f;
	int32 LastPods = 0;
	bool bWasLow = false, bWasEmpty = false;
	int32 CaveSaid = -1;
	TArray<TWeakObjectPtr<AActor>> Anchors;
	float AnchorRefresh = 0.f;
	FVector RopeAnchor = FVector::ZeroVector;

	void LoadLines();
	bool Say(const FString& Event);                  // start a conversation for that event (false if none)
	void UpdateTalk(float Dt);
	void Watch(float Dt);
	void Move(float Dt);
	TSharedRef<SWidget> Caption(bool bPartner);
};
