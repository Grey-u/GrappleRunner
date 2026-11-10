// =====================================================================================
//  GRMenuGameMode.cpp  -  what the start menu DOES (see the .h file for the overview)
// =====================================================================================
//
//  HOW TO READ THIS FILE
//  - The menu is drawn with Slate. A Slate layout is written as nested boxes:
//        SNew(SVerticalBox) + SVerticalBox::Slot() [ ...a widget... ]
//    "SNew" makes a widget, "+ Slot()" adds a space for one, and what's inside [ ] goes in it.
//  - "[this]() { ... }" is a small inline function (a "lambda"), used for "when clicked, do this".
//  - Positions are on a 1920 x 1080 design canvas; SScaleBox stretches it to any screen size.
//  - Colours are (Red, Green, Blue, Alpha) from 0 to 1. The palette copies the EXIT SUIT
//    portfolio: off-white, near-black type, and the suit's orange.
// =====================================================================================
#include "GRMenuGameMode.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace GRMenuStyle
{
	const FLinearColor Ink(0.012f, 0.012f, 0.012f, 1.f);      // near-black, like the portfolio type
	const FLinearColor Paper(0.85f, 0.83f, 0.80f, 1.f);       // the cover's off-white
	const FLinearColor Orange(1.f, 0.16f, 0.02f, 1.f);        // EXIT SUIT orange
	const FLinearColor Grey(0.25f, 0.25f, 0.25f, 1.f);
}

// -------------------------------------------------------------------------------------
//  The menu widget. It lives only in this file: nothing else needs to know its insides.
// -------------------------------------------------------------------------------------
class SGrappleMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGrappleMenu) : _Card(nullptr) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AGRMenuGameMode>, Owner)   // the game mode, to tell it what was clicked
		SLATE_ARGUMENT(UTexture2D*, Card)                         // the title card picture
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		Owner = Args._Owner;
		using namespace GRMenuStyle;

		// The background: the title card picture, or plain off-white if it isn't imported yet.
		if (Args._Card)
		{
			CardBrush.SetResourceObject(Args._Card);
			CardBrush.ImageSize = FVector2D(1920.f, 1080.f);
		}
		else
		{
			CardBrush = FSlateColorBrush(Paper);
		}

		// Button look: black block, orange when the mouse is over it (like the suit's orange parts).
		ButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		ButtonStyle.SetNormal(FSlateColorBrush(Ink));
		ButtonStyle.SetHovered(FSlateColorBrush(Orange));
		ButtonStyle.SetPressed(FSlateColorBrush(Orange * 0.8f));
		ButtonStyle.SetNormalPadding(FMargin(22.f, 12.f));
		ButtonStyle.SetPressedPadding(FMargin(22.f, 13.f, 22.f, 11.f));

		ChildSlot
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
			[
				SNew(SBox).WidthOverride(1920.f).HeightOverride(1080.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[ SNew(SImage).Image(&CardBrush) ]
					+ SOverlay::Slot()
					[
						// Three pages, one shown at a time: 0 = title, 1 = choose a mode, 2 = loading.
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda([this]() { return (Owner.IsValid() && Owner->IsLoading()) ? 2 : Page; })
						+ SWidgetSwitcher::Slot()[ Panel(TitlePage()) ]
						+ SWidgetSwitcher::Slot()[ Panel(ModePage()) ]
						+ SWidgetSwitcher::Slot()[ Panel(LoadingPage()) ]
					]
				]
			]
		];
	}

	// Keyboard focus on the menu, so it gets clicks straight away.
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	TWeakObjectPtr<AGRMenuGameMode> Owner;
	FSlateBrush CardBrush;
	FButtonStyle ButtonStyle;
	int32 Page = 0;

	// Every page sits in the empty space in the middle of the cover (right of "EXIT SUIT").
	TSharedRef<SWidget> Panel(TSharedRef<SWidget> Content)
	{
		return SNew(SCanvas)
			+ SCanvas::Slot().Position(FVector2D(640.f, 175.f)).Size(FVector2D(470.f, 470.f))
			[
				Content
			];
	}

	static TSharedRef<SWidget> Label(const FString& Text, int32 Size, const FLinearColor& Colour, const FName& Weight = "Bold")
	{
		return SNew(STextBlock).Text(FText::FromString(Text))
			.Font(FCoreStyle::GetDefaultFontStyle(Weight, Size))
			.ColorAndOpacity(FSlateColor(Colour));
	}

	TSharedRef<SWidget> Button(const FString& Text, const FString& Note, TFunction<void()> OnClick)
	{
		using namespace GRMenuStyle;
		return SNew(SBox).Padding(FMargin(0.f, 0.f, 0.f, 8.f))
		[
			SNew(SButton).ButtonStyle(&ButtonStyle)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ Label(Text, 20, Paper) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Label(Note, 10, Paper * 0.7f, "Regular") ]
			]
		];
	}

	TSharedRef<SWidget> TitlePage()
	{
		using namespace GRMenuStyle;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Label(TEXT("PROJECT 02  -  GAME"), 11, Grey, "Regular") ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[ Label(TEXT("GRAPPLE RUNNER"), 34, Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 26.f)
			  [ Label(TEXT("A VR grapple game built for the EXIT SUIT"), 12, Ink, "Regular") ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("PLAY"), TEXT("choose a mode"), [this]() { Page = 1; }) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("QUIT"), TEXT(""), [this]() { if (Owner.IsValid()) { Owner->Quit(); } }) ];
	}

	TSharedRef<SWidget> ModePage()
	{
		using namespace GRMenuStyle;
		auto Start = [this](const TCHAR* Mode) { return [this, Mode]() { if (Owner.IsValid()) { Owner->StartMode(Mode); } }; };
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)[ Label(TEXT("CHOOSE A MODE"), 24, Ink) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("1 v 1"), TEXT("duel"), Start(TEXT("1v1"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("2 v 2"), TEXT("pairs"), Start(TEXT("2v2"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("3 v 3"), TEXT("trios"), Start(TEXT("3v3"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("5 v 5"), TEXT("full squads"), Start(TEXT("5v5"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("PvE"), TEXT("monsters"), Start(TEXT("pve"))) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[ Button(TEXT("BACK"), TEXT(""), [this]() { Page = 0; }) ];
	}

	TSharedRef<SWidget> LoadingPage()
	{
		using namespace GRMenuStyle;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Label(TEXT("LOADING AELORIA"), 30, Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16)).ColorAndOpacity(FSlateColor(Orange))
				.Text_Lambda([this]() { return FText::FromString(Owner.IsValid() ? Owner->GetPendingMode().ToUpper() : FString()); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f)
			  [ Label(TEXT("Multiplayer isn't built yet: every mode plays solo for now."), 11, Grey, "Regular") ];
	}
};

// -------------------------------------------------------------------------------------
//  The game mode
// -------------------------------------------------------------------------------------
AGRMenuGameMode::AGRMenuGameMode()
{
	DefaultPawnClass = nullptr;   // the menu has no body to walk around in
}

void AGRMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	CardTexture = TitleCard.LoadSynchronous();   // may be null if the picture isn't imported yet: the menu copes

	if (!GEngine || !GEngine->GameViewport)
	{
		return;   // no screen (e.g. a server): nothing to draw
	}
	TSharedRef<SGrappleMenu> Widget = SNew(SGrappleMenu).Owner(this).Card(CardTexture.Get());
	Menu = Widget;
	GEngine->GameViewport->AddViewportWidgetContent(Widget, 10);

	// Show the mouse and send clicks to the menu instead of the game.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Widget);
		PC->SetInputMode(Mode);
	}
}

void AGRMenuGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Menu.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
	Super::EndPlay(Reason);
}

void AGRMenuGameMode::StartMode(const FString& Mode)
{
	if (IsLoading())
	{
		return;   // already loading: ignore double clicks
	}
	PendingMode = Mode;   // this switches the menu to its loading page
	// Wait a moment so the loading screen is actually drawn, then open the level.
	GetWorldTimerManager().SetTimer(LoadTimer, this, &AGRMenuGameMode::OpenGameLevel, FMath::Max(0.05f, LoadingScreenSeconds), false);
}

void AGRMenuGameMode::OpenGameLevel()
{
	// "?mode=2v2" travels with the level, so the game can read which mode was picked.
	UGameplayStatics::OpenLevel(this, GameLevel, true, FString::Printf(TEXT("mode=%s"), *PendingMode));
}

void AGRMenuGameMode::Quit()
{
	UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}
