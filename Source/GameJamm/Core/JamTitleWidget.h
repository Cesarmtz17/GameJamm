#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JamTitleWidget.generated.h"

class APlayerController;

/** Pantalla inicial de Prompt & Conquer: pausa real hasta Enter o clic, una vez por mundo. */
UCLASS()
class GAMEJAMM_API UJamTitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UJamTitleWidget(const FObjectInitializer& ObjectInitializer);

	/** Llamar desde PossessedBy del jugador local. No reaparece al renacer; jam.UI.Title 0 la omite. */
	static void ShowFor(APlayerController* PlayerController);
	virtual bool Initialize() override;

protected:
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void BeginMatch();

	void RestoreGame();
	bool bOwnsPause = false;
};
