// =====================================================================================
//  GRCompanion.cpp  -  how the AI partner flies, helps and talks
// =====================================================================================
//
//  THE PIECES
//    constructor  the body (Kwang's "Rosewood" look, so it's clearly not you), the rope, and
//                 two subtitle cards (one above the partner, one in front of you)
//    BeginPlay    loads the conversations from CompanionLines.csv, says hello
//    Move         follows a point beside you with a spring; aims the rope at a rock above
//    Watch        notices things worth talking about (low gas, a canister, a milestone...)
//    Say          picks a random conversation for an event and queues its lines
//    UpdateTalk   shows each line long enough to read, then the next
// =====================================================================================
#include "GRCompanion.h"
#include "GrappleRig.h"
#include "GREndlessSpawner.h"
#include "GRGasPod.h"
#include "GRSettings.h"

#include "Animation/AnimSequence.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FVector2D CaptionPixels(900.f, 150.f);
	FSlateRoundedBoxBrush& CaptionBrush() { static FSlateRoundedBoxBrush B(FLinearColor::White, 16.f, FLinearColor(0.55f, 0.65f, 0.47f, 0.5f), 2.f); return B; }

	UWidgetComponent* MakeCaption(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UWidgetComponent* W = Owner->CreateDefaultSubobject<UWidgetComponent>(Name);
		W->SetupAttachment(Parent);
		W->SetWidgetSpace(EWidgetSpace::World);
		W->SetDrawSize(CaptionPixels);
		W->SetPivot(FVector2D(0.5f, 0.5f));
		W->SetTwoSided(true);
		W->SetBlendMode(EWidgetBlendMode::Masked);
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetCastShadow(false);
		W->SetUsingAbsoluteLocation(true);   // we place these ourselves every frame
		W->SetUsingAbsoluteRotation(true);
		W->SetUsingAbsoluteScale(true);
		return W;
	}
}

AGRCompanion::AGRCompanion()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -95.f));      // feet below the actor's centre
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));     // Paragon meshes face +Y; turn to face +X
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRenderCustomDepth(true);                         // the anime outline draws round anything with custom depth

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Rope = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rope"));
	Rope->SetupAttachment(Root);
	Rope->SetStaticMesh(Cylinder.Object);
	Rope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rope->SetCastShadow(false);
	Rope->SetUsingAbsoluteLocation(true);
	Rope->SetUsingAbsoluteRotation(true);
	Rope->SetUsingAbsoluteScale(true);

	PartnerCaption = MakeCaption(this, Root, TEXT("PartnerCaption"));
	YouCaption = MakeCaption(this, Root, TEXT("YouCaption"));
}

void AGRCompanion::BeginPlay()
{
	Super::BeginPlay();
	TActorIterator<AGrappleRig> R(GetWorld());
	Rig = R ? *R : nullptr;
	TActorIterator<AGREndlessSpawner> S(GetWorld());
	Spawner = S ? *S : nullptr;
	bChatter = GRSettings::PartnerChatter();

	// The body: Kwang in his Rosewood outfit. Two poses, each played ONCE and then held (a
	// looping clip "popped" every time it restarted, which looked like glitching):
	//   HookPose = the jump take-off, reaching up: used while hanging on a rope
	//   FlyPose  = the top of a jump, tucked: used while flying free between swings
	if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/KwangRosewood.KwangRosewood")))
	{
		Body->SetSkeletalMeshAsset(Mesh);
	}
	HookPose = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/Jump_TravelMode_Start.Jump_TravelMode_Start"));
	FlyPose = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/Jump_TravelMode_Apex.Jump_TravelMode_Apex"));
	if (FlyPose)
	{
		Body->PlayAnimation(FlyPose, false);
	}
	if (UMaterialInterface* RopeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GrappleRunner/Materials/M_Rope.M_Rope")))
	{
		Rope->SetMaterial(0, RopeMat);
	}

	PartnerCaption->SetSlateWidget(Caption(true));
	YouCaption->SetSlateWidget(Caption(false));
	PartnerCaption->SetWorldScale3D(FVector(110.f / CaptionPixels.X));   // 1.1 m wide above the partner
	YouCaption->SetWorldScale3D(FVector(34.f / CaptionPixels.X));        // 34 cm wide, close to you

	LoadLines();
	if (Rig.IsValid())
	{
		SetActorLocation(Rig->GetBodyPosition() + FVector(-300.f, 600.f, 300.f));
	}
	SinceTalk = QuietSeconds - 2.f;   // say hello about 2 seconds in
}

// ---- the subtitle card: speaker name over the line ------------------------------------
TSharedRef<SWidget> AGRCompanion::Caption(bool bPartner)
{
	return SNew(SBorder).BorderImage(&CaptionBrush()).BorderBackgroundColor(FLinearColor(0.006f, 0.009f, 0.007f, 1.f)).Padding(FMargin(26.f, 12.f))
		.Visibility_Lambda([this, bPartner] { return (bPartner ? PartnerText : YouText).IsEmpty() ? EVisibility::Hidden : EVisibility::Visible; })
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
			.ColorAndOpacity(bPartner ? FLinearColor(0.55f, 0.75f, 0.45f) : FLinearColor(0.75f, 0.8f, 0.72f))
			.Text(FText::FromString(bPartner ? PartnerName : FString(TEXT("YOU"))))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 34)).AutoWrapText(true)
			.ColorAndOpacity(FLinearColor(0.9f, 0.92f, 0.88f))
			.Text_Lambda([this, bPartner] { return FText::FromString(bPartner ? PartnerText : YouText); })
		]
	];
}

// ---- reading CompanionLines.csv ----------------------------------------------------------
void AGRCompanion::LoadLines()
{
	const FString File = FPaths::ProjectContentDir() / TEXT("GrappleRunner/Dialogue/CompanionLines.csv");
	TArray<FString> Rows;
	if (!FFileHelper::LoadFileToStringArray(Rows, *File))
	{
		UE_LOG(LogTemp, Warning, TEXT("AI partner: no lines file at %s"), *File);
		return;
	}
	TMap<FString, TMap<int32, TArray<FLine>>> ByConvo;
	for (int32 i = 1; i < Rows.Num(); ++i)   // row 0 is the header
	{
		// Split on the first three commas only, so a line may contain commas.
		FString Rest = Rows[i], Event, Convo, Speaker;
		if (!Rest.Split(TEXT(","), &Event, &Rest) || !Rest.Split(TEXT(","), &Convo, &Rest) || !Rest.Split(TEXT(","), &Speaker, &Rest))
		{
			continue;
		}
		FLine L;
		L.bPartner = Speaker.TrimStartAndEnd().Equals(TEXT("PARTNER"), ESearchCase::IgnoreCase);
		L.Text = Rest.TrimStartAndEnd().TrimQuotes().Replace(TEXT("\"\""), TEXT("\""));
		if (!L.Text.IsEmpty())
		{
			ByConvo.FindOrAdd(Event.TrimStartAndEnd()).FindOrAdd(FCString::Atoi(*Convo)).Add(L);
		}
	}
	for (TPair<FString, TMap<int32, TArray<FLine>>>& E : ByConvo)
	{
		TArray<TArray<FLine>>& List = Convos.FindOrAdd(E.Key);
		for (TPair<int32, TArray<FLine>>& C : E.Value)
		{
			List.Add(C.Value);
		}
	}
}

bool AGRCompanion::Say(const FString& Event)
{
	if (!bChatter || Queue.Num() > 0)
	{
		return false;   // talking is off, or already in the middle of a conversation
	}
	const TArray<TArray<FLine>>* List = Convos.Find(Event);
	if (!List || List->Num() == 0)
	{
		return false;
	}
	Queue = (*List)[FMath::RandRange(0, List->Num() - 1)];
	LineLeft = 0.f;
	SinceTalk = 0.f;
	return true;
}

void AGRCompanion::UpdateTalk(float Dt)
{
	SinceTalk += Dt;
	LineLeft -= Dt;
	if (LineLeft > 0.f)
	{
		return;
	}
	PartnerText.Reset();
	YouText.Reset();
	if (Queue.Num() == 0)
	{
		return;
	}
	const FLine L = Queue[0];
	Queue.RemoveAt(0);
	(L.bPartner ? PartnerText : YouText) = L.Text;
	LineLeft = FMath::Max(2.2f, 0.065f * L.Text.Len());   // long enough to read
	SinceTalk = 0.f;
}

// ---- noticing things ---------------------------------------------------------------------
void AGRCompanion::Watch(float Dt)
{
	AGrappleRig* R = Rig.Get();
	if (!R)
	{
		return;
	}
	const float Frac = R->GetGas() / FMath::Max(1.f, R->GetGasMax());
	ShareCooldown -= Dt;

	// Out of gas, or nearly: help (if we can), otherwise warn.
	if (Frac <= 0.2f && ShareCooldown <= 0.f)
	{
		R->AddGas(ShareGasAmount);
		ShareCooldown = ShareGasCooldown;
		Say(TEXT("ShareGas"));
	}
	if (Frac <= 0.f && !bWasEmpty)
	{
		Say(TEXT("Empty"));
	}
	else if (Frac < 0.25f && !bWasLow)
	{
		Say(TEXT("LowGas"));
	}
	bWasEmpty = Frac <= 0.f;
	bWasLow = Frac < 0.25f;

	// You sliced a canister (not every time, or it gets repetitive).
	if (AGRGasPod::SlicedThisRun > LastPods)
	{
		if (FMath::FRand() < 0.4f)
		{
			Say(TEXT("Pod"));
		}
		LastPods = AGRGasPod::SlicedThisRun;
	}

	// Distance milestones, a fall (the run reset), and caves ahead.
	if (AGREndlessSpawner* S = Spawner.Get())
	{
		const float D = S->GetDistance();
		if (D + 20.f < LastDistance)
		{
			SaidOnce.Reset();
			Say(TEXT("Fell"));
		}
		for (const int32 M : { 100, 250, 500, 1000 })
		{
			const FString Key = FString::Printf(TEXT("Milestone%d"), M);
			if (D >= M && LastDistance < M && !SaidOnce.Contains(Key))
			{
				SaidOnce.Add(Key);
				Say(Key);
			}
		}
		LastDistance = D;
		const TArray<FVector>& Caves = S->GetCaveCentres();
		for (int32 i = Caves.Num() - 1; i > CaveSaid && i >= 0; --i)
		{
			const FVector ToCave = Caves[i] - R->GetBodyPosition();
			if (ToCave.X > 0.f && ToCave.X < 4000.f)
			{
				CaveSaid = i;
				Say(TEXT("Cave"));
				break;
			}
		}
	}
	if (!SaidOnce.Contains(TEXT("Start")) && SinceTalk > QuietSeconds)
	{
		SaidOnce.Add(TEXT("Start"));
		Say(TEXT("Start"));
	}
	else if (SinceTalk > QuietSeconds)
	{
		Say(TEXT("Idle"));
	}
}

// ---- flying beside you -------------------------------------------------------------------
// Hook a grapple rock ahead of us and above, near where we want to be. True if one was found.
bool AGRCompanion::TryHook(const FVector& At, const FVector& Ahead, const FVector& Target)
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("GrappleAnchor")), Found);
	const FVector Ideal = Target + Ahead * 900.f + FVector(0.f, 0.f, 900.f);   // a good rock is ahead of the spot beside you, and high
	float Best = 1e12f;
	AActor* Pick = nullptr;
	for (AActor* A : Found)
	{
		const FVector D = A->GetActorLocation() - At;
		if (D.Z < 300.f || D.Size() > 2400.f || FVector::DotProduct(D, Ahead) < 300.f)
		{
			continue;   // too low, too far, or behind
		}
		const float Score = FVector::DistSquared(A->GetActorLocation(), Ideal);
		if (Score < Best)
		{
			Best = Score;
			Pick = A;
		}
	}
	if (!Pick)
	{
		return false;
	}
	RopeAnchor = Pick->GetActorLocation();
	RopeLength = FVector::Dist(RopeAnchor, At) * 0.95f;   // a touch shorter, so the rope pulls tight at once
	bSwinging = true;
	SwingTime = 0.f;
	RopeShoot = 0.f;
	if (HookPose)
	{
		Body->PlayAnimation(HookPose, false);
	}
	return true;
}

// Let go of the rope with a little flick forward and up (like you do at the bottom of a swing).
void AGRCompanion::LetGo(const FVector& Ahead)
{
	bSwinging = false;
	HookCooldown = 0.35f;
	Velocity += Ahead * 250.f + FVector(0.f, 0.f, 350.f);
	if (FlyPose)
	{
		Body->PlayAnimation(FlyPose, false);
	}
}

// How the partner flies: real swings on a rope, like you.
//   1. Gravity pulls down; a gentle steer pulls towards a spot beside you (stronger off the rope).
//   2. On a rope: it can't get further from the rock than the rope's length (the rope "catches"
//      it and turns falling into swinging), and it reels in slowly. It lets go once it has
//      swung past the rock, then hooks the next rock ahead.
//   3. It turns smoothly to face where it's going and leans into the swing.
void AGRCompanion::Move(float Dt)
{
	AGrappleRig* R = Rig.Get();
	if (!R)
	{
		return;
	}
	Dt = FMath::Min(Dt, 0.05f);   // a long frame mustn't fling it
	const FVector P = R->GetBodyPosition();
	FVector Ahead = R->GetVelocity();
	Ahead.Z = 0.f;
	Ahead = Ahead.SizeSquared() > 200.f * 200.f ? Ahead.GetSafeNormal() : FVector(1.f, 0.f, 0.f);   // slow: assume the run's direction (+X)
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Ahead);
	const FVector Target = P + Ahead * Offset.X + Right * Offset.Y + FVector(0.f, 0.f, Offset.Z);
	const FVector Me = GetActorLocation();

	// 1. gravity and steering
	Velocity += FVector(0.f, 0.f, -980.f) * (bSwinging ? 1.f : 0.45f) * Dt;   // off the rope it glides a bit
	const float Steer = bSwinging ? 0.8f : 2.5f;
	const float Drag = bSwinging ? 0.15f : 1.2f;
	Velocity += ((Target - Me) * Steer - Velocity * Drag) * Dt;
	Velocity = Velocity.GetClampedToMaxSize(5000.f);
	FVector Next = Me + Velocity * Dt;

	// 2. the rope
	if (bSwinging)
	{
		SwingTime += Dt;
		RopeShoot = FMath::Min(1.f, RopeShoot + Dt / 0.12f);
		RopeLength = FMath::Max(300.f, RopeLength - 120.f * Dt);   // reel in a little
		FVector Out = Next - RopeAnchor;
		const float Dist = Out.Size();
		if (Dist > RopeLength && Dist > KINDA_SMALL_NUMBER)
		{
			Out /= Dist;
			Next = RopeAnchor + Out * RopeLength;                   // the rope catches
			const float Outward = FVector::DotProduct(Velocity, Out);
			if (Outward > 0.f)
			{
				Velocity -= Out * Outward;                           // no more moving away from the rock
			}
		}
		const float Past = FVector::DotProduct(Next - RopeAnchor, Ahead);
		if ((Past > 150.f && Velocity.Z > 0.f) || Past > 900.f || SwingTime > 3.5f)
		{
			LetGo(Ahead);
		}
	}
	else
	{
		HookCooldown -= Dt;
		if (HookCooldown <= 0.f && !TryHook(Next, Ahead, Target))
		{
			HookCooldown = 0.3f;   // nothing in reach: glide and look again soon
		}
	}
	if (FVector::Dist(Next, Target) > 6000.f)
	{
		Next = Target;                       // fell far behind (e.g. you respawned): catch up at once
		Velocity = R->GetVelocity();
		if (bSwinging)
		{
			LetGo(Ahead);
		}
	}
	SetActorLocation(Next);

	// 3. face the way it's going, smoothly (snapping every frame was the "glitch")
	const FVector Flat(Velocity.X, Velocity.Y, 0.f);
	if (Flat.SizeSquared() > 400.f)
	{
		const FRotator Want(FMath::Clamp(-Velocity.Z * 0.012f, -30.f, 30.f), Flat.Rotation().Yaw, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, Dt, 3.5f));
	}

	// The rope from the right hand to the rock, flying out over 0.12 s after each hook.
	const FVector Hand = Body->DoesSocketExist(TEXT("hand_r")) ? Body->GetSocketLocation(TEXT("hand_r")) : Next + FVector(0.f, 0.f, 60.f);
	if (bSwinging)
	{
		const FVector Line = (RopeAnchor - Hand) * RopeShoot;
		Rope->SetVisibility(true);
		Rope->SetWorldLocation(Hand + Line * 0.5f);
		Rope->SetWorldRotation(FRotationMatrix::MakeFromZ(Line.GetSafeNormal()).Rotator());
		Rope->SetWorldScale3D(FVector(0.025f, 0.025f, FMath::Max(1.f, Line.Size()) / 100.f));
	}
	else
	{
		Rope->SetVisibility(false);
	}
}

void AGRCompanion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Move(DeltaSeconds);
	Watch(DeltaSeconds);
	UpdateTalk(DeltaSeconds);

	// Place the two subtitle cards, both turned to face your eyes.
	AGrappleRig* R = Rig.Get();
	UCameraComponent* Eyes = R ? R->GetEyes() : nullptr;
	if (!Eyes)
	{
		return;
	}
	const FVector EyeAt = Eyes->GetComponentLocation();
	const FVector Above = GetActorLocation() + FVector(0.f, 0.f, 150.f);
	PartnerCaption->SetWorldLocation(Above);
	PartnerCaption->SetWorldRotation((EyeAt - Above).Rotation());
	const FRotator Yaw(0.f, Eyes->GetComponentRotation().Yaw, 0.f);
	const FVector Low = EyeAt + Yaw.RotateVector(FVector(60.f, 0.f, -20.f));   // just above the gas gauge
	YouCaption->SetWorldLocation(Low);
	YouCaption->SetWorldRotation((EyeAt - Low).Rotation());
}
