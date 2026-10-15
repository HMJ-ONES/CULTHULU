// CULT-ULHU destructible building implementation.
#include "CultDestructible.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

#include "CultBlastFX.h"

ACultDestructible::ACultDestructible()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	RootComponent = MeshComp_;
	MeshComp_->SetCollisionProfileName(TEXT("BlockAll"));

	// Tick only while debris is flying.
	SetActorTickEnabled(false);
}

UStaticMesh* ACultDestructible::SharedMesh(const TCHAR* Path)
{
	// Lazy load (not in the constructor): a fresh clone has no built
	// content yet, and a constructor-time FObjectFinder would spam the
	// log. Missing assets degrade to "no rubble / no debris".
	return LoadObject<UStaticMesh>(nullptr, Path);
}

void ACultDestructible::SetMesh(UStaticMesh* InMesh)
{
	if (MeshComp_ && InMesh)
		MeshComp_->SetStaticMesh(InMesh);
}

void ACultDestructible::BeginPlay()
{
	Super::BeginPlay();

	// HP from the mesh footprint: columns fall in one smash, towers take
	// several. Bounds are in cm; scale applies on top.
	float RadiusM = 3.0f;
	if (MeshComp_ && MeshComp_->GetStaticMesh())
	{
		const float R = MeshComp_->GetStaticMesh()->GetBounds().SphereRadius;
		RadiusM = FMath::Max(1.0f, R / 100.0f * GetActorScale3D().X);
	}
	MaxHealth_ = Health_ = (50.0f + RadiusM * 18.0f) * HealthMultiplier_;
}

void ACultDestructible::ApplyDamage(float Amount, FVector HitLocation)
{
	if (bDestroyed_ || Amount <= 0.0f)
		return;
	Health_ -= Amount;
	// Hit feedback even when the building survives: a bright pop at the
	// impact point so every smash reads.
	ACultBlastFX::SpawnBlast(GetWorld(), ECultBlastKind::HitFlash,
		HitLocation, 2.2f);
	if (Health_ <= 0.0f)
		Collapse(HitLocation);
}

void ACultDestructible::Collapse(FVector HitLocation)
{
	bDestroyed_ = true;
	if (MeshComp_)
	{
		MeshComp_->SetVisibility(false);
		MeshComp_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const FVector Base = GetActorLocation();
	float RadiusCm = 300.0f;
	if (MeshComp_ && MeshComp_->GetStaticMesh())
		RadiusCm = FMath::Max(150.0f,
			MeshComp_->GetStaticMesh()->GetBounds().SphereRadius * GetActorScale3D().X);

	// A rubble mound stays where the building stood.
	if (UStaticMesh* Rubble = SharedMesh(TEXT("/Game/CultUlhu/World/SM_rubble-large")))
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AStaticMeshActor* R = GetWorld()->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(), Base, FRotator::ZeroRotator, P))
		{
			R->GetStaticMeshComponent()->SetStaticMesh(Rubble);
			const float S = FMath::Clamp(RadiusCm / 350.0f, 0.7f, 2.2f);
			R->SetActorScale3D(FVector(S));
			R->SetActorLabel(FString::Printf(TEXT("WILD_Rubble_%s"), *GetActorLabel()));
		}
	}

	SpawnDebris(Base + FVector(0.0f, 0.0f, RadiusCm * 0.45f), RadiusCm);
	ACultBlastFX::SpawnBlast(GetWorld(), ECultBlastKind::DustBurst,
		Base + FVector(0.0f, 0.0f, RadiusCm * 0.35f), RadiusCm / 100.0f);

	// Scorch mark: a near-black disc where the building stood, so the
	// destruction leaves a lasting read on the ground.
	if (UStaticMesh* Plane = SharedMesh(TEXT("/Engine/BasicShapes/Plane")))
	{
		UMaterialInterface* Scorch = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/CultUlhu/Materials/M_Decal_Scorch"));
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AStaticMeshActor* D = GetWorld()->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(),
				Base + FVector(0.0f, 0.0f, 6.0f), FRotator::ZeroRotator, P))
		{
			D->GetStaticMeshComponent()->SetStaticMesh(Plane);
			if (Scorch)
				D->GetStaticMeshComponent()->SetMaterial(0, Scorch);
			const float S = FMath::Clamp(RadiusCm / 100.0f * 1.4f, 3.0f, 14.0f);
			D->SetActorScale3D(FVector(S, S, 1.0f));
			D->SetActorLabel(FString::Printf(TEXT("WILD_Scorch_%s"),
				*GetActorLabel()));
		}
	}
	(void)HitLocation;
}

void ACultDestructible::SpawnDebris(FVector Center, float RadiusCm)
{
	UStaticMesh* Cube = SharedMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (!Cube)
		return;

	UMaterialInterface* BuildingMat = MeshComp_ ? MeshComp_->GetMaterial(0) : nullptr;
	for (int32 i = 0; i < 8; ++i)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Cube);
		if (BuildingMat)
			C->SetMaterial(0, BuildingMat);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();

		FVector Dir = FMath::VRand();
		Dir.Z = FMath::Abs(Dir.Z) * 0.9f + 0.35f;
		Dir.Normalize();
		C->SetWorldLocation(Center + Dir * RadiusCm * 0.35f);

		FChunk Ch;
		Ch.Comp = C;
		Ch.Vel = Dir * FMath::FRandRange(500.0f, 1100.0f);
		Ch.Spin = FMath::VRand() * 540.0f;
		Ch.Life = Ch.MaxLife = FMath::FRandRange(1.1f, 1.6f);
		Ch.BaseScale = FMath::FRandRange(0.4f, 1.3f); // meters; cube is 1 m
		C->SetWorldScale3D(FVector(Ch.BaseScale));
		Chunks_.Add(Ch);
	}
	SetActorTickEnabled(true);
}

void ACultDestructible::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Chunks_.Num() == 0)
	{
		SetActorTickEnabled(false);
		return;
	}
	for (int32 i = Chunks_.Num() - 1; i >= 0; --i)
	{
		FChunk& Ch = Chunks_[i];
		Ch.Life -= DeltaSeconds;
		if (Ch.Life <= 0.0f || !Ch.Comp)
		{
			if (Ch.Comp)
				Ch.Comp->DestroyComponent();
			Chunks_.RemoveAt(i);
			continue;
		}
		Ch.Vel.Z -= 1500.0f * DeltaSeconds; // slightly heavy, punchy
		Ch.Comp->SetWorldLocation(Ch.Comp->GetComponentLocation() + Ch.Vel * DeltaSeconds);
		Ch.Comp->AddLocalRotation(
			FRotator(Ch.Spin.Y * DeltaSeconds, Ch.Spin.Z * DeltaSeconds, Ch.Spin.X * DeltaSeconds));
		const float K = FMath::Clamp(Ch.Life / 0.45f, 0.0f, 1.0f);
		Ch.Comp->SetWorldScale3D(FVector(Ch.BaseScale * K));
	}
}
