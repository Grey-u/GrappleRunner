// =====================================================================================
//  GRGasGauge.cpp  -  how the gas gauge is drawn and where it floats
// =====================================================================================
//
//  THE PIECES
//    BeginPlay   makes the little panel (a Slate widget shown on a flat card in the world)
//    Build       the picture: "GAS", a bar that empties, and the percentage
//    Tick        reads the tank, eases the bar, and moves the card in front of your body
//    ColourFor   green / orange / red by how full the tank is
//
//  READING SLATE CODE
//  "SNew(SVerticalBox) + SVerticalBox::Slot() [ ... ]" = a column; [ ] holds what's inside.
//  "_Lambda([this]{ ... })" = worked out again every frame, so the bar can move live.
// =====================================================================================
#include "GRGasGauge.h"
#include "GrappleRig.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FVector2D GaugePixels(320.f, 92.f);   // the card's size in pixels (it's scaled to WidthCm)
	// Brushes must outlive the widget, so they live here for the whole game.
	FSlateRoundedBoxBrush& BackBrush() { static FSlateRoundedBoxBrush B(FLinearColor::White, 18.f, FLinearColor(0.55f, 0.65f, 0.47f, 0.5f), 2.f); return B; }
	FSlateRoundedBoxBrush& BarBrush() { static FSlateRoundedBoxBrush B(FLinearColor::White, 9.f); return B; }
}

FLinearColor AGRGasGauge::ColourFor(float Fraction)
{
	if (Fraction > 0.5f)
	{
		return FLinearColor(0.35f, 0.95f, 0.35f);   // green: plenty
	}
	if (Fraction > 0.2f)
	{
		return FLinearColor(1.f, 0.45f, 0.06f);     // orange: getting low
	}
	return FLinearColor(1.f, 0.12f, 0.08f);         // red: nearly empty
}

AGRGasGauge::AGRGasGauge()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the player has moved this frame, so it doesn't lag behind

	Panel = CreateDefaultSubobject<UWidgetComponent>(TEXT("Panel"));
	RootComponent = Panel;
	Panel->SetWidgetSpace(EWidgetSpace::World);
	Panel->SetDrawSize(GaugePixels);
	Panel->SetPivot(FVector2D(0.5f, 0.5f));
	Panel->SetTwoSided(true);
	Panel->SetBlendMode(EWidgetBlendMode::Masked);                // solid card, cut-out corners
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // never blocks hooks or the menu pointer
	Panel->SetCastShadow(false);
}

void AGRGasGauge::BeginPlay()
{
	Super::BeginPlay();
	TActorIterator<AGrappleRig> It(GetWorld());
	Rig = It ? *It : nullptr;
	Root = Build();
	Panel->SetSlateWidget(Root);
	Panel->SetWorldScale3D(FVector(WidthCm / GaugePixels.X));
}

TSharedRef<SWidget> AGRGasGauge::Build()
{
	const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	const FSlateFontInfo Number = FCoreStyle::GetDefaultFontStyle("Bold", 24);
	return SNew(SBorder).BorderImage(&BackBrush()).BorderBackgroundColor(FLinearColor(0.006f, 0.009f, 0.007f, 1.f)).Padding(FMargin(18.f, 10.f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("GAS"))).Font(Small).ColorAndOpacity(FLinearColor(0.75f, 0.8f, 0.72f))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Font(Number)
				.Text_Lambda([this] { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Shown * 100.f))); })
				.ColorAndOpacity_Lambda([this] { return FSlateColor(ColourFor(Shown)); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			// The bar: a dark track with a coloured fill whose width follows the tank.
			SNew(SBox).HeightOverride(18.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(&BarBrush()).BorderBackgroundColor(FLinearColor(0.05f, 0.06f, 0.05f, 1.f))
				]
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SNew(SBox).WidthOverride_Lambda([this] { return FOptionalSize(FMath::Max(0.f, Shown) * (GaugePixels.X - 36.f)); })
					[
						SNew(SBorder).BorderImage(&BarBrush())
						.BorderBackgroundColor_Lambda([this]
						{
							FLinearColor C = ColourFor(Shown);
							if (Shown <= 0.2f)
							{
								C *= 0.6f + 0.4f * FMath::Abs(FMath::Sin(Clock * 6.f));   // low: pulse
							}
							if (FlashLeft > 0.f)
							{
								C = FMath::Lerp(C, FLinearColor::White, FlashLeft);         // just refilled: flash
							}
							return FSlateColor(C);
						})
					]
				]
			]
		]
	];
}

void AGRGasGauge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AGrappleRig* R = Rig.Get();
	UCameraComponent* Eyes = R ? R->GetEyes() : nullptr;
	if (!R || !Eyes)
	{
		return;
	}
	Clock += DeltaSeconds;
	FlashLeft = FMath::Max(0.f, FlashLeft - DeltaSeconds);

	// Read the tank and ease the bar toward it (refills slide up, drains are near-instant).
	const float Real = R->GetGas() / FMath::Max(1.f, R->GetGasMax());
	if (LastGas >= 0.f && R->GetGas() > LastGas + 5.f)
	{
		Flash();   // a big jump up = a canister was sliced
	}
	LastGas = R->GetGas();
	Shown = FMath::FInterpTo(Shown, Real, DeltaSeconds, Real > Shown ? 6.f : 20.f);

	// Where it goes. VR: on the left wrist. Desktop: low in front of your body.
	const FVector EyeAt = Eyes->GetComponentLocation();
	FVector At;
	if (R->IsUsingVR() && R->GetHand(0))
	{
		const USceneComponent* Hand = R->GetHand(0);
		At = Hand->GetComponentLocation() + Hand->GetUpVector() * 6.f - Hand->GetForwardVector() * 8.f;
	}
	else
	{
		// Turn with your body (yaw only), not with your head, so it doesn't swim when you look up or down.
		const FRotator Yaw(0.f, Eyes->GetComponentRotation().Yaw, 0.f);
		At = EyeAt + Yaw.RotateVector(FVector(Ahead, Side, -Down));
	}
	SetActorLocation(At);
	SetActorRotation((EyeAt - At).Rotation());   // always faces your eyes, so it's easy to read
}
