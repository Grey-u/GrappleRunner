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
//    4. Check they didn't go through a rock (if they did, stop them at its surface).
//  This is the same model as the 2D physics test bench (Tools/physics-bench.html).
// =====================================================================================

#include "GrappleRig.h"

// The real definitions of everything we use (the .h only promised they exist):
#include "Camera/CameraComponent.h"            // the player's eyes
#include "Components/StaticMeshComponent.h"    // visible 3D shapes
#include "DrawDebugHelpers.h"                  // quick on-screen dots for aiming
#include "Engine/Engine.h"                     // GEngine: global engine access (on-screen text, VR check)
#include "Engine/StaticMesh.h"
#include "Engine/World.h"                      // the level: lets us ray-trace and sweep
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"         // the spawn point marker
#include "IXRTrackingSystem.h"                 // asks "is a VR headset active?"
#include "Kismet/GameplayStatics.h"            // handy helpers like "find all actors with this tag"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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

	// ---- 5. Colour the ropes orange ----
	// We copy the shapes' default material and change its "Color" setting.
	if (UMaterialInterface* Base = RopeL ? RopeL->GetMaterial(0) : nullptr)
	{
		RopeMaterial = UMaterialInstanceDynamic::Create(Base, this);
		RopeMaterial->SetVectorParameterValue(TEXT("Color"), RopeColor);
		for (UStaticMeshComponent* C : {RopeL.Get(), RopeR.Get(), HeadL.Get(), HeadR.Get()})
		{
			C->SetMaterial(0, RopeMaterial);
		}
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
			bReel[i] = AnyDown(GripKeys[i]) || AnyAxis(GripAxes[i], 0.6f);
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
		bReel[0] = bReel[1] = bShift;   // Shift reels in both hooks

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

	ReadInput(DeltaSeconds);

	// FIXED-STEP PHYSICS
	// Frames can be uneven (11 ms, then 14 ms...). Physics is more stable if it always moves
	// in identical small steps, so we save up the frame time in a "bucket" (Accumulator) and
	// run as many 1/120 s steps as fit. Leftover time waits for next frame.
	// The bucket is capped at 0.1 s, so a big hitch can't launch the player across the map.
	const float Step = 1.f / 120.f;   // same fixed step as the physics bench
	Accumulator = FMath::Min(Accumulator + DeltaSeconds, 0.1f);
	while (Accumulator >= Step)
	{
		Simulate(Step);
		Accumulator -= Step;
	}

	if (BodyPos.Z < KillZ)
	{
		Respawn();   // fell into the mist
	}
	// Actually move the player's body to the new position.
	Pawn->SetActorLocation(BodyPos, false, nullptr, ETeleportType::TeleportPhysics);
	LastSetLocation = Pawn->GetActorLocation();

	UpdateVisuals();
}

// =====================================================================================
//  Simulate: ONE small physics step of Dt seconds (1/120 s). See "THE BIG IDEA" at the top.
// =====================================================================================
void AGrappleRig::Simulate(float Dt)
{
	const bool bAnyHook = Hooks[0].bAttached || Hooks[1].bAttached;
	const FVector C = BodyCenter();   // where the body is right now

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
			Velocity += (H.Anchor - C).GetSafeNormal() * ReelPullAccel * Dt;   // GetSafeNormal = direction only, length 1
		}
	}

	Velocity = Velocity.GetClampedToMaxSize(MaxSpeed);   // obey the speed limit
	FVector NewC = C + Velocity * Dt;                    // STEP 2: where we'd end up if nothing stopped us

	// STEP 3: THE ROPE. A rope can't stretch: you can't be further than Length from the anchor.
	for (int32 i = 0; i < 2; ++i)
	{
		const FGrappleHook& H = Hooks[i];
		if (!H.bAttached)
		{
			continue;
		}
		const FVector D = NewC - H.Anchor;   // arrow from the anchor to the player
		const float Dist = D.Size();
		if (Dist > H.Length && Dist > KINDA_SMALL_NUMBER)   // too far: the rope is pulled tight
		{
			const FVector N = D / Dist;              // direction from anchor to player (length 1)
			NewC = H.Anchor + N * H.Length;          // put the player back on the rope's circle
			// Remove the part of the speed that was pulling AWAY from the anchor (stretching the rope).
			// What's left is the sideways part, and that sideways motion IS the swing.
			const float Outward = FVector::DotProduct(Velocity, N);
			if (Outward > 0.f)
			{
				Velocity -= N * Outward;
			}
		}
	}

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
			// Stretch a cylinder from the hand (S) to the anchor (E).
			const FVector S = HandPosition(i);
			const FVector E = H.Anchor;
			const FVector D = E - S;
			const float Len = D.Size();
			if (Len > 1.f)
			{
				// Put the cylinder halfway between the two points, pointing from one to the other...
				Ropes[i]->SetWorldLocationAndRotation((S + E) * 0.5f, FRotationMatrix::MakeFromZ(D / Len).Rotator());
				// ...and scale it: Unreal's cylinder is 100 cm tall and 100 cm wide, so divide by 100.
				Ropes[i]->SetWorldScale3D(FVector(RopeThickness / 100.f, RopeThickness / 100.f, Len / 100.f));
			}
			Heads[i]->SetWorldLocation(E);
			Heads[i]->SetWorldScale3D(FVector(0.12f));   // a 12 cm ball where the hook bit
			Ropes[i]->SetHiddenInGame(false);
			Heads[i]->SetHiddenInGame(false);
		}
		else
		{
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
			FString::Printf(TEXT("Speed %.0f km/h   Gas %.0f%%   %s   |  LMB/RMB hooks  Shift reel  Space jump/boost  R respawn"),
				Velocity.Size() * 0.036f, 100.f * Gas / FMath::Max(1.f, GasMax), bGrounded ? TEXT("ground") : TEXT("air")));
	}
}
