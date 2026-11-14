// =====================================================================================
//  GRDayNight.cpp  -  what the day/night cycle DOES (see GRDayNight.h for the overview)
// =====================================================================================
#include "GRDayNight.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

AGRDayNight::AGRDayNight()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The four looks of the day: the "living Pandora" art direction (11 Nov, Naomi: "plants
	// should feel green, alive and real but fantastical - think Avatar"). Clean, true daylight
	// so greens stay green; humid blue-green air in the distance; golden dawn and dusk; and a
	// deep blue-teal night when the bioluminescent glow takes over.
	auto Key = [](FLinearColor Sun, float Lux, FLinearColor Fog, FLinearColor Glow, float SkyI)
	{
		FGRSkyKey K;
		K.SunColor = Sun; K.SunLux = Lux; K.FogColor = Fog; K.FogGlow = Glow; K.SkyLight = SkyI;
		return K;
	};
	Dawn = Key(FLinearColor(1.00f, 0.88f, 0.74f), 6.f, FLinearColor(0.50f, 0.52f, 0.58f), FLinearColor(1.0f, 0.78f, 0.55f), 0.8f);
	Noon = Key(FLinearColor(1.00f, 0.97f, 0.92f), 10.f, FLinearColor(0.36f, 0.62f, 0.92f), FLinearColor(1.0f, 0.95f, 0.85f), 1.25f);   // anime sky blue haze
	Dusk = Key(FLinearColor(1.00f, 0.60f, 0.40f), 6.f, FLinearColor(0.48f, 0.40f, 0.52f), FLinearColor(1.0f, 0.58f, 0.40f), 0.7f);
	Night = Key(FLinearColor(0.55f, 0.72f, 1.00f), 1.0f, FLinearColor(0.02f, 0.06f, 0.10f), FLinearColor(0.30f, 0.55f, 0.85f), 0.25f);   // deep blue-teal: the glow takes over
}

void AGRDayNight::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		Sun = It->GetComponent();
		break;
	}
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		Sky = It->GetLightComponent();
		break;
	}
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		Fog = It->GetComponent();
		break;
	}
	// Team modes: one day = one match. Endless and editor play: the day loops.
	const AGameModeBase* GM = GetWorld()->GetAuthGameMode();
	const FString Mode = GM ? UGameplayStatics::ParseOption(GM->OptionsString, TEXT("mode")) : FString();
	bLoop = Mode.IsEmpty() || Mode.Equals(TEXT("endless"), ESearchCase::IgnoreCase);
	Elapsed = StartAt * MatchMinutes * 60.f;
}

// Blend between the four looks. The day runs 0 (dawn) .. 0.3 (noon) .. 0.6 (dusk)
// .. 0.72 (night has fallen) .. 1 (end of the night).
FGRSkyKey AGRDayNight::KeyAt(float T) const
{
	struct FStop { float At; const FGRSkyKey* Look; };
	const FStop Stops[] = { {0.f, &Dawn}, {0.3f, &Noon}, {0.6f, &Dusk}, {0.72f, &Night}, {0.95f, &Night}, {1.f, &Dawn} };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Stops) - 1; ++i)
	{
		if (T <= Stops[i + 1].At)
		{
			const float A = FMath::SmoothStep(Stops[i].At, Stops[i + 1].At, T);   // smooth, not linear, between looks
			const FGRSkyKey& K0 = *Stops[i].Look;
			const FGRSkyKey& K1 = *Stops[i + 1].Look;
			FGRSkyKey K;
			K.SunColor = FMath::Lerp(K0.SunColor, K1.SunColor, A);
			K.SunLux = FMath::Lerp(K0.SunLux, K1.SunLux, A);
			K.FogColor = FMath::Lerp(K0.FogColor, K1.FogColor, A);
			K.FogGlow = FMath::Lerp(K0.FogGlow, K1.FogGlow, A);
			K.SkyLight = FMath::Lerp(K0.SkyLight, K1.SkyLight, A);
			return K;
		}
	}
	return Night;
}

void AGRDayNight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float DaySeconds = FMath::Max(10.f, MatchMinutes * 60.f);
	Elapsed += DeltaSeconds;
	if (bLoop)
	{
		Elapsed = FMath::Fmod(Elapsed, DaySeconds);
	}
	Progress = FMath::Clamp(Elapsed / DaySeconds, 0.f, 1.f);
	const FGRSkyKey K = KeyAt(Progress);

	// Where the light comes from. Day (0..0.66): the sun climbs from the sunrise side to
	// HighestSun at noon and sinks on the far side. Night: a moon, high and opposite.
	FRotator Dir;
	const float DayEnd = 0.66f;
	if (Progress < DayEnd)
	{
		const float D = Progress / DayEnd;
		const float Elevation = 3.f + (HighestSun - 3.f) * FMath::Sin(PI * D);
		Dir = FRotator(-Elevation, SunriseYaw + 180.f * D, 0.f);   // pitch below zero = shining down
	}
	else
	{
		// After sunset the light turns, over a short while, from where the sun set to the moon.
		const FRotator Sunset(-3.f, SunriseYaw + 180.f, 0.f);
		const FRotator Moon(-MoonHeight, SunriseYaw + 90.f, 0.f);
		const float Turn = FMath::SmoothStep(DayEnd, 0.72f, Progress);
		Dir = FQuat::Slerp(Sunset.Quaternion(), Moon.Quaternion(), Turn).Rotator();
	}
	if (Sun.IsValid())
	{
		Sun->GetOwner()->SetActorRotation(Dir);
		Sun->SetLightColor(K.SunColor);
		Sun->SetIntensity(K.SunLux);
	}
	if (Sky.IsValid())
	{
		Sky->SetIntensity(K.SkyLight);
		RecaptureTimer -= DeltaSeconds;
		if (RecaptureTimer <= 0.f)
		{
			Sky->RecaptureSky();          // update the sky light's picture of the sky now and then
			RecaptureTimer = 2.f;
		}
	}
	CurrentFog = K.FogColor;   // the planets read this, so they fade into the same haze
	// Bioluminescence: 0 by day, rising through dusk to 1 at night, falling again before dawn.
	if (UMaterialParameterCollection* Params = WorldParams.LoadSynchronous())
	{
		const float Darkness = FMath::Clamp(FMath::Clamp((Progress - 0.6f) / 0.12f, 0.f, 1.f) + FMath::Clamp((0.08f - Progress) / 0.08f, 0.f, 1.f), 0.f, 1.f);
		UKismetMaterialLibrary::SetScalarParameterValue(this, Params, TEXT("Night"), Darkness);
	}
	if (Fog.IsValid())
	{
		Fog->SetFogInscatteringColor(K.FogColor);
		Fog->SetDirectionalInscatteringColor(K.FogGlow);
	}
	if (!bLoop && Progress >= 1.f && !bAnnouncedEnd && GEngine)
	{
		bAnnouncedEnd = true;   // (team modes will end the round here, once multiplayer exists)
		GEngine->AddOnScreenDebugMessage(4203, 6.f, FColor(180, 190, 255), TEXT("The night is over: match ends"));
	}
}
