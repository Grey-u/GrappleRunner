// =====================================================================================
//  GRLootOrb.cpp  -  what a loot orb DOES (see the .h for the overview)
// =====================================================================================
#include "GRLootOrb.h"
#include "GrappleRig.h"

#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

int32 AGRLootOrb::CollectedThisRun = 0;

namespace
{
	// The MayuOrbs effects (the small versions: an orb about a hand across).
	const TCHAR* OrbSystems[] = {
		TEXT("/Game/MayuOrbs/02_Orbs/Orb_1/NiagaraSystem/NS_Orb_1_Small.NS_Orb_1_Small"),
		TEXT("/Game/MayuOrbs/02_Orbs/Orb_2/NiagaraSystem/NS_Orb_2_Small.NS_Orb_2_Small"),
		TEXT("/Game/MayuOrbs/02_Orbs/Orb_3/NiagaraSystems/NS_Orb_3_Small.NS_Orb_3_Small"),
		TEXT("/Game/MayuOrbs/02_Orbs/Orb_4/NiagaraSystems/NS_Orb_4_Small.NS_Orb_4_Small"),
		TEXT("/Game/MayuOrbs/02_Orbs/Orb_5/NiagaraSystems/NS_Orb_5_Small.NS_Orb_5_Small") };
	// Played where you collect one: a quick sparkle (Free Spells pack).
	const TCHAR* CollectFX = TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Mana.NS_Free_Spells_Buff_Mana");
}

AGRLootOrb::AGRLootOrb()
{
	PrimaryActorTick.bCanEverTick = true;
	Effect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Effect"));
	RootComponent = Effect;
	Effect->SetAutoActivate(true);
}

void AGRLootOrb::BeginPlay()
{
	Super::BeginPlay();
	// A random orb look each time (blue, purple, green...).
	const int32 Pick = FMath::RandRange(0, UE_ARRAY_COUNT(OrbSystems) - 1);
	if (UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, OrbSystems[Pick]))
	{
		Effect->SetAsset(Sys);
		Effect->Activate(true);
	}
	for (TActorIterator<AGrappleRig> It(GetWorld()); It; ++It)
	{
		Rig = *It;
		break;
	}
	Home = GetActorLocation();
	Phase = FMath::FRandRange(0.f, 2.f * PI);
}

void AGRLootOrb::Launch(const FVector& V)
{
	Velocity = V;
	LifeSeconds = 25.f;
}

void AGRLootOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (LifeSeconds > 0.f && Age > LifeSeconds)
	{
		Destroy();   // a dropped orb nobody picked up
		return;
	}
	AGrappleRig* R = Rig.Get();
	if (!R)
	{
		return;
	}
	const FVector Me = GetActorLocation();
	const FVector Body = R->GetBodyPosition() + FVector(0.f, 0.f, 60.f);   // about chest height
	const float Dist = FVector::Dist(Me, Body);

	// Collected: a little gas, count it, sparkle, gone.
	if (Dist < CollectRadius)
	{
		R->AddGas(GasPerOrb);
		++CollectedThisRun;
		if (UNiagaraSystem* Fx = LoadObject<UNiagaraSystem>(nullptr, CollectFX))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Fx, Me, FRotator::ZeroRotator, FVector(0.5f));
		}
		Destroy();
		return;
	}

	// Close enough (and not still flying out of a burst): home in, faster and faster.
	if (Dist < MagnetRadius && Age > 0.35f)
	{
		bHoming = true;
	}
	FVector Next;
	if (bHoming)
	{
		const FVector Want = (Body - Me).GetSafeNormal() * FMath::Max(2500.f, R->GetVelocity().Size() * 1.4f);
		Velocity = FMath::VInterpTo(Velocity, Want, DeltaSeconds, 8.f);
		Next = Me + Velocity * DeltaSeconds;
	}
	else if (!Velocity.IsNearlyZero(5.f))
	{
		Velocity *= FMath::Max(0.f, 1.f - 3.f * DeltaSeconds);   // a dropped orb slows to a hover
		Next = Me + Velocity * DeltaSeconds;
		Home = Next;
	}
	else
	{
		// Hanging in the air: bob gently.
		Next = Home + FVector(0.f, 0.f, 25.f * FMath::Sin(Age * 2.2f + Phase));
	}
	SetActorLocation(Next);
}
