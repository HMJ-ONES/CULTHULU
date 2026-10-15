// CULT-ULHU feral beast implementation.
#include "CultFeralBeast.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

#include "CultBlastFX.h"
#include "CultUlhuPlayerPawn.h"

ACultFeralBeast::ACultFeralBeast()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeastMesh"));
	RootComponent = MeshComp_;
	MeshComp_->SetCollisionProfileName(TEXT("BlockAll"));
}

void ACultFeralBeast::SetMesh(UStaticMesh* InMesh)
{
	if (MeshComp_ && InMesh)
		MeshComp_->SetStaticMesh(InMesh);
}

void ACultFeralBeast::SetSpecies(const FString& SpeciesId)
{
	SpeciesId_ = SpeciesId;
}

void ACultFeralBeast::ApplySpeciesStats()
{
	// HP / speed / body scale per species. Smash does 135: imps, ghouls,
	// wights and risen dead fall in one hit; brutes take two.
	Health_ = MaxHealth_ = 60.0f;
	Speed_ = 5.0f;
	BodyScale_ = 1.0f;
	if (SpeciesId_ == TEXT("charnel_imp"))        { Health_ = MaxHealth_ = 40.0f;  Speed_ = 6.5f; BodyScale_ = 1.0f;  }
	else if (SpeciesId_ == TEXT("skittering_ghoul")) { Health_ = MaxHealth_ = 50.0f;  Speed_ = 8.0f; BodyScale_ = 1.0f;  }
	else if (SpeciesId_ == TEXT("pale_wight"))    { Health_ = MaxHealth_ = 70.0f;  Speed_ = 4.5f; BodyScale_ = 1.25f; }
	else if (SpeciesId_ == TEXT("risen_dead"))    { Health_ = MaxHealth_ = 90.0f;  Speed_ = 3.5f; BodyScale_ = 1.35f; }
	else if (SpeciesId_ == TEXT("ossified_brute")) { Health_ = MaxHealth_ = 160.0f; Speed_ = 2.8f; BodyScale_ = 1.9f;  }
	SetActorScale3D(FVector(BodyScale_));
}

void ACultFeralBeast::BeginPlay()
{
	Super::BeginPlay();
	Home_ = GetActorLocation();
	MoveTarget_ = Home_;
	ApplySpeciesStats();
	Player_ = Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	PickWanderTarget();
}

void ACultFeralBeast::ApplyDamage(float Amount)
{
	if (IsDead() || Amount <= 0.0f)
		return;
	Health_ -= Amount;
	// Getting smashed gets their attention.
	if (Health_ > 0.0f && State_ == EState::Wander)
		State_ = EState::Chase;
	else if (Health_ <= 0.0f)
		Die();
}

void ACultFeralBeast::Die()
{
	State_ = EState::Dying;
	DeathTime_ = 0.0f;
	if (MeshComp_)
		MeshComp_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ACultBlastFX::SpawnBlast(GetWorld(), ECultBlastKind::DustBurst,
		GetActorLocation() + FVector(0.0f, 0.0f, 80.0f), 3.0f);
}

void ACultFeralBeast::PickWanderTarget()
{
	const float A = FMath::FRandRange(0.0f, PI * 2.0f);
	const float R = FMath::FRandRange(400.0f, 2000.0f); // cm, stays near home
	MoveTarget_ = Home_ + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0f);
	ThinkTime_ = FMath::FRandRange(3.0f, 7.0f);
}

void ACultFeralBeast::FaceToward(const FVector& Dest, float DeltaSeconds, float Rate)
{
	FVector D = Dest - GetActorLocation();
	if (D.SizeSquared2D() < 1.0f)
		return;
	const FRotator Want(0.0f, FMath::Atan2(D.Y, D.X) * 180.0f / PI, 0.0f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, Rate));
}

void ACultFeralBeast::MoveToward(const FVector& Dest, float SpeedMs, float DeltaSeconds)
{
	FVector Loc = GetActorLocation();
	FVector D = Dest - Loc;
	D.Z = 0.0f; // steer on the plane at spawn height
	const float Dist = D.Size();
	if (Dist < 5.0f)
		return;
	Loc += D / Dist * SpeedMs * 100.0f * DeltaSeconds;
	Loc.Z = Home_.Z;
	SetActorLocation(Loc);
	FaceToward(Dest, DeltaSeconds);
}

void ACultFeralBeast::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State_ == EState::Dead)
		return;

	if (!Player_)
		Player_ = Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	// Dying: tip over, sink, gone.
	if (State_ == EState::Dying)
	{
		DeathTime_ += DeltaSeconds;
		const float Tip = FMath::Min(1.0f, DeathTime_ / 0.35f);
		FRotator R = GetActorRotation();
		R.Pitch = -88.0f * Tip;
		SetActorRotation(R);
		if (DeathTime_ > 1.2f)
		{
			FVector Loc = GetActorLocation();
			Loc.Z -= 60.0f * DeltaSeconds;
			SetActorLocation(Loc);
		}
		if (DeathTime_ > 3.0f)
		{
			State_ = EState::Dead;
			Destroy();
		}
		return;
	}

	const FVector PlayerLoc = Player_ ? Player_->GetActorLocation() : Home_ + FVector(100000.0f, 0, 0);
	const float DistToPlayer = FVector::Dist2D(GetActorLocation(), PlayerLoc);

	// Far culling: think at 2 Hz past 150 m.
	if (DistToPlayer > 15000.0f)
	{
		FarAcc_ += DeltaSeconds;
		if (FarAcc_ < 0.5f)
			return;
		FarAcc_ = 0.0f;
	}

	if (AttackCooldown_ > 0.0f)
		AttackCooldown_ -= DeltaSeconds;
	ThinkTime_ -= DeltaSeconds;

	switch (State_)
	{
	case EState::Wander:
		MoveToward(MoveTarget_, Speed_ * 0.4f, DeltaSeconds);
		if (ThinkTime_ <= 0.0f || FVector::Dist2D(GetActorLocation(), MoveTarget_) < 100.0f)
			PickWanderTarget();
		if (DistToPlayer < 3500.0f)
			State_ = EState::Chase;
		break;

	case EState::Chase:
		MoveToward(PlayerLoc, Speed_, DeltaSeconds);
		if (DistToPlayer < 550.0f && AttackCooldown_ <= 0.0f)
		{
			// Swipe: lunge + dust + a nudge to the player's camera.
			// Resolves instantly; the beast stays in Chase.
			FaceToward(PlayerLoc, DeltaSeconds, 12.0f);
			FVector Loc = GetActorLocation();
			FVector Lunge = (PlayerLoc - Loc);
			Lunge.Z = 0.0f;
			if (Lunge.SizeSquared2D() > 1.0f)
			{
				Lunge.Normalize();
				Loc += Lunge * 260.0f;
				Loc.Z = Home_.Z;
				SetActorLocation(Loc);
			}
			ACultBlastFX::SpawnBlast(GetWorld(), ECultBlastKind::DustBurst,
				Loc + FVector(0.0f, 0.0f, 120.0f), 2.5f);
			if (Player_)
				Player_->AddCameraKick(0.12f);
			AttackCooldown_ = 1.4f;
		}
		else if (DistToPlayer > 6000.0f)
			State_ = EState::Wander;
		break;

	default:
		break;
	}
}
