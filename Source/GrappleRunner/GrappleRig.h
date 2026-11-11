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
//    - swing on the rope like a pendulum (the rope has a little give, so it doesn't jolt)
//    - draw each rope as a flexible chain that sags, swings and whips
//    - measure how hard the rope pulls on your 70 kg body (in newtons) and let you
//      FEEL it as controller vibration (later: the EXIT SUIT)
//    - reel the rope in to pull yourself up
//    - use gas to boost in the direction you are looking
//    - land on islands and walk, or respawn if you fall into the mist
//
//  CONTROLS
//  --------
//  VR:      Trigger = fire hook / keep holding to stay attached
//           The hook pulls you in by itself (auto-pull); hold Grip to stop pulling and swing
//           A / X / trackpad click = gas boost toward where you look
//  Desktop: Left / right mouse = left / right hook     Shift = stop pulling (swing)
//           Space = jump on the ground, gas boost in the air
//           W A S D = walk     R = respawn on the first island
//  Tests:   press ` while playing, then type  gr.Test snap stiff  (or soft / reel)
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
class USkeletalMesh;          // a 3D character with bones (the visible body)
class UAnimSequence;          // one animation (idle, run, in the air)
class USkeletalMeshComponent; // the body placed in the world
class UStaticMesh;            // a 3D shape asset (we use the engine's cylinder for rope links)
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
	// Auto-pull (Attack on Titan style): once a hook bites, it reels you toward it by itself.
	// Hold grip (VR) or Shift (desktop) to STOP pulling and swing freely instead.
	// Untick for the old way: swing by default, hold grip / Shift to reel in.
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") bool bAutoPull = true;
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float DesktopHookSpread = 0.08f; // desktop only: left/right hooks aim slightly apart
	UPROPERTY(EditAnywhere, Category = "Grapple|Hooks") float RopeThickness = 2.5f;      // how thick the rope looks (cm)

	// ---- Rope feel (comfort) ----
	// Test T-04 showed a perfectly stiff rope stops you dead when it snaps tight: about 73% of
	// your speed vanishes in one frame. Sudden jolts your inner ear can't feel are a big cause
	// of VR motion sickness. A "soft" rope has a little give, like a spring-loaded reel: it
	// stretches slightly and takes the jolt over a fraction of a second instead.
	// Untick bSoftRope to get the old stiff rope back, so the two can be compared in tests.
	UPROPERTY(EditAnywhere, Category = "Grapple|Rope Feel") bool bSoftRope = true;
	// How strongly the rope pulls back for each cm it is stretched. Higher = stiffer, snappier.
	// (The square root of this is how fast the rope "springs": sqrt(64) = 8, so the jolt is
	// spread over roughly 1/8 of a second.)
	UPROPERTY(EditAnywhere, Category = "Grapple|Rope Feel") float RopeStiffness = 64.f;
	// Stops the rope bouncing like a bungee. 0 = bouncy forever, 1 = no bounce at all.
	UPROPERTY(EditAnywhere, Category = "Grapple|Rope Feel") float RopeDampingRatio = 0.6f;
	// Progressive spring (fix for F6): the rope gets stiffer the more it stretches, so a hard
	// catch on a short rope never reaches the safety limit. At this many cm of stretch it is
	// twice as stiff; at double this, five times. 0 = off (plain spring).
	UPROPERTY(EditAnywhere, Category = "Grapple|Rope Feel") float RopeProgressiveStretch = 100.f;
	// Safety limit: the rope can never stretch more than this (cm), however hard you fall.
	UPROPERTY(EditAnywhere, Category = "Grapple|Rope Feel") float MaxRopeStretch = 150.f;
	// While reeling in with the soft rope, the reel "stalls" (waits for you to catch up) once
	// the rope is stretched this much (cm). Without this the reel winds faster than you move,
	// the rope stretches like a catapult and flings you (found in the offline model, Tools/rope_model.py).
	UPROPERTY(EditAnywhere, Category = "Grapple|Rope Feel") float ReelStallStretch = 20.f;

	// ---- Swing feel ----
	// Exaggerates the swing slightly, like a superhero game: while a rope is tight, gravity
	// pulls a bit harder ALONG the swing (never along the rope), so you swing faster and
	// further. 1.0 = real physics; 1.35 = about 16% more speed at the bottom of each arc.
	UPROPERTY(EditAnywhere, Category = "Grapple|Swing Feel") float SwingExaggeration = 1.35f;
	// Wind: a rushing-air sound that gets louder and higher the faster you go.
	UPROPERTY(EditAnywhere, Category = "Grapple|Swing Feel") float WindStartSpeed = 300.f;  // cm/s: wind starts at 11 km/h...
	UPROPERTY(EditAnywhere, Category = "Grapple|Swing Feel") float WindFullSpeed = 3500.f;  // ...and is loudest at 126 km/h
	UPROPERTY(EditAnywhere, Category = "Grapple|Swing Feel") float WindMaxVolume = 0.9f;    // 1.0 = full volume

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
	// The rope is drawn as a chain of this many short links, each moving with its own physics,
	// so it sags when slack, whips when you swing, and snaps straight when pulled tight.
	UPROPERTY(EditAnywhere, Category = "Grapple|Look") int32 RopeLinks = 24;
	// How quickly the rope's wobble dies away (0 = wobbles forever, 0.1 = settles fast).
	UPROPERTY(EditAnywhere, Category = "Grapple|Look") float RopeWobbleDamping = 0.02f;

	// ---- Body ----
	// Your mass in kg. Used to turn the rope's pull into a real force (newtons), which sets
	// how hard the controllers vibrate: the harder the rope pulls, the more you feel it.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float PlayerMass = 70.f;
	// Your visible body (first person): look down and you see your legs, the harness and your
	// arms. Starts as Paragon's Kwang; swap in your own Blender version later.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") bool bShowBody = true;
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") TSoftObjectPtr<USkeletalMesh> BodyMeshAsset =
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/Kwang_GDC.Kwang_GDC")));
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") TSoftObjectPtr<UAnimSequence> IdleAnim =
		TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/Idle.Idle")));
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") TSoftObjectPtr<UAnimSequence> RunAnim =
		TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/Jog_Fwd.Jog_Fwd")));
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") TSoftObjectPtr<UAnimSequence> AirAnim =
		TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/Jump_Apex.Jump_Apex")));
	// How far the body sits BEHIND your eyes (cm), so the camera never ends up inside the head.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float BodyBehindEyes = 18.f;
	// Size of the body (1 = as made). Kwang is very tall, so he's scaled down to match a
	// 165 cm eye height; adjust if your shoulders show in front of the camera.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float BodyScale = 0.85f;
	// The grapple harness v2 (Art/Harness/make_harness.py): a real climbing harness with
	// ODM gear on it, in three pieces so each follows the bone it's strapped to.
	//   Waist piece -> hip bone: belt, buckle, belay loop, gas engine box, two cable launchers.
	//   Thigh pieces -> each thigh bone: leg loop, straps, blade box and gas tank.
	// Offsets are in the BODY's directions (cm): X = sideways, Y = forward, Z = up.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") TSoftObjectPtr<UStaticMesh> HarnessAsset =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Harness/SM_Harness_Waist.SM_Harness_Waist")));
	// Thigh piece whose outer side is +X, and its mirror image (outer side -X).
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") TSoftObjectPtr<UStaticMesh> ThighHarnessA =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Harness/SM_Harness_ThighA.SM_Harness_ThighA")));
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") TSoftObjectPtr<UStaticMesh> ThighHarnessB =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Harness/SM_Harness_ThighB.SM_Harness_ThighB")));
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") FName HarnessBone = TEXT("pelvis");   // the hip bone the belt is strapped to
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") FName ThighBoneLeft = TEXT("thigh_l");
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") FName ThighBoneRight = TEXT("thigh_r");
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") FVector HarnessOffset = FVector::ZeroVector;     // nudge the belt (cm)
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") FRotator HarnessRotation = FRotator::ZeroRotator; // turn it (Yaw 180 if it faces backwards)
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") FVector ThighHarnessOffset = FVector::ZeroVector; // nudge both thigh pieces (X is mirrored per leg)
	// Size of the harness. 1 = made for a normal adult waist; raise it if it sinks into the body.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float HarnessScale = 1.f;
	// The ODM gear itself (harness v3): the sculpted, textured model Naomi downloaded
	// (Art/ThirdParty/make_odm_gear.py). Reel unit at the small of the back, blade boxes and
	// gas tanks along the hips. Held on by the straps above. Empty = no gear.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") TSoftObjectPtr<UStaticMesh> GearAsset =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/GrappleRunner/Harness/SM_ODM_Gear.SM_ODM_Gear")));
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") FVector GearOffset = FVector::ZeroVector;   // nudge it (cm): X sideways, Y forward, Z up
	// Tilt (degrees): positive lowers the back ends of the blade boxes, like real ODM gear.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float GearTilt = 12.f;
	// Stretch: X widens the gap between the blade boxes so they clear the thighs.
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") FVector GearScale = FVector(1.2f, 1.f, 1.f);
	// Where the rope comes out of each launcher nozzle, in the GEAR's own directions (cm).
	// (With no gear, these are measured from the belt instead.)
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float LauncherSpacing = 15.f;  // sideways from the middle
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float LauncherForward = -7.5f; // forward (minus = behind the hip bone)
	UPROPERTY(EditAnywhere, Category = "Grapple|Body") float LauncherHeight = 2.f;    // up

	// ---- Testing ----
	// When ticked, every frame of play is written to a spreadsheet file (CSV) in
	// Saved/Tests/ when you stop playing: time, position, speed, rope lengths, gas, and
	// what the body last bumped into. These files are the measured evidence for the
	// tests in Docs/TESTS.md (for example, "does the rope ever stretch?").
	UPROPERTY(EditAnywhere, Category = "Grapple|Testing") bool bRecordTelemetry = true;

	// Runs a scripted, repeatable physics test high in the empty sky, with no player input,
	// so the result can be compared exactly with the offline model (Tools/rope_model.py).
	// Start it from the in-game console (press ` while playing):  gr.Test snap stiff
	//   Name: "snap" = hook fired while falling away at 17 m/s; "reel" = hang, then reel in;
	//         "catch" = slack 13.3 m rope caught at 22 m/s (the F6 case from test T-11);
	//         "swing" = let go from level with the anchor on a 15 m rope (a pendulum);
	//         "swingreal" = the same pendulum with SwingExaggeration switched off (real physics);
	//         "perf" = stand at the start looking down the route for 10 s and time every frame.
	//   SoftRope: 0 = stiff rope, 1 = soft rope, -1 = keep the current setting.
	void StartTest(const FString& Name, int32 SoftRope);

	// For other parts of the game (the endless spawner): add a new grapple rock to aim assist,
	// a new vine to the snap check, and read where the player is.
	void AddAnchor(AActor* Anchor) { Anchors.Add(Anchor); }
	void AddVine(class AGRVine* Vine);
	FVector GetBodyPosition() const { return BodyPos; }

	// For the weapons (GRArsenal): where your eyes and hands are, and how you're moving.
	UCameraComponent* GetEyes() const;                 // (written out in the .cpp)
	USceneComponent* GetHand(int32 Index) const;
	APlayerController* GetPlayerController() const;
	bool IsUsingVR() const { return bVR; }
	bool IsGrounded() const { return bGrounded; }
	FVector GetBodyVelocity() const { return Velocity; }
	const TArray<TWeakObjectPtr<class AGRVine>>& GetVines() const { return Vines; }
	// Desktop: with a weapon out, the mouse buttons fire and aim, so the hooks move to Q and E.
	// With empty hands (weapon put away), the mouse buttons fire the hooks again.
	void SetMouseFiresHooks(bool bHooks) { bMouseHooks = bHooks; }
	// Recoil: nudge where you're looking (degrees up, degrees right). Desktop only; in VR your
	// head is never moved for you (that causes motion sickness).
	void AddLookKick(float UpDegrees, float RightDegrees)
	{
		if (!bVR)
		{
			DesktopPitch = FMath::Clamp(DesktopPitch + UpDegrees, -85.f, 85.f);
			DesktopYaw += RightDegrees;
		}
	}

protected:
	// BeginPlay runs ONCE when you press Play. We use it to find grapple rocks and controller buttons.
	virtual void BeginPlay() override;

	// EndPlay runs ONCE when you stop playing. We use it to save the test recording.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderAsset;
	UPROPERTY() TObjectPtr<class UAudioComponent> WindAudio;      // plays the wind sound (see UpdateWind)
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;          // your visible body (see UpdateBody)
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Harness;         // the grapple gear on your hips
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Gear;            // the ODM gear (reels, blade boxes, tanks)
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ThighHarnessL;   // leg loop + blade box on the left thigh
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ThighHarnessR;   // ... and on the right thigh
	UPROPERTY() TObjectPtr<UAnimSequence> BodyAnimPlaying;        // which animation the body is playing now
	float WindLevel = 0.f;                                          // 0..1, smoothed, how windy it is right now              // the cylinder shape each rope link is drawn with
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> RopeLinkMeshes; // left rope's links, then right rope's links

	// The flexible rope: the corners of each link, now and one frame ago (see SimulateRopeShape).
	TArray<FVector> RopePoints[2];
	TArray<FVector> RopePointsBefore[2];
	bool bRopeShapeReady[2] = {false, false};      // has this rope's chain been laid out since it was fired?
	float RopeTension[2] = {0.f, 0.f};             // how hard each rope is pulling you this frame (newtons)
	float PulseUntil[2] = {0.f, 0.f};              // game time until which a short "hook bit" buzz is playing

	// "Weak" pointers = "I'm looking at this object, but if it disappears, don't crash, just tell me".
	TWeakObjectPtr<APlayerController> PC;       // reads the player's buttons
	TWeakObjectPtr<APawn> Pawn;                 // the player's body
	TWeakObjectPtr<UCameraComponent> Camera;    // the player's eyes / headset
	TWeakObjectPtr<USceneComponent> Hands[2];   // left and right VR controllers
	TArray<TWeakObjectPtr<AActor>> Anchors;     // every rock tagged "GrappleAnchor" (for aim assist)
	TArray<TWeakObjectPtr<class AGRVine>> Vines; // every vine in the level (they snap when you fly through)

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
	bool bMouseHooks = true;                       // desktop: do the mouse buttons fire the hooks? (no when a weapon is out)

	// Test recording (see bRecordTelemetry above)
	FString Telemetry;                             // the spreadsheet rows collected so far, as text
	float TelemetryClock = 0.f;                    // seconds since Play was pressed
	FString LastHitName = TEXT("none");            // what the body bumped into on the latest physics step
	bool bLastHitPenetrating = false;              // did that bump START inside the object? (a sign of a collision bug)
	FString LastReportedPenetration;               // so the "started inside" warning is logged once per object, not every frame

	// Scripted test in progress (see StartTest)
	FString TestName = TEXT("none");               // which test is running ("none" = normal play)
	float TestTimeLeft = 0.f;                      // seconds until the test ends
	bool bTestReel = false;                        // does this test hold the reel button?
	bool bSoftRopeBeforeTest = true;               // so the rope setting can be put back afterwards
	float SwingExaggerationBeforeTest = 1.f;       // likewise for the swing exaggeration
	float TestPeakFeltG = 0.f;                     // biggest jolt during the test, in g
	float TestMaxStretch = 0.f;                    // most the rope stretched during the test, in cm
	TArray<float> TestFrameTimes;                  // perf test: how long each frame took (seconds)
	void EndTest();                                // finish the test, report, and respawn

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
	void SnapVines(const FVector& From, const FVector& To);   // break any vine the body flew through this frame
	void SimulateRopeShape(int32 Index, float DeltaSeconds); // move the rope's links (sag, swing, whip)
	void FeelRopeTension();                           // vibrate each controller by how hard its rope pulls
	void UpdateWind(float DeltaSeconds);              // wind sound louder / higher the faster you move
	void SetUpBody();                                 // create the visible body and harness (once)
	void UpdateBody();                                // keep the body under your eyes and pick its animation
	UStaticMeshComponent* StrapToBone(UStaticMesh* Mesh, FName Bone, const FVector& OffsetCm, const FQuat& Turn, const FVector& Stretch = FVector::OneVector); // fix one harness piece to a bone
	FVector RopeStart(int32 Index) const;             // where a rope leaves the body (harness launcher, or hand)
	void RecordTelemetry(float DeltaSeconds);         // add this frame as one row of the test spreadsheet
	void SaveTelemetry();                             // write the spreadsheet to Saved/Tests/

	void GetAim(int32 Index, FVector& OutStart, FVector& OutDir) const;   // where a hand is pointing
	bool FindTarget(int32 Index, FVector& OutPoint) const;               // what a hook would hit
	FVector HandPosition(int32 Index) const;                             // where the rope starts
	FVector BodyCenter() const { return BodyPos + FVector(0.f, 0.f, BodyHalfHeight); } // middle of the body (feet + half height)

	bool AnyDown(const TArray<FKey>& Keys) const;                    // is ANY of these buttons pressed?
	bool AnyAxis(const TArray<FKey>& Keys, float Threshold) const;   // is ANY of these triggers squeezed past a point?
	static void AddKeys(TArray<FKey>& Out, std::initializer_list<const TCHAR*> Names); // build a button list
};
