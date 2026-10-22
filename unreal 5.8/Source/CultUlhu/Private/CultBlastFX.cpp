// CULT-ULHU blast FX implementation.
#include "CultBlastFX.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "HAL/IConsoleManager.h"

namespace {
TAutoConsoleVariable<int32> ImpactSignals(TEXT("cult.FX.ImpactSignals"),1,TEXT("Open hit/shock effects; zero uses legacy sphere puffs at spawn."));
}

ACultBlastFX::ACultBlastFX()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("EffectOrigin"));
}

ACultBlastFX* ACultBlastFX::SpawnBlast(UWorld* World, ECultBlastKind Kind,
	FVector Location, float RadiusM, FVector ImpactDirection)
{
	if (!World)
		return nullptr;
	ACultBlastFX* FX = World->SpawnActorDeferred<ACultBlastFX>(
		ACultBlastFX::StaticClass(), FTransform(Location),
		nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!FX)
		return nullptr;
	FX->Kind_ = Kind;
	FX->RadiusM_ = FMath::Max(.05f, RadiusM);
	FX->LegacyRadiusM_ = FMath::Max(2.0f, RadiusM);
	FX->ImpactDirection_=ImpactDirection.GetSafeNormal();
	if(FX->ImpactDirection_.IsNearlyZero()) FX->ImpactDirection_=FVector::UpVector;
	FX->FinishSpawning(FTransform(Location));
	return FX;
}

void ACultBlastFX::BeginPlay()
{
	Super::BeginPlay();
	LegacyRadiusM_=FMath::Max(2.f,RadiusM_);
	Refined_=ImpactSignals.GetValueOnGameThread()!=0&&Kind_!=ECultBlastKind::DustBurst;
	if(Refined_) BuildSignal(); else BuildPuffs();
}

void ACultBlastFX::BuildPuffs()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere"));
	if (!Sphere)
	{
		Destroy();
		return;
	}
	UMaterialInterface* DustMat =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CultUlhu/Materials/M_FX_Dust"));
	UMaterialInterface* FlashMat =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CultUlhu/Materials/M_FX_Flash"));

	auto MakePuff = [&](FVector LocalPos, FVector Vel, float Life, float Scale)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Sphere);
		if (DustMat)
			C->SetMaterial(0, DustMat);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		C->SetWorldLocation(GetActorLocation() + LocalPos);
		C->SetWorldScale3D(FVector(Scale));
		FPuff P;
		P.Comp = C;
		P.Vel = Vel;
		P.Life = P.MaxLife = Life;
		P.BaseScale = Scale;
		Puffs_.Add(P);
	};

	if (Kind_ == ECultBlastKind::ShockRing)
	{
		// 12 puffs racing outward in a flat ring.
		const float Speed = LegacyRadiusM_ * 100.0f * 2.4f; // cross R in ~0.4 s
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = (float)i / 12.0f * PI * 2.0f;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.0f);
			MakePuff(Dir * LegacyRadiusM_ * 30.0f,
				Dir * Speed + FVector(0.0f, 0.0f, 160.0f),
				0.7f, FMath::FRandRange(1.6f, 2.6f));
		}
		// Central flash: pops up fast, then gone.
		Flash_ = NewObject<UStaticMeshComponent>(this);
		Flash_->SetStaticMesh(Sphere);
		if (FlashMat)
			Flash_->SetMaterial(0, FlashMat);
		Flash_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flash_->RegisterComponent();
		Flash_->SetWorldLocation(GetActorLocation() + FVector(0.0f, 0.0f, 120.0f));
		Flash_->SetWorldScale3D(FVector(0.5f));
		FlashLife_ = FlashMaxLife_;
	}
	else if (Kind_ == ECultBlastKind::HitFlash)
	{
		// Single bright pop exactly at the hit: fast in, fast out.
		Flash_ = NewObject<UStaticMeshComponent>(this);
		Flash_->SetStaticMesh(Sphere);
		if (FlashMat)
			Flash_->SetMaterial(0, FlashMat);
		Flash_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flash_->RegisterComponent();
		Flash_->SetWorldLocation(GetActorLocation());
		Flash_->SetWorldScale3D(FVector(FMath::Max(LegacyRadiusM_ * 0.9f, 0.8f)));
		FlashMaxLife_ = FlashLife_ = 0.22f;
	}
	else // DustBurst
	{
		for (int32 i = 0; i < 9; ++i)
		{
			FVector Dir = FMath::VRand();
			Dir.Z = FMath::Abs(Dir.Z);
			Dir.Normalize();
			MakePuff(Dir * LegacyRadiusM_ * 40.0f,
				Dir * FMath::FRandRange(250.0f, 480.0f) + FVector(0.0f, 0.0f, 320.0f),
				FMath::FRandRange(0.9f, 1.2f), FMath::FRandRange(1.4f, 2.8f));
		}
	}
}

void ACultBlastFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if(Refined_) {
		SignalAge_+=FMath::Max(0.f,DeltaSeconds);
		if(SignalAge_>=SignalDuration_) { Destroy(); return; }
		UpdateSignal(); return;
	}

	for (int32 i = Puffs_.Num() - 1; i >= 0; --i)
	{
		FPuff& P = Puffs_[i];
		P.Life -= DeltaSeconds;
		if (P.Life <= 0.0f || !P.Comp)
		{
			if (P.Comp)
				P.Comp->DestroyComponent();
			Puffs_.RemoveAt(i);
			continue;
		}
		P.Vel.Z -= 260.0f * DeltaSeconds; // dust hangs, then settles
		P.Comp->SetWorldLocation(P.Comp->GetComponentLocation() + P.Vel * DeltaSeconds);
		const float T = 1.0f - P.Life / P.MaxLife; // 0 -> 1
		const float Grow = 1.0f + T * 1.2f;
		const float Fade = FMath::Clamp(P.Life / 0.35f, 0.0f, 1.0f);
		P.Comp->SetWorldScale3D(FVector(P.BaseScale * Grow * Fade));
	}

	if (Flash_)
	{
		FlashLife_ -= DeltaSeconds;
		if (FlashLife_ <= 0.0f)
		{
			Flash_->DestroyComponent();
			Flash_ = nullptr;
		}
		else
		{
			const float T = 1.0f - FlashLife_ / FlashMaxLife_;
			const float S = (T < 0.4f)
				? FMath::Lerp(0.5f, LegacyRadiusM_ * 0.85f, T / 0.4f)
				: FMath::Lerp(LegacyRadiusM_ * 0.85f, 0.01f, (T - 0.4f) / 0.6f);
			Flash_->SetWorldScale3D(FVector(FMath::Max(0.01f, S)));
		}
	}

	if (Puffs_.Num() == 0 && !Flash_)
		Destroy();
}
