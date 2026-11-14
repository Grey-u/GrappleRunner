// =====================================================================================
//  GRMenu.h  -  the floating game menu (inspired by Sword Art Online's menus)
// =====================================================================================
//
//  WHAT THIS IS
//  Press Tab or M (desktop) or the left controller's menu button (VR) and a menu appears
//  floating in the air about 1.2 m in front of you, facing you. It is a panel IN THE WORLD,
//  not stuck to the screen (VR comfort rule: no HUD glued to your face).
//
//  WHAT'S ON IT (left to right)
//    1. Status card: your name, a gas bar (styled like SAO's HP bar) and level.
//       Under it, the GEAR PANEL: a body outline with round slots round it (blades, ODM
//       gear, gas tanks...). Point at a slot to see what's in it.
//    2. The ICON COLUMN: round white buttons (Player, Party, Items, Skills, Map, Settings,
//       Log Out). The one you pick turns amber.
//    3. The LIST: the picked category's entries as pill-shaped buttons. Items: your weapons
//       (pick one to equip it). Settings: mouse sensitivity. Log Out: back to the main menu.
//    4. The INFO PANEL: what the pointed-at entry is; otherwise your current quest.
//
//  HOW YOU POINT AND CLICK
//  A "widget interaction" pointer: on desktop it follows the mouse cursor (the cursor shows
//  while the menu is open, and looking around pauses); in VR it is a laser from your right
//  hand and the trigger clicks.
//
//  HOW IT'S BUILT
//  The menu is drawn with Slate, Unreal's own UI code (the same thing the editor is made of),
//  written here in C++ instead of in a Blueprint widget, so all of it lives in this file.
//  It's shown on a "widget component": a flat panel in the world that displays UI.
//
//  HOW TO READ THIS FILE
//  - "UPROPERTY(EditAnywhere)" = a value you can change in the Details panel without code.
//  - Sizes in the UI are pixels on the panel; the panel itself is scaled to centimetres.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateBrush.h"
#include "GRMenu.generated.h"

class AGrappleRig;
class AGRArsenal;
class SWidget;
class UTexture2D;
class UWidgetComponent;
class UWidgetInteractionComponent;

UCLASS()
class GRAPPLERUNNER_API AGRMenu : public AActor
{
	GENERATED_BODY()

public:
	AGRMenu();
	virtual void Tick(float DeltaSeconds) override;

	// Is a menu open in this world? (The weapons and hooks ignore the mouse while it is.)
	static bool IsOpen(const UObject* WorldContext);

	void Open();
	void Close();

	UPROPERTY(EditAnywhere, Category = "Menu") float Distance = 120.f;          // how far in front of your eyes it floats (cm)
	UPROPERTY(EditAnywhere, Category = "Menu") float Drop = 12.f;               // how far below eye level (cm), so you look slightly down at it
	UPROPERTY(EditAnywhere, Category = "Menu") float WidthCm = 120.f;           // how wide it is in the world (cm)
	UPROPERTY(EditAnywhere, Category = "Menu") float OpenSeconds = 0.22f;       // the pop-in animation
	UPROPERTY(EditAnywhere, Category = "Menu") FString PlayerName = TEXT("NAOMI");
	UPROPERTY(EditAnywhere, Category = "Menu") FLinearColor Amber = FLinearColor(1.f, 0.66f, 0.08f);   // the "picked" colour
	UPROPERTY(EditAnywhere, Category = "Menu") FName MainMenuLevel = TEXT("L_MainMenu");

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UWidgetComponent> Panel;
	UPROPERTY() TObjectPtr<UWidgetInteractionComponent> Pointer;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> Textures;     // keeps the icon pictures loaded
	TWeakObjectPtr<AGrappleRig> Rig;
	TSharedPtr<SWidget> Root;

	bool bOpen = false;
	float Alpha = 0.f;            // 0 = closed, 1 = fully open (for the pop-in animation)
	float SavedSensitivity = 0.f;
	int32 Category = 0;           // which round icon is picked
	int32 HoverEntry = -1;        // which list entry the pointer is over
	int32 HoverSlot = -1;         // which gear slot the pointer is over
	int32 HoverIcon = -1;         // which round icon the pointer is over
	bool bPointerDown = false;    // VR trigger held last frame (to spot a fresh press)

	// One row in the list on the right of the icons.
	struct FEntry
	{
		FText Label;
		FText Info;                        // shown in the info panel when pointed at
		TFunction<void()> Action;          // what clicking it does (may be empty)
		TFunction<bool()> IsOn;            // shows it amber when true (e.g. the weapon you hold)
	};
	TArray<FEntry> Entries;               // the current category's rows

	// Brushes (how a shape is painted). They must live as long as the UI, so they live here.
	FSlateBrush CircleBrush, PillBrush, PanelBrush, BarBackBrush, BarBrush, WhiteBrush;
	TArray<TSharedPtr<FSlateBrush>> IconBrushes;
	TSharedPtr<FSlateBrush> BodyBrush;

	void BuildBrushes();
	TSharedRef<SWidget> BuildUI();
	TSharedRef<SWidget> BuildStatusAndGear();
	TSharedRef<SWidget> BuildIconColumn();
	TSharedRef<SWidget> BuildList();
	TSharedRef<SWidget> BuildInfo();
	void SelectCategory(int32 Index);
	void Place();
	void ReadInput();
	AGRArsenal* FindArsenal() const;
	TSharedPtr<class SVerticalBox> ListBox;
	void RefreshList();
};
