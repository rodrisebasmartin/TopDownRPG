#include "UI/EnemyHealthBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"

void UEnemyHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CachedPercent = 1.0f;
	ApplyPercent();
}

void UEnemyHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	ApplyPercent();
}

void UEnemyHealthBarWidget::SetHealthPercent(float Percent)
{
	CachedPercent = FMath::Clamp(Percent, 0.0f, 1.0f);
	ApplyPercent();
}

void UEnemyHealthBarWidget::ApplyPercent()
{
	if (PB_Health)
	{
		PB_Health->SetPercent(CachedPercent);
	}
}

void UEnemyHealthBarWidget::EnsureWidgetTree()
{
	if (PB_Health || !WidgetTree)
	{
		return;
	}

	USizeBox* RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSizeBox"));
	WidgetTree->RootWidget = RootSizeBox;
	RootSizeBox->SetWidthOverride(120.0f);
	RootSizeBox->SetHeightOverride(16.0f);

	PB_Health = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_Health"));
	RootSizeBox->AddChild(PB_Health);
}
