// =====================================================================================
//  GrappleRig.h  -  the "menu" for the grapple system
// =====================================================================================
//
//  WHAT THIS FILE IS (no coding experience needed)
//  ------------------------------------------------
//  C++ code in Unreal is split into two files:
//    * a ".h" file (this one), called a HEADER. It is like a restaurant menu: it LISTS
//      everything the grapple system has (its settings, its memory, its abilities)
//      without explaining how anything is done.
//    * a ".cpp" file (GrappleRig.cpp). It is the kitchen: it contains the actual
//      step-by-step instructions for each thing listed here.
//
//  WHAT THE GRAPPLE RIG DOES
//  -------------------------
//  You place ONE "GrappleRig" object anywhere in the level. When the game starts it
//  finds the player and takes over how the player moves through the air:
//    - fire a hook from each hand (two hooks, like Attack on Titan gear)
//    - swing on the rope like a pendulum
//    - reel the rope in to pull yourself up
//    - use gas to boost in the direction you are looking
//    - land on islands and walk, or respawn if you fall into the mist
//
//  CONTROLS
//  --------
//  VR:      Trigger = fire hook / keep holding to stay attached   Grip = reel that hook in
//           A / X / trackpad click = gas boost toward where you look
//  Desktop: Left / right mouse = left / right hook     Shift = reel both hooks in
//           Space = jump on the ground, gas boost in the air
//           W A S D = walk     R = respawn on the first island
//  (Desktop mode exists so the game can be tested without a VR headset.)
//
//  HOW TO TWEAK THE FEEL WITHOUT CODING
//  -----------------------------------
//  Every line below that starts with UPROPERTY(EditAnywhere ...) is a SETTING.
//  In the Unreal editor: click the GrappleRig in the level, look at the Details panel
//  on the right, find the "Grapple" sections and change the numbers. No recompiling.
//
//  UNITS: Unreal measures distance in centimetres. 100 = 1 metre.
//         Speeds are centimetres per second. 1000 cm/s = 36 km/h.
// =====================================================================================

#pragma once   // "only read this file once", stops the compiler reading it twice

// #include = "this file uses things defined in these other files"
#include "CoreMinimal.h"            // Unreal's basic building blocks (vectors, text, lists)
#include "GameFramework/Actor.h"    // "Actor" = anything that can be placed in a level
#include "InputCoreTypes.h"         // names of keyboard keys, mouse buttons, controller buttons
#include "GrappleRig.generated.h"   // auto-written by Unreal; must always be the LAST include

// "Forward declarations": we promise these types exist, without loading their full files yet.
// It keeps compiling fast. The .cpp file includes the real files.
class UStaticMeshComponent;   // a visible 3D shape (we use cylinders for ropes, spheres for hook heads)
class UCameraComponent;       // the player's eyes
class USceneComponent;        // anything with a position (we use it for the VR hand controllers)
class APlayerController;      // the bit of Unreal that reads the player's buttons
class APawn;                  // the player's body in the world
class UMaterialInstanceDynamic; // a colour/material we can change while the game runs

// -------------------------------------------------------------------------------------
// One grapple line. There are two of these: [0] = left hand, [1] = right hand.
// A "struct" is just a labelled box that groups a few values together.
// -------------------------------------------------------------------------------------
struct FGrappleHook
{
	bool bAttached = false;                   // true while the hook is stuck in something
	bool bWasDown = false;                    // was the fire button held last frame? (to spot press / release)
	FVector Anchor = FVector::ZeroVector;     // the exact point in the world the hook is stuck to
	float Length = 0.f;                       // current rope length in cm (gets shorter when you reel in)
};

// UCLASS() tells Unreal "this is a type of object you should know about" - so it shows up
// in the editor and can be placed in levels.
UCLASS()
class GRAPPLERUNNER_API AGrappleRig : public AActor   // AGrappleRig IS an Actor (a placeable thing)
{
	GENERATED_BODY()   // Unreal fills in hidden plumbing here. Always needed.

public:
	// The "constructor": runs once when the GrappleRig object is created. It builds the rope shapes.
	AGrappleRig();

	// Tick runs EVERY FRAME (about 90 times a second in VR). This is the heartbeat of the system.
	// DeltaSeconds = how much time passed since the last frame (e.g. 0.011 seconds).
	virtual void Tick(float DeltaSeconds) override;

	// ================================================================================
	//  SETTINGS  (change these in the Details panel, no code needed)
	// ================================================================================

	// ---- Hooks ----
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float MaxFireRange = 6000.f;     // furthest a hook can reach (60 m)
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float AimAssistDegrees = 9.f;    // if you miss, snap to a grapple rock within this angle
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float MinRopeLength = 250.f;     // reeling stops at 2.5 m so you don't crash into the rock
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float ReelSpeed = 900.f;         // how fast the rope shortens when reeling (9 m/s)
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float ReelPullAccel = 1400.f;    // extra tug toward the hook while reeling
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float DesktopHookSpread = 0.08f; // desktop only: left/right hooks aim slightly apart
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float RopeThickness = 2.5f;      // how thick the rope looks (cm)

	// ---- Physics ----
	UPROPERTY(EditAnywhere, Category = "Grapple|Physics") float Gravity = 980.f;          // real Earth gravity is 980 cm/s^2
	UPROPERTY(EditAnywhere, Category = "Grapple|Physics") float AirDragPerSecond = 0.06f; // air resistance: 0.06 = lose 6% of speed per second
	UPROPERTY(EditAnywhere, Category = "Grapple|Physics") float MaxSpeed = 4500.f;        // speed limit (162 km/h) so physics never explodes

	// ---- Gas (the boost) ----
	UPROPERTY(EditAnywhere, Category = "Grapple|Gas") float GasMax = 100.f;               // full tank
	UPROPERTY(EditAnywhere, Category = "Grapple|Gas") float GasUsePerSecond = 30.f;       // a full tank lasts about 3 seconds of boosting
	UPROPERTY(EditAnywhere, Category = "Grapple|Gas") float GasRegenPerSecond = 35.f;     // refills while standing on the ground
	UPROPERTY(EditAnywhere, Category = "Grapple|Gas") float BoostAccel = 1600.f;          // how hard the boost pushes

	// ---- The player's body ----
	// The body is treated as a CAPSULE (a pill shape) for bumping into things.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float BodyRadius = 30.f;            // pill is 60 cm wide
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float BodyHalfHeight = 85.f;        // pill is 170 cm tall
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float WalkSpeed = 450.f;            // walking speed on islands
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float JumpSpeed = 520.f;            // jump strength
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float WalkableNormalZ = 0.65f;      // how flat a surface must be to stand on (1 = flat floor, 0 = wall)
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float KillDepthBelowIslands = 4000.f; // fall this far below the lowest island and you respawn

	// ---- Desktop testing ----
	UPROPERTY(EditAnywhere, Category = "Grapple|Desktop") float MouseSensitivity = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Grapple|Desktop") float DesktopEyeHeight = 165.f;  // camera height when there is no headset
	UPROPERTY(EditAnywhere, Category = "Grapple|Desktop") bool bShowDebugHud = true;       // speed / gas text in the corner

	// ---- Look ----
	UPROPERTY(EditAnywhere, Category = "Grapple|Look") FLinearColor RopeColor = FLinearColor(1.f, 0.25f, 0.04f); // orange

protected:
	// BeginPlay runs ONCE when you press Play. We use it to find grapple rocks and controller buttons.
	virtual void BeginPlay() override;

private:
	// ================================================================================
	//  MEMORY  (things the rig keeps track of while the game runs; not editable)
	// ================================================================================

	// The four visible shapes: a rope and a hook head for each hand.
	// UPROPERTY() here (without EditAnywhere) just tells Unreal "don't delete these while we use them".
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RopeL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RopeR;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HeadL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HeadR;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RopeMaterial;   // the orange colour

	// "Weak" pointers = "I'm looking at this object, but if it disappears, don't crash, just tell me".
	TWeakObjectPtr<APlayerController> PC;       // reads the player's buttons
	TWeakObjectPtr<APawn> Pawn;                 // the player's body
	TWeakObjectPtr<UCameraComponent> Camera;    // the player's eyes / headset
	TWeakObjectPtr<USceneComponent> Hands[2];   // left and right VR controllers
	TArray<TWeakObjectPtr<AActor>> Anchors;     // every rock tagged "GrappleAnchor" (for aim assist)

	// Lists of controller button names. Different headsets name their buttons differently
	// (Vive, Quest, Index...), so we keep a list per action and accept any of them.
	TArray<FKey> TriggerKeys[2], TriggerAxes[2], GripKeys[2], GripAxes[2], BoostKeys;

	FGrappleHook Hooks[2];                         // the two grapple lines: [0] left, [1] right
	FVector BodyPos = FVector::ZeroVector;         // where the player's feet are
	FVector LastSetLocation = FVector::ZeroVector; // where WE last put the player (to notice teleports)
	FVector Velocity = FVector::ZeroVector;        // current speed AND direction (an arrow in 3D)
	FVector MoveInput = FVector::ZeroVector;       // which way WASD is pushing (desktop)
	FVector RespawnPoint = FVector::ZeroVector;    // the first island's spawn spot
	float Gas = 100.f;                             // gas left in the tank
	float KillZ = -20000.f;                        // fall below this height = respawn
	float Accumulator = 0.f;                       // leftover time for the fixed-step physics (explained in the .cpp)
	float DesktopYaw = 0.f, DesktopPitch = 0.f;    // where the mouse is looking (left-right, up-down)
	bool bGrounded = false;                        // standing on something?
	bool bVR = false;                              // is a VR headset being used right now?
	bool bReel[2] = {false, false};                // is each hand reeling in?
	bool bBoost = false;                           // is the boost button held?
	bool bJumpWasDown = false;                     // was Space held last frame? (press detection)
	bool bRespawnWasDown = false;                  // was R held last frame?
	bool bDesktopSetupDone = false;                // have we set up the mouse-and-keyboard camera yet?

	// ================================================================================
	//  ABILITIES  (each one is written out step by step in GrappleRig.cpp)
	// ================================================================================
	bool EnsurePlayer();                              // find the player, camera and hands
	void ReadInput(float DeltaSeconds);               // read buttons / mouse
	void Simulate(float Dt);                          // one small step of physics
	void MoveWithCollision(const FVector& From, const FVector& To); // move without going through rocks
	void UpdateVisuals();                             // draw the ropes and aim dots
	void FireHook(int32 Index);                       // shoot hook 0 (left) or 1 (right)
	void ReleaseHook(int32 Index);                    // let go of a hook
	void Respawn();                                   // put the player back on the first island
	void Pulse(int32 Index, float Amplitude);         // vibrate a VR controller

	void GetAim(int32 Index, FVector& OutStart, FVector& OutDir) const;   // where a hand is pointing
	bool FindTarget(int32 Index, FVector& OutPoint) const;               // what a hook would hit
	FVector HandPosition(int32 Index) const;                             // where the rope starts
	FVector BodyCenter() const { return BodyPos + FVector(0.f, 0.f, BodyHalfHeight); } // middle of the body (feet + half height)

	bool AnyDown(const TArray<FKey>& Keys) const;                    // is ANY of these buttons pressed?
	bool AnyAxis(const TArray<FKey>& Keys, float Threshold) const;   // is ANY of these triggers squeezed past a point?
	static void AddKeys(TArray<FKey>& Out, std::initializer_list<const TCHAR*> Names); // build a button list
};
