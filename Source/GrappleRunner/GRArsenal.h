// =====================================================================================
//  GRArsenal.h  -  your weapons: switch, aim, scope, fire, reload, slash
// =====================================================================================
//
//  WHAT THIS IS
//  One actor that looks after every weapon the player can hold. The grapple rig spawns it
//  when you press Play. Each weapon is one "slot" in the Weapons list (edit them in the
//  Details panel). What a weapon does depends on its Kind:
//    Rifle / SMG    hold the left mouse button to keep firing
//    Burst          one click = 3 quick shots
//    Pistol         one click = one shot
//    Sniper         one shot, then a pause; aiming looks through the scope (zoom + reticle)
//    Knife / Swords left click = quick slash, right click = heavy stab
//    Hands          nothing in your hands: the mouse buttons fire the grapple hooks again
//
//  THREE WEAPONS (Naomi, 11 Nov): 1 knife, 2 rifle, 3 sniper.
//
//  HANDS DO THE RELOADING
//  Two hands are drawn (the VR template's hand models): the right one on the grip, the left on
//  the foregrip. A reload is the left hand doing the work: it grabs the magazine, pulls it out,
//  drops to the belt for a fresh one and slaps it in. The sniper is bolt action: after each
//  shot the left hand works the bolt. In VR this becomes MANUAL (see VRReload in the .cpp):
//    grip near the magazine and pull = eject   |   grip at your left hip = take a fresh one
//    bring it to the magwell = it clicks in    |   sniper: grab the bolt, pull back = next round
//
//  DESKTOP CONTROLS (for testing before the headset arrives)
//    1 / 2 / 3, mouse wheel  switch weapon     X  put the weapon away (hands free)
//    Left mouse            fire / slash        Right mouse  aim down sights / heavy stab
//    T                     reload              Q / E  left / right grapple hook
//
//  "RIGGED" ANIMATION WITHOUT AN ANIMATOR
//  These models have no animations of their own, so the movement is worked out in code
//  every frame ("procedural animation"). The weapon's place in front of your eyes is the
//  sum of a few small motions, each explained in the .cpp:
//    hip <-> aim (blend)  +  recoil kick  +  sway (lags behind the mouse)  +  walk bob
//    +  switch (lower, swap, raise)  +  reload (tilt and dip)  +  slash / stab curves.
//  Aiming uses the "Sight" point saved in each model (see Art/ThirdParty/make_skin_weapons.py):
//  the gun is moved so that point sits exactly on the line your eye looks along.
//
//  THE CAMERA RULE (CLAUDE.md)
//  Shots are traced from the player's EYES (first person: the camera is the eyes) or, in VR,
//  from the weapon in the hand. Never from a third-person camera.
//
//  VR NOTE
//  In the headset the weapon is held by the controller and the view is never zoomed or
//  kicked for you (zooming the whole view makes people sick). Trigger-to-fire in VR comes
//  with the headset step; for now VR just holds the weapon.
//
//  HOW TO READ THIS FILE
//  - "UENUM" / "USTRUCT" / "UCLASS" + "UPROPERTY(EditAnywhere)" = Unreal can show and edit it.
//  - Distances are centimetres, angles are degrees, times are seconds.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Brushes/SlateColorBrush.h"
#include "GRArsenal.generated.h"

class AGrappleRig;
class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class SWidget;

// What kind of weapon a slot holds (decides how it fires and moves).
UENUM(BlueprintType)
enum class EGRWeaponKind : uint8
{
	Hands,     // nothing held
	Rifle,     // automatic
	SMG,       // automatic, fast, weaker
	Burst,     // 3-round bursts
	Pistol,    // one shot per click
	Sniper,    // one big shot, scope
	Knife,     // melee, one blade
	Swords     // melee, a blade in each hand (ODM swords)
};

// -------------------------------------------------------------------------------------
// One weapon slot. Everything here can be changed in the Details panel (Weapons > [n]).
// -------------------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FGRWeapon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Weapon") FString Name;
	UPROPERTY(EditAnywhere, Category = "Weapon") EGRWeaponKind Kind = EGRWeaponKind::Rifle;
	UPROPERTY(EditAnywhere, Category = "Weapon") TSoftObjectPtr<UStaticMesh> Mesh;       // right hand
	UPROPERTY(EditAnywhere, Category = "Weapon") TSoftObjectPtr<UStaticMesh> LeftMesh;   // left hand (twin blades)
	UPROPERTY(EditAnywhere, Category = "Weapon") TSoftObjectPtr<UStaticMesh> FallbackMesh; // used until Mesh has been imported
	UPROPERTY(EditAnywhere, Category = "Weapon") float Scale = 0.75f;                    // the models are a little big for a person

	// ---- Where it's held (desktop, from the eyes: forward, right, up) ----
	UPROPERTY(EditAnywhere, Category = "Weapon|Hold") FVector HipOffset = FVector(30.f, 13.f, -15.f);
	UPROPERTY(EditAnywhere, Category = "Weapon|Hold") FRotator HipTurn = FRotator(0.f, -4.f, 0.f);   // angled in a little, like a real hold
	UPROPERTY(EditAnywhere, Category = "Weapon|Hold") float AimDistance = 20.f;   // how far in front of the eye the sight sits when aiming

	// ---- Firing ----
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") float Damage = 40.f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") float FireRate = 9.75f;      // shots per second
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") int32 Magazine = 25;
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") float ReloadSeconds = 2.5f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") float SpreadHip = 2.f;       // degrees of random spread from the hip...
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") float SpreadAim = 0.25f;     // ...and when aiming
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") float RecoilKick = 0.7f;     // degrees your view climbs per shot
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") float Range = 25000.f;       // 250 m
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") float AimFOV = 70.f;         // view angle when aiming (desktop); 0 = no zoom
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") bool bScope = false;         // aiming looks through a scope (hide the gun, show a reticle)
	UPROPERTY(EditAnywhere, Category = "Weapon|Fire") FLinearColor TracerColour = FLinearColor(1.f, 0.55f, 0.15f);

	// ---- Melee ----
	UPROPERTY(EditAnywhere, Category = "Weapon|Melee") float Reach = 160.f;        // how far a slash reaches from your eyes

	// ---- Hands and reloading (points on the weapon, in the model's own centimetres; X = toward the muzzle) ----
	UPROPERTY(EditAnywhere, Category = "Weapon|Reload") FVector SupportHand = FVector(28.f, 0.f, -4.f);   // where the left hand holds (foregrip)
	UPROPERTY(EditAnywhere, Category = "Weapon|Reload") FVector MagazinePoint = FVector(10.f, 0.f, -9.f); // the magazine / magwell
	UPROPERTY(EditAnywhere, Category = "Weapon|Reload") FVector BoltPoint = FVector(-2.f, 4.f, 5.f);      // the bolt handle (bolt-action rifles)
	UPROPERTY(EditAnywhere, Category = "Weapon|Reload") bool bBoltAction = false;   // pull the bolt after every shot
	UPROPERTY(EditAnywhere, Category = "Weapon|Reload") TSoftObjectPtr<UStaticMesh> MagazineMesh;   // optional: the magazine shown in the hand
};

UCLASS()
class GRAPPLERUNNER_API AGRArsenal : public AActor
{
	GENERATED_BODY()

public:
	AGRArsenal();
	virtual void Tick(float DeltaSeconds) override;

	// The slots. Keys 1-9 and 0 pick the first ten; the mouse wheel steps through all of them.
	UPROPERTY(EditAnywhere, Category = "Arsenal") TArray<FGRWeapon> Weapons;
	UPROPERTY(EditAnywhere, Category = "Arsenal") int32 StartWeapon = 1;   // 0 = the first slot

	// ---- Feel ----
	UPROPERTY(EditAnywhere, Category = "Arsenal|Feel") float AimSeconds = 0.18f;    // time to raise the sights
	UPROPERTY(EditAnywhere, Category = "Arsenal|Feel") float SwitchSeconds = 0.3f; // time to put one weapon away and draw the next
	UPROPERTY(EditAnywhere, Category = "Arsenal|Feel") float SwayAmount = 0.6f;     // how much the weapon lags behind the mouse
	UPROPERTY(EditAnywhere, Category = "Arsenal|Feel") float BobAmount = 1.f;       // walk bob strength
	UPROPERTY(EditAnywhere, Category = "Arsenal|Feel") float RecoilRecovery = 10.f; // how fast the gun settles after a shot
	UPROPERTY(EditAnywhere, Category = "Arsenal|Feel") float SlashSeconds = 0.32f;
	UPROPERTY(EditAnywhere, Category = "Arsenal|Feel") float StabSeconds = 0.55f;

	// ---- Hands (the VR template's hand models) ----
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") TSoftObjectPtr<class USkeletalMesh> RightHandModel =
		TSoftObjectPtr<class USkeletalMesh>(FSoftObjectPath(TEXT("/Game/XRMannequins/Meshes/SKM_MannyXR_right.SKM_MannyXR_right")));
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") TSoftObjectPtr<class USkeletalMesh> LeftHandModel =
		TSoftObjectPtr<class USkeletalMesh>(FSoftObjectPath(TEXT("/Game/XRMannequins/Meshes/SKM_MannyXR_left.SKM_MannyXR_left")));
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") TSoftObjectPtr<class UAnimationAsset> GraspPose =
		TSoftObjectPtr<class UAnimationAsset>(FSoftObjectPath(TEXT("/Game/XRMannequins/Animations/A_MannequinsXR_Grasp_Right.A_MannequinsXR_Grasp_Right")));
	// How a hand model sits on a grip point (tune these if a hand looks twisted).
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") FVector RightHandOffset = FVector(-3.f, -3.5f, 4.5f);
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") FRotator RightHandTurn = FRotator(-25.f, 0.f, 90.f);
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") FVector LeftHandOffset = FVector(-3.f, 3.5f, 4.5f);
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") FRotator LeftHandTurn = FRotator(-25.f, 0.f, -90.f);
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") FVector BeltPoint = FVector(10.f, -16.f, -60.f);   // left hip pouch, from the eyes (desktop)
	UPROPERTY(EditAnywhere, Category = "Arsenal|Hands") float BoltSeconds = 0.7f;

	// Glowing material for tracers and sparks (made by Scripts/import_skin_weapons.py).
	UPROPERTY(EditAnywhere, Category = "Arsenal|Effects") TSoftObjectPtr<UMaterialInterface> GlowMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Weapons/FX/M_WeaponGlow.M_WeaponGlow")));

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	// ---- Who and what ----
	TWeakObjectPtr<AGrappleRig> Rig;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RightMesh;     // the weapon in your right hand
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftMesh;      // the second blade
	UPROPERTY() TObjectPtr<UPointLightComponent> MuzzleLight;   // the flash of light when a gun fires
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Effects;   // tracers and sparks, reused
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;                   // the shape tracers and sparks are drawn with
	TArray<float> EffectLife;                                    // seconds left for each effect
	TArray<float> EffectLifeStart;
	TArray<FVector> EffectScale;

	// ---- State ----
	int32 Current = -1;           // slot being held (or being drawn)
	int32 Pending = -1;           // slot we're switching to (-1 = not switching)
	bool bHolstered = false;      // X: hands free
	TArray<int32> Ammo;           // bullets left in each slot's magazine
	float AimAlpha = 0.f;         // 0 = hip, 1 = fully aimed
	float SwitchTime = 0.f;       // counts up during a switch
	float Cooldown = 0.f;         // time until the next shot is allowed
	float ReloadLeft = 0.f;       // >0 while reloading
	int32 BurstLeft = 0;          // shots still to come in this burst
	float RecoilPitch = 0.f;      // visual kick of the gun (degrees), settles back to 0
	float RecoilBack = 0.f;       // visual push back toward you (cm)
	FVector2D Sway = FVector2D::ZeroVector;
	float BobPhase = 0.f;
	float MeleeTime = -1.f;       // <0 = not swinging; otherwise seconds into the swing
	bool bHeavy = false;          // this swing is the heavy stab
	bool bMeleeHit = false;       // already checked for hits this swing
	bool bLeftBlade = false;      // swords: which blade swings this time
	float BaseFOV = 90.f;
	float BaseVignette = 0.4f;
	bool bHaveBaseFOV = false;
	float BaseSensitivity = 0.6f;
	float MuzzleFlash = 0.f;      // seconds of muzzle light left
	bool bWantAim = false;        // right mouse held (guns)
	bool bSwapped = false;        // during a switch: has the new weapon been swapped in yet?
	float BoltTime = -1.f;        // sniper: <0 = not working the bolt; otherwise seconds into it
	// VR manual reload
	bool bMagIn = true;           // is a magazine in the gun?
	bool bMagInLeftHand = false;  // is the left hand holding a magazine?
	bool bChambered = true;       // is a round ready to fire? (bolt action)
	bool bGripWas[2] = { false, false };
	FVector BoltGrabStart = FVector::ZeroVector;
	bool bBoltHeld = false;
	UPROPERTY() TObjectPtr<class USkeletalMeshComponent> HandR;     // the hands
	UPROPERTY() TObjectPtr<class USkeletalMeshComponent> HandL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HeldMag;           // magazine in the left hand during a reload
	void UpdateHands(float Dt);   // place the hands on the weapon (and do the reload / bolt choreography)
	void VRReload();              // the manual VR reload (headset only)

	// ---- Scope reticle (a flat overlay; desktop only) ----
	TSharedPtr<SWidget> Reticle;
	FSlateColorBrush ReticleBrush = FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.9f));

	// ---- Steps (each one explained in the .cpp) ----
	bool FindRig();
	void SetDefaultWeapons();
	void ReadInput(float Dt);
	void StartSwitch(int32 Slot);
	void ShowSlot(int32 Slot);
	void Fire();
	void StartReload();
	void StartMelee(bool bHeavyStab);
	void MeleeHit();
	void Animate(float Dt);
	void UpdateEffects(float Dt);
	void ShowEffect(const FVector& From, const FVector& To, float Thickness, float Life, const FLinearColor& Colour);
	void SnapVinesAlong(const FVector& From, const FVector& To, float Radius);
	void ShowReticle(bool bShow);
	FVector SightPoint(const UStaticMeshComponent* Part) const;    // the model's "Sight" point, in its own space
	FVector MuzzleWorld() const;                                   // where the barrel ends, in the world
	bool IsGun(EGRWeaponKind Kind) const;
	bool IsBlade(EGRWeaponKind Kind) const { return Kind == EGRWeaponKind::Knife || Kind == EGRWeaponKind::Swords; }
	const FGRWeapon* Held() const { return (!bHolstered && Weapons.IsValidIndex(Current)) ? &Weapons[Current] : nullptr; }
};
