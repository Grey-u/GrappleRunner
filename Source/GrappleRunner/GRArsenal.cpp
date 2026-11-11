// =====================================================================================
//  GRArsenal.cpp  -  what the weapons DO (see GRArsenal.h for the list and the controls)
// =====================================================================================
//
//  HOW ONE FRAME WORKS (Tick, at the bottom of this file)
//    1. ReadInput     number keys / wheel / X / T / mouse buttons
//    2. bursts and cooldowns count down; a burst fires its next shot when ready
//    3. Animate       work out where the weapon sits in front of your eyes this frame
//    4. UpdateEffects tracers and sparks fade out
//
//  HOW A SHOT WORKS (Fire)
//    A straight line ("line trace") goes out from your eyes, nudged by a little random
//    spread. Whatever it touches first is hit: it gets damage (enemies will listen for it),
//    loose physics objects get a shove, vines on the way snap. A glowing streak (tracer)
//    is drawn from the barrel to the hit point for a few hundredths of a second.
//
//  HOW A SLASH WORKS (StartMelee / MeleeHit)
//    The blade follows a hand-made curve (wind up, sweep, recover). Halfway through, a fat
//    invisible ball is swept out from your eyes as far as the blade reaches: everything it
//    touches is hit, and vines in the way are cut.
// =====================================================================================
#include "GRArsenal.h"
#include "GrappleRig.h"
#include "GRVine.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

namespace
{
	// A curve made of keys (time 0..1, value). Between two keys it eases smoothly.
	// Used for the slash and stab motions: e.g. {{0,0},{0.3,50},{0.6,-70},{1,0}}.
	float Curve(float P, std::initializer_list<FVector2D> Keys)
	{
		const FVector2D* Prev = nullptr;
		for (const FVector2D& K : Keys)
		{
			if (Prev && P <= K.X)
			{
				const float A = (P - Prev->X) / FMath::Max(0.0001f, K.X - Prev->X);
				return FMath::Lerp(static_cast<float>(Prev->Y), static_cast<float>(K.Y), FMath::SmoothStep(0.f, 1.f, A));
			}
			Prev = &K;
		}
		return Prev ? static_cast<float>(Prev->Y) : 0.f;
	}

	const FName SightSocket(TEXT("Sight"));
	const FName MuzzleSocket(TEXT("Muzzle"));
	constexpr int32 EffectCount = 14;   // tracers + sparks on screen at once
}

// =====================================================================================
//  Constructor: build the parts that always exist (root, muzzle light, effect pool)
// =====================================================================================
AGRArsenal::AGRArsenal()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Cylinder = CylinderAsset.Object;

	// The muzzle flash light: off until a shot, then on for a split second.
	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(RootComponent);
	MuzzleLight->SetUsingAbsoluteLocation(true);
	MuzzleLight->SetIntensityUnits(ELightUnits::Candelas);
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetAttenuationRadius(500.f);
	MuzzleLight->SetCastShadows(false);

	SetDefaultWeapons();
}

// =====================================================================================
//  The starting weapon slots. Change any of it in the Details panel.
//  Meshes come from Scripts/import_skin_weapons.py (kept off GitHub: not our models).
// =====================================================================================
void AGRArsenal::SetDefaultWeapons()
{
	auto Path = [](const TCHAR* Name)
	{
		return TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/ThirdParty/SkinWeapons/%s.%s"), Name, Name)));
	};
	// One helper per kind, filled with numbers close to how these weapons feel in their home game.
	auto Gun = [&](const TCHAR* Label, const TCHAR* Mesh, EGRWeaponKind Kind, float Damage, float Rate, int32 Mag,
		float Reload, float Hip, float Aim, float Kick, float FOV, FLinearColor Tracer)
	{
		FGRWeapon W;
		W.Name = Label;
		W.Mesh = Path(Mesh);
		W.Kind = Kind;
		W.Damage = Damage;
		W.FireRate = Rate;
		W.Magazine = Mag;
		W.ReloadSeconds = Reload;
		W.SpreadHip = Hip;
		W.SpreadAim = Aim;
		W.RecoilKick = Kick;
		W.AimFOV = FOV;
		W.bScope = Kind == EGRWeaponKind::Sniper;
		W.TracerColour = Tracer;
		if (Kind == EGRWeaponKind::Pistol)
		{
			W.HipOffset = FVector(28.f, 12.f, -13.f);
		}
		return W;
	};
	auto Blade = [&](const TCHAR* Label, const TCHAR* Mesh, float Damage)
	{
		FGRWeapon W;
		W.Name = Label;
		W.Mesh = Path(Mesh);
		W.Kind = EGRWeaponKind::Knife;
		W.Damage = Damage;
		W.Scale = 0.7f;
		W.HipOffset = FVector(28.f, 17.f, -19.f);
		W.HipTurn = FRotator(20.f, -12.f, 0.f);   // blade tipped up and in, ready to slash
		return W;
	};
	const FLinearColor KuronamiBlue(0.25f, 0.6f, 1.f);
	const FLinearColor AraxysTeal(0.3f, 1.f, 0.75f);

	Weapons.Reset();
	// Key 1: the twin ODM swords from the gear (our own import, not a skin).
	FGRWeapon Swords;
	Swords.Name = TEXT("ODM swords");
	Swords.Kind = EGRWeaponKind::Swords;
	Swords.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Harness/SM_ODM_Sword_R.SM_ODM_Sword_R")));
	Swords.LeftMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Harness/SM_ODM_Sword_L.SM_ODM_Sword_L")));
	Swords.Scale = 1.f;
	Swords.Damage = 60.f;
	Swords.Reach = 190.f;
	Swords.HipOffset = FVector(40.f, 24.f, -28.f);
	Swords.HipTurn = FRotator(-35.f, 0.f, 0.f);
	Weapons.Add(Swords);
	// Keys 2-6: Kuronami (blue water)
	Weapons.Add(Gun(TEXT("Kuronami Rifle"), TEXT("SM_Kuronami_Rifle"), EGRWeaponKind::Rifle, 40.f, 9.75f, 25, 2.5f, 2.f, 0.25f, 0.7f, 70.f, KuronamiBlue));
	Weapons.Add(Gun(TEXT("Kuronami SMG"), TEXT("SM_Kuronami_SMG"), EGRWeaponKind::SMG, 26.f, 13.33f, 30, 2.25f, 1.6f, 0.4f, 0.45f, 78.f, KuronamiBlue));
	Weapons.Add(Gun(TEXT("Kuronami Marksman"), TEXT("SM_Kuronami_Marksman"), EGRWeaponKind::Sniper, 101.f, 1.5f, 5, 2.5f, 4.f, 0.f, 2.5f, 35.f, KuronamiBlue));
	Weapons.Add(Gun(TEXT("Kuronami Revolver"), TEXT("SM_Kuronami_Revolver"), EGRWeaponKind::Pistol, 55.f, 4.f, 6, 2.25f, 1.2f, 0.5f, 1.6f, 80.f, KuronamiBlue));
	Weapons.Add(Blade(TEXT("Kuronami Blade"), TEXT("SM_Kuronami_Knife"), 50.f));
	// Keys 7-0, then the wheel: Araxys (alien teal)
	Weapons.Add(Gun(TEXT("Araxys Rifle"), TEXT("SM_Araxys_Rifle"), EGRWeaponKind::Rifle, 40.f, 9.75f, 25, 2.5f, 2.f, 0.25f, 0.7f, 70.f, AraxysTeal));
	Weapons.Add(Gun(TEXT("Araxys Burst"), TEXT("SM_Araxys_Burst"), EGRWeaponKind::Burst, 35.f, 15.f, 21, 2.5f, 1.8f, 0.2f, 0.5f, 75.f, AraxysTeal));
	Weapons.Add(Gun(TEXT("Araxys Sniper"), TEXT("SM_Araxys_Sniper"), EGRWeaponKind::Sniper, 150.f, 0.6f, 5, 3.7f, 5.f, 0.f, 3.5f, 25.f, AraxysTeal));
	Weapons.Add(Gun(TEXT("Araxys Sidearm"), TEXT("SM_Araxys_Sidearm"), EGRWeaponKind::Pistol, 30.f, 6.75f, 15, 1.5f, 1.f, 0.4f, 0.8f, 80.f, AraxysTeal));
	Weapons.Add(Blade(TEXT("Araxys Knife"), TEXT("SM_Araxys_Knife"), 50.f));
}

// =====================================================================================
//  BeginPlay: find the rig, fill every magazine, build the effect pool and scope reticle
// =====================================================================================
void AGRArsenal::BeginPlay()
{
	Super::BeginPlay();
	Ammo.Init(0, Weapons.Num());
	for (int32 i = 0; i < Weapons.Num(); ++i)
	{
		Ammo[i] = Weapons[i].Magazine;
	}

	// The effect pool: a few glowing cylinders that get moved where a tracer or spark is needed.
	UMaterialInterface* Glow = GlowMaterial.LoadSynchronous();
	if (!Glow)
	{
		Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	for (int32 i = 0; i < EffectCount; ++i)
	{
		UStaticMeshComponent* E = NewObject<UStaticMeshComponent>(this);
		E->SetStaticMesh(Cylinder);
		E->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		E->SetCastShadow(false);
		E->SetUsingAbsoluteLocation(true);   // placed straight in the world, not relative to this actor
		E->SetUsingAbsoluteRotation(true);
		E->SetUsingAbsoluteScale(true);
		E->RegisterComponent();
		E->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		E->SetMaterial(0, UMaterialInstanceDynamic::Create(Glow, this));   // each its own copy, so each can have its own colour
		E->SetHiddenInGame(true);
		Effects.Add(E);
		EffectLife.Add(0.f);
		EffectLifeStart.Add(1.f);
		EffectScale.Add(FVector::OneVector);
	}

	// Scope reticle: two thin black lines across the screen. Hidden until you look through a scope.
	if (GEngine && GEngine->GameViewport)
	{
		Reticle = SNew(SOverlay)
			.Visibility(EVisibility::Collapsed)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Fill)
			[
				SNew(SBox).WidthOverride(2.f)[SNew(SImage).Image(&ReticleBrush)]
			]
			+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(2.f)[SNew(SImage).Image(&ReticleBrush)]
			];
		GEngine->GameViewport->AddViewportWidgetContent(Reticle.ToSharedRef(), 5);
	}
	FindRig();
}

void AGRArsenal::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Reticle.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Reticle.ToSharedRef());
	}
	Reticle.Reset();
	// Put the view back the way the rig had it.
	if (AGrappleRig* R = Rig.Get())
	{
		if (bHaveBaseFOV && R->GetEyes())
		{
			R->GetEyes()->SetFieldOfView(BaseFOV);
		}
		R->MouseSensitivity = BaseSensitivity;
		R->SetMouseFiresHooks(true);
	}
	Super::EndPlay(Reason);
}

bool AGRArsenal::FindRig()
{
	if (Rig.IsValid())
	{
		return true;
	}
	for (TActorIterator<AGrappleRig> It(GetWorld()); It; ++It)
	{
		Rig = *It;
		AddTickPrerequisiteActor(*It);   // tick AFTER the rig, so the camera has already turned this frame
		BaseSensitivity = It->MouseSensitivity;
		return true;
	}
	return false;
}

bool AGRArsenal::IsGun(EGRWeaponKind Kind) const
{
	return Kind == EGRWeaponKind::Rifle || Kind == EGRWeaponKind::SMG || Kind == EGRWeaponKind::Burst
		|| Kind == EGRWeaponKind::Pistol || Kind == EGRWeaponKind::Sniper;
}

// =====================================================================================
//  Switching: lower the old weapon, swap at the bottom, raise the new one
// =====================================================================================
void AGRArsenal::StartSwitch(int32 Slot)
{
	if (!Weapons.IsValidIndex(Slot) || (Slot == Current && !bHolstered && Pending == -1))
	{
		return;
	}
	const FGRWeapon& W = Weapons[Slot];
	if (W.Kind != EGRWeaponKind::Hands && W.Mesh.IsNull())
	{
		return;   // empty slot
	}
	ReloadLeft = 0.f;      // switching cancels a reload, a burst and a swing
	BurstLeft = 0;
	MeleeTime = -1.f;
	if (bHolstered || Current < 0)
	{
		// Nothing in hand: skip the "lower" half and just raise the new weapon.
		bHolstered = false;
		ShowSlot(Slot);
		Pending = Slot;
		bSwapped = true;
		SwitchTime = SwitchSeconds * 0.5f;
	}
	else
	{
		Pending = Slot;
		bSwapped = false;
		SwitchTime = 0.f;
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(4201, 1.5f, FColor::White, FString::Printf(TEXT("%d  %s"), (Slot + 1) % 10, *W.Name));
	}
}

// Put the meshes for this slot in your hands (desktop: in front of the eyes; VR: in the controllers).
void AGRArsenal::ShowSlot(int32 Slot)
{
	Current = Slot;
	AGrappleRig* R = Rig.Get();
	if (!R || !Weapons.IsValidIndex(Slot))
	{
		return;
	}
	const FGRWeapon& W = Weapons[Slot];
	auto Holder = [R](int32 Side) -> USceneComponent*
	{
		if (R->IsUsingVR() && R->GetHand(Side))
		{
			return R->GetHand(Side);
		}
		return R->GetEyes() ? static_cast<USceneComponent*>(R->GetEyes()) : R->GetRootComponent();
	};
	auto Show = [&](TObjectPtr<UStaticMeshComponent>& Part, const TSoftObjectPtr<UStaticMesh>& Asset, int32 Side)
	{
		UStaticMesh* Mesh = Asset.IsNull() ? nullptr : Asset.LoadSynchronous();
		if (!Mesh)
		{
			if (Part)
			{
				Part->SetVisibility(false);
			}
			return;
		}
		if (!Part)
		{
			Part = NewObject<UStaticMeshComponent>(this);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // held items never block anything
			Part->SetCastShadow(false);
			Part->RegisterComponent();
			AddInstanceComponent(Part);
		}
		Part->SetStaticMesh(Mesh);
		Part->AttachToComponent(Holder(Side), FAttachmentTransformRules::KeepRelativeTransform);
		Part->SetRelativeScale3D(FVector(W.Scale));
		Part->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);   // Animate places it properly
		Part->SetVisibility(true);
		Part->SetHiddenInGame(false);
	};
	Show(RightMesh, W.Mesh, 1);
	Show(LeftMesh, W.LeftMesh, 0);
	if (W.LeftMesh.IsNull() && LeftMesh)
	{
		LeftMesh->SetVisibility(false);
	}
	if (Ammo.IsValidIndex(Slot) && Ammo[Slot] <= 0)
	{
		StartReload();
	}
}

// =====================================================================================
//  Input (desktop). VR weapon buttons come with the headset step.
// =====================================================================================
void AGRArsenal::ReadInput(float Dt)
{
	AGrappleRig* R = Rig.Get();
	APlayerController* P = R ? R->GetPlayerController() : nullptr;
	bWantAim = false;
	if (!P || R->IsUsingVR())
	{
		return;
	}
	// Number keys: 1 = first slot ... 9 = ninth, 0 = tenth.
	static const FKey Numbers[10] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
	for (int32 i = 0; i < 10 && i < Weapons.Num(); ++i)
	{
		if (P->WasInputKeyJustPressed(Numbers[i]))
		{
			StartSwitch(i);
		}
	}
	// Mouse wheel: the next (or previous) slot that has something in it.
	const int32 Step = P->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? -1 : (P->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1 : 0);
	if (Step != 0 && Weapons.Num() > 0)
	{
		const int32 From = Pending >= 0 ? Pending : FMath::Max(0, Current);
		for (int32 Tries = 1; Tries <= Weapons.Num(); ++Tries)
		{
			const int32 Next = (From + Step * Tries + Weapons.Num() * 2) % Weapons.Num();
			if (!Weapons[Next].Mesh.IsNull())
			{
				StartSwitch(Next);
				break;
			}
		}
	}
	// X: put the weapon away (hands free, mouse buttons fire hooks) or take it out again.
	if (P->WasInputKeyJustPressed(EKeys::X) && Pending == -1)
	{
		if (bHolstered)
		{
			StartSwitch(FMath::Max(0, Current));
		}
		else if (Current >= 0)
		{
			Pending = -2;          // -2 = "putting it away"
			bSwapped = false;
			SwitchTime = 0.f;
			ReloadLeft = 0.f;
			MeleeTime = -1.f;
		}
	}

	const FGRWeapon* W = Held();
	if (!W || Pending != -1 || W->Kind == EGRWeaponKind::Hands)
	{
		return;
	}
	if (P->WasInputKeyJustPressed(EKeys::T))
	{
		StartReload();
	}
	const bool bFireDown = P->IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bFirePressed = P->WasInputKeyJustPressed(EKeys::LeftMouseButton);
	const bool bAltDown = P->IsInputKeyDown(EKeys::RightMouseButton);
	const bool bAltPressed = P->WasInputKeyJustPressed(EKeys::RightMouseButton);

	if (IsBlade(W->Kind))
	{
		if (MeleeTime < 0.f && (bFirePressed || bAltPressed))
		{
			StartMelee(bAltPressed);
		}
		return;
	}
	bWantAim = bAltDown && ReloadLeft <= 0.f;
	const bool bReady = Cooldown <= 0.f && ReloadLeft <= 0.f;
	switch (W->Kind)
	{
	case EGRWeaponKind::Rifle:
	case EGRWeaponKind::SMG:
		if (bFireDown && bReady) { Fire(); }           // automatic: keeps firing while held
		break;
	case EGRWeaponKind::Burst:
		if (bFirePressed && bReady && BurstLeft == 0) { BurstLeft = 3; }   // Tick fires the three shots
		break;
	default:
		if (bFirePressed && bReady) { Fire(); }        // one shot per click
		break;
	}
}

// =====================================================================================
//  One shot
// =====================================================================================
void AGRArsenal::Fire()
{
	AGrappleRig* R = Rig.Get();
	if (!R || !R->GetEyes() || !Weapons.IsValidIndex(Current))
	{
		return;
	}
	const FGRWeapon& W = Weapons[Current];
	if (Ammo[Current] <= 0)
	{
		BurstLeft = 0;
		StartReload();
		return;
	}
	Ammo[Current]--;
	Cooldown = 1.f / FMath::Max(0.1f, W.FireRate);

	// 1. Where the shot starts and which way it goes: from the EYES (camera rule), or in VR
	//    from the barrel. Spread: a random tilt inside a small cone, smaller when aiming,
	//    bigger while flying through the air.
	UCameraComponent* Eyes = R->GetEyes();
	FVector Start = Eyes->GetComponentLocation();
	FVector Dir = Eyes->GetForwardVector();
	if (R->IsUsingVR() && RightMesh)
	{
		Start = MuzzleWorld();
		Dir = RightMesh->GetForwardVector();
	}
	float Spread = FMath::Lerp(W.SpreadHip, W.SpreadAim, AimAlpha);
	if (!R->IsGrounded())
	{
		Spread *= 1.5f;
	}
	Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(FMath::Max(0.01f, Spread)));
	const FVector End = Start + Dir * W.Range;

	// 2. The line trace: what's the first thing on that line?
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GRShot), true, this);
	Query.AddIgnoredActor(R);
	if (APlayerController* P = R->GetPlayerController())
	{
		Query.AddIgnoredActor(P->GetPawn());
	}
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
	const FVector HitPoint = bHit ? Hit.ImpactPoint : End;
	if (bHit)
	{
		if (AActor* Target = Hit.GetActor())
		{
			// Damage: anything that cares (enemies, later) receives it through Unreal's damage system.
			UGameplayStatics::ApplyPointDamage(Target, W.Damage, Dir, Hit, R->GetPlayerController(), this, UDamageType::StaticClass());
		}
		if (UPrimitiveComponent* C = Hit.GetComponent(); C && C->IsSimulatingPhysics())
		{
			C->AddImpulseAtLocation(Dir * W.Damage * 300.f, Hit.ImpactPoint);   // loose things get knocked
		}
		ShowEffect(HitPoint - Dir * 4.f, HitPoint + Hit.ImpactNormal * 14.f, 5.f, 0.12f, W.TracerColour);   // spark
	}
	SnapVinesAlong(Start, HitPoint, 12.f);

	// 3. What you see: tracer from the barrel, a flash of light, the gun kicking back.
	const FVector Muzzle = MuzzleWorld();
	ShowEffect(Muzzle, HitPoint, W.Kind == EGRWeaponKind::Sniper ? 2.5f : 1.2f, W.Kind == EGRWeaponKind::Sniper ? 0.15f : 0.06f, W.TracerColour);
	MuzzleLight->SetWorldLocation(Muzzle);
	MuzzleLight->SetLightColor(W.TracerColour);
	MuzzleLight->SetIntensity(300.f);
	MuzzleFlash = 0.05f;
	RecoilPitch += W.RecoilKick * 3.f;
	RecoilBack += FMath::Clamp(W.RecoilKick * 2.5f, 1.f, 8.f);
	R->AddLookKick(W.RecoilKick * (1.f - 0.4f * AimAlpha), FMath::FRandRange(-0.3f, 0.3f) * W.RecoilKick);

	if (Ammo[Current] <= 0)
	{
		BurstLeft = 0;
		StartReload();   // empty: reload by itself
	}
}

void AGRArsenal::StartReload()
{
	const FGRWeapon* W = Held();
	if (!W || !IsGun(W->Kind) || ReloadLeft > 0.f || Ammo[Current] >= W->Magazine)
	{
		return;
	}
	ReloadLeft = W->ReloadSeconds;
	BurstLeft = 0;
}

// =====================================================================================
//  Melee
// =====================================================================================
void AGRArsenal::StartMelee(bool bHeavyStab)
{
	MeleeTime = 0.f;
	bHeavy = bHeavyStab;
	bMeleeHit = false;
	bLeftBlade = !bLeftBlade && LeftMesh && LeftMesh->IsVisible();   // twin swords take turns
}

void AGRArsenal::MeleeHit()
{
	AGrappleRig* R = Rig.Get();
	const FGRWeapon* W = Held();
	if (!R || !W || !R->GetEyes())
	{
		return;
	}
	bMeleeHit = true;
	const FVector Start = R->GetEyes()->GetComponentLocation();
	const FVector Dir = R->GetEyes()->GetForwardVector();
	const FVector End = Start + Dir * W->Reach * (bHeavy ? 1.15f : 1.f);
	const float Radius = bHeavy ? 20.f : 40.f;   // a stab is narrow, a slash is wide

	FCollisionQueryParams Query(SCENE_QUERY_STAT(GRMelee), false, this);
	Query.AddIgnoredActor(R);
	if (APlayerController* P = R->GetPlayerController())
	{
		Query.AddIgnoredActor(P->GetPawn());
	}
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Query);
	TSet<AActor*> Done;   // hit each thing once per swing
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Done.Contains(Target))
		{
			continue;
		}
		Done.Add(Target);
		UGameplayStatics::ApplyPointDamage(Target, W->Damage * (bHeavy ? 1.5f : 1.f), Dir, Hit, R->GetPlayerController(), this, UDamageType::StaticClass());
		if (UPrimitiveComponent* C = Hit.GetComponent(); C && C->IsSimulatingPhysics())
		{
			C->AddImpulseAtLocation(Dir * 20000.f, Hit.ImpactPoint);
		}
		ShowEffect(Hit.ImpactPoint, Hit.ImpactPoint + Hit.ImpactNormal * 18.f, 4.f, 0.1f, FLinearColor(1.f, 0.9f, 0.7f));
	}
	SnapVinesAlong(Start, End, Radius + 15.f);   // blades cut vines
}

// =====================================================================================
//  Vines on the line From -> To snap (same check the body uses when you fly through them)
// =====================================================================================
void AGRArsenal::SnapVinesAlong(const FVector& From, const FVector& To, float Radius)
{
	AGrappleRig* R = Rig.Get();
	if (!R)
	{
		return;
	}
	const FVector Push = (To - From).GetSafeNormal() * 5000.f;   // fast enough to count as a real hit
	for (const TWeakObjectPtr<AGRVine>& Weak : R->GetVines())
	{
		AGRVine* Vine = Weak.Get();
		if (!Vine || Vine->IsSnapped())
		{
			continue;
		}
		const float Reach = Vine->BoundsRadius() + Radius;
		if (FMath::PointDistToSegmentSquared(Vine->BoundsCentre(), From, To) > Reach * Reach)
		{
			continue;
		}
		Vine->TrySnap(From, To, Radius, Push);
	}
}

// =====================================================================================
//  Effects: a glowing cylinder stretched from A to B, fading over Life seconds
// =====================================================================================
void AGRArsenal::ShowEffect(const FVector& From, const FVector& To, float Thickness, float Life, const FLinearColor& Colour)
{
	if (Effects.Num() == 0)
	{
		return;
	}
	int32 Pick = 0;   // reuse the one closest to finishing
	for (int32 i = 1; i < Effects.Num(); ++i)
	{
		if (EffectLife[i] < EffectLife[Pick])
		{
			Pick = i;
		}
	}
	UStaticMeshComponent* E = Effects[Pick];
	const FVector Line = To - From;
	const float Length = FMath::Max(1.f, Line.Size());
	// The engine cylinder is 100 cm tall (along Z) and 100 cm wide, centred on its middle.
	E->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Line / Length).Rotator());
	EffectScale[Pick] = FVector(Thickness / 100.f, Thickness / 100.f, Length / 100.f);
	E->SetWorldScale3D(EffectScale[Pick]);
	if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(E->GetMaterial(0)))
	{
		M->SetVectorParameterValue(TEXT("Colour"), Colour);   // our glow material
		M->SetVectorParameterValue(TEXT("Color"), Colour);    // the engine's fallback material
	}
	E->SetHiddenInGame(false);
	EffectLife[Pick] = Life;
	EffectLifeStart[Pick] = Life;
}

void AGRArsenal::UpdateEffects(float Dt)
{
	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		if (EffectLife[i] <= 0.f)
		{
			continue;
		}
		EffectLife[i] -= Dt;
		if (EffectLife[i] <= 0.f)
		{
			Effects[i]->SetHiddenInGame(true);
			continue;
		}
		// 1 -> 0.2: thins out as it fades (never all the way to 0: a zero-width shape breaks the
		// engine's maths for that frame, which floods the log with "non-invertible matrix")
		const float Left = FMath::Max(0.2f, EffectLife[i] / EffectLifeStart[i]);
		Effects[i]->SetWorldScale3D(FVector(EffectScale[i].X * Left, EffectScale[i].Y * Left, EffectScale[i].Z));
	}
	if (MuzzleFlash > 0.f)
	{
		MuzzleFlash -= Dt;
		if (MuzzleFlash <= 0.f)
		{
			MuzzleLight->SetIntensity(0.f);
		}
	}
}

void AGRArsenal::ShowReticle(bool bShow)
{
	if (Reticle.IsValid())
	{
		Reticle->SetVisibility(bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
}

// The "Sight" point saved in the model (falls back to 8 cm above the grip).
FVector AGRArsenal::SightPoint(const UStaticMeshComponent* Part) const
{
	const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
	const UStaticMeshSocket* Socket = Mesh ? Mesh->FindSocket(SightSocket) : nullptr;
	return Socket ? Socket->RelativeLocation : FVector(0.f, 0.f, 8.f);
}

FVector AGRArsenal::MuzzleWorld() const
{
	if (!RightMesh)
	{
		return GetActorLocation();
	}
	if (RightMesh->DoesSocketExist(MuzzleSocket))
	{
		return RightMesh->GetSocketLocation(MuzzleSocket);
	}
	return RightMesh->GetComponentLocation() + RightMesh->GetForwardVector() * 50.f;
}

// =====================================================================================
//  Animate: where the weapon sits in front of your eyes this frame (desktop)
// =====================================================================================
void AGRArsenal::Animate(float Dt)
{
	AGrappleRig* R = Rig.Get();
	UCameraComponent* Eyes = R ? R->GetEyes() : nullptr;
	if (!Eyes || !Weapons.IsValidIndex(Current) || !RightMesh)
	{
		return;
	}
	if (!bHaveBaseFOV)
	{
		BaseFOV = Eyes->FieldOfView;
		BaseVignette = Eyes->PostProcessSettings.bOverride_VignetteIntensity ? Eyes->PostProcessSettings.VignetteIntensity : 0.4f;
		bHaveBaseFOV = true;
	}
	const bool bVR = R->IsUsingVR();

	// ---- Switching (lower, swap at the bottom, raise) ----
	float Down = 0.f;   // 0 = up in your hands, 1 = fully lowered out of view
	if (Pending != -1)
	{
		SwitchTime += Dt;
		const float Half = SwitchSeconds * 0.5f;
		if (SwitchTime < Half)
		{
			Down = SwitchTime / Half;
		}
		else
		{
			if (!bSwapped)
			{
				bSwapped = true;
				if (Pending == -2)
				{
					bHolstered = true;                      // put away: hands are empty now
					RightMesh->SetVisibility(false);
					if (LeftMesh) { LeftMesh->SetVisibility(false); }
				}
				else
				{
					ShowSlot(Pending);
				}
			}
			Down = 1.f - (SwitchTime - Half) / Half;
		}
		if (SwitchTime >= SwitchSeconds || (Pending == -2 && bSwapped))
		{
			Pending = -1;
			Down = 0.f;
		}
	}

	const FGRWeapon& W = Weapons[Current];   // (read after the switch: it may have just changed)

	// ---- Aim blend: 0 = hip, 1 = sights on the eye line ----
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bWantAim ? 1.f : 0.f, Dt, 1.f / FMath::Max(0.01f, AimSeconds));
	const float A = FMath::SmoothStep(0.f, 1.f, AimAlpha);
	const bool bScoped = W.bScope && A > 0.92f && !bVR;

	// ---- Desktop view: zoom, slower mouse when zoomed, scope vignette and reticle ----
	if (!bVR)
	{
		const float FOV = W.AimFOV > 0.f ? FMath::Lerp(BaseFOV, W.AimFOV, A) : BaseFOV;
		Eyes->SetFieldOfView(FOV);
		R->MouseSensitivity = BaseSensitivity * FOV / FMath::Max(1.f, BaseFOV);   // so a zoomed view doesn't feel twitchy
		Eyes->PostProcessSettings.bOverride_VignetteIntensity = true;
		Eyes->PostProcessSettings.VignetteIntensity = bScoped ? 2.2f : BaseVignette;
		ShowReticle(bScoped);
	}
	RightMesh->SetHiddenInGame(bScoped || bHolstered);   // looking through a scope: the gun itself is out of view

	if (bVR)
	{
		return;   // in VR the controller IS the animation: the weapon simply sits in your hand
	}

	// ---- 1. Hip and aim poses, blended ----
	FVector Loc = W.HipOffset;
	FQuat Rot = W.HipTurn.Quaternion();
	if (IsGun(W.Kind))
	{
		// Aimed: move the gun so its Sight point lands exactly AimDistance in front of the eye.
		const FVector AimLoc = FVector(W.AimDistance, 0.f, 0.f) - SightPoint(RightMesh) * W.Scale;
		Loc = FMath::Lerp(Loc, AimLoc, A);
		Rot = FQuat::Slerp(Rot, FQuat::Identity, A);
	}
	FRotator Turn = Rot.Rotator();

	// ---- 2. Recoil: the gun kicks up and back, then settles ----
	RecoilPitch = FMath::FInterpTo(RecoilPitch, 0.f, Dt, RecoilRecovery);
	RecoilBack = FMath::FInterpTo(RecoilBack, 0.f, Dt, RecoilRecovery);
	Loc.X -= RecoilBack;
	Turn.Pitch += RecoilPitch * (1.f - 0.6f * A);

	// ---- 3. Sway: the weapon lags a little behind fast mouse movement ----
	float Dx = 0.f, Dy = 0.f;
	if (APlayerController* P = R->GetPlayerController())
	{
		P->GetInputMouseDelta(Dx, Dy);
	}
	const FVector2D SwayTarget = FVector2D(FMath::Clamp(-Dx * SwayAmount, -6.f, 6.f), FMath::Clamp(-Dy * SwayAmount, -6.f, 6.f));
	Sway = FMath::Vector2DInterpTo(Sway, SwayTarget, Dt, 8.f);
	const float Steady = 1.f - 0.8f * A;   // aiming holds the weapon steadier
	Turn.Yaw += Sway.X * Steady;
	Turn.Pitch += Sway.Y * Steady;
	Loc.Y += Sway.X * 0.3f * Steady;

	// ---- 4. Bob: a gentle figure-of-eight while walking; a buzz when flying fast ----
	const FVector V = R->GetBodyVelocity();
	const float Speed = FVector(V.X, V.Y, 0.f).Size();
	if (R->IsGrounded())
	{
		BobPhase += Dt * Speed / 70.f * PI;   // one step every 70 cm
		const float Amount = BobAmount * FMath::Clamp(Speed / 600.f, 0.f, 1.f) * (1.f - 0.85f * A);
		Loc.Z += FMath::Sin(2.f * BobPhase) * 0.6f * Amount;
		Loc.Y += FMath::Cos(BobPhase) * 0.8f * Amount;
	}
	else
	{
		const float Wind = FMath::Clamp(V.Size() / 3000.f, 0.f, 1.f) * (1.f - 0.7f * A);
		const float T = GetWorld()->GetTimeSeconds();
		Loc.Z += FMath::Sin(T * 23.f) * 0.15f * Wind;
		Turn.Roll += FMath::Sin(T * 17.f) * 0.6f * Wind;
	}

	// ---- 5. Reload: tilt the gun over, dip it, bring it back ----
	if (ReloadLeft > 0.f && IsGun(W.Kind))
	{
		const float Pr = 1.f - ReloadLeft / FMath::Max(0.01f, W.ReloadSeconds);   // 0 -> 1 through the reload
		const float S = FMath::Sin(PI * Pr);
		Loc.Z -= 9.f * S;
		Loc.X -= 4.f * S;
		Turn.Roll += 40.f * S;
		Turn.Pitch += 10.f * S * FMath::Sin(2.f * PI * Pr);
	}

	// ---- 6. Switching: lowered out of view ----
	Loc.Z -= 24.f * Down;
	Turn.Pitch -= 50.f * Down;

	// ---- 7. Melee curves ----
	FVector SwingLoc = FVector::ZeroVector;
	FRotator SwingTurn = FRotator::ZeroRotator;
	if (MeleeTime >= 0.f && IsBlade(W.Kind))
	{
		MeleeTime += Dt;
		const float Dur = bHeavy ? StabSeconds : SlashSeconds;
		const float Pr = FMath::Clamp(MeleeTime / Dur, 0.f, 1.f);
		if (bHeavy)
		{
			// Stab: draw back, drive forward, recover.
			SwingLoc.X = Curve(Pr, { {0.f, 0.f}, {0.35f, -12.f}, {0.55f, 38.f}, {1.f, 0.f} });
			SwingTurn.Pitch = Curve(Pr, { {0.f, 0.f}, {0.35f, 10.f}, {0.55f, -8.f}, {1.f, 0.f} });
		}
		else
		{
			// Slash: wind up to the right, sweep across to the left, recover.
			SwingTurn.Yaw = Curve(Pr, { {0.f, 0.f}, {0.25f, 50.f}, {0.6f, -70.f}, {1.f, 0.f} });
			SwingTurn.Roll = Curve(Pr, { {0.f, 0.f}, {0.25f, -30.f}, {0.6f, 25.f}, {1.f, 0.f} });
			SwingLoc.Y = Curve(Pr, { {0.f, 0.f}, {0.25f, 10.f}, {0.6f, -20.f}, {1.f, 0.f} });
			SwingLoc.X = Curve(Pr, { {0.f, 0.f}, {0.25f, -4.f}, {0.6f, 12.f}, {1.f, 0.f} });
		}
		if (!bMeleeHit && Pr >= 0.45f)
		{
			MeleeHit();
		}
		if (Pr >= 1.f)
		{
			MeleeTime = -1.f;
		}
	}

	// ---- Put it all on the meshes ----
	const bool bSwingRight = !(W.Kind == EGRWeaponKind::Swords && bLeftBlade && !bHeavy);
	RightMesh->SetRelativeLocationAndRotation(Loc + (bSwingRight ? SwingLoc : FVector::ZeroVector),
		Turn + (bSwingRight ? SwingTurn : FRotator::ZeroRotator));
	if (LeftMesh && LeftMesh->IsVisible())
	{
		// The left blade is the mirror image: same pose, other side (Y, yaw and roll flipped).
		const bool bSwingLeft = W.Kind == EGRWeaponKind::Swords && (bLeftBlade || bHeavy);
		FVector LLoc = Loc + (bSwingLeft ? SwingLoc : FVector::ZeroVector);
		FRotator LTurn = Turn + (bSwingLeft ? SwingTurn : FRotator::ZeroRotator);
		LLoc.Y = -LLoc.Y;
		LTurn.Yaw = -LTurn.Yaw;
		LTurn.Roll = -LTurn.Roll;
		LeftMesh->SetRelativeLocationAndRotation(LLoc, LTurn);
	}
}

// =====================================================================================
//  Tick: every frame
// =====================================================================================
void AGRArsenal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!FindRig() || !Rig->GetEyes())
	{
		return;   // the rig hasn't found the player yet
	}
	if (Current < 0 && Weapons.Num() > 0)
	{
		StartSwitch(FMath::Clamp(StartWeapon, 0, Weapons.Num() - 1));   // first frame: draw the start weapon
	}

	ReadInput(DeltaSeconds);

	// Timers
	Cooldown = FMath::Max(0.f, Cooldown - DeltaSeconds);
	if (ReloadLeft > 0.f)
	{
		ReloadLeft -= DeltaSeconds;
		if (ReloadLeft <= 0.f && Weapons.IsValidIndex(Current))
		{
			ReloadLeft = 0.f;
			Ammo[Current] = Weapons[Current].Magazine;   // full magazine
		}
	}
	if (BurstLeft > 0 && Cooldown <= 0.f && ReloadLeft <= 0.f && Pending == -1)
	{
		Fire();
		if (--BurstLeft == 0)
		{
			Cooldown += 0.3f;   // short pause after each burst
		}
	}

	Animate(DeltaSeconds);
	UpdateEffects(DeltaSeconds);

	// The mouse buttons fire the hooks only when your hands are empty.
	const FGRWeapon* W = Held();
	Rig->SetMouseFiresHooks(!W || W->Kind == EGRWeaponKind::Hands);

	// Desktop status line (VR: an ammo counter on the weapon itself comes later; no text on your face).
	if (GEngine && !Rig->IsUsingVR())
	{
		FString Status;
		if (!W)
		{
			Status = TEXT("Hands free: mouse buttons fire the hooks   (X: draw weapon)");
		}
		else if (IsGun(W->Kind))
		{
			Status = FString::Printf(TEXT("%s   %d / %d%s   |  LMB fire  RMB aim  T reload  X holster  Q/E hooks"),
				*W->Name, Ammo[Current], W->Magazine, ReloadLeft > 0.f ? TEXT("   RELOADING") : TEXT(""));
		}
		else
		{
			Status = FString::Printf(TEXT("%s   |  LMB slash  RMB heavy stab  X holster  Q/E hooks"), *W->Name);
		}
		GEngine->AddOnScreenDebugMessage(4202, 0.f, FColor(255, 200, 120), Status);
	}
}
