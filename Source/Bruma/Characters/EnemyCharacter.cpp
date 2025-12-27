#include "Characters/EnemyCharacter.h"
#include "Components/HealthComponent.h"
#include "UI/EnemyHealthBarWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	HealthWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidgetComp"));
	HealthWidgetComp->SetupAttachment(GetRootComponent());
	HealthWidgetComp->SetWidgetSpace(EWidgetSpace::World);
	HealthWidgetComp->SetDrawAtDesiredSize(false);
	HealthWidgetComp->SetDrawSize(FVector2D(120.0f, 16.0f));
	HealthWidgetComp->SetPivot(FVector2D(0.5f, 1.0f));
	HealthWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HealthWidgetOffset = FVector(0.0f, 0.0f, 120.0f);
	CachedHealthWidget = nullptr;
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	HealthWidgetComp->SetRelativeLocation(HealthWidgetOffset);

	if (HealthWidgetClass)
	{
		HealthWidgetComp->SetWidgetClass(HealthWidgetClass);
	}

	CachedHealthWidget = Cast<UEnemyHealthBarWidget>(HealthWidgetComp->GetUserWidgetObject());
	UpdateHealthWidget(HealthComponent->CurrentHealth, HealthComponent->MaxHealth);

	HealthComponent->OnHealthChanged.AddDynamic(this, &AEnemyCharacter::HandleHealthChanged);
	HealthComponent->OnDeath.AddDynamic(this, &AEnemyCharacter::HandleDeath);
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HealthWidgetComp && HealthWidgetComp->IsVisible())
	{
		UpdateWidgetFacing();
	}
}

void AEnemyCharacter::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	UpdateHealthWidget(CurrentHealth, MaxHealth);
}

void AEnemyCharacter::HandleDeath()
{
	if (HealthWidgetComp)
	{
		HealthWidgetComp->SetVisibility(false, true);
		HealthWidgetComp->SetComponentTickEnabled(false);
	}
}

void AEnemyCharacter::UpdateHealthWidget(float CurrentHealth, float MaxHealth)
{
	if (!CachedHealthWidget)
	{
		CachedHealthWidget = Cast<UEnemyHealthBarWidget>(HealthWidgetComp->GetUserWidgetObject());
	}

	if (CachedHealthWidget)
	{
		CachedHealthWidget->SetHealthPercent(MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f);
	}
}

float AEnemyCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (HealthComponent && AppliedDamage > 0.0f)
	{
		HealthComponent->ApplyDamage(AppliedDamage);
	}

	return AppliedDamage;
}

void AEnemyCharacter::UpdateWidgetFacing()
{
	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return;
	}

	const FVector WidgetLocation = HealthWidgetComp->GetComponentLocation();
	const FVector CameraLocation = CameraManager->GetCameraLocation();
	const FRotator LookAtRotation = (CameraLocation - WidgetLocation).Rotation();
	HealthWidgetComp->SetWorldRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f));
}
