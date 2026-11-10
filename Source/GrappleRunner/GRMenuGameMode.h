// =====================================================================================
//  GRMenuGameMode.h  -  the start menu (title card, mode select, loading screen)
// =====================================================================================
//
//  WHAT THIS IS
//  The "rules" for the main-menu level (L_MainMenu). When that level starts, this game
//  mode puts the menu on screen:
//    1. TITLE:   the EXIT SUIT portfolio cover as the title card, with PLAY and QUIT.
//    2. MODES:   1v1, 2v2, 3v3, 5v5 squads (the planned five-person teams) and PvE.
//    3. LOADING: the cover again with "Loading..." while the game level opens.
//  The chosen mode is passed to the game level as an option ("?mode=2v2"), so the game
//  can read it later. Multiplayer isn't built yet, so every mode currently plays solo.
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
	UPROPERTY(EditAnywhere, Category = "Menu") FName GameLevel = TEXT("L_FloatingIslands_2");
	// How long the loading screen shows before the level opens (seconds), so you can see it.
	UPROPERTY(EditAnywhere, Category = "Menu") float LoadingScreenSeconds = 1.0f;

	void StartMode(const FString& Mode);   // a mode button was clicked: show "loading", then open the level
	void Quit();                           // the QUIT button
	bool IsLoading() const { return !PendingMode.IsEmpty(); }
	const FString& GetPendingMode() const { return PendingMode; }

protected:
	virtual void BeginPlay() override;     // put the menu on screen
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;   // take it off again

private:
	UPROPERTY() TObjectPtr<UTexture2D> CardTexture;   // keeps the picture loaded while the menu shows
	TSharedPtr<SWidget> Menu;                          // the menu itself (built in the .cpp)
	FString PendingMode;                               // the mode being loaded ("" = none yet)
	FTimerHandle LoadTimer;
	void OpenGameLevel();
};
