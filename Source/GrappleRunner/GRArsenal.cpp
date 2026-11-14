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
#include "GRCutter.h"
#include "GRMenu.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
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
//  Tuning commands (type in the game console, the ` key, while playing):
//    gr.Arms  X Y Z  Pitch Yaw Roll   - where the animated arms sit, from the eyes
//    gr.Gun   X Y Z  Pitch Yaw Roll   - the rigged gun's offset in the right hand
//  Lets us line the hands and gun up live, then copy the numbers into the defaults.
// =====================================================================================
static void GRTuneTransform(const TArray<FString>& Args, UWorld* World, bool bArms)
{
	if (!World || Args.Num() < 6)
	{
		return;
	}
	const FVector L(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
	const FRotator T(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), FCString::Atof(*Args[5]));
	for (TActorIterator<AGRArsenal> It(World); It; ++It)
	{
		It->ApplyTuning(bArms, L, T);
	}
}
static FAutoConsoleCommandWithWorldAndArgs GRArmsCmd(TEXT("gr.Arms"), TEXT("gr.Arms X Y Z Pitch Yaw Roll"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { GRTuneTransform(A, W, true); }));
static FAutoConsoleCommandWithWorldAndArgs GRGunCmd(TEXT("gr.Gun"), TEXT("gr.Gun X Y Z Pitch Yaw Roll"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { GRTuneTransform(A, W, false); }));

void AGRArsenal::ApplyTuning(bool bArms, const FVector& L, const FRotator& T)
{
	if (bArms)
	{
		ArmsOffset = L;
		ArmsTurn = T;
	}
	else if (ArmsGun)
	{
		ArmsGun->SetRelativeLocationAndRotation(L, T);
	}
	UE_LOG(LogTemp, Log, TEXT("Grapple Runner tuning: %s %s %s"), bArms ? TEXT("arms") : TEXT("gun"), *L.ToString(), *T.ToString());
}

// =====================================================================================
//  Constructor: build the parts that always exist (root, muzzle light, effect pool)
// =====================================================================================
// Testing helpers (file-scope so they can be changed with Live Coding): F9 slow motion, and a
// count of cuts shown in the desktop status line.
static bool GSlowMo = false;
static int32 GCutCount = 0;

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

	// The First Person Rifle Animation pack's animations (arms) and the gun's own (magazine, bolt).
	auto Anim = [](const TCHAR* Path)
	{
		return TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(Path));
	};
	auto Montage = [](const TCHAR* Path)
	{
		return TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(Path));
	};
	const FString A = TEXT("/Game/FirstPersonRifleAnimations/Animations/");
	ArmsMontages.Add(TEXT("Draw"), Montage(*(A + TEXT("AM_Rifle_Draw_Montage.AM_Rifle_Draw_Montage"))));
	ArmsMontages.Add(TEXT("Holster"), Montage(*(A + TEXT("AM_Rifle_Holster_Montage.AM_Rifle_Holster_Montage"))));
	ArmsMontages.Add(TEXT("Fire"), Montage(*(A + TEXT("AM_Rifle_Fire_Montage.AM_Rifle_Fire_Montage"))));
	ArmsMontages.Add(TEXT("ADSFire"), Montage(*(A + TEXT("AM_Rifle_ADS_Fire_Montage.AM_Rifle_ADS_Fire_Montage"))));
	ArmsMontages.Add(TEXT("Reload"), Montage(*(A + TEXT("AM_Rifle_Reload_Montage.AM_Rifle_Reload_Montage"))));
	ArmsMontages.Add(TEXT("ReloadEmpty"), Montage(*(A + TEXT("AM_Rifle_Reload_Empty_Montage.AM_Rifle_Reload_Empty_Montage"))));
	GunMontages.Add(TEXT("Fire"), Montage(*(A + TEXT("Weapons/AM_Weapon_Rifle_Fire_Montage.AM_Weapon_Rifle_Fire_Montage"))));
	GunMontages.Add(TEXT("Reload"), Montage(*(A + TEXT("Weapons/AM_Weapon_Rifle_Reload_Montage.AM_Weapon_Rifle_Reload_Montage"))));
	GunMontages.Add(TEXT("ReloadEmpty"), Montage(*(A + TEXT("Weapons/AM_Weapon_Rifle_Reload_Empty_Montage.AM_Weapon_Rifle_Reload_Empty_Montage"))));
	ArmsAnims.Add(TEXT("Idle"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/AS_Rifle_Idle_Pose.AS_Rifle_Idle_Pose")));
	ArmsAnims.Add(TEXT("Draw"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/AS_Rifle_Draw.AS_Rifle_Draw")));
	ArmsAnims.Add(TEXT("Holster"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/AS_Rifle_Holster.AS_Rifle_Holster")));
	ArmsAnims.Add(TEXT("Fire"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/AS_Rifle_Fire.AS_Rifle_Fire")));
	ArmsAnims.Add(TEXT("ADSPose"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/AS_Rifle_ADS_Pose.AS_Rifle_ADS_Pose")));
	ArmsAnims.Add(TEXT("ADSFire"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/AS_Rifle_ADS_Fire.AS_Rifle_ADS_Fire")));
	ArmsAnims.Add(TEXT("Reload"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/AS_Rifle_Reload.AS_Rifle_Reload")));
	ArmsAnims.Add(TEXT("ReloadEmpty"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/AS_Rifle_Reload_Empty.AS_Rifle_Reload_Empty")));
	GunAnims.Add(TEXT("Idle"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/Weapons/AS_Weapon_Rifle_Idle.AS_Weapon_Rifle_Idle")));
	GunAnims.Add(TEXT("Fire"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/Weapons/AS_Weapon_Rifle_Fire.AS_Weapon_Rifle_Fire")));
	GunAnims.Add(TEXT("Reload"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/Weapons/AS_Weapon_Rifle_Reload.AS_Weapon_Rifle_Reload")));
	GunAnims.Add(TEXT("ReloadEmpty"), Anim(TEXT("/Game/FirstPersonRifleAnimations/Animations/Weapons/AS_Weapon_Rifle_Reload_Empty.AS_Weapon_Rifle_Reload_Empty")));
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
	// THREE WEAPONS (Naomi, 11 Nov): knife, rifle, sniper. Their new models are imported to
	// /Game/GrappleRunner/Weapons/Arsenal/ (SM_Knife, SM_Rifle, SM_Sniper). Until then each slot
	// shows its FallbackMesh, so the game still works.
	auto Ours = [](const TCHAR* Name)
	{
		return TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/GrappleRunner/Weapons/Arsenal/%s.%s"), Name, Name)));
	};
	const FLinearColor Tracer(0.55f, 0.85f, 1.f);   // cool white-blue: clean, sci-fi

	Weapons.Reset();
	// Key 1: the twin ODM blades (our own model, Art/Weapons/make_odm_swords.py). One in each hand.
	FGRWeapon Blades = Blade(TEXT("Blades"), TEXT("SM_Araxys_Knife"), 55.f);
	Blades.Kind = EGRWeaponKind::Swords;
	const TSoftObjectPtr<UStaticMesh> Sword(FSoftObjectPath(TEXT("/Game/GrappleRunner/Weapons/SM_ODMSword.SM_ODMSword")));
	Blades.FallbackMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Weapons/SM_Blade.SM_Blade")));
	Blades.Mesh = Sword;
	Blades.LeftMesh = Sword;
	Blades.Scale = 1.f;                                   // modelled at real size: 95 cm of blade
	Blades.HipOffset = FVector(40.f, 21.f, -17.f);         // ready stance: fists in view, chest height...
	Blades.HipTurn = FRotator(38.f, -14.f, 0.f);           // ...blades angled up and slightly in, edge forward
	Blades.Reach = 190.f;
	Weapons.Add(Blades);                                                                        // key 1

	FGRWeapon Rifle = Gun(TEXT("Rifle"), TEXT("SM_Kuronami_Rifle"), EGRWeaponKind::Rifle, 40.f, 9.75f, 25, 2.4f, 2.f, 0.25f, 0.7f, 70.f, Tracer);
	Rifle.FallbackMesh = Rifle.Mesh;
	Rifle.Mesh = Ours(TEXT("SM_Rifle"));
	Rifle.bAnimatedArms = true;   // real hands and a rigged rifle with real draw / fire / reload animations
	Rifle.ArmsWeapon = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/FirstPersonRifleAnimations/Meshes/Weapon/SKM_Weapon_Full.SKM_Weapon_Full")));
	Weapons.Add(Rifle);                                                                         // key 2

	FGRWeapon Sniper = Gun(TEXT("Sniper"), TEXT("SM_Araxys_Sniper"), EGRWeaponKind::Sniper, 150.f, 0.6f, 5, 3.2f, 5.f, 0.f, 3.5f, 25.f, Tracer);
	Sniper.FallbackMesh = Sniper.Mesh;
	Sniper.Mesh = Ours(TEXT("SM_Sniper"));
	Sniper.bBoltAction = true;
	Sniper.SupportHand = FVector(38.f, 0.f, -3.f);
	Weapons.Add(Sniper);                                                                        // key 3
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
	if (W.Kind != EGRWeaponKind::Hands && W.Mesh.IsNull() && W.FallbackMesh.IsNull())
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
		if (!Mesh && (Side == 1 || W.Kind == EGRWeaponKind::Swords) && !W.FallbackMesh.IsNull())
		{
			Mesh = W.FallbackMesh.LoadSynchronous();   // new model not imported yet
		}
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

	// Animated arms: real hands holding a rigged gun (desktop). Made once, reused.
	USkeletalMesh* ArmsMesh = (W.bAnimatedArms && !R->IsUsingVR()) ? ArmsModel.LoadSynchronous() : nullptr;
	USkeletalMesh* GunMesh = ArmsMesh && !W.ArmsWeapon.IsNull() ? W.ArmsWeapon.LoadSynchronous() : nullptr;
	if (ArmsMesh && GunMesh && R->GetEyes())
	{
		auto MakeSkel = [this](TObjectPtr<USkeletalMeshComponent>& Comp)
		{
			if (!Comp)
			{
				Comp = NewObject<USkeletalMeshComponent>(this);
				Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Comp->SetCastShadow(false);
				Comp->RegisterComponent();
				AddInstanceComponent(Comp);
			}
		};
		MakeSkel(Arms);
		MakeSkel(Gloves);
		MakeSkel(ArmsGun);
		Arms->SetSkeletalMesh(ArmsMesh);
		Arms->AttachToComponent(R->GetEyes(), FAttachmentTransformRules::KeepRelativeTransform);
		Arms->SetRelativeLocationAndRotation(ArmsOffset, ArmsTurn);
		Gloves->SetSkeletalMesh(GlovesModel.LoadSynchronous());
		Gloves->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Gloves->SetLeaderPoseComponent(Arms);   // the gloves copy every move the hands make
		ArmsGun->SetSkeletalMesh(GunMesh);
		ArmsGun->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GunBone);
		// The pack's animation blueprints: they keep the arms in a proper rifle hold (idle,
		// breathing, walk) and the gun's magazine/bolt bones in place. Montages play on top.
		if (UClass* ArmsBP = ArmsAnimBP.LoadSynchronous())
		{
			Arms->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Arms->SetAnimInstanceClass(ArmsBP);
		}
		if (UClass* GunBP = GunAnimBP.LoadSynchronous())
		{
			ArmsGun->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			ArmsGun->SetAnimInstanceClass(GunBP);
		}
		for (USkeletalMeshComponent* C : { Arms.Get(), Gloves.Get(), ArmsGun.Get() })
		{
			C->SetVisibility(true);
			C->SetHiddenInGame(false);
		}
		RightMesh->SetVisibility(false);   // the static stand-in isn't needed
		PlayArms(TEXT("Draw"), false);
	}
	else
	{
		for (USkeletalMeshComponent* C : { Arms.Get(), Gloves.Get(), ArmsGun.Get() })
		{
			if (C)
			{
				C->SetVisibility(false);
			}
		}
	}
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
	if (!P || R->IsUsingVR() || AGRMenu::IsOpen(this))   // the menu has the mouse while it's open
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
	// F9: slow motion on/off (10% speed), to watch a swing or a cut closely while testing.
	if (P->WasInputKeyJustPressed(EKeys::F9))
	{
		GSlowMo = !GSlowMo;
		UGameplayStatics::SetGlobalTimeDilation(this, GSlowMo ? 0.1f : 1.f);
	}
	// Mouse wheel: the next (or previous) slot that has something in it.
	const int32 Step = P->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? -1 : (P->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1 : 0);
	if (Step != 0 && Weapons.Num() > 0)
	{
		const int32 From = Pending >= 0 ? Pending : FMath::Max(0, Current);
		for (int32 Tries = 1; Tries <= Weapons.Num(); ++Tries)
		{
			const int32 Next = (From + Step * Tries + Weapons.Num() * 2) % Weapons.Num();
			if (!Weapons[Next].Mesh.IsNull() || !Weapons[Next].FallbackMesh.IsNull())
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
		// Mouse, or V = slash and C = cross-cut (keyboard alternatives).
		const bool bSlashKey = P->WasInputKeyJustPressed(EKeys::V);
		const bool bCrossKey = P->WasInputKeyJustPressed(EKeys::C);
		if (MeleeTime < 0.f && (bFirePressed || bAltPressed || bSlashKey || bCrossKey))
		{
			StartMelee(bAltPressed || bCrossKey);
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
	if (UsingArms())
	{
		PlayArms(AimAlpha > 0.5f ? FName(TEXT("ADSFire")) : FName(TEXT("Fire")), false);
	}
	const FVector Muzzle = MuzzleWorld();
	ShowEffect(Muzzle, HitPoint, W.Kind == EGRWeaponKind::Sniper ? 2.5f : 1.2f, W.Kind == EGRWeaponKind::Sniper ? 0.15f : 0.06f, W.TracerColour);
	MuzzleLight->SetWorldLocation(Muzzle);
	MuzzleLight->SetLightColor(W.TracerColour);
	MuzzleLight->SetIntensity(300.f);
	MuzzleFlash = 0.05f;
	RecoilPitch += W.RecoilKick * 3.f;
	RecoilBack += FMath::Clamp(W.RecoilKick * 2.5f, 1.f, 8.f);
	R->AddLookKick(W.RecoilKick * (1.f - 0.4f * AimAlpha), FMath::FRandRange(-0.3f, 0.3f) * W.RecoilKick);

	if (W.bBoltAction)
	{
		BoltTime = 0.f;        // desktop: the left hand works the bolt (see UpdateHands)
		bChambered = false;    // VR: you must pull the bolt yourself
	}
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
	if (UsingArms())
	{
		const FName Which = Ammo[Current] <= 0 ? FName(TEXT("ReloadEmpty")) : FName(TEXT("Reload"));
		PlayArms(Which, false);
		ReloadLeft = FMath::Max(0.3f, ArmsOneShotLeft);   // the reload lasts exactly as long as the animation
	}
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
	SwingIds[0] = SwingIds[1] = NextSwingId++;                        // a new swing: everything can be hit once again
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
	if (UsingArms() && ArmsGun)
	{
		for (const TCHAR* Name : { TEXT("Muzzle"), TEXT("muzzle"), TEXT("SOCKET_Muzzle"), TEXT("b_gun_muzzleflash") })
		{
			if (ArmsGun->DoesSocketExist(Name))
			{
				return ArmsGun->GetSocketLocation(Name);
			}
		}
		return ArmsGun->GetComponentLocation() + ArmsGun->GetForwardVector() * 60.f;
	}
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
					for (USkeletalMeshComponent* C : { Arms.Get(), Gloves.Get(), ArmsGun.Get() })
					{
						if (C) { C->SetVisibility(false); }
					}
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
		// In VR the controller IS the animation: the weapon simply sits in your hand. The blades
		// are turned so the grip lies in your fist and the blade comes out above your thumb.
		if (W.Kind == EGRWeaponKind::Swords)
		{
			for (UStaticMeshComponent* Part : { RightMesh.Get(), LeftMesh.Get() })
			{
				if (Part && Part->IsVisible())
				{
					Part->SetRelativeLocationAndRotation(VRGripOffset, VRGripTurn.Quaternion() * SwordFrame(Part).Inverse());
				}
			}
		}
		return;
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

	// ---- 6. Switching: a quick, eased dip down and to the side (not a big flip) ----
	const float E = FMath::SmoothStep(0.f, 1.f, Down);
	Loc.Z -= 16.f * E;
	Loc.Y += 6.f * E;
	Turn.Pitch -= 20.f * E;
	Turn.Roll += 14.f * E;

	// ---- The twin blades have their own two-handed moves (AnimateSwords, below) ----
	if (W.Kind == EGRWeaponKind::Swords)
	{
		AnimateSwords(Dt, Loc - W.HipOffset, Turn - W.HipTurn);   // pass on only the extras: sway, bob, switch dip
		return;
	}

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
	if (UsingArms())
	{
		UpdateArms(Dt, FVector::ZeroVector, FRotator::ZeroRotator);
		return;   // the animated arms carry the gun; the procedural pose below is for the other weapons
	}
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
//  Hands: the right hand on the grip, the left on the foregrip; the left hand reloads
// =====================================================================================
//  Desktop reload choreography (each step is a share of ReloadSeconds):
//    0.00-0.20  left hand slides from the foregrip to the magazine
//    0.20-0.45  pulls it out and drops down to the belt pouch (old magazine gone)
//    0.45-0.70  comes back up with a fresh magazine
//    0.70-0.85  pushes it into the magwell (the gun bumps)
//    0.85-1.00  back to the foregrip
//  Sniper bolt (BoltSeconds after each shot): left hand to the bolt, pull back, push forward, back.
void AGRArsenal::UpdateHands(float Dt)
{
	AGrappleRig* R = Rig.Get();
	UCameraComponent* Eyes = R ? R->GetEyes() : nullptr;
	if (!Eyes)
	{
		return;
	}
	// Make the two hands once (the VR template's hand models, holding a "grasp" pose).
	auto MakeHand = [this](TObjectPtr<USkeletalMeshComponent>& Hand, const TSoftObjectPtr<USkeletalMesh>& Model)
	{
		if (Hand)
		{
			return;
		}
		USkeletalMesh* Mesh = Model.LoadSynchronous();
		if (!Mesh)
		{
			return;
		}
		Hand = NewObject<USkeletalMeshComponent>(this);
		Hand->SetSkeletalMesh(Mesh);
		Hand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hand->SetCastShadow(false);
		Hand->RegisterComponent();
		Hand->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		Hand->SetUsingAbsoluteLocation(true);
		Hand->SetUsingAbsoluteRotation(true);
		if (UAnimationAsset* Pose = GraspPose.LoadSynchronous())
		{
			Hand->PlayAnimation(Pose, true);   // fingers curled round the grip (if this hand's skeleton accepts it)
		}
	};
	// ---- Twin blades: a hand closed round each grip (desktop AND headset) ----
	const FGRWeapon* Wb = Held();
	const bool bBlades = Wb && Wb->Kind == EGRWeaponKind::Swords && RightMesh && RightMesh->IsVisible();
	if (R->IsUsingVR())
	{
		// The VR template draws its own hand models on the controllers. While the blades are out
		// they step aside for ours (which hold the grips properly), so there is only ever ONE pair.
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (USceneComponent* Ctrl = R->GetHand(Side))
			{
				TArray<USceneComponent*> Kids;
				Ctrl->GetChildrenComponents(true, Kids);
				for (USceneComponent* K : Kids)
				{
					USkeletalMeshComponent* Sk = Cast<USkeletalMeshComponent>(K);
					if (Sk && Sk != HandR && Sk != HandLM)
					{
						Sk->SetHiddenInGame(bBlades);
					}
				}
			}
		}
	}
	if (bBlades)
	{
		MakeHand(HandR, RightHandModel);
		// The left hand is the RIGHT hand model mirrored (scale -1 sideways). The template's
		// grasp animation only curls right-hand fingers, so a mirrored right hand is the only
		// way to get a properly closed left fist.
		if (!HandLM)
		{
			MakeHand(HandLM, RightHandModel);
			if (HandLM)
			{
				HandLM->SetUsingAbsoluteScale(true);
				HandLM->SetWorldScale3D(FVector(1.f, -1.f, 1.f));
			}
		}
		if (HandL) { HandL->SetHiddenInGame(true); }
		if (HeldMag) { HeldMag->SetHiddenInGame(true); }
		const bool bRight = !RightMesh->bHiddenInGame;
		const bool bLeft = LeftMesh && LeftMesh->IsVisible() && !LeftMesh->bHiddenInGame;
		if (HandR) { HandR->SetHiddenInGame(!bRight); if (bRight) { FitHandToBlade(HandR, RightMesh); } }
		if (HandLM) { HandLM->SetHiddenInGame(!bLeft); if (bLeft) { FitHandToBlade(HandLM, LeftMesh); } }
		return;
	}
	if (HandLM)
	{
		HandLM->SetHiddenInGame(true);
	}
	if (R->IsUsingVR())
	{
		if (HandR) { HandR->SetHiddenInGame(true); }
		return;   // in VR your real hands (the controllers / template hands) are the hands
	}
	MakeHand(HandR, RightHandModel);
	MakeHand(HandL, LeftHandModel);
	if (!HandR || !HandL)
	{
		return;
	}
	const FGRWeapon* W = Held();
	if (UsingArms())
	{
		HandR->SetHiddenInGame(true);   // the animated arms are the hands now
		HandL->SetHiddenInGame(true);
		return;
	}
	const bool bScopedIn = W && W->bScope && AimAlpha > 0.92f;   // looking through the scope: no gun, no hands
	const bool bShow = W && RightMesh && RightMesh->IsVisible() && !bHolstered && !bScopedIn;
	HandR->SetHiddenInGame(!bShow);
	HandL->SetHiddenInGame(!bShow);
	if (HeldMag)
	{
		HeldMag->SetHiddenInGame(true);
	}
	if (!bShow)
	{
		return;
	}
	const FTransform Gun = RightMesh->GetComponentTransform();          // the weapon in the world
	const FTransform View = Eyes->GetComponentTransform();              // the eyes in the world
	const FQuat GunRot = Gun.GetRotation();

	// Right hand: always on the grip (the weapon's pivot).
	HandR->SetWorldLocationAndRotation(Gun.TransformPosition(RightHandOffset / FMath::Max(0.01f, W->Scale)),
		GunRot * RightHandTurn.Quaternion());

	// Left hand: where should it be this frame?
	const FVector Fore = Gun.TransformPosition(W->SupportHand);
	FVector Left = Fore;
	bool bMagInHand = false;
	if (IsBlade(W->Kind))
	{
		Left = View.TransformPosition(FVector(26.f, -16.f, -24.f));   // knife: left hand up and ready, off to the side
	}
	else if (ReloadLeft > 0.f && W->ReloadSeconds > 0.f)
	{
		const float P = 1.f - ReloadLeft / W->ReloadSeconds;
		const FVector Mag = Gun.TransformPosition(W->MagazinePoint);
		const FVector Belt = View.TransformPosition(BeltPoint);
		auto Ease = [](float A, float B, float T) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp((T - A) / (B - A), 0.f, 1.f)); };
		if (P < 0.20f)      { Left = FMath::Lerp(Fore, Mag, Ease(0.f, 0.20f, P)); }
		else if (P < 0.45f) { Left = FMath::Lerp(Mag, Belt, Ease(0.20f, 0.45f, P)); bMagInHand = P < 0.30f; }
		else if (P < 0.70f) { Left = FMath::Lerp(Belt, Mag - Gun.GetUnitAxis(EAxis::Z) * 12.f, Ease(0.45f, 0.70f, P)); bMagInHand = true; }
		else if (P < 0.85f) { Left = FMath::Lerp(Mag - Gun.GetUnitAxis(EAxis::Z) * 12.f, Mag, Ease(0.70f, 0.85f, P)); bMagInHand = true; }
		else                { Left = FMath::Lerp(Mag, Fore, Ease(0.85f, 1.f, P)); }
	}
	else if (BoltTime >= 0.f && W->bBoltAction)
	{
		BoltTime += Dt;
		const float P = BoltTime / FMath::Max(0.05f, BoltSeconds);
		const FVector Bolt = Gun.TransformPosition(W->BoltPoint);
		const FVector Back = Bolt - Gun.GetUnitAxis(EAxis::X) * 7.f;      // pulled back 7 cm
		const float A = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(P / 0.3f, 0.f, 1.f));
		if (P < 0.3f)       { Left = FMath::Lerp(Fore, Bolt, A); }
		else if (P < 0.55f) { Left = FMath::Lerp(Bolt, Back, FMath::SmoothStep(0.f, 1.f, (P - 0.3f) / 0.25f)); }
		else if (P < 0.75f) { Left = FMath::Lerp(Back, Bolt, FMath::SmoothStep(0.f, 1.f, (P - 0.55f) / 0.2f)); }
		else                { Left = FMath::Lerp(Bolt, Fore, FMath::SmoothStep(0.f, 1.f, (P - 0.75f) / 0.25f)); }
		if (P >= 1.f)
		{
			BoltTime = -1.f;
			bChambered = true;
		}
	}
	HandL->SetWorldLocationAndRotation(Left + GunRot.RotateVector(LeftHandOffset), GunRot * LeftHandTurn.Quaternion());

	// The fresh magazine travelling in the left hand (only if the weapon names a magazine model).
	if (bMagInHand && !W->MagazineMesh.IsNull())
	{
		if (!HeldMag)
		{
			HeldMag = NewObject<UStaticMeshComponent>(this);
			HeldMag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			HeldMag->SetCastShadow(false);
			HeldMag->RegisterComponent();
			HeldMag->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
			HeldMag->SetUsingAbsoluteLocation(true);
			HeldMag->SetUsingAbsoluteRotation(true);
			HeldMag->SetUsingAbsoluteScale(true);
		}
		HeldMag->SetStaticMesh(W->MagazineMesh.LoadSynchronous());
		HeldMag->SetWorldLocationAndRotation(Left, GunRot);
		HeldMag->SetWorldScale3D(FVector(W->Scale));
		HeldMag->SetHiddenInGame(false);
	}
}

// =====================================================================================
//  VR manual reload (runs only in the headset). Your left hand does it for real:
//    1. squeeze grip near the magazine and pull away   -> magazine out (gun is empty)
//    2. let go                                          -> it drops
//    3. squeeze grip at your left hip                   -> a fresh magazine in your hand
//    4. bring it to the magwell (within 7 cm)           -> click: loaded
//    5. sniper: squeeze grip on the bolt, pull back 5 cm -> next round ready
//  Firing in VR (trigger) arrives with the headset controls; it will check bMagIn and bChambered.
//  Note: in VR the grip also stops a hook reeling. With a gun out, a grip NEAR the gun or the
//  hip counts as a grab; elsewhere it still works the hooks.
// =====================================================================================
void AGRArsenal::VRReload()
{
	AGrappleRig* R = Rig.Get();
	const FGRWeapon* W = Held();
	if (!R || !R->IsUsingVR() || !W || !IsGun(W->Kind) || !RightMesh || !R->GetHand(0))
	{
		return;
	}
	const bool bGrip = R->IsGripping(0);
	const bool bPressed = bGrip && !bGripWas[0];
	const bool bReleased = !bGrip && bGripWas[0];
	bGripWas[0] = bGrip;

	const FTransform Gun = RightMesh->GetComponentTransform();
	const FVector Hand = R->GetHand(0)->GetComponentLocation();
	const FVector Mag = Gun.TransformPosition(W->MagazinePoint);
	const FVector Bolt = Gun.TransformPosition(W->BoltPoint);
	const FVector Hip = R->GetBodyPosition() + FVector(0.f, 0.f, -10.f) - R->GetEyes()->GetRightVector() * 18.f;
	auto Click = [R](int32 Side, float Strength)
	{
		if (APlayerController* P = R->GetPlayerController())
		{
			P->SetHapticsByValue(1.f, Strength, Side == 0 ? EControllerHand::Left : EControllerHand::Right);
		}
	};

	// 1. grab the magazine out
	if (bPressed && bMagIn && FVector::Dist(Hand, Mag) < 10.f)
	{
		bMagIn = false;
		bMagInLeftHand = true;
		Ammo[Current] = 0;
		Click(0, 0.5f);
	}
	// 2. drop it
	else if (bReleased && bMagInLeftHand)
	{
		bMagInLeftHand = false;
	}
	// 3. fresh magazine from the hip
	else if (bPressed && !bMagInLeftHand && !bMagIn && FVector::Dist(Hand, Hip) < 20.f)
	{
		bMagInLeftHand = true;
		Click(0, 0.3f);
	}
	// 4. insert
	if (bMagInLeftHand && !bMagIn && bGrip && Ammo[Current] == 0 && FVector::Dist(Hand, Mag) < 7.f && !bPressed)
	{
		bMagIn = true;
		bMagInLeftHand = false;
		Ammo[Current] = W->Magazine;
		Click(0, 0.8f);
		Click(1, 0.8f);
	}
	// 5. bolt
	if (W->bBoltAction && !bChambered)
	{
		if (bPressed && FVector::Dist(Hand, Bolt) < 8.f)
		{
			bBoltHeld = true;
			BoltGrabStart = Hand;
		}
		if (bBoltHeld && FVector::DotProduct(BoltGrabStart - Hand, Gun.GetUnitAxis(EAxis::X)) > 5.f)
		{
			bChambered = true;
			bBoltHeld = false;
			Click(0, 0.6f);
		}
	}
	if (!bGrip)
	{
		bBoltHeld = false;
	}
}

// =====================================================================================
//  Animated arms (First Person Rifle Animation pack)
//  The arms play one animation at a time ("single node" mode): a looping Idle (or the aim
//  pose while the right mouse button is held) and one-off clips for Draw, Fire and Reload.
//  The gun plays its matching clip at the same moment, so the magazine drops and the bolt
//  moves exactly with the hands. When a one-off clip ends, the arms go back to Idle.
// =====================================================================================
bool AGRArsenal::UsingArms() const
{
	const FGRWeapon* W = Held();
	return W && W->bAnimatedArms && Arms && Arms->IsVisible() && ArmsGun && Rig.IsValid() && !Rig->IsUsingVR();
}

void AGRArsenal::PlayArms(FName Anim, bool bLoop, bool bAlsoGun)
{
	if (!Arms)
	{
		return;
	}
	// With the pack's blueprint running: idle is the blueprint's job; one-offs are montages.
	if (UAnimInstance* AI = Arms->GetAnimInstance(); AI && Arms->GetAnimationMode() == EAnimationMode::AnimationBlueprint)
	{
		ArmsState = Anim;
		ArmsOneShotLeft = 0.f;
		if (const TSoftObjectPtr<UAnimMontage>* M = ArmsMontages.Find(Anim))
		{
			if (UAnimMontage* Montage = M->LoadSynchronous())
			{
				AI->Montage_Play(Montage);
				ArmsOneShotLeft = Montage->GetPlayLength();
			}
		}
		if (bAlsoGun && ArmsGun && ArmsGun->GetAnimInstance())
		{
			const FName GunClip = (Anim == TEXT("ADSFire")) ? FName(TEXT("Fire")) : Anim;
			if (const TSoftObjectPtr<UAnimMontage>* GM = GunMontages.Find(GunClip))
			{
				if (UAnimMontage* GunMontage = GM->LoadSynchronous())
				{
					ArmsGun->GetAnimInstance()->Montage_Play(GunMontage);
				}
			}
		}
		return;
	}
	const TSoftObjectPtr<UAnimSequenceBase>* Soft = ArmsAnims.Find(Anim);
	UAnimSequenceBase* Clip = Soft ? Soft->LoadSynchronous() : nullptr;
	if (!Clip)
	{
		return;
	}
	Arms->PlayAnimation(Clip, bLoop);
	ArmsState = Anim;
	ArmsOneShotLeft = bLoop ? 0.f : Clip->GetPlayLength();
	if (bAlsoGun && ArmsGun)
	{
		// The gun's own clip: fire / reload / reload-empty, otherwise its idle.
		FName GunClip = TEXT("Idle");
		if (Anim == TEXT("Fire") || Anim == TEXT("ADSFire")) { GunClip = TEXT("Fire"); }
		else if (Anim == TEXT("Reload")) { GunClip = TEXT("Reload"); }
		else if (Anim == TEXT("ReloadEmpty")) { GunClip = TEXT("ReloadEmpty"); }
		const TSoftObjectPtr<UAnimSequenceBase>* GunSoft = GunAnims.Find(GunClip);
		if (UAnimSequenceBase* G = GunSoft ? GunSoft->LoadSynchronous() : nullptr)
		{
			ArmsGun->PlayAnimation(G, GunClip == TEXT("Idle"));
		}
	}
}

void AGRArsenal::UpdateArms(float Dt, const FVector& LocDelta, const FRotator& TurnDelta)
{
	// 1. When a one-off clip ends, go back to idle (or the aim pose). (With the pack's
	//    blueprint, idle runs by itself; we only count down the montage.)
	const bool bBlueprint = Arms->GetAnimationMode() == EAnimationMode::AnimationBlueprint;
	if (bBlueprint)
	{
		ArmsOneShotLeft = FMath::Max(0.f, ArmsOneShotLeft - Dt);
	}
	else if (ArmsOneShotLeft > 0.f)
	{
		ArmsOneShotLeft -= Dt;
		if (ArmsOneShotLeft <= 0.f)
		{
			PlayArms(bWantAim ? FName(TEXT("ADSPose")) : FName(TEXT("Idle")), true);
		}
	}
	else
	{
		const FName Want = bWantAim ? FName(TEXT("ADSPose")) : FName(TEXT("Idle"));
		if (ArmsState != Want)
		{
			PlayArms(Want, true);
		}
	}
	// 2. Small extra motion on top of the animation: recoil push, mouse sway, switching dip.
	const float Down = Pending != -1 ? FMath::Clamp(1.f - FMath::Abs(SwitchTime / FMath::Max(0.01f, SwitchSeconds) - 0.5f) * 2.f, 0.f, 1.f) : 0.f;
	const FVector Extra = FVector(-RecoilBack, Sway.X * 0.3f, -16.f * Down) + LocDelta;
	const FRotator Tilt = FRotator(Sway.Y * 0.5f - 20.f * Down, Sway.X * 0.5f, 0.f) + TurnDelta;
	Arms->SetRelativeLocationAndRotation(ArmsOffset + Extra, (Tilt.Quaternion() * ArmsTurn.Quaternion()).Rotator());
	// Scoped in (sniper) hides the arms like it hides the gun.
	const bool bScopedIn = Held() && Held()->bScope && AimAlpha > 0.92f;
	Arms->SetHiddenInGame(bScopedIn);
	Gloves->SetHiddenInGame(bScopedIn);
	ArmsGun->SetHiddenInGame(bScopedIn);
}

// =====================================================================================
//  TWIN BLADES
// =====================================================================================
//  The "sword frame": inside the model, which way does the blade point (X) and which way
//  is the back of the blade (Z, so the sharp edge is -Z)? Read from the three marker points
//  saved in the model (Tip, Base, Edge). If they're missing, guess from the model's box.
FQuat AGRArsenal::SwordFrame(const UStaticMeshComponent* Part) const
{
	const UStaticMesh* M = Part ? Part->GetStaticMesh() : nullptr;
	if (!M)
	{
		return FQuat::Identity;
	}
	const UStaticMeshSocket* TipS = M->FindSocket(TEXT("Tip"));
	const UStaticMeshSocket* EdgeS = M->FindSocket(TEXT("Edge"));
	FVector Tip, Edge;
	if (TipS && EdgeS)
	{
		Tip = TipS->RelativeLocation;
		Edge = EdgeS->RelativeLocation;
	}
	else
	{
		// No markers: the blade runs along the box's longest side, toward its far end.
		const FBox B = M->GetBoundingBox();
		const FVector Ext = B.GetExtent();
		const int32 Long = Ext.X >= Ext.Y && Ext.X >= Ext.Z ? 0 : (Ext.Y >= Ext.Z ? 1 : 2);
		const int32 Wide = Long == 0 ? (Ext.Y >= Ext.Z ? 1 : 2) : (Long == 1 ? (Ext.X >= Ext.Z ? 0 : 2) : (Ext.X >= Ext.Y ? 0 : 1));
		Tip = FVector::ZeroVector;
		Tip[Long] = FMath::Abs(B.Max[Long]) >= FMath::Abs(B.Min[Long]) ? B.Max[Long] : B.Min[Long];
		Edge = FVector::ZeroVector;
		Edge[Wide] = B.Max[Wide];
	}
	const FVector X = Tip.GetSafeNormal();
	const FVector EdgeDir = (Edge - X * FVector::DotProduct(Edge, X)).GetSafeNormal();
	return FRotationMatrix::MakeFromXZ(X, -EdgeDir).ToQuat();
}

//  Where the cutting part of a blade is in the world right now: from where it leaves the
//  handle to the tip, along the middle of the blade (half way between the back and the edge).
bool AGRArsenal::BladeEnds(const UStaticMeshComponent* Part, FVector& Base, FVector& Tip) const
{
	if (!Part || !Part->GetStaticMesh())
	{
		return false;
	}
	const FTransform T = Part->GetComponentTransform();
	const UStaticMesh* M = Part->GetStaticMesh();
	const UStaticMeshSocket* TipS = M->FindSocket(TEXT("Tip"));
	const UStaticMeshSocket* BaseS = M->FindSocket(TEXT("Base"));
	const UStaticMeshSocket* EdgeS = M->FindSocket(TEXT("Edge"));
	if (TipS && BaseS && EdgeS)
	{
		const FVector Axis = TipS->RelativeLocation.GetSafeNormal();
		const FVector Half = (EdgeS->RelativeLocation - Axis * FVector::DotProduct(EdgeS->RelativeLocation, Axis)) * 0.5f;
		Base = T.TransformPosition(BaseS->RelativeLocation + Half);
		Tip = T.TransformPosition(TipS->RelativeLocation + Half);
		return true;
	}
	const FVector X = SwordFrame(Part).GetAxisX();
	const float Len = M->GetBoundingBox().GetSize().GetMax();
	Base = T.TransformPosition(X * Len * 0.15f);
	Tip = T.TransformPosition(X * Len * 0.9f);
	return true;
}

//  Desktop moves for the twin blades. Each move is a few "key poses" for the RIGHT blade
//  (where it is in front of your eyes and how it is turned); the left blade does the mirror
//  image. Between keys the pose eases smoothly (Slerp = smooth turning between two rotations).
//    Slash (left click)  : wind up out to the side, sweep across in front of you, recover.
//                          The blades take turns, so clicking fast gives right-left-right.
//    Cross-cut (right)   : both blades raised high, then driven down across each other: an X.
//  Rotations: Pitch = tip up/down, Yaw = tip right/left, Roll = turns the edge (90 = edge
//  leading a right-to-left swing).
void AGRArsenal::AnimateSwords(float Dt, const FVector& ExtraLoc, const FRotator& ExtraTurn)
{
	const FGRWeapon& W = Weapons[Current];
	struct FKey { float T; FVector L; FRotator R; };
	const FKey Ready{ 0.f, W.HipOffset, W.HipTurn };
	const TArray<FKey> Slash = {
		Ready,
		{ 0.22f, FVector(22.f, 48.f, -2.f),  FRotator(10.f, 78.f, 90.f) },    // wound up, out to the right
		{ 0.42f, FVector(58.f, 8.f, -8.f),   FRotator(0.f, 5.f, 90.f) },      // through the middle, arm extended
		{ 0.62f, FVector(40.f, -32.f, -20.f), FRotator(-12.f, -72.f, 90.f) }, // follow-through, out to the left
		{ 1.f, Ready.L, Ready.R } };
	const TArray<FKey> Cross = {
		Ready,
		{ 0.30f, FVector(14.f, 30.f, 30.f),  FRotator(72.f, 15.f, 0.f) },     // both blades raised high
		{ 0.55f, FVector(50.f, -14.f, -34.f), FRotator(-60.f, -25.f, 0.f) },  // driven down across each other
		{ 1.f, Ready.L, Ready.R } };

	auto Pose = [](const TArray<FKey>& Keys, float P, FVector& OutL, FQuat& OutQ)
	{
		for (int32 i = 1; i < Keys.Num(); ++i)
		{
			if (P <= Keys[i].T || i == Keys.Num() - 1)
			{
				const float A = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((P - Keys[i - 1].T) / FMath::Max(0.001f, Keys[i].T - Keys[i - 1].T), 0.f, 1.f));
				OutL = FMath::Lerp(Keys[i - 1].L, Keys[i].L, A);
				OutQ = FQuat::Slerp(Keys[i - 1].R.Quaternion(), Keys[i].R.Quaternion(), A);
				return;
			}
		}
	};

	// Timing (and the actor/vine hit half way, for things that aren't cuttable scenery).
	float P = -1.f;
	if (MeleeTime >= 0.f)
	{
		MeleeTime += Dt;
		P = FMath::Clamp(MeleeTime / (bHeavy ? StabSeconds : SlashSeconds), 0.f, 1.f);
		if (!bMeleeHit && P >= 0.45f)
		{
			MeleeHit();
		}
		if (P >= 1.f)
		{
			MeleeTime = -1.f;
		}
	}
	for (int32 Side = 0; Side < 2; ++Side)   // 0 = left, 1 = right
	{
		UStaticMeshComponent* Part = Side == 1 ? RightMesh.Get() : LeftMesh.Get();
		if (!Part || !Part->IsVisible())
		{
			continue;
		}
		const bool bSwinging = P >= 0.f && (bHeavy || (Side == 0) == bLeftBlade);
		FVector L = Ready.L;
		FQuat Q = Ready.R.Quaternion();
		if (bSwinging)
		{
			Pose(bHeavy ? Cross : Slash, P, L, Q);
		}
		if (Side == 0)
		{
			// The left blade is the mirror image: flip it to the other side (Y), and flip which
			// way it points sideways (Yaw) and which way the edge leans (Roll).
			FRotator M = Q.Rotator();
			M.Yaw = -M.Yaw;
			M.Roll = -M.Roll;
			L.Y = -L.Y;
			Q = M.Quaternion();
		}
		L += ExtraLoc;
		Q = ExtraTurn.Quaternion() * Q;
		Part->SetRelativeLocationAndRotation(L, Q * SwordFrame(Part).Inverse());
	}
}

//  Close a hand round a blade's grip.
//  1. In the hand's "grasp" pose each finger is curled, so its three joints sit on a circle
//     round the hollow of the fist. The middle of that circle ("circumcentre") is on the line
//     the grip must run along. Doing this for all four fingers gives four points on that line.
//  2. The line runs from the little finger to the index finger: the blade comes out above
//     the index finger and thumb. The middle knuckles say which way the sharp edge faces
//     (sword fighters line the edge up with them).
//  3. Move and turn the hand so that this "fist line" sits exactly on the sword's grip.
void AGRArsenal::FitHandToBlade(USkeletalMeshComponent* Hand, const UStaticMeshComponent* Blade)
{
	if (!Hand || !Blade)
	{
		return;
	}
	const TArray<FTransform>& Bones = Hand->GetComponentSpaceTransforms();
	if (Bones.Num() == 0)
	{
		return;
	}
	const FVector S = Hand->GetComponentScale();   // (1,-1,1) for the mirrored left hand
	auto At = [&](const FString& Name, FVector& Out)
	{
		const int32 I = Hand->GetBoneIndex(FName(*Name));
		if (I == INDEX_NONE || !Bones.IsValidIndex(I))
		{
			return false;
		}
		Out = Bones[I].GetLocation() * S;
		return true;
	};
	auto Circumcentre = [](const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector AB = A - C, BC = B - C;
		const FVector N = FVector::CrossProduct(AB, BC);
		const double Den = 2.0 * N.SizeSquared();
		if (Den < 1e-6)
		{
			return (A + B + C) / 3.f;   // joints in a straight line (open hand): use their middle
		}
		return C + FVector::CrossProduct(BC * AB.SizeSquared() - AB * BC.SizeSquared(), N) / Den;
	};
	const TCHAR* Fingers[4] = { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
	FVector Centres[4];
	FVector Knuckles = FVector::ZeroVector;
	for (int32 F = 0; F < 4; ++F)
	{
		FVector J1, J2, J3;
		if (!At(FString::Printf(TEXT("%s_01_r"), Fingers[F]), J1) || !At(FString::Printf(TEXT("%s_02_r"), Fingers[F]), J2)
			|| !At(FString::Printf(TEXT("%s_03_r"), Fingers[F]), J3))
		{
			return;   // not the hand model we expect
		}
		Centres[F] = Circumcentre(J1, J2, J3);
		Knuckles += J2 / 4.f;
	}
	const FVector Origin = (Centres[0] + Centres[1] + Centres[2] + Centres[3]) / 4.f;
	const FVector Axis = (Centres[0] - Centres[3]).GetSafeNormal();                     // little finger -> index finger
	FVector Edge = Knuckles - Origin;
	Edge = (Edge - Axis * FVector::DotProduct(Edge, Axis)).GetSafeNormal();            // toward the middle knuckles
	const FQuat Fist = FRotationMatrix::MakeFromXZ(Axis, -Edge).ToQuat();             // same layout as SwordFrame

	// The sword's grip in the world, with the tuning knobs (slide along it, twist round it).
	const FQuat Grip = Blade->GetComponentQuat() * SwordFrame(Blade) * FQuat(FVector::XAxisVector, FMath::DegreesToRadians(GripTwist));
	const FVector GripAt = Blade->GetComponentLocation() + Grip.GetAxisX() * GripSlide;
	const FQuat HandRot = Grip * Fist.Inverse();
	Hand->SetWorldLocationAndRotation(GripAt - HandRot.RotateVector(Origin), HandRot);
}

//  Every frame: give the cutter the path each blade took since last frame, while it is
//  swinging (desktop: during the strike part of a slash; headset: whenever the tip moves
//  faster than MinCutSpeed). Then show chips / sparks / leaves and give a little kick.
void AGRArsenal::CutWithBlades(float Dt)
{
	AGrappleRig* R = Rig.Get();
	const FGRWeapon* W = Held();
	const bool bVR = R && R->IsUsingVR();
	APlayerController* PC = R ? R->GetPlayerController() : nullptr;
	for (int32 Side = 0; Side < 2; ++Side)   // fade out any buzz
	{
		if (HapticLeft[Side] > 0.f && (HapticLeft[Side] -= Dt) <= 0.f && PC)
		{
			PC->SetHapticsByValue(0.f, 0.f, Side == 0 ? EControllerHand::Left : EControllerHand::Right);
		}
	}
	if (!R || !W || !IsBlade(W->Kind) || Pending != -1)
	{
		bBladePrevValid[0] = bBladePrevValid[1] = false;
		return;
	}
	if (!Cutter.IsValid())
	{
		TActorIterator<AGRCutter> It(GetWorld());
		Cutter = It ? *It : GetWorld()->SpawnActor<AGRCutter>();
	}
	AGRCutter* C = Cutter.Get();
	if (!C)
	{
		return;
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		UStaticMeshComponent* Part = Side == 1 ? RightMesh.Get() : LeftMesh.Get();
		FVector A, B;
		if (!Part || !Part->IsVisible() || Part->bHiddenInGame || !BladeEnds(Part, A, B))
		{
			bBladePrevValid[Side] = false;
			continue;
		}
		bool bActive = false;
		if (bVR)
		{
			const float TipSpeed = bBladePrevValid[Side] ? FVector::Dist(B, BladePrev[Side][1]) / FMath::Max(0.001f, Dt) : 0.f;
			bActive = TipSpeed > MinCutSpeed;
			if (bActive && !bVRSwinging[Side])
			{
				SwingIds[Side] = NextSwingId++;   // a new real-life swing
			}
			bVRSwinging[Side] = bActive;
		}
		else if (MeleeTime >= 0.f)
		{
			const float P = MeleeTime / (bHeavy ? StabSeconds : SlashSeconds);
			const bool bThisBlade = bHeavy || (Side == 0) == bLeftBlade || W->Kind == EGRWeaponKind::Knife;
			bActive = bThisBlade && P > 0.12f && P < 0.8f;   // the strike, not the wind-up or recovery
		}
		if (bActive && bBladePrevValid[Side])
		{
			TArray<FGRCutResult> Hits;
			C->BladePass(BladePrev[Side][0], BladePrev[Side][1], A, B, bHeavy && !bVR ? 1.5f : 1.f, SwingIds[Side], Hits);
			for (const FGRCutResult& H : Hits)
			{
				GCutCount += H.bCut ? 1 : 0;
				// Bits flying off: green for plants, pale wood for trees, sparks for the rest.
				const FLinearColor Col = H.bSoft ? FLinearColor(0.35f, 0.9f, 0.3f) : (H.bTree ? FLinearColor(0.8f, 0.55f, 0.3f) : FLinearColor(1.f, 0.8f, 0.45f));
				for (int32 k = 0; k < (H.bCut ? 6 : 3); ++k)
				{
					const FVector Dir = (FMath::VRand() + FVector(0.f, 0.f, 0.6f)).GetSafeNormal();
					ShowEffect(H.Point, H.Point + Dir * FMath::FRandRange(12.f, 35.f), 2.5f, 0.22f, Col);
				}
				if (bVR && PC)
				{
					PC->SetHapticsByValue(0.6f, H.bCut ? 1.f : 0.55f, Side == 0 ? EControllerHand::Left : EControllerHand::Right);
					HapticLeft[Side] = H.bCut ? 0.12f : 0.07f;
				}
				else if (!bVR && H.bTree)
				{
					R->AddLookKick(H.bCut ? 0.8f : 0.4f, 0.f);   // a hack into wood jolts the view a touch
				}
			}
		}
		BladePrev[Side][0] = A;
		BladePrev[Side][1] = B;
		bBladePrevValid[Side] = true;
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
	UpdateHands(DeltaSeconds);
	CutWithBlades(DeltaSeconds);   // the blades slice whatever they pass through
	VRReload();
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
			Status = FString::Printf(TEXT("%s   |  LMB/V slash  RMB/C cross-cut  X holster  Q/E hooks  F9 slow-mo   (swing %.2f, cuts %d)"),
				*W->Name, MeleeTime, GCutCount);
		}
		GEngine->AddOnScreenDebugMessage(4202, 0.f, FColor(255, 200, 120), Status);
	}
}
