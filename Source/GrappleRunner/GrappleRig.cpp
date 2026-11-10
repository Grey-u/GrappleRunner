// =====================================================================================
//  GrappleRig.cpp  -  HOW the grapple system works, step by step
// =====================================================================================
//
//  Read GrappleRig.h first: it lists every setting and ability. This file is the
//  "kitchen" that does the actual work.
//
//  READING TIPS FOR NON-CODERS
//  ---------------------------
//  * Lines starting with // are COMMENTS: notes for humans, the computer ignores them.
//  * A FUNCTION is a named recipe, e.g. "void AGrappleRig::FireHook(...) { ... }".
//    Everything between its { curly brackets } are the steps, run top to bottom.
//  * "if (something) { ... }" = only do the steps inside if "something" is true.
//  * "for (...) { ... }" = repeat the steps inside, once per item.
//  * "return" = stop this recipe now (and maybe hand back an answer).
//  * Index 0 means LEFT hand, index 1 means RIGHT hand, everywhere in this file.
//  * FVector = an arrow / point in 3D space with X (forward), Y (right), Z (up).
//
//  THE BIG IDEA: HOW A ROPE SWING IS SIMULATED
//  ------------------------------------------
//  Every tiny slice of time (1/120th of a second) we:
//    1. Pull the player down with gravity and slow them a little with air drag.
//    2. Move them by their speed.
//    3. If they are now further from a hook than the rope length, put them back ON
//       the rope's circle and cancel the part of their speed that was stretching the rope.
//       Step 3 is all a rope is: "you can't go further than L from the anchor".
//       Doing that over and over automatically produces a realistic pendulum swing.
//       COMFORT VERSION (bSoftRope, on by default since 9 Nov): before step 2, a rope
//       that is stretched past its length pulls back like a stiff spring, so the jolt
//       of it snapping tight is spread over ~1/8 s instead of one frame. Step 3 then
//       only acts as a safety limit (MaxRopeStretch). See test T-09 in Docs/TESTS.md.
//    4. Check they didn't go through a rock (if they did, stop them at its surface).
//  This is the same model as the 2D physics test bench (Tools/physics-bench.html).
// =====================================================================================

#include "GrappleRig.h"

// The real definitions of everything we use (the .h only promised they exist):
#include "Camera/CameraComponent.h"            // the player's eyes
#include "Components/AudioComponent.h"         // plays a sound (the wind)
#include "Components/SkeletalMeshComponent.h"  // a character body with bones and animation
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundBase.h"
#include "Components/StaticMeshComponent.h"    // visible 3D shapes
#include "DrawDebugHelpers.h"                  // quick on-screen dots for aiming
#include "Engine/Engine.h"                     // GEngine: global engine access (on-screen text, VR check)
#include "Engine/StaticMesh.h"
#include "Engine/World.h"                      // the level: lets us ray-trace and sweep
#include "EngineUtils.h"                       // TActorIterator: "go through every actor of this type"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"         // the spawn point marker
#include "HAL/FileManager.h"                   // make folders on disk (for the test recordings)
#include "HAL/IConsoleManager.h"               // add our own console command (gr.Test)
#include "IXRTrackingSystem.h"                 // asks "is a VR headset active?"
#include "Kismet/GameplayStatics.h"            // handy helpers like "find all actors with this tag"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DateTime.h"                     // today's date and time (to name each recording)
#include "Misc/FileHelper.h"                   // save text to a file
#include "Misc/Paths.h"                        // where the project's Saved folder is
#include "TimerManager.h"                      // "do this after 0.08 seconds" (stopping vibration)
#include "UObject/ConstructorHelpers.h"        // load engine shapes when the object is built

// =====================================================================================
//  CONSTRUCTOR: runs once when the GrappleRig object is created (even in the editor).
//  Job: build the 4 shapes we draw: a rope (cylinder) and a hook head (sphere) per hand.
// =====================================================================================
AGrappleRig::AGrappleRig()
{
	// Ask Unreal to call our Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;

	// Every actor needs a "root": an invisible point everything else hangs off.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Load Unreal's built-in cylinder and sphere shapes (they ship with the engine).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// A small helper recipe (a "lambda") that makes one shape with the same settings each time.
	auto MakeMesh = [this, Root](const TCHAR* Name, UStaticMesh* Mesh) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		if (Mesh)
		{
			C->SetStaticMesh(Mesh);
		}
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision); // ropes must never block the player
		C->SetCastShadow(false);                                // ropes don't need shadows
		// "Absolute" = we give this shape world positions directly, ignoring where the rig sits.
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->SetHiddenInGame(true);                               // invisible until a hook is fired
		return C;
	};
	CylinderAsset = CylinderMesh.Object;   // kept so BeginPlay can build the rope's links from it
	RopeL = MakeMesh(TEXT("RopeLeft"), CylinderMesh.Object);
	RopeR = MakeMesh(TEXT("RopeRight"), CylinderMesh.Object);
	HeadL = MakeMesh(TEXT("HookLeft"), SphereMesh.Object);
	HeadR = MakeMesh(TEXT("HookRight"), SphereMesh.Object);
}

// =====================================================================================
//  AddKeys: turn a list of button NAMES into real button objects, skipping any name
//  this computer doesn't know (e.g. Quest button names when only Vive support is loaded).
// =====================================================================================
void AGrappleRig::AddKeys(TArray<FKey>& Out, std::initializer_list<const TCHAR*> Names)
{
	for (const TCHAR* N : Names)          // for each name in the list...
	{
		FKey K{FName(N)};                 // ...turn the text into a button
		if (K.IsValid())                  // ...keep it only if Unreal recognises it
		{
			Out.Add(K);
		}
	}
}

// =====================================================================================
//  BeginPlay: runs once when you press Play.
//  Job: learn the controller buttons, find the grapple rocks, work out the death height.
// =====================================================================================
void AGrappleRig::BeginPlay()
{
	Super::BeginPlay();   // let Unreal do its own start-up first

	// ---- 1. Controller buttons ----
	// "Click" = the button is pressed. "Axis" = how far the trigger is squeezed (0 to 1).
	// We list the Vive (Naomi's target headset) first, then Quest, Index, Windows MR and Pico,
	// so the game works on any OpenXR headset.
	AddKeys(TriggerKeys[0], {TEXT("Vive_Left_Trigger_Click"), TEXT("OculusTouch_Left_Trigger_Click"), TEXT("ValveIndex_Left_Trigger_Click"), TEXT("MixedReality_Left_Trigger_Click"), TEXT("PicoTouch_Left_Trigger_Click")});
	AddKeys(TriggerKeys[1], {TEXT("Vive_Right_Trigger_Click"), TEXT("OculusTouch_Right_Trigger_Click"), TEXT("ValveIndex_Right_Trigger_Click"), TEXT("MixedReality_Right_Trigger_Click"), TEXT("PicoTouch_Right_Trigger_Click")});
	AddKeys(TriggerAxes[0], {TEXT("Vive_Left_Trigger_Axis"), TEXT("OculusTouch_Left_Trigger_Axis"), TEXT("ValveIndex_Left_Trigger_Axis"), TEXT("MixedReality_Left_Trigger_Axis")});
	AddKeys(TriggerAxes[1], {TEXT("Vive_Right_Trigger_Axis"), TEXT("OculusTouch_Right_Trigger_Axis"), TEXT("ValveIndex_Right_Trigger_Axis"), TEXT("MixedReality_Right_Trigger_Axis")});
	AddKeys(GripKeys[0], {TEXT("Vive_Left_Grip_Click"), TEXT("OculusTouch_Left_Grip_Click"), TEXT("MixedReality_Left_Grip_Click")});
	AddKeys(GripKeys[1], {TEXT("Vive_Right_Grip_Click"), TEXT("OculusTouch_Right_Grip_Click"), TEXT("MixedReality_Right_Grip_Click")});
	AddKeys(GripAxes[0], {TEXT("OculusTouch_Left_Grip_Axis"), TEXT("ValveIndex_Left_Grip_Axis")});
	AddKeys(GripAxes[1], {TEXT("OculusTouch_Right_Grip_Axis"), TEXT("ValveIndex_Right_Grip_Axis")});
	AddKeys(BoostKeys, {TEXT("Vive_Left_Trackpad_Click"), TEXT("Vive_Right_Trackpad_Click"), TEXT("OculusTouch_Left_X_Click"), TEXT("OculusTouch_Right_A_Click"), TEXT("ValveIndex_Left_A_Click"), TEXT("ValveIndex_Right_A_Click")});

	// ---- 2. Grapple rocks ----
	// The level builder script tagged every anchor rock with the word "GrappleAnchor".
	// We collect them all so aim assist can snap to the nearest one.
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("GrappleAnchor")), Found);
	for (AActor* A : Found)
	{
		Anchors.Add(A);
	}

	// ---- 3. Death height ----
	// Find the bottom of the lowest landing island, then go 40 m further down.
	// Fall below that = you've dropped into the mist, so respawn.
	TArray<AActor*> Islands;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("LandingIsland")), Islands);
	float LowestZ = TNumericLimits<float>::Max();   // start with "infinitely high"
	for (AActor* A : Islands)
	{
		FVector Origin, Extent;                      // Origin = centre of the island, Extent = half its size
		A->GetActorBounds(true, Origin, Extent);
		LowestZ = FMath::Min(LowestZ, static_cast<float>(Origin.Z - Extent.Z)); // keep the lowest bottom found
	}
	KillZ = Islands.Num() > 0 ? LowestZ - KillDepthBelowIslands : -20000.f;   // fallback if no islands are tagged

	// ---- 4. Respawn point = the level's PlayerStart marker ----
	if (AActor* Start = UGameplayStatics::GetActorOfClass(GetWorld(), APlayerStart::StaticClass()))
	{
		RespawnPoint = Start->GetActorLocation();
	}

	// ---- 5. Build the flexible ropes: a chain of short cylinder "links" per hand ----
	// (The single straight cylinders RopeL / RopeR are kept only as a fallback and stay hidden.)
	RopeLinks = FMath::Clamp(RopeLinks, 2, 64);
	for (int32 k = 0; k < 2 * RopeLinks; ++k)
	{
		UStaticMeshComponent* Link = NewObject<UStaticMeshComponent>(this);
		Link->SetStaticMesh(CylinderAsset);
		Link->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // ropes never block anything
		Link->SetCastShadow(false);
		Link->SetUsingAbsoluteLocation(true);
		Link->SetUsingAbsoluteRotation(true);
		Link->SetUsingAbsoluteScale(true);
		Link->SetupAttachment(RootComponent);
		Link->RegisterComponent();                                    // make it part of the world
		Link->SetHiddenInGame(true);
		RopeLinkMeshes.Add(Link);
	}

	// ---- 6. Colour the ropes orange ----
	// Test T-03 found the engine's default shape material has no "Color" setting, so the
	// ropes came out grey. Scripts/make_rope_material.py makes M_Rope, which does have one.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GrappleRunner/Materials/M_Rope.M_Rope"));
	if (!Base && RopeL)
	{
		Base = RopeL->GetMaterial(0);   // fallback: the grey default
	}
	if (Base)
	{
		RopeMaterial = UMaterialInstanceDynamic::Create(Base, this);
		RopeMaterial->SetVectorParameterValue(TEXT("Color"), RopeColor);
		for (UStaticMeshComponent* C : {RopeL.Get(), RopeR.Get(), HeadL.Get(), HeadR.Get()})
		{
			C->SetMaterial(0, RopeMaterial);
		}
		for (UStaticMeshComponent* C : RopeLinkMeshes)
		{
			C->SetMaterial(0, RopeMaterial);
		}
	}
	// ---- 7. Visible body and hip harness ----
	SetUpBody();

	// ---- 8. Wind sound (made by Tools/make_wind_sound.py, imported by Scripts/import_sfx.py) ----
	if (USoundBase* Wind = LoadObject<USoundBase>(nullptr, TEXT("/Game/GrappleRunner/SFX/SFX_Wind.SFX_Wind")))
	{
		WindAudio = NewObject<UAudioComponent>(this);
		WindAudio->SetSound(Wind);
		WindAudio->bAllowSpatialization = false;   // heard "in your ears", not from a point in the world
		WindAudio->SetupAttachment(RootComponent);
		WindAudio->RegisterComponent();
		WindAudio->SetVolumeMultiplier(0.001f);    // silent until you move
		WindAudio->Play();
	}

	Gas = GasMax;   // start with a full tank

	// Write a note in the Output Log so we can check the rig found everything.
	UE_LOG(LogTemp, Log, TEXT("GrappleRig: %d anchors, kill Z %.0f"), Anchors.Num(), KillZ);
}

// =====================================================================================
//  EnsurePlayer: make sure we know who the player is. Returns false if there's no player yet.
//  Runs every frame, but only does real work the first time (or if the player changes).
// =====================================================================================
bool AGrappleRig::EnsurePlayer()
{
	UWorld* World = GetWorld();
	APlayerController* NewPC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* NewPawn = NewPC ? NewPC->GetPawn() : nullptr;
	if (!NewPawn)
	{
		return false;   // the player hasn't spawned yet: try again next frame
	}

	// Is a VR headset actively tracking the player's head right now?
	bVR = GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();

	if (NewPawn != Pawn.Get())   // a new player body: look it over
	{
		PC = NewPC;
		Pawn = NewPawn;
		Camera = NewPawn->FindComponentByClass<UCameraComponent>();
		Hands[0] = nullptr;
		Hands[1] = nullptr;

		// Find the VR hand controllers. The Unreal VR template names them like
		// "MotionControllerLeftAim" and "MotionControllerLeftGrip". The "Aim" one points
		// where the controller points, so we prefer it.
		TArray<USceneComponent*> Comps;
		NewPawn->GetComponents<USceneComponent>(Comps);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const TCHAR* SideName = Side == 0 ? TEXT("Left") : TEXT("Right");
			for (USceneComponent* C : Comps)
			{
				const FString N = C->GetName();
				if (N.Contains(TEXT("MotionController")) && N.Contains(SideName))
				{
					if (!Hands[Side].IsValid() || N.Contains(TEXT("Aim")))
					{
						Hands[Side] = C;
					}
				}
			}
		}
		// Start the physics from wherever the player currently stands.
		BodyPos = NewPawn->GetActorLocation();
		LastSetLocation = BodyPos;
		if (RespawnPoint.IsZero())
		{
			RespawnPoint = BodyPos;
		}
		Velocity = FVector::ZeroVector;
		DesktopYaw = NewPawn->GetActorRotation().Yaw;
		bDesktopSetupDone = false;
	}

	// No headset? Set up a normal first-person camera at eye height, and hide the mouse
	// cursor so the mouse can be used to look around.
	if (!bVR && !bDesktopSetupDone)
	{
		if (Camera.IsValid())
		{
			Camera->SetRelativeLocation(FVector(0.f, 0.f, DesktopEyeHeight));
		}
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		bDesktopSetupDone = true;
	}
	return true;
}

// Is ANY button in this list held down?
bool AGrappleRig::AnyDown(const TArray<FKey>& Keys) const
{
	for (const FKey& K : Keys)
	{
		if (PC->IsInputKeyDown(K))
		{
			return true;
		}
	}
	return false;
}

// Is ANY trigger in this list squeezed further than Threshold (0 = not at all, 1 = fully)?
bool AGrappleRig::AnyAxis(const TArray<FKey>& Keys, float Threshold) const
{
	for (const FKey& K : Keys)
	{
		if (PC->GetInputAnalogKeyState(K) > Threshold)
		{
			return true;
		}
	}
	return false;
}

// =====================================================================================
//  ReadInput: read every button / mouse movement for this frame and react.
// =====================================================================================
void AGrappleRig::ReadInput(float DeltaSeconds)
{
	APlayerController* P = PC.Get();
	bool bHookDown[2];   // is each hand's "fire hook" button held this frame?

	if (bVR)
	{
		// ---- VR controllers ----
		for (int32 i = 0; i < 2; ++i)
		{
			bHookDown[i] = AnyDown(TriggerKeys[i]) || AnyAxis(TriggerAxes[i], 0.6f); // trigger more than 60% squeezed
			const bool bGrip = AnyDown(GripKeys[i]) || AnyAxis(GripAxes[i], 0.6f);
			// Auto-pull (AoT style): the hook reels you in by itself; squeeze grip to STOP
			// pulling and swing freely. With auto-pull off, it's the other way round.
			bReel[i] = bAutoPull ? !bGrip : bGrip;
		}
		bBoost = AnyDown(BoostKeys);
		MoveInput = FVector::ZeroVector;   // in VR you move with the grapple (and the template's teleport)
	}
	else
	{
		// ---- Mouse and keyboard (testing without a headset) ----
		bHookDown[0] = P->IsInputKeyDown(EKeys::LeftMouseButton);
		bHookDown[1] = P->IsInputKeyDown(EKeys::RightMouseButton);
		const bool bShift = P->IsInputKeyDown(EKeys::LeftShift);
		// Auto-pull on: hooks reel you in by themselves, Shift = stop pulling and swing.
		// Auto-pull off: Shift = reel in.
		bReel[0] = bReel[1] = bAutoPull ? !bShift : bShift;

		// Mouse look: how far did the mouse move since last frame?
		float Dx = 0.f, Dy = 0.f;
		P->GetInputMouseDelta(Dx, Dy);
		DesktopYaw += Dx * MouseSensitivity;                                          // left / right
		DesktopPitch = FMath::Clamp(DesktopPitch + Dy * MouseSensitivity, -85.f, 85.f); // up / down, but never flip over
		Pawn->SetActorRotation(FRotator(0.f, DesktopYaw, 0.f));      // turn the whole body left/right
		if (Camera.IsValid())
		{
			Camera->SetRelativeRotation(FRotator(DesktopPitch, 0.f, 0.f)); // tilt only the head up/down
		}

		// WASD walking, relative to where you are facing.
		const FRotator YawRot(0.f, DesktopYaw, 0.f);
		const FVector Fwd = YawRot.Vector();                               // arrow pointing forward
		const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y); // arrow pointing right
		FVector Move = FVector::ZeroVector;
		if (P->IsInputKeyDown(EKeys::W)) Move += Fwd;
		if (P->IsInputKeyDown(EKeys::S)) Move -= Fwd;
		if (P->IsInputKeyDown(EKeys::D)) Move += Right;
		if (P->IsInputKeyDown(EKeys::A)) Move -= Right;
		MoveInput = Move.GetClampedToMaxSize(1.f);   // diagonal walking isn't faster than straight

		// Space: jump if on the ground (only on the moment it's pressed), boost if in the air (while held).
		const bool bSpace = P->IsInputKeyDown(EKeys::SpaceBar);
		if (bSpace && !bJumpWasDown && bGrounded)
		{
			Velocity.Z = JumpSpeed;
			bGrounded = false;
		}
		bBoost = bSpace && !bGrounded;
		bJumpWasDown = bSpace;

		// R: respawn (only once per press).
		const bool bR = P->IsInputKeyDown(EKeys::R);
		if (bR && !bRespawnWasDown)
		{
			Respawn();
		}
		bRespawnWasDown = bR;
	}

	// Hooks: compare this frame with last frame to spot the moment of PRESS or RELEASE.
	//   pressed now + not pressed before  = just pressed  -> fire
	//   not pressed now + pressed before  = just released -> let go
	for (int32 i = 0; i < 2; ++i)
	{
		if (bHookDown[i] && !Hooks[i].bWasDown)
		{
			FireHook(i);
		}
		else if (!bHookDown[i] && Hooks[i].bWasDown)
		{
			ReleaseHook(i);
		}
		Hooks[i].bWasDown = bHookDown[i];
	}
}

// =====================================================================================
//  HandPosition: where should the drawn rope START for this hand?
//  VR: the real controller. Desktop: a point just below and beside the camera, like a hip launcher.
// =====================================================================================
FVector AGrappleRig::HandPosition(int32 Index) const
{
	if (bVR && Hands[Index].IsValid())
	{
		return Hands[Index]->GetComponentLocation();
	}
	if (Camera.IsValid())
	{
		const FRotator R = Camera->GetComponentRotation();
		const FVector Right = FRotationMatrix(R).GetUnitAxis(EAxis::Y);
		// 30 cm forward, 28 cm to the side (left hand = minus), 45 cm down
		return Camera->GetComponentLocation() + R.Vector() * 30.f + Right * (Index == 0 ? -28.f : 28.f) - FVector(0.f, 0.f, 45.f);
	}
	return BodyCenter();
}

// =====================================================================================
//  GetAim: where does this hand's hook shoot FROM, and in which DIRECTION?
// =====================================================================================
void AGrappleRig::GetAim(int32 Index, FVector& OutStart, FVector& OutDir) const
{
	if (bVR && Hands[Index].IsValid())
	{
		// VR: straight out of the controller, the way it's pointing.
		OutStart = Hands[Index]->GetComponentLocation();
		OutDir = Hands[Index]->GetForwardVector();
		return;
	}
	// Desktop: from the camera, where you're looking, with the left/right hooks angled
	// slightly outward so they don't land on exactly the same spot.
	const FVector CamLoc = Camera.IsValid() ? Camera->GetComponentLocation() : BodyPos + FVector(0.f, 0.f, DesktopEyeHeight);
	const FRotator CamRot = Camera.IsValid() ? Camera->GetComponentRotation() : FRotator(DesktopPitch, DesktopYaw, 0.f);
	const FVector Fwd = CamRot.Vector();
	const FVector Right = FRotationMatrix(CamRot).GetUnitAxis(EAxis::Y);
	OutStart = CamLoc + Fwd * 20.f;
	OutDir = (Fwd + Right * (Index == 0 ? -DesktopHookSpread : DesktopHookSpread)).GetSafeNormal();
}

// =====================================================================================
//  FindTarget: if this hook fired now, where would it stick? Returns false if nowhere.
// =====================================================================================
bool AGrappleRig::FindTarget(int32 Index, FVector& OutPoint) const
{
	UWorld* World = GetWorld();
	FVector Start, Dir;
	GetAim(Index, Start, Dir);

	// Settings for the "ray trace" (an invisible laser): ignore the rig itself and the player's body.
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GrappleAim), true);
	Q.AddIgnoredActor(this);
	Q.AddIgnoredActor(Pawn.Get());

	// Attempt 1: fire an invisible laser straight ahead, up to MaxFireRange. Did it hit anything solid?
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, Start + Dir * MaxFireRange, ECC_Visibility, Q))
	{
		OutPoint = Hit.ImpactPoint;   // yes: the hook sticks exactly there
		return true;
	}

	// Attempt 2 (aim assist): missed everything? Look for the grapple rock closest to where
	// you're pointing, within a small cone (AimAssistDegrees), and snap to it.
	// "Cos" of an angle: 1 = pointing exactly at it, smaller = further off. Bigger is better.
	float BestCos = FMath::Cos(FMath::DegreesToRadians(AimAssistDegrees));
	bool bFound = false;
	for (const TWeakObjectPtr<AActor>& W : Anchors)
	{
		AActor* A = W.Get();
		if (!A)
		{
			continue;   // this rock was deleted: skip it
		}
		FVector Origin, Extent;
		A->GetActorBounds(true, Origin, Extent);   // the rock's centre
		const FVector To = Origin - Start;         // arrow from the hand to the rock
		const float Dist = To.Size();
		if (Dist < 100.f || Dist > MaxFireRange)
		{
			continue;   // too close or out of range
		}
		// Dot product = "how much do these two directions agree?" (1 = same way)
		const float C = FVector::DotProduct(To / Dist, Dir);
		if (C > BestCos)   // closer to where we're aiming than anything so far
		{
			// Aim at the rock's centre and use the first surface the laser hits, so the hook sits ON the rock.
			FHitResult H2;
			OutPoint = World->LineTraceSingleByChannel(H2, Start, Origin, ECC_Visibility, Q) ? FVector(H2.ImpactPoint) : Origin;
			BestCos = C;
			bFound = true;
		}
	}
	return bFound;
}

// =====================================================================================
//  FireHook: shoot hook 0 (left) or 1 (right).
// =====================================================================================
void AGrappleRig::FireHook(int32 Index)
{
	FVector Point;
	if (!FindTarget(Index, Point))
	{
		Pulse(Index, 0.15f);   // nothing in range: tiny buzz so you feel the miss
		return;
	}
	FGrappleHook& H = Hooks[Index];
	H.bAttached = true;
	H.Anchor = Point;
	// The rope starts exactly as long as the distance to the anchor (no sudden yank),
	// but never shorter than the minimum.
	H.Length = FMath::Max(MinRopeLength, static_cast<float>(FVector::Dist(BodyCenter(), Point)));
	Pulse(Index, 0.7f);   // strong buzz: the hook has bitten
}

// Let go of a hook: from now on that rope no longer holds you.
void AGrappleRig::ReleaseHook(int32 Index)
{
	Hooks[Index].bAttached = false;
}

// =====================================================================================
//  Pulse: vibrate a VR controller for 0.08 seconds. Amplitude: 0 = nothing, 1 = full.
// =====================================================================================
void AGrappleRig::Pulse(int32 Index, float Amplitude)
{
	APlayerController* P = PC.Get();
	if (!bVR || !P)
	{
		return;   // no controllers to vibrate on desktop
	}
	const EControllerHand Hand = Index == 0 ? EControllerHand::Left : EControllerHand::Right;
	P->SetHapticsByValue(0.6f, Amplitude, Hand);   // start vibrating
	PulseUntil[Index] = GetWorld()->GetTimeSeconds() + 0.08f;   // FeelRopeTension leaves this hand alone until then
	// Set a timer to switch the vibration off again after 0.08 seconds.
	FTimerHandle Handle;
	TWeakObjectPtr<APlayerController> WeakPC = P;
	GetWorldTimerManager().SetTimer(Handle, [WeakPC, Hand]()
	{
		if (WeakPC.IsValid())
		{
			WeakPC->SetHapticsByValue(0.f, 0.f, Hand);
		}
	}, 0.08f, false);
}

// =====================================================================================
//  Respawn: put the player back at the start, standing still, hooks off, full gas.
// =====================================================================================
void AGrappleRig::Respawn()
{
	BodyPos = RespawnPoint + FVector(0.f, 0.f, 20.f);   // 20 cm above the spawn so we don't start inside the ground
	Velocity = FVector::ZeroVector;
	Hooks[0].bAttached = Hooks[1].bAttached = false;
	bGrounded = false;
	Gas = GasMax;
}

// =====================================================================================
//  Tick: the heartbeat. Runs every frame.
// =====================================================================================
void AGrappleRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!EnsurePlayer())
	{
		return;   // no player yet
	}

	// Did something else move the player (e.g. the VR template's teleport)? If the player is
	// more than 60 cm from where we left them, accept the new position and start fresh.
	const FVector Now = Pawn->GetActorLocation();
	if (FVector::DistSquared(Now, LastSetLocation) > FMath::Square(60.f))
	{
		BodyPos = Now;
		Velocity = FVector::ZeroVector;
		Hooks[0].bAttached = Hooks[1].bAttached = false;
	}

	if (TestTimeLeft > 0.f)
	{
		// A scripted test is running: ignore the controllers/mouse so every run is identical.
		MoveInput = FVector::ZeroVector;
		bBoost = false;
		bReel[0] = bTestReel;
		bReel[1] = false;
	}
	else
	{
		ReadInput(DeltaSeconds);
	}

	// FIXED-STEP PHYSICS
	// Frames can be uneven (11 ms, then 14 ms...). Physics is more stable if it always moves
	// in identical small steps, so we save up the frame time in a "bucket" (Accumulator) and
	// run as many 1/120 s steps as fit. Leftover time waits for next frame.
	// The bucket is capped at 0.1 s, so a big hitch can't launch the player across the map.
	const float Step = 1.f / 120.f;   // same fixed step as the physics bench
	Accumulator = FMath::Min(Accumulator + DeltaSeconds, 0.1f);
	while (Accumulator >= Step)
	{
		const FVector VelocityBefore = Velocity;
		Simulate(Step);
		Accumulator -= Step;
		if (TestTimeLeft > 0.f && TestName.StartsWith(TEXT("perf")))
		{
			TestTimeLeft -= Step;            // perf test: just count down (frames are recorded below)
			if (TestTimeLeft <= 0.f)
			{
				EndTest();
			}
		}
		else if (TestTimeLeft > 0.f)
		{
			// During a test, record EVERY physics step (not every frame), so the data lines up
			// step-for-step with the offline model. "Felt" acceleration = the change in velocity
			// minus gravity's share, because your inner ear can't feel gravity while falling.
			const FVector Felt = (Velocity - VelocityBefore) / Step + FVector(0.f, 0.f, Gravity);
			TestPeakFeltG = FMath::Max(TestPeakFeltG, static_cast<float>(Felt.Size() / Gravity));
			if (Hooks[0].bAttached)
			{
				TestMaxStretch = FMath::Max(TestMaxStretch,
					static_cast<float>(FVector::Dist(BodyCenter(), Hooks[0].Anchor)) - Hooks[0].Length);
			}
			RecordTelemetry(Step);
			RopeTension[0] = RopeTension[1] = 0.f;   // each recorded step shows only its own pull
			TestTimeLeft -= Step;
			if (TestTimeLeft <= 0.f)
			{
				EndTest();
			}
		}
	}

	if (BodyPos.Z < KillZ)
	{
		Respawn();   // fell into the mist
	}
	// Actually move the player's body to the new position.
	Pawn->SetActorLocation(BodyPos, false, nullptr, ETeleportType::TeleportPhysics);
	LastSetLocation = Pawn->GetActorLocation();

	UpdateBody();                        // body follows your eyes, plays idle / run / air
	UpdateVisuals();
	FeelRopeTension();                   // VR: vibrate the controllers by how hard each rope pulls
	UpdateWind(DeltaSeconds);            // wind sound follows your speed
	if (TestName.StartsWith(TEXT("perf")))
	{
		TestFrameTimes.Add(DeltaSeconds);    // perf test: remember how long this frame took
	}
	if (TestTimeLeft <= 0.f || TestName.StartsWith(TEXT("perf")))
	{
		RecordTelemetry(DeltaSeconds);   // normal play: write down this frame for the test log
	}
	RopeTension[0] = RopeTension[1] = 0.f;   // measured afresh every frame
}

// =====================================================================================
//  Simulate: ONE small physics step of Dt seconds (1/120 s). See "THE BIG IDEA" at the top.
// =====================================================================================
void AGrappleRig::Simulate(float Dt)
{
	const bool bAnyHook = Hooks[0].bAttached || Hooks[1].bAttached;
	const FVector C = BodyCenter();   // where the body is right now
	float StepTension[2] = {0.f, 0.f};   // how hard each rope pulls during THIS step (newtons), added up below

	if (bGrounded && !bAnyHook)
	{
		// ---- On the ground, no hooks: walking ----
		// Smoothly change speed toward the WASD direction (FInterpTo = "ease toward", 10 = how quickly).
		const FVector Wish = MoveInput * WalkSpeed;
		Velocity.X = FMath::FInterpTo(static_cast<float>(Velocity.X), static_cast<float>(Wish.X), Dt, 10.f);
		Velocity.Y = FMath::FInterpTo(static_cast<float>(Velocity.Y), static_cast<float>(Wish.Y), Dt, 10.f);
		Velocity.Z = FMath::Max(Velocity.Z, 0.0);                       // don't sink into the floor
		Gas = FMath::Min(GasMax, Gas + GasRegenPerSecond * Dt);         // refill gas while standing
	}
	else
	{
		// ---- In the air (or swinging) ----
		Velocity.Z -= Gravity * Dt;                  // STEP 1a: gravity pulls you down
		Velocity -= Velocity * AirDragPerSecond * Dt; // STEP 1b: air drag slows you slightly
		Velocity += MoveInput * 300.f * Dt;          // a little steering with WASD (desktop)

		// SWING EXAGGERATION: while a rope is tight, add a little extra gravity, but only the
		// part that points ALONG the swing (sideways to the rope). Pulling along the rope would
		// just stretch it; pulling along the swing makes each arc faster and bigger.
		if (SwingExaggeration != 1.f)
		{
			for (int32 i = 0; i < 2; ++i)
			{
				const FGrappleHook& H = Hooks[i];
				const FVector D = C - H.Anchor;
				const float Dist = D.Size();
				if (H.bAttached && Dist >= H.Length - 5.f && Dist > KINDA_SMALL_NUMBER)   // rope tight
				{
					const FVector N = D / Dist;                                                  // along the rope
					const FVector Extra(0.f, 0.f, -Gravity * (SwingExaggeration - 1.f));         // the extra gravity
					Velocity += (Extra - N * FVector::DotProduct(Extra, N)) * Dt;               // keep only the sideways part
					break;                                                                       // once, even with two hooks
				}
			}
		}
	}

	// Gas boost: push in the direction the camera is looking, while there's gas left.
	if (bBoost && Gas > 0.f && !bGrounded)
	{
		const FVector Dir = Camera.IsValid() ? Camera->GetForwardVector() : Pawn->GetActorForwardVector();
		Velocity += Dir * BoostAccel * Dt;
		Gas = FMath::Max(0.f, Gas - GasUsePerSecond * Dt);
	}

	// Reeling in: shorten the rope, and tug the player toward the anchor.
	for (int32 i = 0; i < 2; ++i)
	{
		FGrappleHook& H = Hooks[i];
		if (H.bAttached && bReel[i])
		{
			H.Length = FMath::Max(MinRopeLength, H.Length - ReelSpeed * Dt);
			if (bSoftRope)
			{
				// Soft rope: don't wind the rope more than ReelStallStretch shorter than the
				// player's actual distance, or the stretched rope would act like a catapult.
				H.Length = FMath::Max(H.Length, static_cast<float>(FVector::Dist(C, H.Anchor)) - ReelStallStretch);
			}
			Velocity += (H.Anchor - C).GetSafeNormal() * ReelPullAccel * Dt;   // GetSafeNormal = direction only, length 1
			// The reel's tug travels through the rope too, so it counts as rope pull (newtons).
			StepTension[i] += PlayerMass * ReelPullAccel / 100.f;
		}
	}

	// STEP 2b: SOFT ROPE (comfort). If the rope is stretched past its length, it pulls back
	// like a spring: the more it's stretched, the harder it pulls. The "damping" part also
	// pulls harder the faster you're moving away, which stops it bouncing like a bungee.
	// This spreads the jolt of the rope snapping tight over about 1/8 of a second.
	if (bSoftRope)
	{
		const float Omega = FMath::Sqrt(FMath::Max(RopeStiffness, 0.f));   // how fast the rope "springs"
		const float Damping = 2.f * RopeDampingRatio * Omega;               // standard spring-damper formula
		for (int32 i = 0; i < 2; ++i)
		{
			const FGrappleHook& H = Hooks[i];
			if (!H.bAttached)
			{
				continue;
			}
			const FVector D = C - H.Anchor;                 // arrow from the anchor to the player
			const float Dist = D.Size();
			const float Stretch = Dist - H.Length;          // how far past the rope's length we are
			if (Stretch > 0.f && Dist > KINDA_SMALL_NUMBER)
			{
				const FVector N = D / Dist;                                         // direction away from the anchor
				const float Outward = FVector::DotProduct(Velocity, N);            // speed away from the anchor
				// Progressive spring: stiffness grows with the square of the stretch (F6 fix).
				const float Stiff = RopeProgressiveStretch > 0.f
					? RopeStiffness * (1.f + FMath::Square(Stretch / RopeProgressiveStretch))
					: RopeStiffness;
				const float Pull = Stiff * Stretch + Damping * Outward;            // spring + damper
				if (Pull > 0.f)                                                     // a rope can pull, never push
				{
					Velocity -= N * Pull * Dt;
					// Force = mass x acceleration (Newton's second law). Pull is in cm/s^2, so
					// divide by 100 for m/s^2. Keep the biggest pull this frame, per rope.
					StepTension[i] += PlayerMass * Pull / 100.f;
				}
			}
		}
	}

	Velocity = Velocity.GetClampedToMaxSize(MaxSpeed);   // obey the speed limit
	FVector NewC = C + Velocity * Dt;                    // STEP 2: where we'd end up if nothing stopped us

	// STEP 3: THE ROPE'S HARD LIMIT. You can never be further than Length from the anchor
	// (plus MaxRopeStretch when the soft rope is on). Normally the soft rope stops you well
	// before this; it's a safety net for very hard falls.
	const float Give = bSoftRope ? MaxRopeStretch : 0.f;
	for (int32 i = 0; i < 2; ++i)
	{
		const FGrappleHook& H = Hooks[i];
		if (!H.bAttached)
		{
			continue;
		}
		const FVector D = NewC - H.Anchor;   // arrow from the anchor to the player
		const float Dist = D.Size();
		const float Limit = H.Length + Give;
		if (Dist > Limit && Dist > KINDA_SMALL_NUMBER)   // too far: the rope is at its absolute limit
		{
			const FVector N = D / Dist;              // direction from anchor to player (length 1)
			NewC = H.Anchor + N * Limit;             // put the player back on the limit circle
			// Remove the part of the speed that was pulling AWAY from the anchor (stretching the rope).
			// What's left is the sideways part, and that sideways motion IS the swing.
			const float Outward = FVector::DotProduct(Velocity, N);
			if (Outward > 0.f)
			{
				Velocity -= N * Outward;
				// The rope stopped that outward speed within one step: force = mass x (speed / time).
				StepTension[i] += PlayerMass * (Outward / Dt) / 100.f;
			}
		}
	}

	// Keep the biggest total pull of this frame for each rope (shown on screen, felt as vibration).
	RopeTension[0] = FMath::Max(RopeTension[0], StepTension[0]);
	RopeTension[1] = FMath::Max(RopeTension[1], StepTension[1]);

	// A hook pulling you upward lifts you off the ground.
	if (bGrounded && bAnyHook && Velocity.Z > 30.f)
	{
		bGrounded = false;
	}
	MoveWithCollision(C, NewC);   // STEP 4: move, but don't pass through rocks
}

// =====================================================================================
//  MoveWithCollision: slide the body's capsule from "From" to "To", stopping at solid
//  surfaces and sliding along them (like running your hand along a wall).
// =====================================================================================
void AGrappleRig::MoveWithCollision(const FVector& From, const FVector& To)
{
	UWorld* World = GetWorld();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(BodyRadius, BodyHalfHeight);   // the body's pill shape
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GrappleMove), false);
	Q.AddIgnoredActor(this);
	Q.AddIgnoredActor(Pawn.Get());

	FVector Cur = From;
	FVector Target = To;
	bool bHitWalkable = false;

	// Try up to 3 times: move, hit something, slide along it, try the rest of the move.
	for (int32 Iter = 0; Iter < 3; ++Iter)
	{
		// A "sweep" drags the pill shape along a line and reports the first thing it bumps.
		FHitResult Hit;
		if (!World->SweepSingleByChannel(Hit, Cur, Target, FQuat::Identity, ECC_Pawn, Shape, Q))
		{
			Cur = Target;   // nothing in the way: move all the way
			break;
		}
		// The "normal" = the direction the surface faces (straight up for a floor, sideways for a wall).
		const FVector N = Hit.ImpactNormal.IsNearlyZero() ? FVector(Hit.Normal) : FVector(Hit.ImpactNormal);
		// Remember what we bumped into, for the test recording.
		LastHitName = Hit.GetActor() ? Hit.GetActor()->GetActorNameOrLabel() : TEXT("unknown");
		bLastHitPenetrating |= Hit.bStartPenetrating;   // |= : stays true if ANY step this frame started inside
		if (Hit.bStartPenetrating && LastHitName != LastReportedPenetration)
		{
			// Starting a move already INSIDE something is a warning sign, so say so in the
			// Output Log (once per object, so the log doesn't fill up).
			UE_LOG(LogTemp, Warning, TEXT("GrappleRig: body started inside %s (%s), pushed out %.1f cm"),
				*LastHitName, Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("?"), Hit.PenetrationDepth);
			LastReportedPenetration = LastHitName;
		}
		if (Hit.bStartPenetrating)
		{
			// We started INSIDE something (e.g. spawned in the ground): push out and try again.
			Cur += FVector(Hit.Normal) * (Hit.PenetrationDepth + 1.f);
			Target += FVector(Hit.Normal) * (Hit.PenetrationDepth + 1.f);
			continue;
		}
		Cur = FVector(Hit.Location) + N * 0.5f;   // stop just before the surface (0.5 cm gap)
		// Remove the part of the speed that goes INTO the surface, so we slide instead of sticking.
		const float Vn = FVector::DotProduct(Velocity, N);
		if (Vn < 0.f)
		{
			Velocity -= N * Vn;
		}
		if (N.Z > WalkableNormalZ)
		{
			bHitWalkable = true;   // the surface faces up enough to stand on
		}
		// Whatever movement was left, flatten it along the surface and try again.
		const FVector Remaining = Target - Cur;
		Target = Cur + (Remaining - N * FVector::DotProduct(Remaining, N));
	}

	if (bHitWalkable)
	{
		bGrounded = true;   // we landed
	}
	else if (bGrounded)
	{
		// We were standing last step: is there still ground 12 cm below? (if not, we walked off an edge)
		FHitResult Ground;
		const bool bHit = World->SweepSingleByChannel(Ground, Cur, Cur - FVector(0.f, 0.f, 12.f), FQuat::Identity, ECC_Pawn, Shape, Q);
		bGrounded = bHit && Ground.ImpactNormal.Z > WalkableNormalZ;
	}

	BodyPos = Cur - FVector(0.f, 0.f, BodyHalfHeight);   // convert body centre back to feet position
}

// =====================================================================================
//  UpdateVisuals: draw the ropes, the hook heads, the aiming dots and the desktop readout.
// =====================================================================================
void AGrappleRig::UpdateVisuals()
{
	UStaticMeshComponent* Ropes[2] = {RopeL.Get(), RopeR.Get()};
	UStaticMeshComponent* Heads[2] = {HeadL.Get(), HeadR.Get()};
	UWorld* World = GetWorld();

	for (int32 i = 0; i < 2; ++i)
	{
		const FGrappleHook& H = Hooks[i];
		if (!Ropes[i] || !Heads[i])
		{
			continue;
		}
		if (H.bAttached)
		{
			// Move the rope's links with their own little physics, then draw each link as a
			// short cylinder between two neighbouring points of the chain.
			SimulateRopeShape(i, World ? World->GetDeltaSeconds() : 1.f / 90.f);
			const TArray<FVector>& P = RopePoints[i];
			for (int32 k = 0; k + 1 < P.Num() && i * RopeLinks + k < RopeLinkMeshes.Num(); ++k)
			{
				UStaticMeshComponent* Link = RopeLinkMeshes[i * RopeLinks + k];
				const FVector D = P[k + 1] - P[k];
				const float Len = D.Size();
				if (Len > 0.1f)
				{
					// Put the cylinder halfway between the two points, pointing from one to the other...
					Link->SetWorldLocationAndRotation((P[k] + P[k + 1]) * 0.5f, FRotationMatrix::MakeFromZ(D / Len).Rotator());
					// ...and scale it: Unreal's cylinder is 100 cm tall and 100 cm wide, so divide by 100.
					Link->SetWorldScale3D(FVector(RopeThickness / 100.f, RopeThickness / 100.f, Len / 100.f));
				}
				Link->SetHiddenInGame(false);
			}
			Heads[i]->SetWorldLocation(H.Anchor);
			Heads[i]->SetWorldScale3D(FVector(0.12f));   // a 12 cm ball where the hook bit
			Ropes[i]->SetHiddenInGame(true);              // the old single straight rope is no longer used
			Heads[i]->SetHiddenInGame(false);
		}
		else
		{
			bRopeShapeReady[i] = false;                   // next time this hook fires, lay the chain out fresh
			for (int32 k = 0; k < RopeLinks && i * RopeLinks + k < RopeLinkMeshes.Num(); ++k)
			{
				RopeLinkMeshes[i * RopeLinks + k]->SetHiddenInGame(true);
			}
			Ropes[i]->SetHiddenInGame(true);
			Heads[i]->SetHiddenInGame(true);
			// Not attached: show a dot where this hook WOULD land (green = left, blue = right).
			FVector P;
			if (FindTarget(i, P))
			{
				DrawDebugPoint(World, P, bVR ? 8.f : 10.f, i == 0 ? FColor(80, 255, 120) : FColor(80, 200, 255), false, -1.f, SDPG_Foreground);
			}
		}
	}

	// Desktop only: a white crosshair in the middle, and speed / gas text in the corner.
	// (VR rules say no text stuck to your face, so this never shows in the headset.)
	if (!bVR && bShowDebugHud && GEngine)
	{
		DrawDebugPoint(World, Camera.IsValid() ? Camera->GetComponentLocation() + Camera->GetForwardVector() * 200.f : BodyCenter(), 4.f, FColor::White, false, -1.f, SDPG_Foreground);
		// Speed in cm/s x 0.036 = km/h.
		GEngine->AddOnScreenDebugMessage(9001, 0.f, FColor::White,
			FString::Printf(TEXT("Speed %.0f km/h   Gas %.0f%%   %s   Rope pull %.0f N (%.1fx body weight)   |  LMB/RMB hooks  Shift %s  Space jump/boost  R respawn"),
				Velocity.Size() * 0.036f, 100.f * Gas / FMath::Max(1.f, GasMax), bGrounded ? TEXT("ground") : TEXT("air"),
				RopeTension[0] + RopeTension[1], (RopeTension[0] + RopeTension[1]) / FMath::Max(1.f, PlayerMass * 9.8f),
				bAutoPull ? TEXT("stop pulling") : TEXT("reel in")));
	}
}

// =====================================================================================
//  TEST RECORDING ("telemetry" = measurements sent from a machine while it runs)
//  Every frame we add one row to a spreadsheet. When you stop playing, the
//  spreadsheet is saved as a .csv file (opens in Excel or Google Sheets) in
//  Saved/Tests/. Each test in Docs/TESTS.md uses these files as its measured evidence.
// =====================================================================================
void AGrappleRig::RecordTelemetry(float DeltaSeconds)
{
	if (!bRecordTelemetry)
	{
		return;   // recording switched off in the Details panel
	}
	if (Telemetry.IsEmpty())
	{
		// First frame: write the settings at the top (lines starting with # are notes),
		// so anyone reading the file knows exactly what was tested. Then the column names.
		Telemetry.Reserve(4 * 1024 * 1024);   // set aside room for ~4 million letters up front (about 5 minutes of play) so the text doesn't keep resizing
		Telemetry += FString::Printf(TEXT("# Grapple Runner telemetry, recorded %s\n"), *FDateTime::Now().ToString());
		Telemetry += FString::Printf(TEXT("# Gravity=%.1f cm/s2, AirDragPerSecond=%.3f, MaxSpeed=%.0f cm/s, ReelSpeed=%.0f cm/s, ReelPullAccel=%.0f, BoostAccel=%.0f, MaxFireRange=%.0f cm, MinRopeLength=%.0f cm, Body radius %.0f / half-height %.0f cm, VR=%d\n"),
			Gravity, AirDragPerSecond, MaxSpeed, ReelSpeed, ReelPullAccel, BoostAccel, MaxFireRange, MinRopeLength, BodyRadius, BodyHalfHeight, bVR ? 1 : 0);
		Telemetry += FString::Printf(TEXT("# SoftRope=%d, RopeStiffness=%.1f, RopeDampingRatio=%.2f, MaxRopeStretch=%.0f cm, ReelStallStretch=%.0f cm, RopeProgressiveStretch=%.0f cm, SwingExaggeration=%.2f, AutoPull=%d\n"),
			bSoftRope ? 1 : 0, RopeStiffness, RopeDampingRatio, MaxRopeStretch, ReelStallStretch, RopeProgressiveStretch, SwingExaggeration, bAutoPull ? 1 : 0);
		Telemetry += TEXT("time_s,frame_ms,x_cm,y_cm,z_cm,vx,vy,vz,speed_cms,grounded,hookL,ropeL_len,ropeL_dist,hookR,ropeR_len,ropeR_dist,reel,boost,gas,last_hit,hit_started_inside,test,tensionL_N,tensionR_N\n");
	}
	TelemetryClock += DeltaSeconds;
	const FVector C = BodyCenter();

	// For each hook: is it attached, how long is the rope, and how far is the body
	// actually from the anchor? If the rope works, distance is never more than length.
	float Len[2] = {0.f, 0.f}, Dist[2] = {0.f, 0.f};
	for (int32 i = 0; i < 2; ++i)
	{
		if (Hooks[i].bAttached)
		{
			Len[i] = Hooks[i].Length;
			Dist[i] = static_cast<float>(FVector::Dist(C, Hooks[i].Anchor));
		}
	}

	// One row: the numbers, separated by commas, in the same order as the column names.
	Telemetry += FString::Printf(TEXT("%.4f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d,%.1f,%.1f,%d,%.1f,%.1f,%d,%d,%.1f,%s,%d,%s,%.0f,%.0f\n"),
		TelemetryClock, DeltaSeconds * 1000.f, BodyPos.X, BodyPos.Y, BodyPos.Z,
		Velocity.X, Velocity.Y, Velocity.Z, Velocity.Size(), bGrounded ? 1 : 0,
		Hooks[0].bAttached ? 1 : 0, Len[0], Dist[0], Hooks[1].bAttached ? 1 : 0, Len[1], Dist[1],
		(bReel[0] || bReel[1]) ? 1 : 0, bBoost ? 1 : 0, Gas, *LastHitName, bLastHitPenetrating ? 1 : 0, *TestName, RopeTension[0], RopeTension[1]);

	// Clear the collision notes, ready for the next frame. (They are only reset here, once a
	// frame, so a bump in ANY of this frame's physics steps gets recorded, not just the last.)
	LastHitName = TEXT("none");
	bLastHitPenetrating = false;
}

void AGrappleRig::SaveTelemetry()
{
	if (Telemetry.IsEmpty())
	{
		return;   // nothing recorded
	}
	// Saved/Tests/telemetry_2026.11.08-16.30.00.csv  (date and time in the name, so files never overwrite)
	const FString Folder = FPaths::ProjectSavedDir() / TEXT("Tests");
	IFileManager::Get().MakeDirectory(*Folder, true);   // make the folder if it isn't there yet
	const FString File = Folder / FString::Printf(TEXT("telemetry_%s.csv"), *FDateTime::Now().ToString());
	if (FFileHelper::SaveStringToFile(Telemetry, *File))
	{
		UE_LOG(LogTemp, Log, TEXT("GrappleRig: test recording saved to %s"), *FPaths::ConvertRelativePathToFull(File));
	}
	Telemetry.Empty();
}

// =====================================================================================
//  EndPlay: runs once when you stop playing. Save the test recording.
// =====================================================================================
void AGrappleRig::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveTelemetry();
	Super::EndPlay(EndPlayReason);   // then let Unreal do its own shutdown
}

// =====================================================================================
//  SimulateRopeShape: the rope as a chain of links ("Verlet" rope, a standard game technique).
//  Each point remembers where it was last frame; the difference is its speed. Every frame:
//    1. each point keeps moving (with a little damping) and falls with gravity;
//    2. the two ends are pinned: one to your hand, one to the hook;
//    3. we nudge neighbouring points back to the right spacing, many times over, so the
//       chain behaves like a rope that can bend but not stretch.
//  If the rope is longer than the gap (slack), the chain hangs and sags; when it's pulled
//  tight, the spacing forces it straight. Moving your hand sends waves along it.
//  This only changes how the rope LOOKS; how you swing is still decided in Simulate().
// =====================================================================================
void AGrappleRig::SimulateRopeShape(int32 Index, float DeltaSeconds)
{
	const FVector S = RopeStart(Index);         // rope start: the hip launcher (or your hand)
	const FVector E = Hooks[Index].Anchor;      // rope end: where the hook bit
	const int32 N = RopeLinks;                  // number of links (points = links + 1)
	TArray<FVector>& P = RopePoints[Index];
	TArray<FVector>& Old = RopePointsBefore[Index];

	if (!bRopeShapeReady[Index] || P.Num() != N + 1)
	{
		// Just fired: lay the points out in a straight line from hand to hook.
		P.SetNum(N + 1);
		Old.SetNum(N + 1);
		for (int32 k = 0; k <= N; ++k)
		{
			P[k] = Old[k] = FMath::Lerp(S, E, static_cast<float>(k) / N);
		}
		bRopeShapeReady[Index] = true;
	}

	// How long each link should be. The rope's length comes from the physics (Hooks.Length);
	// if the soft rope is stretched past that, use the real gap so the drawn rope stays straight.
	const float Gap = FVector::Dist(S, E);
	const float Total = FMath::Max(Hooks[Index].Length, Gap);
	const float LinkLen = Total / N;

	// 1. Keep moving + gravity (positions only: "where I was" -> "where I am" -> "where I'll be").
	const float Dt = FMath::Clamp(DeltaSeconds, 1.f / 240.f, 1.f / 30.f);
	const FVector Fall(0.f, 0.f, -Gravity * Dt * Dt);
	for (int32 k = 1; k < N; ++k)
	{
		const FVector Speed = (P[k] - Old[k]) * (1.f - RopeWobbleDamping);
		Old[k] = P[k];
		P[k] += Speed + Fall;
	}

	// 2 + 3. Pin the ends, then fix the spacing between neighbours (20 rounds = firm rope).
	for (int32 Round = 0; Round < 20; ++Round)
	{
		P[0] = S;
		P[N] = E;
		for (int32 k = 0; k < N; ++k)
		{
			const FVector D = P[k + 1] - P[k];
			const float Len = D.Size();
			if (Len < KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector Fix = D * (0.5f * (Len - LinkLen) / Len);   // half the error goes to each point
			if (k > 0)
			{
				P[k] += Fix;
			}
			if (k + 1 < N)
			{
				P[k + 1] -= Fix;
			}
		}
	}
	P[0] = S;
	P[N] = E;
	Old[0] = S;
	Old[N] = E;
}

// =====================================================================================
//  FeelRopeTension: "feel the weight of your body". The harder a rope is pulling you
//  (measured in newtons in Simulate), the harder that hand's controller vibrates.
//  Hanging still = 1x your body weight = a light buzz; a hard swing or snap = strong.
//  Later the same number will drive the EXIT SUIT's haptics.
// =====================================================================================
void AGrappleRig::FeelRopeTension()
{
	APlayerController* P = PC.Get();
	if (!bVR || !P)
	{
		return;   // desktop: shown as "Rope pull" on screen instead
	}
	const float BodyWeight = FMath::Max(1.f, PlayerMass * 9.8f);   // your weight in newtons
	for (int32 i = 0; i < 2; ++i)
	{
		if (GetWorld()->GetTimeSeconds() < PulseUntil[i])
		{
			continue;   // a short "hook bit" buzz is playing on this hand: don't interrupt it
		}
		const EControllerHand Hand = i == 0 ? EControllerHand::Left : EControllerHand::Right;
		// 0 at no pull, 0.25 when hanging still (1x body weight), full strength at 4x body weight.
		const float Strength = Hooks[i].bAttached ? FMath::Clamp(RopeTension[i] / (4.f * BodyWeight), 0.f, 1.f) : 0.f;
		P->SetHapticsByValue(0.3f, Strength, Hand);
	}
}

// =====================================================================================
//  UpdateWind: the faster you move, the louder and higher-pitched the rushing air.
//  "Level" goes from 0 (below WindStartSpeed) to 1 (at WindFullSpeed) and is smoothed so
//  the sound swells and fades rather than jumping. Hearing speed helps your brain make
//  sense of the motion you see, which is also good for comfort.
// =====================================================================================
void AGrappleRig::UpdateWind(float DeltaSeconds)
{
	if (!WindAudio)
	{
		return;   // no wind sound imported yet
	}
	const float Target = FMath::Clamp((Velocity.Size() - WindStartSpeed) / FMath::Max(1.f, WindFullSpeed - WindStartSpeed), 0.f, 1.f);
	WindLevel = FMath::FInterpTo(WindLevel, Target, DeltaSeconds, 3.f);   // ease toward the target
	WindAudio->SetVolumeMultiplier(FMath::Max(0.001f, WindMaxVolume * FMath::Pow(WindLevel, 1.5f)));
	WindAudio->SetPitchMultiplier(0.8f + 0.6f * WindLevel);              // 0.8 when slow, 1.4 when fast
}

// =====================================================================================
//  THE VISIBLE BODY
//  SetUpBody makes a character (Kwang for now) and straps the grapple harness to its hip
//  bone. UpdateBody runs every frame: it stands the body on your feet position, turned the
//  way your head faces, a little BEHIND your eyes, and hides the head bone so the camera
//  is never inside it. It also picks an animation: idle, running, or the in-air pose.
//  (VR arm tracking - hands following the controllers - comes later with IK.)
// =====================================================================================
void AGrappleRig::SetUpBody()
{
	if (!bShowBody)
	{
		return;
	}
	USkeletalMesh* Mesh = BodyMeshAsset.LoadSynchronous();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("GrappleRig: body mesh not found (%s)"), *BodyMeshAsset.ToString());
		return;
	}
	Body = NewObject<USkeletalMeshComponent>(this);
	Body->SetSkeletalMeshAsset(Mesh);
	Body->SetUsingAbsoluteScale(true);
	Body->SetWorldScale3D(FVector(BodyScale));                    // resize so the shoulders sit below your eyes
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // the body never blocks anything (physics uses the capsule)
	Body->SetUsingAbsoluteLocation(true);
	Body->SetUsingAbsoluteRotation(true);
	Body->RegisterComponent();
	// AttachToComponent (not SetupAttachment) is the call that works for parts made while the game runs.
	Body->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	AddInstanceComponent(Body);                                   // lists the body under this actor, so it shows in the Outliner
	Body->HideBoneByName(TEXT("head"), EPhysBodyOp::PBO_None);  // no head in your own view
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	// The harness: the belt on the hip bone, and one piece on each thigh so the leg loops and
	// blade boxes swing with your legs when you run.
	Harness = StrapToBone(HarnessAsset.LoadSynchronous(), HarnessBone, HarnessOffset, HarnessRotation.Quaternion());

	// The ODM gear goes on the same hip bone, tilted so the blade boxes' back ends hang lower.
	// The tilt turns round the body's sideways (X) axis.
	if (UStaticMesh* GearMesh = GearAsset.LoadSynchronous())
	{
		const FQuat Tilt(FVector::XAxisVector, FMath::DegreesToRadians(GearTilt));
		Gear = StrapToBone(GearMesh, HarnessBone, GearOffset, Tilt, GearScale);
	}

	// Which thigh piece goes on which leg? Piece A's outer side is +X, so it goes on whichever
	// thigh bone sits on the +X side of the hips. (Checked rather than assumed, so a different
	// body that's built the other way round still gets the boxes on the outside.)
	const FName Thighs[2] = { ThighBoneLeft, ThighBoneRight };
	const FVector HipsAt = Body->GetComponentTransform().InverseTransformPosition(Body->GetBoneLocation(HarnessBone));
	for (int32 i = 0; i < 2; ++i)
	{
		if (Body->GetBoneIndex(Thighs[i]) == INDEX_NONE)
		{
			continue;   // this body has no bone with that name
		}
		const FVector ThighAt = Body->GetComponentTransform().InverseTransformPosition(Body->GetBoneLocation(Thighs[i]));
		const bool bPlusX = ThighAt.X > HipsAt.X;
		const FVector Offset(bPlusX ? ThighHarnessOffset.X : -ThighHarnessOffset.X, ThighHarnessOffset.Y, ThighHarnessOffset.Z);
		UStaticMeshComponent* Piece = StrapToBone((bPlusX ? ThighHarnessA : ThighHarnessB).LoadSynchronous(),
			Thighs[i], Offset, FQuat::Identity);
		if (i == 0)
		{
			ThighHarnessL = Piece;
		}
		else
		{
			ThighHarnessR = Piece;
		}
	}
}

// Fixes one harness piece to a bone. The piece is placed upright in the BODY's directions
// (forward, sideways, up) at the bone, at real-world size, and only THEN attached to the bone.
// Why: every bone has its own private directions (a thigh bone's "forward" points down the
// leg), so a piece attached "straight" to a bone would end up sideways. Placing it first in
// the body's directions, then attaching with "keep where it is", avoids guessing those.
UStaticMeshComponent* AGrappleRig::StrapToBone(UStaticMesh* Mesh, FName Bone, const FVector& OffsetCm, const FQuat& Turn, const FVector& Stretch)
{
	if (!Mesh || !Body || Body->GetBoneIndex(Bone) == INDEX_NONE)
	{
		if (!Mesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("GrappleRig: harness piece for bone %s not found (run Scripts/import_harness.py)"), *Bone.ToString());
		}
		return nullptr;
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetStaticMesh(Mesh);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // never blocks anything
	Part->RegisterComponent();
	const FQuat BodyTurn = Body->GetComponentQuat();
	Part->SetWorldLocationAndRotation(Body->GetBoneLocation(Bone) + BodyTurn.RotateVector(OffsetCm),
		BodyTurn * Turn);
	Part->SetWorldScale3D(Stretch * HarnessScale);                // real size, whatever the body's scale
	Part->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, Bone);
	AddInstanceComponent(Part);
	return Part;
}

void AGrappleRig::UpdateBody()
{
	if (!Body)
	{
		return;
	}
	// Which way is the head facing (left / right only)?
	const FRotator View = Camera.IsValid() ? Camera->GetComponentRotation() : FRotator(0.f, DesktopYaw, 0.f);
	const FRotator Facing(0.f, View.Yaw, 0.f);
	const FVector Eyes = Camera.IsValid() ? Camera->GetComponentLocation() : BodyCenter();
	// Feet under the eyes, nudged back so the camera sits just in front of the (hidden) face.
	const FVector Feet(Eyes.X, Eyes.Y, BodyPos.Z);
	Body->SetWorldLocationAndRotation(Feet - Facing.Vector() * BodyBehindEyes,
		FRotator(0.f, View.Yaw - 90.f, 0.f));                        // Paragon meshes face +Y, so turn by -90

	// Pick the animation: in the air, running, or standing.
	UAnimSequence* Want = nullptr;
	if (!bGrounded)
	{
		Want = AirAnim.LoadSynchronous();
	}
	else if (FVector(Velocity.X, Velocity.Y, 0.f).Size() > 60.f)
	{
		Want = RunAnim.LoadSynchronous();
	}
	else
	{
		Want = IdleAnim.LoadSynchronous();
	}
	if (Want && Want != BodyAnimPlaying)
	{
		Body->PlayAnimation(Want, true);   // true = loop
		BodyAnimPlaying = Want;
	}
}

// Ropes leave from the harness's left / right launchers (like Attack on Titan gear);
// with no harness they come from your hands.
FVector AGrappleRig::RopeStart(int32 Index) const
{
	if ((Gear || Harness) && Body)
	{
		// The muzzle position in the belt's own directions (X = sideways, Y = forward, Z = up),
		// turned into a world position. The body faces +Y, so its LEFT side is +X: rope 0 (left)
		// leaves from the +X launcher. TransformPosition also applies HarnessScale, so a bigger
		// harness moves the launchers out with it.
		// The gear's nozzles if it's there, otherwise measured from the belt.
		const FVector Muzzle(Index == 0 ? LauncherSpacing : -LauncherSpacing, LauncherForward, LauncherHeight);
		const UStaticMeshComponent* From = Gear ? Gear.Get() : Harness.Get();
		return From->GetComponentTransform().TransformPosition(Muzzle);
	}
	return HandPosition(Index);
}

// =====================================================================================
//  SCRIPTED TESTS
//  Playing by hand is never exactly the same twice. These tests set up an exact
//  situation (same position, speed and rope every time), high in the empty sky so
//  nothing gets in the way, and let the physics run for 3 seconds with no input.
//  That makes them repeatable, and comparable step-for-step with Tools/rope_model.py.
// =====================================================================================
void AGrappleRig::StartTest(const FString& Name, int32 SoftRope)
{
	if (!Pawn.IsValid())
	{
		return;   // not playing yet
	}
	bSoftRopeBeforeTest = bSoftRope;
	SwingExaggerationBeforeTest = SwingExaggeration;
	if (Name == TEXT("swingreal"))
	{
		SwingExaggeration = 1.f;   // real physics for this test only
	}
	if (SoftRope >= 0)
	{
		bSoftRope = (SoftRope == 1);   // the test chooses stiff or soft rope
	}
	if (Name == TEXT("perf"))
	{
		// Frame-rate test: same spot and same view every time, so results can be compared
		// before and after a change (e.g. adding plants). No hooks, no input, 10 seconds.
		Respawn();
		DesktopYaw = 0.f;                                   // face +X: down the route
		DesktopPitch = -5.f;                                // looking very slightly down
		Pawn->SetActorRotation(FRotator(0.f, DesktopYaw, 0.f));
		if (!bVR && Camera.IsValid())
		{
			Camera->SetRelativeRotation(FRotator(DesktopPitch, 0.f, 0.f));
		}
		TestName = TEXT("perf");
		TestTimeLeft = 10.f;
		TestFrameTimes.Reset();
		UE_LOG(LogTemp, Log, TEXT("GrappleRig: test perf started"));
		return;
	}
	// The anchor floats 300 m up, straight above wherever the player is: open sky.
	const FVector Anchor(BodyPos.X, BodyPos.Y, 30000.f);
	FVector Center;
	if (Name == TEXT("catch"))
	{
		// The F6 case from test T-11: a 13.3 m rope, slack, caught while flying outward at 22 m/s.
		Center = Anchor + FVector(0.f, 0.f, -1300.f);
		Velocity = FVector(500.f, 0.f, -2200.f);
		Hooks[0].Length = 1330.f;
		bTestReel = false;
	}
	else if (Name == TEXT("swing") || Name == TEXT("swingreal"))
	{
		// A pendulum: let go from level with the anchor, on a 15 m rope, starting still.
		// Real physics predicts a bottom speed of sqrt(2 x g x L) = 17.1 m/s (more with exaggeration).
		Center = Anchor + FVector(1500.f, 0.f, 0.f);
		Velocity = FVector::ZeroVector;
		Hooks[0].Length = 1500.f;
		bTestReel = false;
	}
	else if (Name == TEXT("reel"))
	{
		Center = Anchor + FVector(0.f, 0.f, -3730.f);   // hanging still on a 37.3 m rope
		Velocity = FVector::ZeroVector;
		Hooks[0].Length = 3730.f;
		bTestReel = true;                              // and reel in for the whole test
	}
	else   // "snap"
	{
		Center = Anchor + FVector(1200.f, 0.f, -3330.f);   // 4 m inside the rope's reach...
		Velocity = FVector(300.f, 0.f, -1700.f);            // ...falling away at about 17 m/s
		Hooks[0].Length = 3530.f;
		bTestReel = false;
	}
	BodyPos = Center - FVector(0.f, 0.f, BodyHalfHeight);   // BodyPos is the feet; Center is the middle
	Hooks[0].bAttached = true;
	Hooks[0].Anchor = Anchor;
	Hooks[1].bAttached = false;
	bGrounded = false;
	Accumulator = 0.f;
	TestName = FString::Printf(TEXT("%s_%s"), *Name, bSoftRope ? TEXT("soft") : TEXT("stiff"));
	TestTimeLeft = 3.f;
	TestPeakFeltG = 0.f;
	TestMaxStretch = 0.f;
	UE_LOG(LogTemp, Log, TEXT("GrappleRig: test %s started"), *TestName);
}

void AGrappleRig::EndTest()
{
	if (TestName == TEXT("perf"))
	{
		// Sort the frame times and read off the middle (median) and the slow end (95th / 99th).
		TestFrameTimes.Sort();
		const int32 Num = TestFrameTimes.Num();
		auto At = [this, Num](float Fraction) { return Num ? 1000.f * TestFrameTimes[FMath::Min(Num - 1, static_cast<int32>(Fraction * Num))] : 0.f; };
		int32 Over = 0;
		for (float T : TestFrameTimes)
		{
			Over += T > 1.f / 90.f ? 1 : 0;      // slower than a 90 Hz headset allows
		}
		const FString Msg = FString::Printf(TEXT("Test perf: %d frames, median %.2f ms (%.0f fps), 95th %.2f ms, 99th %.2f ms, over 11.1 ms: %.1f%%"),
			Num, At(0.5f), At(0.5f) > 0.f ? 1000.f / At(0.5f) : 0.f, At(0.95f), At(0.99f), Num ? 100.f * Over / Num : 0.f);
		UE_LOG(LogTemp, Log, TEXT("GrappleRig: %s"), *Msg);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, Msg);
		}
		TestName = TEXT("none");
		TestTimeLeft = 0.f;
		return;
	}
	// Report the headline numbers straight away (the full data is in the recording).
	UE_LOG(LogTemp, Log, TEXT("GrappleRig: test %s finished: peak felt %.1f g, max stretch %.0f cm"),
		*TestName, TestPeakFeltG, TestMaxStretch);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, FString::Printf(
			TEXT("Test %s: peak felt %.1f g, max stretch %.0f cm"), *TestName, TestPeakFeltG, TestMaxStretch));
	}
	TestName = TEXT("none");
	TestTimeLeft = 0.f;
	bTestReel = false;
	bSoftRope = bSoftRopeBeforeTest;   // put the rope setting back how it was
	SwingExaggeration = SwingExaggerationBeforeTest;
	Hooks[0].bWasDown = Hooks[1].bWasDown = false;
	Respawn();                         // back to the first island
}

// The console command "gr.Test". Typing it in the game's console runs this function,
// which finds the GrappleRig in the level and starts the test.
static void RunGrappleTest(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	const FString Name = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("snap"));
	int32 Soft = -1;                                   // -1 = use the current setting
	if (Args.Num() > 1)
	{
		Soft = Args[1].Equals(TEXT("stiff"), ESearchCase::IgnoreCase) ? 0 : 1;
	}
	for (TActorIterator<AGrappleRig> It(World); It; ++It)
	{
		It->StartTest(Name, Soft);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("gr.Test: no GrappleRig in this level"));
}

static FAutoConsoleCommandWithWorldAndArgs GGrappleTestCommand(
	TEXT("gr.Test"),
	TEXT("Grapple Runner scripted physics test. Usage: gr.Test snap|reel [stiff|soft]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunGrappleTest));
