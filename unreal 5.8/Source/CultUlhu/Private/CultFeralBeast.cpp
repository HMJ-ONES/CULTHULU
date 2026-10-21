#include "CultFeralBeast.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);
// CULT-ULHU feral beast implementation.
#include "CultSkillAttack.h"
#include "CultCorpse.h"
#include "CultJungleCamp.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

#include "CultBlastFX.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"

ACultFeralBeast::ACultFeralBeast()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision_ = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BeastCollision"));
	RootComponent = Collision_;
	Collision_->InitCapsuleSize(40.f, 90.f);
	Collision_->SetCollisionProfileName(TEXT("Pawn"));
 Collision_->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
 Collision_->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	MeshComp_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeastMesh"));
	MeshComp_->SetupAttachment(Collision_);
	MeshComp_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp_->SetMobility(EComponentMobility::Movable);

	// Glowing eyes: plain sphere components, positioned from the mesh
	// bounds in BeginPlay so they fit any creature model.
	EyeL_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EyeL"));
	EyeR_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EyeR"));
	for (UStaticMeshComponent* E : { EyeL_, EyeR_ })
	{
		E->SetupAttachment(MeshComp_);
		E->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		E->SetCastShadow(false);
		E->SetVisibility(false); // shown once AttachEyes places them
	}
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
	// PvE health is tuned against an unbuffed 135-damage aimed slash.
	Health_ = MaxHealth_ = 45.0f;
	Speed_ = 5.0f;
	BodyScale_ = 1.0f;
	AttackDamage_ = 6.f;
	if (SpeciesId_ == TEXT("charnel_imp"))        { Health_ = MaxHealth_ = 135.f; Speed_ = 6.5f; BodyScale_ = .9f; AttackDamage_ = 5.f; }
	else if (SpeciesId_ == TEXT("skittering_ghoul")) { Health_ = MaxHealth_ = 270.f; Speed_ = 8.f; BodyScale_ = 1.1f; AttackDamage_ = 6.f; }
	else if (SpeciesId_ == TEXT("pale_wight"))    { Health_ = MaxHealth_ = 1350.f; Speed_ = 4.5f; BodyScale_ = 1.25f; AttackDamage_ = 7.f; }
	else if (SpeciesId_ == TEXT("risen_dead"))    { Health_ = MaxHealth_ = 40.f; Speed_ = 3.5f; BodyScale_ = 1.f; AttackDamage_ = 5.f; }
	else if (SpeciesId_ == TEXT("ossified_brute")) { Health_ = MaxHealth_ = 2025.f; Speed_ = 4.f; BodyScale_ = 1.8f; AttackDamage_ = 10.f; }
	SetActorScale3D(FVector(BodyScale_));
}

void ACultFeralBeast::BeginPlay()
{
	Super::BeginPlay();
 // The incomplete Risen Dead material import is retired from live encounters.
 if(SpeciesId_==TEXT("risen_dead")) SpeciesId_=TEXT("skittering_ghoul");
	ApplySpeciesStats();
	AssembleCreature();
	if (!CreaturePose_ && !MeshComp_->GetStaticMesh())
	{
		MeshComp_->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere")));
		if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CultUlhu/Materials/M_PawnVessel"))) MeshComp_->SetMaterial(0, Material);
	}
	Home_ = GetActorLocation();
	FVector Grounded;
	if (GroundAt(Home_, Grounded)) SetActorLocation(Grounded, false, nullptr, ETeleportType::TeleportPhysics);
	Home_ = GetActorLocation();
	MoveTarget_ = Home_;
	if (!CreaturePose_)
	{
		MeshComp_->SetRelativeScale3D(FVector(.8f, .8f, 1.8f));
		AttachEyes();
	}
	Player_ = Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	PickWanderTarget();
}

void ACultFeralBeast::AssembleCreature()
{
	FString Prefix;
	TArray<FString> Parts;
	TArray<FString> Accessories;
	if (SpeciesId_ == TEXT("charnel_imp")) { Prefix = TEXT("Skeleton_Minion_"); Parts = {TEXT("Body"), TEXT("Head"), TEXT("Jaw"), TEXT("Eyes"), TEXT("ArmLeft"), TEXT("ArmRight"), TEXT("LegLeft"), TEXT("LegRight"), TEXT("Cloak")}; }
	else if (SpeciesId_ == TEXT("skittering_ghoul")) { Prefix = TEXT("Skeleton_Rogue_"); Parts = {TEXT("Body"), TEXT("Head"), TEXT("Jaw"), TEXT("Eyes"), TEXT("ArmLeft"), TEXT("ArmRight"), TEXT("LegLeft"), TEXT("LegRight")}; Accessories = {TEXT("Hood"), TEXT("Cape")}; }
	else if (SpeciesId_ == TEXT("pale_wight")) { Prefix = TEXT("Skeleton_Mage_"); Parts = {TEXT("Body"), TEXT("Skull"), TEXT("Eyes"), TEXT("ArmLeft"), TEXT("ArmRight"), TEXT("LegLeft"), TEXT("LegRight")}; Accessories = {TEXT("Hat")}; }
	else if (SpeciesId_ == TEXT("ossified_brute")) { Prefix = TEXT("Skeleton_Warrior_"); Parts = {TEXT("Body"), TEXT("Head"), TEXT("Jaw"), TEXT("Eyes"), TEXT("ArmLeft"), TEXT("ArmRight"), TEXT("LegLeft"), TEXT("LegRight"), TEXT("Cloak")}; Accessories = {TEXT("Helmet")}; }
	else if (SpeciesId_ == TEXT("risen_dead")) Parts = {TEXT("torso"), TEXT("head"), TEXT("arm-left"), TEXT("arm-right"), TEXT("leg-left"), TEXT("leg-right")};
	else return;

	const FString Folder = TEXT("/Game/CultUlhu/Creatures/") + SpeciesId_;
	FBox Bounds(ForceInit);
	for (const FString& Part : Parts)
	{
		const FString Name = Prefix + Part;
		const FString Path = Folder + TEXT("/SkeletalMeshes/") + Name + TEXT(".") + Name;
		USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, *Path);
		if (!Asset) { UE_LOG(LogTemp, Warning, TEXT("[CultUlhu] Creature part unavailable: %s"), *Path); continue; }
		UPoseableMeshComponent* Component = NewObject<UPoseableMeshComponent>(this);
		Component->SetupAttachment(Collision_);
		Component->SetSkinnedAssetAndUpdate(Asset);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->RegisterComponent();
		Bounds += Asset->GetBounds().GetBox();
		CreatureParts_.Add(Component);
		if (!CreaturePose_) CreaturePose_ = Component;
	}
	if (!CreaturePose_) return;
	const float VisualScale = 180.f / FMath::Max(1.f, Bounds.GetSize().Z);
	CreaturePose_->SetRelativeScale3D(FVector(VisualScale));
	// Imported characters face +Y; rotate their visual forward onto +X.
	CreaturePose_->SetRelativeRotation(FRotator(0, -90, 0));
	CreaturePose_->SetRelativeLocation(FVector(0, 0, -90.f - Bounds.Min.Z * VisualScale));
	for (UPoseableMeshComponent* Component : CreatureParts_)
		if (Component != CreaturePose_)
		{
			Component->AttachToComponent(CreaturePose_, FAttachmentTransformRules::SnapToTargetIncludingScale);
			Component->SetLeaderPoseComponent(CreaturePose_, true);
		}
	for (const FString& Accessory : Accessories)
	{
		const FString Name = Prefix + Accessory;
		UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, *(Folder + TEXT("/StaticMeshes/") + Name + TEXT(".") + Name));
		if (!Asset || CreaturePose_->GetBoneIndex(FName(*Name)) == INDEX_NONE) continue;
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Asset);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Imported accessories retain their model-space placement; a bone socket
		// would apply that placement a second time and float them above the head.
		Component->SetupAttachment(CreaturePose_);
		Component->RegisterComponent();
	}
	MeshComp_->SetVisibility(false);
	RestPose_ = CreaturePose_->GetBoneSpaceTransforms();
	const TArray<FName> Names = {TEXT("hips"), TEXT("upperleg_l"), TEXT("upperleg_r"), TEXT("lowerleg_l"), TEXT("lowerleg_r"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("leg-left"), TEXT("leg-right"), TEXT("arm-left"), TEXT("arm-right"), TEXT("torso")};
	for (FName Name : Names)
	{
		const int32 Index = CreaturePose_->GetBoneIndex(Name);
		if (RestPose_.IsValidIndex(Index)) AnimatedBones_.Add(Name, Index);
	}
	UE_LOG(LogTemp, Log, TEXT("[CultUlhu] %s: %d authored skeletal parts, height %.0f cm, health %.0f, swipe %.0f"), *SpeciesId_, CreatureParts_.Num(), 180.f * BodyScale_, Health_, AttackDamage_);
}

void ACultFeralBeast::AnimateCreature(float DeltaSeconds, float DistanceMoved)
{
	if (!CreaturePose_ || RestPose_.IsEmpty()) return;
	GaitPhase_ += DistanceMoved / (110.f * BodyScale_) * 2.f * PI;
	GaitPhase_ = FMath::Fmod(GaitPhase_, 2.f * PI);
	AttackAnimTime_ = FMath::Max(0.f, AttackAnimTime_ - DeltaSeconds);
	const float Walk = DistanceMoved > .1f ? FMath::Sin(GaitPhase_) * .45f : 0.f;
	const float Strike = AttackAnimTime_ > .18f ? .25f : AttackAnimTime_ > 0.f ? FMath::Sin(PI * (1.f - AttackAnimTime_ / .18f)) : 0.f;
	auto Rotate = [&](FName Name, float Angle)
	{
		if (const int32* Index = AnimatedBones_.Find(Name))
		{
			FTransform Pose = RestPose_[*Index];
			Pose.SetRotation((Pose.GetRotation() * FQuat(FVector::XAxisVector, Angle)).GetNormalized());
			CreaturePose_->BoneSpaceTransforms[*Index] = Pose;
		}
	};
	Rotate(TEXT("upperleg_l"), Walk); Rotate(TEXT("upperleg_r"), -Walk);
	Rotate(TEXT("lowerleg_l"), FMath::Max(0.f, -Walk)); Rotate(TEXT("lowerleg_r"), FMath::Max(0.f, Walk));
	Rotate(TEXT("upperarm_l"), -Walk * .8f - Strike); Rotate(TEXT("upperarm_r"), Walk * .8f - Strike);
	Rotate(TEXT("lowerarm_l"), -.2f - Strike * .4f); Rotate(TEXT("lowerarm_r"), -.2f - Strike * .4f);
	Rotate(TEXT("leg-left"), Walk); Rotate(TEXT("leg-right"), -Walk);
	Rotate(TEXT("arm-left"), -Walk * .8f - Strike); Rotate(TEXT("arm-right"), Walk * .8f - Strike);
	Rotate(TEXT("hips"), StaggerTime_>0?-.22f:0.f);
	Rotate(TEXT("torso"), Strike * .15f+(StaggerTime_>0?-.3f:0.f));
	CreaturePose_->MarkRefreshTransformDirty();
	CreaturePose_->RefreshBoneTransforms();
}

bool ACultFeralBeast::GroundAt(FVector Position, FVector& OutPosition) const
{
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CultBeastGround), false, this);
	if (Player_) Params.AddIgnoredActor(Player_);
	const auto Shape=FCollisionShape::MakeCapsule(Collision_->GetScaledCapsuleRadius(),Collision_->GetScaledCapsuleHalfHeight());
 if (!GetWorld()->SweepSingleByObjectType(Ground, Position + FVector(0, 0, 500.f * BodyScale_), Position - FVector(0, 0, 5000.f),FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Params)) return false;
 if (Ground.bStartPenetrating || Ground.ImpactNormal.Z < .65f) return false;
 OutPosition = Ground.Location + FVector(0, 0, 2.4f);
	return true;
}

void ACultFeralBeast::AttachEyes()
{
	// Two HDR-glow spheres at the front-upper of the body. Positions come
	// from the mesh's own bounding box, so every species gets fitting eyes
	// without per-model tuning. Assumes the model faces +X (same assumption
	// as movement — if beasts walk sideways, both get fixed together).
	UStaticMesh* Mesh = MeshComp_ ? MeshComp_->GetStaticMesh() : nullptr;
	if (!Mesh) return;
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Engine/BasicShapes/Sphere"));
	if (!Sphere) return;
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/CultUlhu/Materials/M_Glow_Eye"));

	const FBoxSphereBounds B = Mesh->GetBounds();
	const float FrontX = B.Origin.X + B.BoxExtent.X * 0.72f;
	const float EyeY = FMath::Max(B.BoxExtent.Y * 0.30f, 8.0f);
	const float EyeZ = B.Origin.Z + B.BoxExtent.Z * 0.55f;
	const float EyeD = FMath::Clamp(B.SphereRadius * 0.09f, 6.0f, 30.0f);
	// BasicShapes/Sphere is 100 cm across: scale to the wanted diameter.
	const float S = EyeD / 100.0f;

	EyeL_->SetStaticMesh(Sphere);
	EyeR_->SetStaticMesh(Sphere);
	if (Glow)
	{
		EyeL_->SetMaterial(0, Glow);
		EyeR_->SetMaterial(0, Glow);
	}
	EyeL_->SetRelativeLocation(FVector(FrontX, EyeY, EyeZ));
	EyeR_->SetRelativeLocation(FVector(FrontX, -EyeY, EyeZ));
	EyeL_->SetRelativeScale3D(FVector(S));
	EyeR_->SetRelativeScale3D(FVector(S));
	EyeL_->SetVisibility(true);
	EyeR_->SetVisibility(true);
}

void ACultFeralBeast::ApplyDamage(float Amount) { ReceiveCombatDamage(Amount,nullptr); }

void ACultFeralBeast::ReceiveCombatDamage(float Amount,AActor* Source)
{
	if (IsDead() || Amount <= 0.0f)
		return;
 if(auto* Camp=JungleCamp_.Get()) { if(!Camp->DamageAllowed(Source)) return; Camp->Damaged(Source); }
 Health_ -= Amount;
 if(Health_>0) {
  PoiseReset_=2.f;
  if(StaggerCooldown_<=0) {
   PoiseDamage_+=Amount;
   const float Threshold=IsJungleMonster()?MaxHealth_*(SpeciesId_==TEXT("ossified_brute")?.45f:.3f):SpeciesId_==TEXT("ossified_brute")?405.f:SpeciesId_==TEXT("pale_wight")?270.f:135.f;
   if(PoiseDamage_>=Threshold) {
    const bool HeavyPoise=IsJungleMonster()?SpeciesId_==TEXT("ossified_brute"):BodyScale_>1.2f;
    PoiseDamage_=0; StaggerTime_=HeavyPoise?.35f:.22f; StaggerCooldown_=HeavyPoise?2.4f:1.5f;
    RecoveryTime_=FMath::Max(RecoveryTime_,.45f); AttackAnimTime_=0;
    for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this&&It->IsWindingUp()) It->Destroy();
   }
  }
 }
	// Getting smashed gets their attention.
	if (Health_ > 0.0f && State_ == EState::Wander)
		State_ = EState::Chase;
	else if (Health_ <= 0.0f)
		Die();
}

void ACultFeralBeast::Die()
{
	State_ = EState::Dying;
 if(auto* Camp=JungleCamp_.Get()) Camp->MonsterDied(this);
 for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this) It->Destroy();
	DeathTime_ = 0.0f;
	if (MeshComp_)
		MeshComp_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
 if(Control_.Stop) return;
	FVector Loc = GetActorLocation();
	FVector D = Dest - Loc;
	D.Z = 0.0f; // steer on the plane at spawn height
	const float Dist = D.Size();
	if (Dist < 5.0f)
		return;
 const float Step=FMath::Min(Dist,SpeedMs*Control_.Speed*100.f*DeltaSeconds);
 for(float Turn:{0.f,35.f,-35.f,70.f,-70.f}) {
  const FVector Direction=(D/Dist).RotateAngleAxis(Turn,FVector::UpVector);
  FVector Grounded;
  if(!GroundAt(Loc+Direction*Step,Grounded)||FMath::Abs(Grounded.Z-Loc.Z)>100.f*BodyScale_) continue;
  SetActorLocation(Grounded,true);
  if(FVector::Dist2D(Loc,GetActorLocation())>.1f) { FaceToward(Loc+Direction*200,DeltaSeconds); return; }
 }
}

void ACultFeralBeast::Tick(float DeltaSeconds)
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, CreatureTick);
	Super::Tick(DeltaSeconds);
 Control_.Tick(DeltaSeconds);
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
  if(DeathTime_>.35f) { ACultCorpse::Leave(this); State_=EState::Dead; Destroy(); }

		return;
	}

 if(auto* Camp=JungleCamp_.Get()) { Camp->TickMonster(this,DeltaSeconds); return; }

	// Predators use the nearest entity, regardless of whether a human controls it.
 float Closest=6000.f; ACultUlhuPlayerPawn* Nearest=nullptr;
 for(TActorIterator<ACultUlhuPlayerPawn> It(GetWorld());It;++It) { const float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation()); if(D<Closest) { Closest=D; Nearest=*It; } }
 Player_=Nearest;
 const FVector PlayerLoc = Player_ ? Player_->GetActorLocation() : Home_ + FVector(100000.0f, 0, 0);
	const float DistToPlayer = FVector::Dist2D(GetActorLocation(), PlayerLoc);
 // A giant capsule narrows near its feet. Use its cross-section at arm height,
 // rather than the widest body radius, or small creatures swing outside contact.
 float Contact=Collision_->GetScaledCapsuleRadius()+110.f;
 if(Player_) {
  const auto* Cap=Player_->GetCapsuleComponent(); const float Radius=Cap->GetScaledCapsuleRadius(),Half=Cap->GetScaledCapsuleHalfHeight();
  const float Height=GetActorLocation().Z-Collision_->GetScaledCapsuleHalfHeight()+100.f*BodyScale_-(PlayerLoc.Z-Half);
  if(Height>=0&&Height<=2*Half) {
   const float Offset=FMath::Max(0.f,FMath::Max(Radius-Height,Height-(2*Half-Radius)));
   Contact+=FMath::Sqrt(FMath::Max(0.f,Radius*Radius-Offset*Offset));
  }
 }


	// Far culling: think at 2 Hz past 150 m.
	if (DistToPlayer > 15000.0f)
	{
		FarAcc_ += DeltaSeconds;
		if (FarAcc_ < 0.5f)
			return;
		DeltaSeconds = FarAcc_;
		FarAcc_ = 0.0f;
	}

	if (AttackCooldown_ > 0.0f)
		AttackCooldown_ -= DeltaSeconds;
 ThinkTime_ -= DeltaSeconds;
 StaggerCooldown_=FMath::Max(0.f,StaggerCooldown_-DeltaSeconds);
 PoiseReset_=FMath::Max(0.f,PoiseReset_-DeltaSeconds); if(PoiseReset_<=0) PoiseDamage_=0;
 if(StaggerTime_>0) { StaggerTime_=FMath::Max(0.f,StaggerTime_-DeltaSeconds); AnimateCreature(DeltaSeconds,0); return; }

	const FVector PreviousLocation = GetActorLocation();
 if(bCaptured_) { for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this) It->Destroy(); AttackAnimTime_=0; const FVector Before=GetActorLocation(); if(Captor_.IsValid()&&FVector::Dist2D(Before,Captor_->GetActorLocation())>300) MoveToward(Captor_->GetActorLocation(),Speed_*.6f,DeltaSeconds); AnimateCreature(DeltaSeconds,FVector::Dist2D(Before,GetActorLocation())); return; }
 if(Control_.Stun) { for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this) It->Destroy(); AttackAnimTime_=0; AnimateCreature(DeltaSeconds,0); return; }
 if(Control_.Fear) { for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this) It->Destroy(); AttackAnimTime_=0; MoveToward(GetActorLocation()+(GetActorLocation()-Control_.Origin).GetSafeNormal2D()*1000,Speed_,DeltaSeconds); AnimateCreature(DeltaSeconds,FVector::Dist2D(PreviousLocation,GetActorLocation())); return; }
 if(AttackAnimTime_>0) { AnimateCreature(DeltaSeconds,0); return; }
 if(RecoveryTime_>0) { RecoveryTime_=FMath::Max(0.f,RecoveryTime_-DeltaSeconds); AnimateCreature(DeltaSeconds,0); return; }
 // Hunters and escaped/bred ferals can actually harm cultists in reach.
 if(AttackCooldown_<=0) for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->IsCultist()&&!It->IsCaptive()&&FVector::Dist2D(It->GetActorLocation(),GetActorLocation())<Collision_->GetScaledCapsuleRadius()+220&&FMath::Abs(It->GetActorLocation().Z-GetActorLocation().Z)<300) { BeginSwipe(It->GetActorLocation()); break; }

 if(AttackAnimTime_>0) { AnimateCreature(DeltaSeconds,0); return; }
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
  if(Player_) {
   if(DistToPlayer>Contact) {
    FVector Goal=PlayerLoc;
    if((SpeciesId_==TEXT("skittering_ghoul")||SpeciesId_==TEXT("charnel_imp"))&&DistToPlayer<Contact+1200) {
     const float Side=(GetUniqueID()%2)?1.f:-1.f;
     const FVector Around=(GetActorLocation()-PlayerLoc).GetSafeNormal2D().RotateAngleAxis(Side*24.f,FVector::UpVector);
     Goal=PlayerLoc+Around*Contact*.9f;
    }
    MoveToward(Goal,Speed_,DeltaSeconds);
   } else FaceToward(PlayerLoc,DeltaSeconds);
  }
		if (Player_ && DistToPlayer < Contact+40.f && AttackCooldown_ <= 0.0f)
		{
			const FVector PlayerFeet = PlayerLoc - FVector(0, 0, Player_->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const FVector EnemyFeet = GetActorLocation() - FVector(0, 0, Collision_->GetScaledCapsuleHalfHeight());
			FHitResult Obstacle;
			FCollisionQueryParams Sight(SCENE_QUERY_STAT(CultBeastSwipe), false, this);
			Sight.AddIgnoredActor(Player_);
			if (FMath::Abs(PlayerFeet.Z - EnemyFeet.Z) > 220.f * BodyScale_ ||
				GetWorld()->LineTraceSingleByChannel(Obstacle, EnemyFeet + FVector(0, 0, 100.f), PlayerFeet + FVector(0, 0, 100.f), ECC_Visibility, Sight)) break;
			FaceToward(PlayerLoc, DeltaSeconds, 12.0f);
			const FVector Loc = GetActorLocation();
			ACultBlastFX::SpawnBlast(GetWorld(), ECultBlastKind::DustBurst,
				Loc + FVector(0.0f, 0.0f, 50.0f), .8f);
   BeginSwipe(PlayerFeet+FVector(0,0,100));
		}
		else if (DistToPlayer > 6000.0f)
			State_ = EState::Wander;
		break;

	default:
		break;
	}
	AnimateCreature(DeltaSeconds, FVector::Dist2D(PreviousLocation, GetActorLocation()));
}

void ACultFeralBeast::BeginSwipe(FVector Target) {
 const FVector Origin=GetActorLocation()-FVector(0,0,Collision_->GetScaledCapsuleHalfHeight())+FVector(0,0,100*BodyScale_);
 FTransform T(Origin); auto* A=GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); if(!A) return;
 A->Shape=ACultSkillAttack::EShape::Sweep; A->Direction=(Target-Origin).GetSafeNormal(); A->Radius=30*BodyScale_; A->Reach=220*BodyScale_; const bool Heavy=IsJungleMonster()?SpeciesId_==TEXT("ossified_brute"):BodyScale_>1.5f; A->Windup=Heavy?.72f:SpeciesId_==TEXT("pale_wight")?.55f:.4f; A->Duration=.18f; A->HalfAngle=55;
 TWeakObjectPtr<ACultFeralBeast> Weak(this); const float Damage=AttackDamage_; A->Hit=[Weak,Damage](AActor* Other,FVector P) { if(Weak.IsValid()&&!Weak->IsDead()) ACultSkillAttack::Hurt(Other,Damage,Weak.Get(),P); }; A->FinishSpawning(T);
 RecoveryTime_=Heavy?1.05f:SpeciesId_==TEXT("pale_wight")?.8f:.5f;
 AttackAnimTime_=A->Windup+A->Duration; AttackCooldown_=AttackAnimTime_+RecoveryTime_+.18f;
}
