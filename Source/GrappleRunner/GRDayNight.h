// =====================================================================================
//  GRDayNight.h  -  the match clock is the sky: dawn, day, dusk, then night ends the match
// =====================================================================================
//
//  WHAT THIS IS
//  Instead of a countdown timer (like Valorant's), a match lasts one day. The sun rises,
//  crosses the sky, sets, and the night that follows is the last stretch: when night ends,
//  the match is over. Players read the time from the light, not from a number, which also
//  keeps text off the screen (a VR comfort rule).
//  In Endless mode (and in the editor) the day never ends: it loops.
//
//  It drives the level's sun, sky light and fog: their colours and brightness follow
//  keyframes (dawn, noon, dusk, night), blended smoothly in between. At night the sun
//  becomes a pale blue moon, high and opposite, so the islands are lit softly; the glowing
//  orbs, the planets and the coloured lights then carry the scene.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = change it in the Details panel without code.
//  - Colours: Red, Green, Blue from 0 to 1. Light brightness: lux.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GRDayNight.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;

// One moment of the day: how the light, sky and fog look then.
USTRUCT(BlueprintType)
struct FGRSkyKey
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Sky") FLinearColor SunColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, Category = "Sky") float SunLux = 8.f;
	UPROPERTY(EditAnywhere, Category = "Sky") FLinearColor FogColor = FLinearColor(0.4f, 0.4f, 0.5f);
	UPROPERTY(EditAnywhere, Category = "Sky") FLinearColor FogGlow = FLinearColor(1.f, 0.7f, 0.5f);   // the fog's colour toward the sun
	UPROPERTY(EditAnywhere, Category = "Sky") float SkyLight = 1.f;
};

UCLASS()
class GRAPPLERUNNER_API AGRDayNight : public AActor
{
	GENERATED_BODY()

public:
	AGRDayNight();
	virtual void Tick(float DeltaSeconds) override;

	// How long one day (one match) lasts, in minutes.
	UPROPERTY(EditAnywhere, Category = "Day and Night") float MatchMinutes = 12.f;
	// Where in the day play starts (0 = dawn, 0.3 = noon, 0.6 = dusk, 0.75 = night).
	UPROPERTY(EditAnywhere, Category = "Day and Night") float StartAt = 0.02f;
	// The direction the sun rises from (degrees round the sky; it sets on the opposite side).
	UPROPERTY(EditAnywhere, Category = "Day and Night") float SunriseYaw = -60.f;
	UPROPERTY(EditAnywhere, Category = "Day and Night") float HighestSun = 32.f;   // degrees above the horizon at noon (kept low: long hazy light all day)
	UPROPERTY(EditAnywhere, Category = "Day and Night") float MoonHeight = 40.f;

	UPROPERTY(EditAnywhere, Category = "Day and Night|Looks") FGRSkyKey Dawn;
	UPROPERTY(EditAnywhere, Category = "Day and Night|Looks") FGRSkyKey Noon;
	UPROPERTY(EditAnywhere, Category = "Day and Night|Looks") FGRSkyKey Dusk;
	UPROPERTY(EditAnywhere, Category = "Day and Night|Looks") FGRSkyKey Night;

	float GetDayProgress() const { return Progress; }   // 0..1 through the match
	FLinearColor GetFogColor() const { return CurrentFog; } // the haze colour right now

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<USkyLightComponent> Sky;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	bool bLoop = true;
	bool bAnnouncedEnd = false;
	float Elapsed = 0.f;
	float Progress = 0.f;
	FLinearColor CurrentFog = FLinearColor(0.7f, 0.55f, 0.44f);
	float RecaptureTimer = 0.f;
	FGRSkyKey KeyAt(float T) const;   // the blended look at this point in the day
};
