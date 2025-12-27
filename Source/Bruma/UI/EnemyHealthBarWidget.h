#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

class UProgressBar;

UCLASS()
class BRUMA_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetHealthPercent(float Percent);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UPROPERTY()
	UProgressBar* PB_Health;

private:
	float CachedPercent;
	void ApplyPercent();
	void EnsureWidgetTree();
};
