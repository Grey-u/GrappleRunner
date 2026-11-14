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
#include "Brushes/SlateRoundedBoxBrush.h"
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
#include "Widgets/Images/SThrobber.h"
#include "GRSettings.h"
#include "MoviePlayer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
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
	// Same look as the in-game menu (Naomi, 14 Nov): dark charcoal, off-white writing, sage green.
	const FLinearColor Back(0.004f, 0.006f, 0.005f, 1.f);      // the background: near-black with a hint of green
	const FLinearColor ButtonBack(0.02f, 0.027f, 0.022f, 1.f); // buttons, a step lighter
	const FLinearColor Ink(0.88f, 0.91f, 0.86f, 1.f);         // main writing: soft off-white
	const FLinearColor Paper = Ink;                            // writing on buttons
	const FLinearColor Orange(0.26f, 0.38f, 0.19f, 1.f);      // the accent (name kept from the old orange): sage green
	const FLinearColor Grey(0.45f, 0.52f, 0.43f, 1.f);        // quiet writing: muted grey-green
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

		// The background: plain dark (the old portfolio cover was only a placeholder).
		CardBrush = FSlateColorBrush(Back);

		// Button look: dark block, sage green when the pointer is over it.
		ButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		ButtonStyle.SetNormal(FSlateColorBrush(ButtonBack));
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
					// A big, faint sage "moon" on the right and a thin line on the left: quiet decoration.
					+ SOverlay::Slot()
					[
						SNew(SCanvas)
						+ SCanvas::Slot().Position(FVector2D(1060.f, 120.f)).Size(FVector2D(840.f, 840.f))
						[
							SNew(SImage).Image(&Moon).ColorAndOpacity(FLinearColor(Orange.R, Orange.G, Orange.B, 0.10f))
						]
						+ SCanvas::Slot().Position(FVector2D(1240.f, 300.f)).Size(FVector2D(480.f, 480.f))
						[
							SNew(SImage).Image(&Moon).ColorAndOpacity(FLinearColor(Orange.R, Orange.G, Orange.B, 0.08f))
						]
						+ SCanvas::Slot().Position(FVector2D(120.f, 140.f)).Size(FVector2D(4.f, 800.f))
						[
							SNew(SImage).Image(&LineBrush).ColorAndOpacity(Orange)
						]
					]
					+ SOverlay::Slot()
					[
						// Five pages, one shown at a time:
						// 0 = starting (loading bar), 1 = title, 2 = modes, 3 = settings, 4 = loading a level.
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda([this]()
						{
							if (!Owner.IsValid()) { return Page; }
							if (Owner->IsLoading()) { return 4; }
							return Owner->GetBootProgress() < 1.f ? 0 : Page;
						})
						+ SWidgetSwitcher::Slot()[ Panel(BootPage()) ]
						+ SWidgetSwitcher::Slot()[ Panel(TitlePage()) ]
						+ SWidgetSwitcher::Slot()[ Panel(ModePage()) ]
						+ SWidgetSwitcher::Slot()[ Panel(SettingsPage()) ]
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
	FSlateColorBrush BarBack = FSlateColorBrush(GRMenuStyle::Grey * 0.6f);   // the loading bar's empty track
	FSlateColorBrush BarFill = FSlateColorBrush(GRMenuStyle::Orange);        // ...and its fill
	FSlateRoundedBoxBrush Moon = FSlateRoundedBoxBrush(FLinearColor::White, 420.f);   // a circle (corners rounded all the way)
	FSlateColorBrush LineBrush = FSlateColorBrush(FLinearColor::White);
	int32 Page = 1;   // the title, once the starting screen is done

	// Every page: the game's title at the top, then the page's own content under it.
	TSharedRef<SWidget> Panel(TSharedRef<SWidget> Content)
	{
		using namespace GRMenuStyle;
		return SNew(SCanvas)
			+ SCanvas::Slot().Position(FVector2D(160.f, 130.f)).Size(FVector2D(760.f, 900.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Label(TEXT("A VR GAME FOR THE EXIT SUIT"), 13, Grey) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 34.f)[ Label(TEXT("GRAPPLE RUNNER"), 64, Ink) ]
				+ SVerticalBox::Slot().AutoHeight()[ SNew(SBox).WidthOverride(520.f)[ Content ] ]
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
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("PLAY"), TEXT("choose a mode"), [this]() { Page = 2; }) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("SETTINGS"), TEXT("controls and comfort"), [this]() { Page = 3; }) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("QUIT"), TEXT(""), [this]() { if (Owner.IsValid()) { Owner->Quit(); } }) ];
	}

	TSharedRef<SWidget> ModePage()
	{
		using namespace GRMenuStyle;
		auto Start = [this](const TCHAR* Mode) { return [this, Mode]() { if (Owner.IsValid()) { Owner->StartMode(Mode); } }; };
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[ Label(TEXT("CHOOSE A MODE"), 24, Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 6.f)[ Label(TEXT("SOLO  -  ENDLESS RUN"), 11, Grey) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("ENDLESS RUNNER"), TEXT("swing as far as your gas lets you"), Start(TEXT("endless"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("ENDLESS + AI PARTNER"), TEXT("a partner who helps and talks"), Start(TEXT("endless_partner"))) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 6.f)[ Label(TEXT("MULTIPLAYER  -  FLOATING ISLANDS (solo practice for now)"), 11, Grey) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("1 v 1"), TEXT("duel"), Start(TEXT("1v1"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("2 v 2"), TEXT("pairs"), Start(TEXT("2v2"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("3 v 3"), TEXT("trios"), Start(TEXT("3v3"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("5 v 5"), TEXT("full squads"), Start(TEXT("5v5"))) ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("PvE"), TEXT("monsters"), Start(TEXT("pve"))) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[ Button(TEXT("BACK"), TEXT(""), [this]() { Page = 1; }) ];
	}

	// The starting screen: title, a bar that fills, and what's happening.
	TSharedRef<SWidget> BootPage()
	{
		using namespace GRMenuStyle;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[ Label(TEXT("LOADING"), 14, Grey) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).WidthOverride(420.f).HeightOverride(10.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[ SNew(SImage).Image(&BarBack) ]
					+ SOverlay::Slot().HAlign(HAlign_Left)
					[
						SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(420.f * (Owner.IsValid() ? Owner->GetBootProgress() : 0.f)); })
						[
							SNew(SImage).Image(&BarFill)
						]
					]
				]
			];
	}

	// Settings: each row is a name, its value, and buttons to change it. Saved straight away.
	TSharedRef<SWidget> SettingRow(const FString& Name, TFunction<FString()> Value, TFunction<void()> Less, TFunction<void()> More)
	{
		using namespace GRMenuStyle;
		return SNew(SBox).Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ Label(Name, 14, Ink) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)
			[
				SNew(SBox).WidthOverride(110.f).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14)).ColorAndOpacity(FSlateColor(Orange))
					.Text_Lambda([Value]() { return FText::FromString(Value()); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(56.f)[ Button(TEXT("-"), TEXT(""), Less) ] ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)[ SNew(SBox).WidthOverride(56.f)[ Button(TEXT("+"), TEXT(""), More) ] ]
		];
	}

	TSharedRef<SWidget> SettingsPage()
	{
		using namespace GRMenuStyle;
		auto Sens = [](float Step) { return [Step]() { GRSettings::SetFloat(TEXT("MouseSensitivity"), FMath::Clamp(GRSettings::MouseSensitivity() + Step, 0.1f, 3.f)); }; };
		auto Gas = [](int32 Step) { return [Step]() { GRSettings::SetFloat(TEXT("GasDifficulty"), static_cast<float>(FMath::Clamp(GRSettings::GasDifficulty() + Step, 0, 2))); }; };
		auto Chat = [](bool bOn) { return [bOn]() { GRSettings::SetFloat(TEXT("PartnerChatter"), bOn ? 1.f : 0.f); }; };
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)[ Label(TEXT("SETTINGS"), 24, Ink) ]
			+ SVerticalBox::Slot().AutoHeight()[ SettingRow(TEXT("MOUSE SENSITIVITY"), []() { return FString::Printf(TEXT("%.1f"), GRSettings::MouseSensitivity()); }, Sens(-0.1f), Sens(0.1f)) ]
			+ SVerticalBox::Slot().AutoHeight()[ SettingRow(TEXT("GAS DIFFICULTY"), []() { return GRSettings::GasDifficultyName(); }, Gas(-1), Gas(1)) ]
			+ SVerticalBox::Slot().AutoHeight()[ SettingRow(TEXT("PARTNER CHATTER"), []() { return FString(GRSettings::PartnerChatter() ? TEXT("ON") : TEXT("OFF")); }, Chat(false), Chat(true)) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 14.f)
			  [ Label(TEXT("Relaxed gas burns 40% slower; hard burns 40% faster."), 11, Grey, "Regular") ]
			+ SVerticalBox::Slot().AutoHeight()[ Button(TEXT("BACK"), TEXT(""), [this]() { Page = 1; }) ];
	}

	TSharedRef<SWidget> LoadingPage()
	{
		using namespace GRMenuStyle;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Label(TEXT("LOADING"), 30, Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16)).ColorAndOpacity(FSlateColor(Orange))
				.Text_Lambda([this]() { return FText::FromString(Owner.IsValid() ? Owner->GetPendingMode().ToUpper() : FString()); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f)[ SNew(SBox).HAlign(HAlign_Left)[ SNew(SThrobber) ] ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f)
			  [ Label(TEXT("Team modes play solo until multiplayer is built."), 11, Grey, "Regular") ];
	}
};

// -------------------------------------------------------------------------------------
//  The game mode
// -------------------------------------------------------------------------------------
AGRMenuGameMode::AGRMenuGameMode()
{
	DefaultPawnClass = nullptr;   // the menu has no body to walk around in
	PrimaryActorTick.bCanEverTick = true;   // to fill the starting screen's bar
}

void AGRMenuGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BootClock += DeltaSeconds;
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
	// Solo modes go to the endless level; the team modes to the floating islands.
	const bool bEndless = PendingMode.StartsWith(TEXT("endless"));
	const FName Level = bEndless ? EndlessLevel : GameLevel;
	const FString Options = bEndless
		? FString::Printf(TEXT("mode=endless?partner=%d"), PendingMode == TEXT("endless_partner") ? 1 : 0)
		: FString::Printf(TEXT("mode=%s"), *PendingMode);

	// A real loading screen for the time the level takes to open (packaged game only:
	// in the editor the level opens inside the editor, which has its own progress).
	if (!GIsEditor && IsMoviePlayerEnabled())
	{
		using namespace GRMenuStyle;
		FLoadingScreenAttributes Screen;
		Screen.bAutoCompleteWhenLoadingCompletes = true;
		Screen.MinimumLoadingScreenDisplayTime = 1.f;
		Screen.WidgetLoadingScreen = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Back)
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("GRAPPLE RUNNER"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 48)).ColorAndOpacity(FSlateColor(Ink))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("LOADING"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16)).ColorAndOpacity(FSlateColor(Grey))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 16.f)[ SNew(SThrobber) ]
			];
		GetMoviePlayer()->SetupLoadingScreen(Screen);
	}
	UGameplayStatics::OpenLevel(this, Level, true, Options);
}

void AGRMenuGameMode::Quit()
{
	UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}
