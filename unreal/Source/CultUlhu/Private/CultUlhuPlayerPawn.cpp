// CULT-ULHU player pawn implementation.
#include "CultUlhuPlayerPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

#include "CultBlastFX.h"
#include "CultDestructible.h"
#include "CultFeralBeast.h"
#include "CultUlhuGameInstance.h"

ACultUlhuPlayerPawn::ACultUlhuPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArm_ = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm_->SetupAttachment(GetCapsuleComponent());
	SpringArm_->bUsePawnControlRotation = true;

	Camera_ = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera_->SetupAttachment(SpringArm_, USpringArmComponent::SocketName);
}

void ACultUlhuPlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	// Scale to the selected entity's canonical height (baseline: 1.8 m human).
	float HeightM = 30.0f;
	if (UCultUlhuGameInstance* GI = Cast<UCultUlhuGameInstance>(GetGameInstance()))
	{
		for (const FCultRosterEntry& E : UCultUlhuGameInstance::Roster())
			if (E.Id == GI->SelectedCharacterId) { HeightM = E.HeightM; break; }
	}
	CharScale_ = HeightM / 1.8f;
	SetActorScale3D(FVector(CharScale_));

	// Kaiju stride: walk speed grows with the square root of scale.
	GetCharacterMovement()->MaxWalkSpeed = 600.0f * FMath::Sqrt(CharScale_);

	// Camera rig for the scaled body.
	const float HalfH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SpringArm_->TargetArmLength = 700.0f * CharScale_;
	SpringArm_->SetRelativeLocation(FVector(0.0f, 0.0f, HalfH * 0.75f));
	bThirdPerson_ = true;

	// Vessel treatment: the roster FBX models haven't landed yet, so wear
	// a shadowy vessel — dark unlit capsule + glowing eyes — instead of a
	// flat gray capsule. Positions use the UNSCALED half-height: the actor
	// scale applies once on top.
	if (UMaterialInterface* Vessel = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/CultUlhu/Materials/M_PawnVessel")))
		GetCapsuleComponent()->SetMaterial(0, Vessel);
	if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(
			nullptr, TEXT("/Engine/BasicShapes/Sphere")))
	{
		UMaterialInterface* EyeGlow = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/CultUlhu/Materials/M_Glow_Eye"));
		const float UH = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		const float S = FMath::Clamp(UH * 0.09f, 8.0f, 60.0f) / 100.0f;
		for (int32 i = 0; i < 2; ++i)
		{
			UStaticMeshComponent* E = NewObject<UStaticMeshComponent>(this);
			E->SetStaticMesh(Sphere);
			if (EyeGlow)
				E->SetMaterial(0, EyeGlow);
			E->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			E->SetCastShadow(false);
			E->RegisterComponent();
			E->AttachToComponent(GetCapsuleComponent(),
				FAttachmentTransformRules::KeepRelativeTransform);
			E->SetRelativeLocation(FVector(UH * 0.30f,
				(i == 0 ? UH * 0.14f : -UH * 0.14f), UH * 0.72f));
			E->SetRelativeScale3D(FVector(S));
		}
	}
}

void ACultUlhuPlayerPawn::SetupPlayerInputComponent(UInputComponent* IC)
{
	Super::SetupPlayerInputComponent(IC);
	// Zero-asset bindings: no Input Action assets required.
	IC->BindKey(EKeys::V, IE_Pressed, this, &ACultUlhuPlayerPawn::ToggleCamera);
	IC->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACharacter::Jump);
	IC->BindKey(EKeys::SpaceBar, IE_Released, this, &ACharacter::StopJumping);
	IC->BindKey(EKeys::Escape, IE_Pressed, this, &ACultUlhuPlayerPawn::ReturnToMenu);
	IC->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACultUlhuPlayerPawn::Smash);
}

void ACultUlhuPlayerPawn::ToggleCamera()
{	bThirdPerson_ = !bThirdPerson_;
	const float HalfH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (bThirdPerson_)
	{
		SpringArm_->TargetArmLength = 700.0f * CharScale_;
		SpringArm_->SetRelativeLocation(FVector(0.0f, 0.0f, HalfH * 0.75f));
	}
	else
	{
		// First person: collapse the boom, park the pivot at head height.
		SpringArm_->TargetArmLength = 0.0f;
		SpringArm_->SetRelativeLocation(FVector(40.0f * CharScale_, 0.0f, HalfH * 1.85f));
	}
}

void ACultUlhuPlayerPawn::ReturnToMenu()
{
	if (UCultUlhuGameInstance* GI = Cast<UCultUlhuGameInstance>(GetGameInstance()))
		GI->ReturnToMenu();
}

void ACultUlhuPlayerPawn::Smash()
{
	if (SmashCooldown_ > 0.0f)
		return;
	SmashCooldown_ = 0.9f;

	// Radius ~1.25x the entity's canonical height, in cm.
	const float HeightCm = CharScale_ * 1.8f * 100.0f;
	const float RadiusCm = 1.25f * HeightCm;
	const float HalfH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, HalfH);

	for (TActorIterator<ACultDestructible> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), Feet);
		if (D < RadiusCm)
		{
			const float Falloff = 1.0f - 0.6f * (D / RadiusCm);
			It->ApplyDamage(135.0f * Falloff, Feet);
		}
	}
	for (TActorIterator<ACultFeralBeast> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), Feet);
		if (D < RadiusCm)
		{
			const float Falloff = 1.0f - 0.6f * (D / RadiusCm);
			It->ApplyDamage(135.0f * Falloff);
		}
	}

	ACultBlastFX::SpawnBlast(GetWorld(), ECultBlastKind::ShockRing,
		Feet + FVector(0.0f, 0.0f, 50.0f), RadiusCm / 100.0f);
	ShakeTime_ = ShakeDur_;
}

void ACultUlhuPlayerPawn::AddCameraKick(float Strength)
{
	ShakeTime_ = FMath::Max(ShakeTime_,
		ShakeDur_ * FMath::Clamp(Strength, 0.0f, 1.0f));
}

void ACultUlhuPlayerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (SmashCooldown_ > 0.0f)
		SmashCooldown_ -= DeltaSeconds;

	// Manual camera kick: decays over ShakeDur_. Version-proof (no
	// UCameraShakeBase subclass needed).
	if (ShakeTime_ > 0.0f)
	{
		ShakeTime_ -= DeltaSeconds;
		const float K = FMath::Clamp(ShakeTime_ / ShakeDur_, 0.0f, 1.0f);
		PC->AddPitchInput(FMath::FRandRange(-1.0f, 1.0f) * 0.012f * K);
		PC->AddYawInput(FMath::FRandRange(-1.0f, 1.0f) * 0.012f * K);
	}

	// Mouse-look via raw delta (no axis mappings needed). DX/DY are the
	// pixels moved since last frame, so a flat per-pixel factor is
	// frame-rate independent.
	float DX = 0.0f, DY = 0.0f;
	PC->GetInputMouseDelta(DX, DY);
	if (DX != 0.0f) PC->AddYawInput(DX * 0.0025f);
	if (DY != 0.0f) PC->AddPitchInput(-DY * 0.0025f);

	PollMoveKeys(DeltaSeconds);
}

void ACultUlhuPlayerPawn::PollMoveKeys(float DeltaSeconds)
{
	(void)DeltaSeconds;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	FVector Move = FVector::ZeroVector;
	const FRotator Yaw(0.0f, PC->GetControlRotation().Yaw, 0.0f);
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))    Move += Yaw.Vector();
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))  Move -= Yaw.Vector();
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) Move += FRotationMatrix(Yaw).GetScaledAxis(EAxis::Y);
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))  Move -= FRotationMatrix(Yaw).GetScaledAxis(EAxis::Y);
	if (Move.SizeSquared() > 0.0f)
		AddMovementInput(Move.GetSafeNormal(), 1.0f);
}
