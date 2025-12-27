#include "UI/EnemyHealthBarWidget.h"
#include "Components/ProgressBar.h"

void UEnemyHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CachedPercent = 1.0f;
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
