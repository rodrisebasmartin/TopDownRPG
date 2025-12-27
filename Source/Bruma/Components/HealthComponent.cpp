#include "Components/HealthComponent.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MaxHealth = 100.0f;
	CurrentHealth = MaxHealth;
	bIsDead = false;
	bHasBroadcastDeath = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
	bIsDead = CurrentHealth <= 0.0f;
	BroadcastHealthChanged();
	if (bIsDead)
	{
		HandleDeath();
	}
}

void UHealthComponent::ApplyDamage(float Amount)
{
	if (bIsDead || Amount <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.0f, MaxHealth);
	BroadcastHealthChanged();

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
}

float UHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

bool UHealthComponent::IsDead() const
{
	return bIsDead;
}

void UHealthComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::HandleDeath()
{
	if (bHasBroadcastDeath)
	{
		return;
	}

	bIsDead = true;
	bHasBroadcastDeath = true;
	OnDeath.Broadcast();
}
