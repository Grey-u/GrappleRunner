// =====================================================================================
//  GRGasPod.cpp  -  how a gas canister floats, gets sliced and bursts into light
// =====================================================================================
//
//  THE PIECES
//    constructor  builds the canister from Unreal's basic shapes: a glowing cylinder with two
//                 dark caps, a thin beam of light above, and a soft light
//    BeginPlay    gives it its glowing material (M_GasGlow, made by Scripts/make_materials.py)
//    Tick         bobs and turns it; after a burst, moves the bits of light outward
//    TrySlice     the blade test (closest distance from the blade line to the centre)
//    Burst        hides the canister, refills the tank, throws out the bits of light
// =====================================================================================
#include "GRGasPod.h"
#include "GrappleRig.h"
#include "GRGasGauge.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

int32 AGRGasPod::SlicedThisRun = 0;

namespace
{
	// Makes a shape part with no collision (hooks, the player and the camera pass through it).
	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Scale, const FVector& Offset)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(Mesh);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeLocation(Offset);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		return C;
	}
}

AGRGasPod::AGRGasPod()
{
	PrimaryActorTick.bCanEverTick = true;
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	RootComponent = Pivot;

	// Unreal's basic shapes are 100 cm across, so a scale of 0.3 = 30 cm.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cyl = Cylinder.Object;
	Body = MakePart(this, Pivot, TEXT("Body"), Cyl, FVector(0.32f, 0.32f, 0.62f), FVector::ZeroVector);
	CapTop = MakePart(this, Body, TEXT("CapTop"), Cyl, FVector(1.15f, 1.15f, 0.12f), FVector(0.f, 0.f, 52.f));
	CapBottom = MakePart(this, Body, TEXT("CapBottom"), Cyl, FVector(1.15f, 1.15f, 0.12f), FVector(0.f, 0.f, -52.f));
	Beam = MakePart(this, Pivot, TEXT("Beam"), Cyl, FVector(0.025f, 0.025f, 7.f), FVector(0.f, 0.f, 390.f));   // 7 m of light straight up

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Pivot);
	Light->SetIntensity(9000.f);
	Light->SetAttenuationRadius(700.f);
	Light->SetCastShadows(false);
}

void AGRGasPod::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	Clock = FMath::FRandRange(0.f, 6.f);   // so neighbouring canisters don't bob in step

	// The glowing material, tinted to this canister's colour. Falls back to the plain shape
	// material if M_GasGlow hasn't been made yet.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GrappleRunner/Materials/M_GasGlow.M_GasGlow"));
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	GlowMat = UMaterialInstanceDynamic::Create(Base, this);
	GlowMat->SetVectorParameterValue(TEXT("Color"), Glow);
	GlowMat->SetScalarParameterValue(TEXT("Glow"), 6.f);
	Body->SetMaterial(0, GlowMat);
	UMaterialInstanceDynamic* BeamMat = UMaterialInstanceDynamic::Create(Base, this);
	BeamMat->SetVectorParameterValue(TEXT("Color"), Glow);
	BeamMat->SetScalarParameterValue(TEXT("Glow"), 2.f);
	Beam->SetMaterial(0, BeamMat);
	UMaterialInstanceDynamic* CapMat = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), this);
	CapMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.03f, 0.035f, 0.04f));   // dark metal caps
	CapTop->SetMaterial(0, CapMat);
	CapBottom->SetMaterial(0, CapMat);
	Light->SetLightColor(Glow);
}

bool AGRGasPod::TrySlice(const FVector& Base, const FVector& Tip, AGrappleRig* Slicer)
{
	if (bBurst || !Slicer)
	{
		return false;
	}
	// The closest point on the blade line to the canister's centre, and how far that is.
	const FVector Centre = Body->GetComponentLocation();
	const FVector Closest = FMath::ClosestPointOnSegment(Centre, Base, Tip);
	if (FVector::Dist(Closest, Centre) > Radius)
	{
		return false;
	}
	// The burst takes the gauge's colour right NOW (before the refill), as Naomi asked.
	const FLinearColor Colour = AGRGasGauge::ColourFor(Slicer->GetGas() / FMath::Max(1.f, Slicer->GetGasMax()));
	Slicer->AddGas(Refill);
	++SlicedThisRun;
	Burst(Colour);
	return true;
}

void AGRGasPod::Burst(const FLinearColor& Colour)
{
	bBurst = true;
	Age = 0.f;
	Body->SetVisibility(false, true);   // hide the canister and its caps
	Beam->SetVisibility(false);

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GrappleRunner/Materials/M_GasGlow.M_GasGlow"));
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	BurstMat = UMaterialInstanceDynamic::Create(Base, this);
	BurstMat->SetVectorParameterValue(TEXT("Color"), Colour);
	BurstMat->SetScalarParameterValue(TEXT("Glow"), 25.f);   // much brighter than the canister: a flash of light
	UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// Throw out the bits: each one gets a random direction, mostly sideways and up.
	const FVector Centre = Body->GetComponentLocation();
	for (int32 i = 0; i < Sparks; ++i)
	{
		UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(this);
		B->SetStaticMesh(Ball);
		B->SetMaterial(0, BurstMat);
		B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		B->SetCastShadow(false);
		B->SetupAttachment(RootComponent);
		B->RegisterComponent();
		B->SetWorldLocation(Centre);
		B->SetWorldScale3D(FVector(FMath::FRandRange(0.05f, 0.11f)));
		Bits.Add(B);
		BitSpeed.Add((FMath::VRand() + FVector(0.f, 0.f, 0.35f)).GetSafeNormal() * FMath::FRandRange(250.f, 900.f));
	}
	Light->SetLightColor(Colour);
	Light->SetAttenuationRadius(1600.f);
	LightStart = 120000.f;
	Light->SetIntensity(LightStart);
}

void AGRGasPod::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	if (!bBurst)
	{
		// Fly through it (or close past, within TouchRadius) and it counts too: on a keyboard
		// you're busy aiming hooks, so slicing alone was too hard (Naomi, 14 Nov).
		if (!Rig.IsValid())
		{
			TActorIterator<AGrappleRig> It(GetWorld());
			Rig = It ? *It : nullptr;
		}
		if (AGrappleRig* R = Rig.Get())
		{
			const FVector Chest = R->GetBodyPosition() + FVector(0.f, 0.f, 85.f);
			if (FVector::Dist(Chest, GetActorLocation()) < TouchRadius)
			{
				TrySlice(GetActorLocation(), GetActorLocation(), R);   // a zero-length "blade" right through the middle
				return;
			}
		}
		// Bob up and down 25 cm and turn slowly: it reads as "pick me up" from far away.
		SetActorLocation(Home + FVector(0.f, 0.f, FMath::Sin(Clock * 1.8f) * 25.f));
		Body->SetRelativeRotation(FRotator(18.f, Clock * 70.f, 0.f));
		return;
	}

	// The burst: bits fly out, slow down (drag), drift down a little, shrink and vanish.
	Age += DeltaSeconds;
	const float Life = 0.8f;
	const float Left = FMath::Clamp(1.f - Age / Life, 0.f, 1.f);
	for (int32 i = 0; i < Bits.Num(); ++i)
	{
		BitSpeed[i] *= FMath::Max(0.f, 1.f - 3.f * DeltaSeconds);
		BitSpeed[i].Z -= 200.f * DeltaSeconds;
		Bits[i]->AddWorldOffset(BitSpeed[i] * DeltaSeconds);
		Bits[i]->SetWorldScale3D(Bits[i]->GetComponentScale() * FMath::Max(0.f, 1.f - 1.5f * DeltaSeconds));
	}
	Light->SetIntensity(LightStart * Left * Left);
	if (Age >= Life)
	{
		Destroy();
	}
}
