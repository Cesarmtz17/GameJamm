#include "Core/JamTitleWidget.h"
#include "Core/JamAudio.h"
#include "GameJamm.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarJamTitle(TEXT("jam.UI.Title"), 1,
		TEXT("1 = paused title screen once per world. 0 = skip title (testing)."));
	TSet<TWeakObjectPtr<UWorld>> GJamTitleWorlds;
}

UJamTitleWidget::UJamTitleWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UJamTitleWidget::ShowFor(APlayerController* PlayerController)
{
	UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() || !World || !World->IsGameWorld()
		|| CVarJamTitle.GetValueOnGameThread() == 0)
	{
		return;
	}
	for (auto It = GJamTitleWorlds.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) { It.RemoveCurrent(); }
	}
	const TWeakObjectPtr<UWorld> WeakWorld(World);
	if (GJamTitleWorlds.Contains(WeakWorld)) { return; }
	GJamTitleWorlds.Add(WeakWorld); // tambien evita duplicados si se posee otro peon antes del siguiente tick
	const TWeakObjectPtr<APlayerController> WeakPC(PlayerController);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld, WeakPC]()
	{
		UWorld* TitleWorld = WeakWorld.Get();
		APlayerController* PC = WeakPC.Get();
		if (!TitleWorld || TitleWorld->bIsTearingDown || !PC || PC->GetWorld() != TitleWorld
			|| CVarJamTitle.GetValueOnGameThread() == 0)
		{
			return;
		}
		UJamTitleWidget* Title = CreateWidget<UJamTitleWidget>(PC, UJamTitleWidget::StaticClass());
		if (!Title) { return; }
		Title->AddToViewport(10000);
		if (!UGameplayStatics::SetGamePaused(TitleWorld, true))
		{
			// Si algun GameMode rechaza pausar, nunca dejamos una partida corriendo detras de un menu bloqueante.
			Title->RemoveFromParent();
			UE_LOG(LogJam, Warning, TEXT("Titulo omitido: el mundo no acepto la pausa"));
			return;
		}
		Title->bOwnsPause = true;
		PC->FlushPressedKeys();
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Title->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		Title->SetKeyboardFocus();
		// JamAudio usa SpawnSound2D, que marca bIsUISound: la musica sigue aunque el mundo este pausado.
		JamAudio::Music(Title, TEXT("MUS_Menu_Loop"), 0.4f);
		UE_LOG(LogJam, Log, TEXT("Titulo Prompt & Conquer: mundo pausado, Enter o clic para jugar"));
	}));
}

bool UJamTitleWidget::Initialize()
{
	if (!Super::Initialize()) { return false; }
	if (!WidgetTree || WidgetTree->RootWidget) { return true; }

	const FLinearColor Gold(0.95f, 0.76f, 0.19f, 1.f);
	const FLinearColor White(0.96f, 0.96f, 0.93f, 1.f);
	const FLinearColor Dim(0.7f, 0.77f, 0.8f, 1.f);
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleBackground"));
	Background->SetBrushColor(FLinearColor(0.025f, 0.045f, 0.065f, 1.f));
	Background->SetPadding(FMargin(32.f));
	WidgetTree->RootWidget = Background;
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("TitleFit"));
	Fit->SetStretch(EStretch::ScaleToFit);
	Fit->SetStretchDirection(EStretchDirection::DownOnly);
	UBorderSlot* FitSlot = Cast<UBorderSlot>(Background->AddChild(Fit));
	FitSlot->SetHorizontalAlignment(HAlign_Fill);
	FitSlot->SetVerticalAlignment(VAlign_Fill);
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TitleWidth"));
	Width->SetWidthOverride(900.f);
	Fit->AddChild(Width);
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleLines"));
	Width->AddChild(Lines);
	auto Line = [this, Lines](const TCHAR* Name, const TCHAR* Text, int32 Size, const FLinearColor& Color, float Bottom)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = Size;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetJustification(ETextJustify::Center);
		Label->SetWrapTextAt(860.f);
		UVerticalBoxSlot* Slot = Lines->AddChildToVerticalBox(Label);
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
	};
	Line(TEXT("Title"), TEXT("Prompt & Conquer"), 64, Gold, 12.f);
	Line(TEXT("Tagline"), TEXT("Your words. Your battlefield."), 26, White, 38.f);
	Line(TEXT("Goal"), TEXT("Destroy tower 1, tower 2, and the enemy base"), 26, White, 20.f);
	Line(TEXT("Hook"), TEXT("Ask for a weapon or a vehicle in your own words.\nIt assembles from your parts and works through physics."), 22, Dim, 28.f);
	Line(TEXT("FirstStep"), TEXT("First, walk over the chest beside you.\nB: ask  |  C: power  |  E: ride"), 20, White, 34.f);

	UButton* Play = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayButton"));
	Play->SetBackgroundColor(Gold);
	Play->OnClicked.AddDynamic(this, &UJamTitleWidget::BeginMatch);
	UTextBlock* PlayText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayText"));
	PlayText->SetText(FText::FromString(TEXT("Enter to play")));
	FSlateFontInfo ButtonFont = PlayText->GetFont();
	ButtonFont.Size = 26;
	PlayText->SetFont(ButtonFont);
	PlayText->SetColorAndOpacity(FSlateColor(FLinearColor(0.04f, 0.06f, 0.08f, 1.f)));
	UButtonSlot* TextSlot = Cast<UButtonSlot>(Play->AddChild(PlayText));
	TextSlot->SetPadding(FMargin(48.f, 16.f));
	UVerticalBoxSlot* PlaySlot = Lines->AddChildToVerticalBox(Play);
	PlaySlot->SetHorizontalAlignment(HAlign_Center);
	return true;
}

FReply UJamTitleWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Captura tambien cuando un clic dio foco al boton: Espacio/Escape no empiezan ni cierran nada.
	return NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UJamTitleWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter && !InKeyEvent.IsRepeat()) { BeginMatch(); }
	return FReply::Handled();
}

void UJamTitleWidget::RestoreGame()
{
	if (!bOwnsPause) { return; }
	bOwnsPause = false;
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown) { return; }
	UGameplayStatics::SetGamePaused(World, false);
	if (APlayerController* PC = GetOwningPlayer())
	{
		// No pasar Enter al cuadro B ni el clic del boton al arma en el primer frame de partida.
		PC->FlushPressedKeys();
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	JamAudio::Music(this, TEXT("MUS_Match_Loop"), 0.4f);
	UE_LOG(LogJam, Log, TEXT("Titulo: comienza la partida, mundo sin pausa"));
}

void UJamTitleWidget::BeginMatch()
{
	RestoreGame();
	RemoveFromParent();
}

void UJamTitleWidget::NativeDestruct()
{
	// Una retirada externa del widget tampoco puede dejar el mundo pausado sin boton para volver.
	RestoreGame();
	Super::NativeDestruct();
}
