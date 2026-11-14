// =====================================================================================
//  GRMenu.cpp  -  how the floating menu is built and used (see GRMenu.h for the idea)
// =====================================================================================
//
//  THE PIECES
//    BuildBrushes   the shapes things are painted with: circles, pills, soft panels
//    BuildUI        the whole layout, left to right (status + gear | icons | list | info)
//    SelectCategory fills the list for the round icon you picked
//    Place          puts the panel 1.2 m in front of your eyes, facing you
//    ReadInput      Tab / M / VR menu button opens and closes. Every frame it casts a ray
//                   (centre of view on desktop, right hand in VR), finds which button the
//                   ray lands on (FindUnder), and on a fresh click runs that button's action
//    Tick           the pop-in animation
//
//  READING SLATE CODE
//  Slate describes UI as nested boxes. "SNew(SVerticalBox) + SVerticalBox::Slot() [ ... ]"
//  means "a column; in its first row put ...". [ ] holds a box's content. Anything written
//  "_Lambda([this]{ ... })" is worked out again every frame, so it can change live (a bar
//  that shrinks, a button that turns green).
// =====================================================================================
#include "GRMenu.h"
#include "GrappleRig.h"
#include "GRArsenal.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GRMenu"

namespace
{
	// The panel is drawn as a 1600 x 900 picture, then scaled to WidthCm in the world.
	const FVector2D PanelPixels(1600.f, 900.f);

	// Colours (Red, Green, Blue, see-through-ness 0..1)
	// Dark mode (Naomi, 14 Nov): charcoal panels with a hint of green, light writing, sage highlights.
	const FLinearColor PanelTint(0.006f, 0.009f, 0.007f, 0.94f);   // see-through charcoal panels
	const FLinearColor Button(0.018f, 0.024f, 0.02f, 0.97f);     // round buttons and pills, a step lighter
	const FLinearColor TextMain(0.88f, 0.91f, 0.86f, 1.f);      // soft off-white writing
	const FLinearColor IconGrey(0.62f, 0.68f, 0.60f, 1.f);      // icons that aren't picked
	const FLinearColor SlotEmpty(0.012f, 0.016f, 0.013f, 0.95f);  // gear slots with nothing in them
	const FLinearColor Disabled(0.04f, 0.04f, 0.04f, 0.5f);     // greyed-out buttons (Party, for now)
	const FLinearColor Divider(0.22f, 0.27f, 0.21f, 1.f);       // thin lines between sections
	const FLinearColor HoverTint(0.06f, 0.095f, 0.045f, 0.98f);   // deep sage glow under the pointer

	// Buttons with no look of their own (we paint the shapes ourselves, inside them).
	const FButtonStyle& PlainButton()
	{
		static FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource())
			.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		return Style;
	}

	FSlateFontInfo Font(int32 Size, const TCHAR* Weight = TEXT("Light"))
	{
		return FCoreStyle::GetDefaultFontStyle(Weight, Size);
	}

	// The round icons, in the order they appear in the column.
	const TCHAR* IconFiles[] = { TEXT("T_Icon_Player"), TEXT("T_Icon_Party"), TEXT("T_Icon_Items"), TEXT("T_Icon_Skills"),
		TEXT("T_Icon_Map"), TEXT("T_Icon_Settings"), TEXT("T_Icon_Logout") };
	const TCHAR* CategoryNames[] = { TEXT("Player"), TEXT("Party"), TEXT("Items"), TEXT("Skills"), TEXT("Map"), TEXT("Settings"), TEXT("Log Out") };
	constexpr int32 NumCategories = 7;

	// The gear slots round the body outline: name, where (pixels in the gear panel), filled?
	struct FGearSlot { const TCHAR* Name; FVector2D At; bool bFilled; int32 Blade; };   // Blade: 0 left, 1 right, -1 not a blade
	const FGearSlot GearSlots[] = {
		{ TEXT("Head band (empty)"), FVector2D(28, 70), false, -1 },
		{ TEXT("Left blade"), FVector2D(28, 190), true, 0 },
		{ TEXT("Gas tanks: twin canisters on the hips"), FVector2D(28, 310), true, -1 },
		{ TEXT("Boots: grip soles for landings"), FVector2D(28, 430), true, -1 },
		{ TEXT("Cloak (empty)"), FVector2D(292, 70), false, -1 },
		{ TEXT("Right blade"), FVector2D(292, 190), true, 1 },
		{ TEXT("ODM gear: twin hooks and reels"), FVector2D(292, 310), true, -1 },
		{ TEXT("Hook cables: 18 m each"), FVector2D(292, 430), true, -1 },
	};
}

AGRMenu::AGRMenu()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera has moved
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The flat panel in the world that shows the menu.
	Panel = CreateDefaultSubobject<UWidgetComponent>(TEXT("Panel"));
	Panel->SetupAttachment(RootComponent);
	Panel->SetUsingAbsoluteLocation(true);
	Panel->SetUsingAbsoluteRotation(true);
	Panel->SetUsingAbsoluteScale(true);
	Panel->SetWidgetSpace(EWidgetSpace::World);
	Panel->SetDrawSize(PanelPixels);
	Panel->SetPivot(FVector2D(0.5f, 0.5f));
	Panel->SetTwoSided(true);
	// Masked = every pixel is either solid or cut out. Panels are fully solid (a see-through
	// dark panel looked washed-out grey against the bright sky), and the gaps between them
	// are cut away so you still see the world around the menu.
	Panel->SetBlendMode(EWidgetBlendMode::Masked);
	Panel->SetCollisionProfileName(TEXT("UI"));               // the pointer's ray can hit it
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // ...but only while it's open
	Panel->SetVisibility(false);
	// The scene's exposure brightens a world-space panel (more so in shade), so the colours
	// above are set very dark to still read as dark on screen.
	Panel->SetTintColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.f));

	// The pointer: we tell it exactly what it's pointing at each frame ("Custom").
	Pointer = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("Pointer"));
	Pointer->SetupAttachment(RootComponent);
	Pointer->InteractionSource = EWidgetInteractionSource::Custom;
	Pointer->InteractionDistance = 600.f;
	Pointer->bShowDebug = false;
}

bool AGRMenu::IsOpen(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AGRMenu> It(World); It; ++It)
	{
		if (It->bOpen)
		{
			return true;
		}
	}
	return false;
}

AGRArsenal* AGRMenu::FindArsenal() const
{
	TActorIterator<AGRArsenal> It(GetWorld());
	return It ? *It : nullptr;
}

void AGRMenu::BeginPlay()
{
	Super::BeginPlay();
	TActorIterator<AGrappleRig> It(GetWorld());
	Rig = It ? *It : nullptr;
	BuildBrushes();
	Root = BuildUI();
	Panel->SetSlateWidget(Root);
	SelectCategory(2);   // start on Items
}

// =====================================================================================
//  Brushes: the painted shapes
// =====================================================================================
void AGRMenu::BuildBrushes()
{
	// Rounded boxes: white, so each use can tint them (white, sage green, grey).
	CircleBrush = FSlateRoundedBoxBrush(FLinearColor::White, 52.f);
	PillBrush = FSlateRoundedBoxBrush(FLinearColor::White, 29.f, FLinearColor(1.f, 1.f, 1.f, 0.06f), 1.f);   // faint light rim
	PanelBrush = FSlateRoundedBoxBrush(FLinearColor::White, 18.f, FLinearColor(0.55f, 0.65f, 0.47f, 0.45f), 2.f);   // thin sage rim
	BarBackBrush = FSlateRoundedBoxBrush(FLinearColor(0.2f, 0.22f, 0.25f, 0.5f), 7.f);
	BarBrush = FSlateRoundedBoxBrush(FLinearColor::White, 7.f);
	WhiteBrush = FSlateRoundedBoxBrush(FLinearColor::White, 2.f);

	// Pictures: our own icons (Art/UI/make_menu_icons.py), imported to /Game/GrappleRunner/UI/Menu.
	auto Picture = [this](const TCHAR* Name, FVector2D Size) -> TSharedPtr<FSlateBrush>
	{
		const FString Path = FString::Printf(TEXT("/Game/GrappleRunner/UI/Menu/%s.%s"), Name, Name);
		UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path);
		TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>();
		B->ImageSize = FVector2f(Size);
		if (Tex)
		{
			Textures.Add(Tex);
			B->SetResourceObject(Tex);
		}
		else
		{
			B->DrawAs = ESlateBrushDrawType::NoDrawType;   // not imported yet: draw nothing
		}
		return B;
	};
	IconBrushes.Reset();
	for (const TCHAR* File : IconFiles)
	{
		IconBrushes.Add(Picture(File, FVector2D(56.f, 56.f)));
	}
	IconBrushes.Add(Picture(TEXT("T_Icon_Quest"), FVector2D(40.f, 40.f)));   // index 7: the info panel's scroll
	BodyBrush = Picture(TEXT("T_BodyOutline"), FVector2D(200.f, 400.f));
}

// =====================================================================================
//  The layout
// =====================================================================================
TSharedRef<SWidget> AGRMenu::BuildUI()
{
	return SNew(SBox).WidthOverride(PanelPixels.X).HeightOverride(PanelPixels.Y)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(20.f, 30.f, 10.f, 30.f) [ BuildStatusAndGear() ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(26.f, 70.f, 10.f, 30.f) [ BuildIconColumn() ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(18.f, 70.f, 10.f, 30.f) [ BuildList() ]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(18.f, 70.f, 24.f, 60.f) [ BuildInfo() ]
	];
}

// ---- 1. Status card (name, gas bar, level) and the gear panel
TSharedRef<SWidget> AGRMenu::BuildStatusAndGear()
{
	TSharedRef<SCanvas> Gear = SNew(SCanvas);
	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(GearSlots)); ++i)
	{
		const FGearSlot& G = GearSlots[i];
		TSharedPtr<SButton> Btn;
		Gear->AddSlot().Position(G.At).Size(FVector2D(60.f, 60.f))
		[
			SAssignNew(Btn, SButton).ButtonStyle(&PlainButton())
			[
				SNew(SImage).Image(&CircleBrush)
				.ColorAndOpacity_Lambda([this, i]
				{
					const FGearSlot& S = GearSlots[i];
					AGRArsenal* A = FindArsenal();
					const int32 H = A ? A->HeldSlot() : -1;
					const bool bHeld = S.Blade >= 0 && H >= 0 && A->Weapons[H].Kind == EGRWeaponKind::Swords;   // blades in your hands
					if (bHeld) { return FSlateColor(Accent); }
					if (HoverSlot == i) { return FSlateColor(HoverTint); }
					return FSlateColor(S.bFilled ? Button : SlotEmpty);
				})
			]
		];
		SlotButtons.Add(Btn);   // remembered for our own hit test
	}

	return SNew(SVerticalBox)
	// Name, gas bar and level
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(SBorder).BorderImage(&PanelBrush).BorderBackgroundColor(PanelTint).Padding(FMargin(22.f, 14.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(PlayerName)).Font(Font(30, TEXT("Regular"))).ColorAndOpacity(TextMain)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 4.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot() [ SNew(SBox).WidthOverride(330.f).HeightOverride(16.f) [ SNew(SImage).Image(&BarBackBrush) ] ]
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SNew(SBox).HeightOverride(16.f)
					.WidthOverride_Lambda([this]
					{
						const AGrappleRig* R = Rig.Get();
						return FOptionalSize(R ? 330.f * FMath::Clamp(R->GetGas() / FMath::Max(1.f, R->GasMax), 0.f, 1.f) : 330.f);
					})
					[
						// Green while there's plenty, yellow when low, red when nearly empty (like SAO's HP bar).
						SNew(SImage).Image(&BarBrush).ColorAndOpacity_Lambda([this]
						{
							const AGrappleRig* R = Rig.Get();
							const float F = R ? R->GetGas() / FMath::Max(1.f, R->GasMax) : 1.f;
							return FSlateColor(F > 0.5f ? FLinearColor(0.45f, 0.85f, 0.2f) : (F > 0.2f ? FLinearColor(0.95f, 0.8f, 0.15f) : FLinearColor(0.9f, 0.2f, 0.15f)));
						})
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(17)).ColorAndOpacity(TextMain)
				.Text_Lambda([this]
				{
					const AGrappleRig* R = Rig.Get();
					return FText::Format(LOCTEXT("StatusLine", "GAS  {0} / {1}        Lv 1"),
						FText::AsNumber(FMath::RoundToInt(R ? R->GetGas() : 0.f)), FText::AsNumber(FMath::RoundToInt(R ? R->GasMax : 100.f)));
				})
			]
		]
	]
	// The gear panel
	+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
	[
		SNew(SBorder).BorderImage(&PanelBrush).BorderBackgroundColor(PanelTint).Padding(0.f)
		[
			SNew(SBox).WidthOverride(380.f).HeightOverride(590.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 40.f, 0.f, 0.f)
				[
					SNew(SImage).Image(BodyBrush.Get()).ColorAndOpacity(FLinearColor(0.07f, 0.09f, 0.065f, 0.95f))
				]
				+ SOverlay::Slot() [ Gear ]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(16.f, 0.f, 16.f, 20.f)
				[
					SNew(STextBlock).Font(Font(17)).ColorAndOpacity(TextMain).AutoWrapText(true).Justification(ETextJustify::Center)
					.Text_Lambda([this]
					{
						return HoverSlot >= 0 ? FText::FromString(GearSlots[HoverSlot].Name) : LOCTEXT("GearHint", "Point at a slot");
					})
				]
			]
		]
	];
}

// ---- 2. The column of round icons
TSharedRef<SWidget> AGRMenu::BuildIconColumn()
{
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
	for (int32 i = 0; i < NumCategories; ++i)
	{
		const bool bEnabled = i != 1;   // Party comes with multiplayer
		TSharedPtr<SButton> Btn;
		Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
		[
			SAssignNew(Btn, SButton).ButtonStyle(&PlainButton()).IsEnabled(bEnabled)
			[
				SNew(SBox).WidthOverride(96.f).HeightOverride(96.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage).Image(&CircleBrush).ColorAndOpacity_Lambda([this, i, bEnabled]
						{
							if (Category == i) { return FSlateColor(Accent); }
							if (HoverIcon == i) { return FSlateColor(HoverTint); }
							return FSlateColor(bEnabled ? Button : Disabled);
						})
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SImage).Image(IconBrushes[i].Get()).ColorAndOpacity_Lambda([this, i]
						{
							return FSlateColor(Category == i ? FLinearColor::White : IconGrey);
						})
					]
				]
			]
		];
		IconButtons.Add(Btn);   // remembered for our own hit test (click handled in ReadInput)
	}
	return Column;
}

// ---- 3. The list for the picked icon
TSharedRef<SWidget> AGRMenu::BuildList()
{
	return SNew(SBox).WidthOverride(420.f)
	[
		SNew(SVerticalBox)
		// The category name, in a sage-green pill (like SAO's highlighted tab)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
		[
			SNew(SBorder).BorderImage(&PillBrush).BorderBackgroundColor(Accent).Padding(FMargin(26.f, 8.f))
			[
				SNew(STextBlock).Font(Font(26, TEXT("Regular"))).ColorAndOpacity(FLinearColor::White)
				.Text_Lambda([this] { return FText::FromString(CategoryNames[Category]); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight() [ SAssignNew(ListBox, SVerticalBox) ]
	];
}

void AGRMenu::RefreshList()
{
	if (!ListBox.IsValid())
	{
		return;
	}
	ListBox->ClearChildren();
	EntryButtons.Reset();
	HoverEntry = -1;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		TSharedPtr<SButton> Btn;
		ListBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			SAssignNew(Btn, SButton).ButtonStyle(&PlainButton())
			[
				SNew(SBorder).BorderImage(&PillBrush).Padding(FMargin(26.f, 0.f)).VAlign(VAlign_Center)
				.BorderBackgroundColor_Lambda([this, i]
				{
					const bool bOn = Entries.IsValidIndex(i) && Entries[i].IsOn && Entries[i].IsOn();
					return FSlateColor(bOn ? Accent : (HoverEntry == i ? HoverTint : Button));
				})
				[
					SNew(SBox).HeightOverride(58.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(Entries[i].Label).Font(Font(22))
						.ColorAndOpacity_Lambda([this, i]
						{
							const bool bOn = Entries.IsValidIndex(i) && Entries[i].IsOn && Entries[i].IsOn();
							return FSlateColor(bOn ? FLinearColor::White : TextMain);
						})
					]
				]
			]
		];
		EntryButtons.Add(Btn);   // remembered for our own hit test (click handled in ReadInput)
	}
}

// ---- 4. The info panel: what you're pointing at, otherwise your quest
TSharedRef<SWidget> AGRMenu::BuildInfo()
{
	return SNew(SBorder).BorderImage(&PanelBrush).BorderBackgroundColor(PanelTint).Padding(FMargin(30.f, 24.f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SImage).Image(IconBrushes[7].Get()).ColorAndOpacity(IconGrey)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(28, TEXT("Regular"))).ColorAndOpacity(TextMain)
				.Text_Lambda([this]
				{
					return Entries.IsValidIndex(HoverEntry) ? Entries[HoverEntry].Label : LOCTEXT("QuestTitle", "Quest");
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f)
		[
			SNew(SBox).HeightOverride(2.f) [ SNew(SImage).Image(&WhiteBrush).ColorAndOpacity(Divider) ]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(STextBlock).Font(Font(20)).ColorAndOpacity(TextMain).AutoWrapText(true)
			.Text_Lambda([this]
			{
				return Entries.IsValidIndex(HoverEntry) ? Entries[HoverEntry].Info
					: LOCTEXT("QuestBody", "Keep the run alive.\n\nSwing, slice and dash as far through Aeloria as you can before you fall.\n\nScore = metres travelled x your flow multiplier. Chain hooks without touching the ground to raise it.\n\nSlice glowing gas pods to refill your tanks.");
			})
		]
	];
}

// =====================================================================================
//  What each round icon lists
// =====================================================================================
void AGRMenu::SelectCategory(int32 Index)
{
	Category = FMath::Clamp(Index, 0, NumCategories - 1);
	Entries.Reset();
	auto Add = [this](const FText& Label, const FText& Info, TFunction<void()> Action = nullptr, TFunction<bool()> IsOn = nullptr)
	{
		Entries.Add({ Label, Info, MoveTemp(Action), MoveTemp(IsOn) });
	};
	AGRArsenal* A = FindArsenal();
	AGrappleRig* R = Rig.Get();
	switch (Category)
	{
	case 0:   // Player
		Add(LOCTEXT("Status", "Status"), LOCTEXT("StatusInfo", "Level 1.\nYour gas is the bar under your name: it drains when you boost and refills on the ground."));
		Add(LOCTEXT("Gear", "Equipment"), LOCTEXT("GearInfo", "Your gear is on the body outline to the left. Point at a slot to see what's in it. Slots turn green when that item is in your hands."));
		break;
	case 1:   // Party (comes with multiplayer)
		Add(LOCTEXT("Invite", "Invite a friend"), LOCTEXT("InviteInfo", "Multiplayer comes in a later version."));
		break;
	case 2:   // Items: your weapons, pick one to equip it
		if (A)
		{
			for (int32 i = 0; i < A->Weapons.Num(); ++i)
			{
				const FGRWeapon& W = A->Weapons[i];
				const FText Info = W.Kind == EGRWeaponKind::Swords
					? LOCTEXT("BladesInfo", "Twin ODM blades, one in each hand. Slash with the left mouse button (or V), cross-cut with the right (or C). Plants fall in one hit; trees take several hacks.")
					: FText::Format(LOCTEXT("GunInfo", "{0}: {1} rounds per magazine. Left mouse fires, right mouse aims, T reloads (in VR you reload by hand)."), FText::FromString(W.Name), FText::AsNumber(W.Magazine));
				Add(FText::FromString(W.Name), Info, [this, i] { if (AGRArsenal* Ar = FindArsenal()) { Ar->Equip(i); } },
					[this, i] { AGRArsenal* Ar = FindArsenal(); return Ar && Ar->HeldSlot() == i; });
			}
		}
		Add(LOCTEXT("Canister", "Reserve gas canister  x1"), LOCTEXT("CanisterInfo", "Your second wind: grab it from your hip to survive one empty tank per run. (Coming with the gas pods.)"));
		break;
	case 3:   // Skills: what you can do, and the controls
		Add(LOCTEXT("SkGrapple", "Grapple"), LOCTEXT("SkGrappleInfo", "Fire a hook: Q (left) and E (right), or the mouse buttons with your weapons away. In VR: the triggers."));
		Add(LOCTEXT("SkReel", "Reel in"), LOCTEXT("SkReelInfo", "Hold to pull yourself up the cable. Shift stops pulling."));
		Add(LOCTEXT("SkBoost", "Gas boost"), LOCTEXT("SkBoostInfo", "Space: a burst of gas that pushes you forward. Uses gas."));
		Add(LOCTEXT("SkSlash", "Slash"), LOCTEXT("SkSlashInfo", "Blades out: left mouse or V. The blades take turns."));
		Add(LOCTEXT("SkCross", "Cross-cut"), LOCTEXT("SkCrossInfo", "Blades out: right mouse or C. Both blades at once, in an X. Counts as one and a half hacks."));
		break;
	case 4:   // Map
		Add(LOCTEXT("MapHere", "Aeloria: the floating islands"), LOCTEXT("MapHereInfo", "A city of spires and islands in the sky. The islands go on forever ahead of you in Endless Runner mode."));
		Add(LOCTEXT("MapRespawn", "Back to the start"), LOCTEXT("MapRespawnInfo", "Close the menu and press R to respawn on the first island."));
		break;
	case 5:   // Settings
	{
		const float Sens = R ? R->MouseSensitivity : 0.6f;
		Add(FText::Format(LOCTEXT("SensUp", "Mouse sensitivity  {0}   +"), FText::AsNumber(Sens)), LOCTEXT("SensUpInfo", "Look around faster with the mouse."),
			[this] { if (AGrappleRig* Rr = Rig.Get()) { Rr->MouseSensitivity = FMath::Clamp(Rr->MouseSensitivity + 0.1f, 0.1f, 3.f); } SelectCategory(5); });
		Add(LOCTEXT("SensDown", "Mouse sensitivity   -"), LOCTEXT("SensDownInfo", "Look around slower with the mouse."),
			[this] { if (AGrappleRig* Rr = Rig.Get()) { Rr->MouseSensitivity = FMath::Clamp(Rr->MouseSensitivity - 0.1f, 0.1f, 3.f); } SelectCategory(5); });
		break;
	}
	case 6:   // Log Out
		Add(LOCTEXT("ToMenu", "Back to the main menu"), LOCTEXT("ToMenuInfo", "Leave this run and go back to the start menu."),
			[this] { Close(); UGameplayStatics::OpenLevel(this, MainMenuLevel); });
		Add(LOCTEXT("Quit", "Quit the game"), LOCTEXT("QuitInfo", "Close Grapple Runner."),
			[this] { UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false); });
		break;
	default:
		break;
	}
	RefreshList();
}

// =====================================================================================
//  Opening, closing, placing
// =====================================================================================
void AGRMenu::Place()
{
	const AGrappleRig* R = Rig.Get();
	const UCameraComponent* Eyes = R ? R->GetEyes() : nullptr;
	if (!Eyes)
	{
		return;
	}
	const FVector EyeAt = Eyes->GetComponentLocation();
	// Where you're looking, but never more than 25 degrees up or down (so it never ends up on
	// the floor or overhead).
	FRotator Look = Eyes->GetComponentRotation();
	Look.Pitch = FMath::Clamp(Look.Pitch, -25.f, 25.f);
	Look.Roll = 0.f;
	const FVector Ahead = Look.Vector();
	const FVector At = EyeAt + Ahead * Distance - FVector(0.f, 0.f, Drop);
	// The panel's front faces along its X axis, so turn X toward your eyes.
	Panel->SetWorldLocationAndRotation(At, (EyeAt - At).Rotation());
}

void AGRMenu::Open()
{
	AGrappleRig* R = Rig.Get();
	if (bOpen || !R)
	{
		return;
	}
	bOpen = true;
	Place();
	Panel->SetVisibility(true);
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Panel->SetCollisionResponseToAllChannels(ECR_Ignore);
	Panel->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);   // only the pointer's ray stops on it
	SelectCategory(Category);
}

void AGRMenu::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // the ray passes through it again
	HoverIcon = HoverSlot = HoverEntry = -1;
}

// =====================================================================================
//  Every frame
// =====================================================================================
void AGRMenu::ReadInput()
{
	AGrappleRig* R = Rig.Get();
	APlayerController* PC = R ? R->GetPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	// Open / close: Tab or M on the keyboard; the left controller's menu button in VR.
	static const FKey Toggles[] = { EKeys::Tab, EKeys::M, FKey(TEXT("Vive_Left_Menu_Click")), FKey(TEXT("OculusTouch_Left_Menu_Click")),
		FKey(TEXT("MotionController_Left_Shoulder")) };
	for (const FKey& K : Toggles)
	{
		if (K.IsValid() && PC->WasInputKeyJustPressed(K))
		{
			bOpen ? Close() : Open();
			return;
		}
	}
	if (!bOpen)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape))
	{
		Close();
		return;
	}

	// What are you pointing at? Desktop: the centre of your view (look at a button, click).
	// VR: a ray from your right hand.
	FHitResult Hit;
	bool bHit = false;
	const USceneComponent* Ray = R->IsUsingVR() ? R->GetHand(1) : static_cast<const USceneComponent*>(R->GetEyes());
	if (Ray)
	{
		const FVector From = Ray->GetComponentLocation();
		const FVector To = From + Ray->GetForwardVector() * Pointer->InteractionDistance;
		// Ignore the player's own body (so the ray doesn't stop on your arm), but NOT this menu actor:
		// the panel belongs to this actor, so ignoring it would make the panel impossible to hit.
		FCollisionQueryParams Params(TEXT("GRMenuPointer"), false, R);
		bHit = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) && Hit.GetComponent() == Panel;
	}
	Pointer->SetCustomHitResult(bHit ? Hit : FHitResult());

	// Which button is under the pointer? Get2DHitLocation is the spot on the panel in
	// pixels (0..1600 across, 0..900 down). We turn that into Slate's "absolute" space and
	// check each button's rectangle.
	HoverIcon = HoverSlot = HoverEntry = -1;
	if (bHit && Root.IsValid())
	{
		const FVector2D Abs = Root->GetTickSpaceGeometry().LocalToAbsolute(Pointer->Get2DHitLocation());
		HoverIcon = FindUnder(IconButtons, Abs);
		HoverSlot = FindUnder(SlotButtons, Abs);
		HoverEntry = FindUnder(EntryButtons, Abs);
	}

	// Click: left mouse on desktop, the right trigger in VR. We act on the moment it goes down.
	bool bDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
	for (const TCHAR* Name : { TEXT("Vive_Right_Trigger_Click"), TEXT("OculusTouch_Right_Trigger_Click"), TEXT("MotionController_Right_Trigger") })
	{
		const FKey K(Name);
		bDown = bDown || (K.IsValid() && PC->IsInputKeyDown(K));
	}
	if (bDown && !bPointerDown)
	{
		if (HoverIcon >= 0 && IconButtons[HoverIcon]->IsEnabled())
		{
			SelectCategory(HoverIcon);                     // switch the list to that icon's category
		}
		else if (Entries.IsValidIndex(HoverEntry) && Entries[HoverEntry].Action)
		{
			Entries[HoverEntry].Action();                  // e.g. equip that weapon
		}
	}
	bPointerDown = bDown;
}

int32 AGRMenu::FindUnder(const TArray<TSharedPtr<SWidget>>& Buttons, const FVector2D& AbsolutePoint)
{
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		if (Buttons[i].IsValid() && Buttons[i]->GetTickSpaceGeometry().IsUnderLocation(AbsolutePoint))
		{
			return i;
		}
	}
	return -1;
}

void AGRMenu::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rig.IsValid())
	{
		TActorIterator<AGrappleRig> It(GetWorld());
		Rig = It ? *It : nullptr;
	}
	ReadInput();

	// Pop-in / pop-out: grows from 85% to full size and fades in, in OpenSeconds.
	Alpha = FMath::FInterpConstantTo(Alpha, bOpen ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(0.01f, OpenSeconds));
	const float Ease = FMath::SmoothStep(0.f, 1.f, Alpha);
	const float Scale = WidthCm / PanelPixels.X * FMath::Lerp(0.85f, 1.f, Ease);
	Panel->SetWorldScale3D(FVector(Scale));
	if (Root.IsValid())
	{
		Root->SetRenderOpacity(Ease);
	}
	if (!bOpen && Alpha <= 0.f && Panel->IsVisible())
	{
		Panel->SetVisibility(false);
	}
}

#undef LOCTEXT_NAMESPACE
