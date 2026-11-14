// =====================================================================================
//  GRMenuGameMode.h  -  the start menu (title card, mode select, loading screen)
// =====================================================================================
//
//  WHAT THIS IS
//  The "rules" for the main-menu level (L_MainMenu). When that level starts, this game
//  mode puts the menu on screen, one page at a time:
//    0. STARTING: a loading screen with a filling bar while the game warms up.
//    1. TITLE:    the game's title on a dark screen (same look as the in-game menu): PLAY, SETTINGS, QUIT.
//    2. MODES:    SOLO: Endless Runner, or Endless Runner with an AI partner (L_Endless).
//                 MULTIPLAYER: 1v1, 2v2, 3v3, 5v5 squads and PvE (on L_FloatingIslands_2).
//    3. SETTINGS: mouse sensitivity, gas difficulty, partner chatter (saved, see GRSettings.h).
//    4. LOADING:  shown while the chosen level opens (in a packaged game Unreal's loading-
//                 screen system keeps it up until the level is ready). Still to do for VR:
//                 the headset's own loading splash (its header moved in Unreal 5.8).
//  The chosen mode is passed to the game level as options ("?mode=endless?partner=1"), so the
//  game can read it. Multiplayer isn't built yet, so the team modes play solo for now.
//
//  WHY C++ AND NOT A BLUEPRINT WIDGET
//  Project rule: gameplay and UI logic live in C++ you can read. The menu is drawn with
//  Slate, the UI toolkit Unreal's own editor is built with (the code is in the .cpp).
//
//  VR NOTE
//  This is a flat, on-screen menu for desktop testing. In the headset a menu must sit IN
//  the world (a floating panel you point at), not stuck to your eyes: that's a later step.
//
//  HOW TO READ THIS FILE
//  - "UCLASS()" / "GENERATED_BODY()" = boilerplate that lets Unreal see the class.
//  - "UPROPERTY(EditAnywhere)" = can be changed in the editor's World Settings without code.
//  - A ".h" file only LISTS what exists; the ".cpp" file says what each thing DOES.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GRMenuGameMode.generated.h"

class UTexture2D;
class SWidget;

UCLASS()
class GRAPPLERUNNER_API AGRMenuGameMode : public AGameModeBase   // a game mode: the rules of one level
{
	GENERATED_BODY()

public:
	AGRMenuGameMode();

	// The picture behind the menu (the EXIT SUIT portfolio cover, imported by Scripts/make_main_menu.py).
	UPROPERTY(EditAnywhere, Category = "Menu") TSoftObjectPtr<UTexture2D> TitleCard =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GrappleRunner/UI/T_TitleCard.T_TitleCard")));
	// The level each mode opens (every mode uses the floating islands for now).
	// MULTIPLAYER MAP ROTATION (Grey, 14 Nov): five maps, and each match draws one at random,
	// like Valorant. A map that isn't built yet is skipped, and the map you just played isn't
	// drawn twice in a row (when there's more than one to choose from).
	// Only the Floating Islands exists so far: the other four names are placeholders. To add a
	// map, save it in Content/GrappleRunner/Maps under one of these names (or change the names here).
	UPROPERTY(EditAnywhere, Category = "Menu|Multiplayer") TArray<FName> MultiplayerMaps =
		{ TEXT("L_FloatingIslands_2"), TEXT("L_MP_Map2"), TEXT("L_MP_Map3"), TEXT("L_MP_Map4"), TEXT("L_MP_Map5") };
	// The names shown on the loading screen, in the same order as the maps above.
	UPROPERTY(EditAnywhere, Category = "Menu|Multiplayer") TArray<FString> MultiplayerMapNames =
		{ TEXT("FLOATING ISLANDS"), TEXT("MAP 2"), TEXT("MAP 3"), TEXT("MAP 4"), TEXT("MAP 5") };
	// The solo endless run's level: a start island in open sky; the run is built ahead of you.
	UPROPERTY(EditAnywhere, Category = "Menu") FName EndlessLevel = TEXT("L_Endless");
	// How long the "starting" loading screen shows when the game opens (seconds).
	UPROPERTY(EditAnywhere, Category = "Menu") float BootSeconds = 2.5f;
	// How long the loading screen shows before the level opens (seconds), so you can see it.
	UPROPERTY(EditAnywhere, Category = "Menu") float LoadingScreenSeconds = 1.0f;

	void StartMode(const FString& Mode);   // a mode button was clicked: show "loading", then open the level
	void Quit();                           // the QUIT button
	bool IsLoading() const { return !PendingMode.IsEmpty(); }
	float GetBootProgress() const { return FMath::Clamp(BootClock / FMath::Max(0.1f, BootSeconds), 0.f, 1.f); }
	virtual void Tick(float DeltaSeconds) override;
	const FString& GetPendingMode() const { return PendingMode; }
	const FString& GetPendingMapName() const { return PendingMapName; }   // the map drawn for this match ("" for solo)

protected:
	virtual void BeginPlay() override;     // put the menu on screen
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;   // take it off again

private:
	UPROPERTY() TObjectPtr<UTexture2D> CardTexture;   // keeps the picture loaded while the menu shows
	TSharedPtr<SWidget> Menu;                          // the menu itself (built in the .cpp)
	FString PendingMode;                               // the mode being loaded ("" = none yet)
	FName PendingMap;                                  // the multiplayer map drawn for this match
	FString PendingMapName;
	int32 DrawMap() const;                             // picks the map: index into MultiplayerMaps
	FTimerHandle LoadTimer;
	float BootClock = 0.f;
	void OpenGameLevel();
};
