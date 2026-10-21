#include "CultUlhuPlayerPawn.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);
// CULT-ULHU player pawn implementation.
#include "CultCanonicalKit.h"
#include "CultMatchDirector.h"
#include "CultCombatAudio.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "core/EventBus.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

#include "CultBlastFX.h"
#include "CultSkillAttack.h"
#include "CultCorpse.h"
#include "CultDestructible.h"
#include "CultFeralBeast.h"
#include "CultUlhuGameInstance.h"
#include "CultPopulation.h"
#include "CultKitRuntime.h"
#include "CultPlayHUD.h"
#include "CultLivingLandscape.h"
#include "CultWorldDirector.h"

ACultUlhuPlayerPawn::ACultUlhuPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	VesselBody_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VesselBody"));
	VesselBody_->SetupAttachment(GetCapsuleComponent());
	VesselBody_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 VesselBody_->ComponentTags.Add(TEXT("CultCorpseBody"));

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
 if(CharacterId_.IsEmpty()) if(auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance())) CharacterId_=GI->SelectedCharacterId;
 if(!CultCanonicalKit::IsReleased(CharacterId_)) { Destroy(); return; }
 Kit_=new FCultKitRuntime(this);
 if(!Kit_->Initialize(CharacterId_)) { delete Kit_; Kit_=nullptr; }
 if(Kit_) Health_=Kit_->Hp();
	if (UCultUlhuGameInstance* GI = Cast<UCultUlhuGameInstance>(GetGameInstance()))
	{
		for (const FCultRosterEntry& E : UCultUlhuGameInstance::Roster())
			if (E.Id == CharacterId_) { HeightM = E.HeightM; break; }
	}
	CharScale_ = HeightM / 1.8f;
	SetActorScale3D(FVector(CharScale_));
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	// Resolve the final capsule, before the first movement tick (also works in PIE).
	if (!PlaceOnWalkableGround())
	{
		GetCharacterMovement()->DisableMovement();
		UE_LOG(LogTemp, Error, TEXT("[CultUlhu] No clear walkable spawn near %s. Repair terrain collision; movement disabled to prevent falling through the map."), *GetActorLocation().ToString());
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Red,
			TEXT("No safe ground found. Check terrain collision and PlayerStart. See log."));
	}

	// Kaiju stride: walk speed grows with the square root of scale.
	WalkSpeed_ = (Kit_ ? Kit_->Def.moveSpeed * 100.f : 600.f) * FMath::Sqrt(CharScale_) * 1.25f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed_;
	GetCharacterMovement()->MaxAcceleration = WalkSpeed_ * 3.f;
	GetCharacterMovement()->BrakingDecelerationWalking = WalkSpeed_ * 4.f;
	GetCharacterMovement()->MaxStepHeight = 45.f * CharScale_;
	GetCharacterMovement()->GravityScale = 2.f * FMath::Sqrt(CharScale_);
	// A visible jump of 40% of body height, with a deliberate heavy landing.
	GetCharacterMovement()->JumpZVelocity = FMath::Sqrt(2.f * FMath::Abs(GetCharacterMovement()->GetGravityZ()) * HeightM * 100.f * 0.4f);
	HomeLocation_ = LastSafeLocation_ = GetActorLocation();
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// Camera rig for the scaled body.
	const float HalfH = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	SpringArm_->TargetArmLength = (ACultMatchDirector::IsMatch(GetWorld())?260.f:700.f) * CharScale_;
 SpringArm_->SocketOffset=ACultMatchDirector::IsMatch(GetWorld())?FVector(0,90*CharScale_,15*CharScale_):FVector::ZeroVector;
	SpringArm_->SetRelativeLocation(FVector(0.0f, 0.0f, HalfH * 0.75f));
	bThirdPerson_ = true;

	// Vessel treatment: the roster FBX models haven't landed yet, so wear
	// a shadowy vessel â€” dark unlit capsule + glowing eyes â€” instead of a
	// flat gray capsule. Positions use the UNSCALED half-height: the actor
	// scale applies once on top.
	if (UMaterialInterface* Vessel = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/CultUlhu/Materials/M_PawnVessel")))
		GetCapsuleComponent()->SetMaterial(0, Vessel);
	VesselBody_->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere")));
	VesselBody_->SetRelativeScale3D(FVector(.84f, .84f, 1.76f));

 // A quiet shadow vessel keeps the placeholder readable until authored entity rigs arrive.
 if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"))) {
  auto* Shadow=UMaterialInstanceDynamic::Create(Base,this);
  Shadow->SetVectorParameterValue(TEXT("Color"),Faction_==0?FLinearColor(.025f,.055f,.07f):FLinearColor(.075f,.025f,.04f));
  VesselBody_->SetMaterial(0,Shadow);
 }

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
			E->ComponentTags.Add(TEXT("VesselEye"));
			E->RegisterComponent();
			E->AttachToComponent(GetCapsuleComponent(),
				FAttachmentTransformRules::KeepRelativeTransform);
			E->SetRelativeLocation(FVector(UH * 0.30f,
				(i == 0 ? UH * 0.14f : -UH * 0.14f), UH * 0.72f));
			E->SetRelativeScale3D(FVector(S));
		}
	}
}

bool ACultUlhuPlayerPawn::PlaceOnWalkableGround()
{
	// Existing maps may predate the builder's collision settings. Repair only
	// the known static terrain, never water, props, or arbitrary meshes.
	FBox GroundBounds(ForceInit);
 if(auto* Land=ACultLivingLandscape::Find(GetWorld())) GroundBounds=Land->GroundBounds();
 for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("CultMatchFloor"))) GroundBounds+=It->GetStaticMeshComponent()->Bounds.GetBox();
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		UStaticMeshComponent* Comp = It->GetStaticMeshComponent();
		UStaticMesh* TerrainMesh = Comp ? Comp->GetStaticMesh() : nullptr;
		if (!TerrainMesh || TerrainMesh->GetName() != TEXT("dark_wilds_ground") || ACultLivingLandscape::Find(GetWorld())) continue;
		if (UBodySetup* Body = TerrainMesh->GetBodySetup())
		{
			const bool NeedsRepair=Body->CollisionTraceFlag!=CTF_UseComplexAsSimple ||
				Comp->GetCollisionProfileName()!=FName(TEXT("BlockAll")) ||
				Comp->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics ||
				Comp->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block;
			if(NeedsRepair) {
				Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
				Comp->SetCollisionProfileName(TEXT("BlockAll"));
				Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Comp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				Comp->RecreatePhysicsState();
			}
			GroundBounds += Comp->Bounds.GetBox();
		}
	}

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TerrainBounds_ = GroundBounds;
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Origin = GetActorLocation();
	const float Top = GroundBounds.IsValid ? GroundBounds.Max.Z + 2.f * HalfHeight + 100.f : Origin.Z + 100000.f;
	const float Bottom = GroundBounds.IsValid ? GroundBounds.Min.Z - 2.f * HalfHeight - 100.f : Origin.Z - 100000.f;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CultGroundSpawn), false, this);
	// A full capsule sweep handles slopes and the whole creature footprint.
	// Bounded nearby candidates allow a start under a prop to recover safely.
	for (int32 Candidate = 0; Candidate < 33; ++Candidate)
	{
		FVector Offset = FVector::ZeroVector;
		if (Candidate > 0)
		{
			const float Angle = (Candidate - 1) % 8 * PI / 4.f;
			const float Distance = (1 + (Candidate - 1) / 8) * Radius * 2.5f;
			Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
		}
		FVector Start = Origin + Offset;
		Start.Z = Top;
		FVector End = Start;
		End.Z = Bottom;
		FHitResult Hit;
		const bool FoundGround=GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,ECC_Pawn, Shape, Params);
  if(Candidate==0) UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Spawn sweep found=%d actor=%s component=%s normal=%s penetrating=%d walkable=%d"),FoundGround,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactNormal.ToString(),Hit.bStartPenetrating,GetCharacterMovement()->IsWalkable(Hit));
		if (!FoundGround || Hit.bStartPenetrating ||
			!GetCharacterMovement()->IsWalkable(Hit)) continue;
		// Avoid starting atop buildings, beasts, or water in the generated world.
		const UStaticMeshComponent* Surface = Cast<UStaticMeshComponent>(Hit.GetComponent());
		if (GroundBounds.IsValid && !Hit.GetActor()->ActorHasTag(TEXT("CultMatchFloor")) && !Cast<ACultLivingLandscape>(Hit.GetActor()) && (!Surface || !Surface->GetStaticMesh() ||
			Surface->GetStaticMesh()->GetName() != TEXT("dark_wilds_ground"))) continue;
		const FVector Location = Hit.Location + FVector(0.f, 0.f, 2.4f);
		if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity,
			ECC_Pawn, Shape, Params)) continue;
		SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		UE_LOG(LogTemp, Log, TEXT("[CultUlhu] Grounded spawn: scale=%.3f capsuleHalfHeight=%.1f start=%s placed=%s surface=%s"),
			CharScale_, HalfHeight, *Origin.ToString(), *Location.ToString(), *GetNameSafe(Hit.GetActor()));
		return true;
	}
	return false;
}

void ACultUlhuPlayerPawn::SetupPlayerInputComponent(UInputComponent* IC)
{
	Super::SetupPlayerInputComponent(IC);
	// Zero-asset bindings: no Input Action assets required.
	IC->BindKey(EKeys::G, IE_Pressed, this, &ACultUlhuPlayerPawn::RecallMatch);
 IC->BindKey(EKeys::Enter, IE_Pressed, this, &ACultUlhuPlayerPawn::RematchConfirm).bExecuteWhenPaused=true;
 IC->BindKey(EKeys::V, IE_Pressed, this, &ACultUlhuPlayerPawn::ToggleCamera);
	IC->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACultUlhuPlayerPawn::TryJump);
	IC->BindKey(EKeys::SpaceBar, IE_Released, this, &ACharacter::StopJumping);
	IC->BindKey(EKeys::Escape, IE_Pressed, this, &ACultUlhuPlayerPawn::ReturnToMenu).bExecuteWhenPaused=true;
	IC->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACultUlhuPlayerPawn::Smash).bExecuteWhenPaused=true;
 IC->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ACultUlhuPlayerPawn::CommandReleased);
	IC->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ACultUlhuPlayerPawn::HeavyAttack);
 IC->BindKey(EKeys::RightMouseButton, IE_Released, this, &ACultUlhuPlayerPawn::ReleaseHeavy);
 IC->BindKey(EKeys::LeftAlt, IE_Pressed, this, &ACultUlhuPlayerPawn::AltPressed);
 IC->BindKey(EKeys::LeftAlt, IE_Released, this, &ACultUlhuPlayerPawn::AltReleased);
 IC->BindKey(EKeys::B, IE_Pressed, this, &ACultUlhuPlayerPawn::ToggleFaith);
 IC->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ACultUlhuPlayerPawn::FaithScrollUp);
 IC->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ACultUlhuPlayerPawn::FaithScrollDown);
 IC->BindKey(EKeys::Tab, IE_Pressed, this, &ACultUlhuPlayerPawn::ToggleStats);
	IC->BindKey(EKeys::Q, IE_Pressed, this, &ACultUlhuPlayerPawn::AbilityQ);
	IC->BindKey(EKeys::F, IE_Pressed, this, &ACultUlhuPlayerPawn::AbilityF);
	IC->BindKey(EKeys::R, IE_Pressed, this, &ACultUlhuPlayerPawn::AbilityR);
	IC->BindKey(EKeys::E, IE_Pressed, this, &ACultUlhuPlayerPawn::Interact);
	IC->BindAxisKey(EKeys::MouseX, this, &ACultUlhuPlayerPawn::LookYaw);
	IC->BindAxisKey(EKeys::MouseY, this, &ACultUlhuPlayerPawn::LookPitch);
}

void ACultUlhuPlayerPawn::ToggleCamera()
{	bThirdPerson_ = !bThirdPerson_;
	VesselBody_->SetVisibility(bThirdPerson_);
	TArray<UStaticMeshComponent*> Visuals;
	GetComponents(Visuals);
	for (UStaticMeshComponent* Visual : Visuals)
		if (Visual->ComponentHasTag(TEXT("VesselEye"))) Visual->SetVisibility(bThirdPerson_);
	const float HalfH = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (bThirdPerson_)
	{
		SpringArm_->TargetArmLength = (ACultMatchDirector::IsMatch(GetWorld())?260.f:700.f) * CharScale_;
 SpringArm_->SocketOffset=ACultMatchDirector::IsMatch(GetWorld())?FVector(0,90*CharScale_,15*CharScale_):FVector::ZeroVector;
		SpringArm_->SetRelativeLocation(FVector(0.0f, 0.0f, HalfH * 0.75f));
	}
	else
	{
		// First person: collapse the boom, park the pivot at head height.
  SpringArm_->SocketOffset=FVector::ZeroVector;
		SpringArm_->TargetArmLength = 0.0f;
		SpringArm_->SetRelativeLocation(FVector(40.0f, 0.0f, HalfH * 0.85f));
	}
}

void ACultUlhuPlayerPawn::ReturnToMenu()
{
 if(auto* M=ACultMatchDirector::Find(GetWorld())) if(!M->IsOver()) { M->TogglePause(); return; }
	if (UCultUlhuGameInstance* GI = Cast<UCultUlhuGameInstance>(GetGameInstance()))
		GI->ReturnToMenu();
}

void ACultUlhuPlayerPawn::Smash()
{
 if(auto* M=ACultMatchDirector::Find(GetWorld())) {
  if(M->IsOver()||M->IsPaused()) { M->ClickResult(); return; }
  if(!M->CanAct(this)) return;
  if(auto* PC=Cast<APlayerController>(GetController())) if(PC->IsInputKeyDown(EKeys::LeftAlt)) { if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) { if(HUD->FaithVisible()) HUD->CommandClick(); else M->Ping(this); } return; }
  M->OnAction(this);
 }
 if(auto* PC=Cast<APlayerController>(GetController())) if(PC->IsInputKeyDown(EKeys::LeftAlt)) { if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) HUD->CommandClick(); return; }
 if(Kit_ && Kit_->LeftClick()) return;
	if(Control_.Stun || Control_.Fear) return;
 if (SmashCooldown_ > 0.0f || IsSwinging()) { if(!IsBot()&&SmashCooldown_<=.18f) MeleeBuffer_=.18f; return; }
 MeleeBuffer_=0; SmashCooldown_ = .62f;
 const float ComboMult=Kit_?Kit_->BeginMelee():1.f;

 StartMelee(135.f*ComboMult);

}

void ACultUlhuPlayerPawn::AddCameraKick(float Strength)
{
	ShakeTime_ = FMath::Max(ShakeTime_,
		ShakeDur_ * FMath::Clamp(Strength, 0.0f, 1.0f));
}

float ACultUlhuPlayerPawn::GetHealth() const { return Kit_?Kit_->Hp():Health_; }

void ACultUlhuPlayerPawn::SetSprinting(bool bSprint)
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed_ * (DashTime_>0?DashSpeed_/WalkSpeed_:bSprint ? 1.85f : 1.f) * (Control_.Stop?0.f:Control_.Speed);
}

void ACultUlhuPlayerPawn::LookYaw(float Value) { if(auto* PC=Cast<APlayerController>(GetController())) if(PC->bShowMouseCursor) return; AddControllerYawInput(Value); if(Kit_) Kit_->Mouse(Value,0); }
void ACultUlhuPlayerPawn::LookPitch(float Value) { if(auto* PC=Cast<APlayerController>(GetController())) if(PC->bShowMouseCursor) return; AddControllerPitchInput(-Value); if(Kit_) Kit_->Mouse(0,Value); }

void ACultUlhuPlayerPawn::GuardTerrainBoundary()
{
	if (!TerrainBounds_.IsValid) return;
	const FVector Position = GetActorLocation();
	const float Margin = GetCapsuleComponent()->GetScaledCapsuleRadius() + 150.f;
	const bool Outside = Position.X < TerrainBounds_.Min.X + Margin || Position.X > TerrainBounds_.Max.X - Margin ||
		Position.Y < TerrainBounds_.Min.Y + Margin || Position.Y > TerrainBounds_.Max.Y - Margin;
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CultBoundary), false, this);
	const bool Supported = GetWorld()->LineTraceSingleByChannel(Ground,
		FVector(Position.X, Position.Y, TerrainBounds_.Max.Z + 100.f),
		FVector(Position.X, Position.Y, TerrainBounds_.Min.Z - 100.f), ECC_WorldStatic, Params);
	if (Outside || !Supported || Position.Z < TerrainBounds_.Min.Z - 200.f)
	{
		SetActorLocation(LastSafeLocation_, false, nullptr, ETeleportType::TeleportPhysics);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		return;
	}
	if (GetCharacterMovement()->IsMovingOnGround() && !GetCharacterMovement()->CurrentFloor.HitResult.bStartPenetrating)
		LastSafeLocation_ = Position;
}

void ACultUlhuPlayerPawn::AreaAttack(FVector Center, float Radius, float Damage)
{
	for (TActorIterator<ACultDestructible> It(GetWorld()); It; ++It)
		if (FVector::Dist(It->GetActorLocation(), Center) < Radius) It->ApplyDamage(Damage, Center);
	for (TActorIterator<ACultFeralBeast> It(GetWorld()); It; ++It)
		if (FVector::Dist(It->GetActorLocation(), Center) < Radius) It->ApplyDamage(Damage);
	for (TActorIterator<ACultPopulation> It(GetWorld()); It; ++It)
		if (It->GetFaction()!=Faction_ && FVector::Dist(It->GetActorLocation(), Center) < Radius) It->ReceiveAttack(Damage);
	ACultBlastFX::SpawnBlast(GetWorld(), ECultBlastKind::ShockRing, Center, Radius / 100.f);
	AddCameraKick(0.7f);
}

void ACultUlhuPlayerPawn::HeavyAttack() { if(auto* M=ACultMatchDirector::Find(GetWorld())) { if(!M->CanAct(this)) return; M->OnAction(this); } if(Control_.Stun || Control_.Fear) return; if(auto* PC=Cast<APlayerController>(GetController())) if(PC->bShowMouseCursor) { if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) HUD->CloseCommands(); return; } bRmbHeld_=true; if(Kit_) Kit_->Heavy(true); }
void ACultUlhuPlayerPawn::ReleaseHeavy() { bRmbHeld_=false; if(Kit_) Kit_->Heavy(false); }
void ACultUlhuPlayerPawn::AbilityQ() { if(auto* M=ACultMatchDirector::Find(GetWorld())) { if(!M->AbilityAvailable(this,0,true)) { if(!IsBot()) Notify(TEXT("Ability locked: choose Q or F at level 1, both at level 3, R at level 6.")); return; } } if(!Control_.Stun && !Control_.Fear && Kit_) Kit_->Cast(0); }
void ACultUlhuPlayerPawn::AbilityF() { if(auto* M=ACultMatchDirector::Find(GetWorld())) { if(!M->AbilityAvailable(this,1,true)) { if(!IsBot()) Notify(TEXT("Ability locked: choose Q or F at level 1, both at level 3, R at level 6.")); return; } } if(!Control_.Stun && !Control_.Fear && Kit_) Kit_->Cast(1); }
void ACultUlhuPlayerPawn::AbilityR() { if(auto* M=ACultMatchDirector::Find(GetWorld())) { if(!M->AbilityAvailable(this,2,true)) { if(!IsBot()) Notify(TEXT("Ability locked: choose Q or F at level 1, both at level 3, R at level 6.")); return; } } if(!Control_.Stun && !Control_.Fear && Kit_) Kit_->Cast(2); }
void ACultUlhuPlayerPawn::ReceiveDamage(float Amount,AActor* Source)
{
 if(Amount<=0) return;
 if(auto* M=ACultMatchDirector::Find(GetWorld())) if(!M->DamageAllowed(this,Source)) return;
 if(IsEvading()) { ConfirmEvade(); return; }
 MarkCombat();
 const float Before=GetHealth();
 Health_=Kit_?Kit_->Damage(Amount):Health_-Amount;
 const float Actual=FMath::Clamp(Before-Health_,0.f,Before);
 if(auto* M=ACultMatchDirector::Find(GetWorld())) M->NoteDamage(this,Source,Actual);
 if(Actual>0) {
  if(auto* Attacker=Cast<ACultUlhuPlayerPawn>(Source)) Attacker->MarkCombat();
  const bool PlayHurtCue=HurtFeedbackTime_<=.23f;
  HurtFeedbackTime_=.32f; bIncomingDirection_=IsValid(Source); if(bIncomingDirection_) IncomingOrigin_=Source->GetActorLocation();
  if(auto* Attacker=Cast<ACultUlhuPlayerPawn>(Source)) Attacker->RecordCombatHit(this,GetActorLocation(),Actual,Health_<=0);
  AddCameraKick(.2f);
  if(!IsBot()&&PlayHurtCue) PlayCultCombatCue(GetWorld(),ECultCombatCue::Hurt,.32f);
 }
 if(Health_<=0) {
  GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); DashTime_=0; MeleeBuffer_=0; SwingTime_=0;
  for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this) It->Destroy();
  ACultCorpse::Leave(this);
  if(auto* M=ACultMatchDirector::Find(GetWorld())) { M->HeroDied(this,Source); return; }
  if(auto* D=ACultWorldDirector::Find(GetWorld())) { cultulhu::GameEvent E(cultulhu::EventType::Defeated); D->Bus(Faction_).publish(E); }
  if(IsBot()) { Destroy(); return; }
  SetActorLocation(HomeLocation_,false,nullptr,ETeleportType::TeleportPhysics); PlaceOnWalkableGround(); LastSafeLocation_=GetActorLocation();
  delete Kit_; Kit_=new FCultKitRuntime(this); Kit_->Initialize(CharacterId_); Health_=Kit_->Hp(); Tags.Remove(TEXT("CorpseRecorded")); Notify(TEXT("Your vessel reforms at the sanctum"));
 }
}
void ACultUlhuPlayerPawn::Interact()
{
 if(auto* M=ACultMatchDirector::Find(GetWorld())) { M->Interact(this); return; }
 const FHitResult Hit=CameraAim();
 if(auto* P=Cast<ACultPopulation>(Hit.GetActor())) if(FVector::Dist2D(FeetLocation(),P->GetActorLocation())<=300.f && P->IsCaptive()) { P->RecruitTo(Faction_); return; }
 if(Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("CultTemple")) && FVector::Dist2D(FeetLocation(),Hit.ImpactPoint)<=300.f+GetCapsuleComponent()->GetScaledCapsuleRadius()) { Notify(TEXT("Altar: hold ALT and click to choose a ritual")); return; }
 Notify(TEXT("E interacts with an altar or captive within 3 metres"));
}
FString ACultUlhuPlayerPawn::GetStatusText() const { return Kit_?Kit_->Status():TEXT("Original character kit unavailable"); }
float ACultUlhuPlayerPawn::GetMaxHealth() const { return Kit_?Kit_->MaxHp():500.f; }
FVector ACultUlhuPlayerPawn::GetLastEffectLocation() const { return Kit_?Kit_->LastEffectLocation:FVector::ZeroVector; }
void ACultUlhuPlayerPawn::EndPlay(const EEndPlayReason::Type Reason) { delete Kit_; Kit_=nullptr; Super::EndPlay(Reason); }
FVector ACultUlhuPlayerPawn::FeetLocation() const { return GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()); }
FRotator ACultUlhuPlayerPawn::AimRotation() const { return IsBot()&&BotAim_.IsValid()?(BotAim_->GetActorLocation()-GetActorLocation()).Rotation():Camera_->GetComponentRotation(); }
FHitResult ACultUlhuPlayerPawn::CameraAim(float RangeCm,bool Cursor) const
{
 FVector Start=Camera_->GetComponentLocation(), Direction=Camera_->GetForwardVector();
 if(IsBot()) { Start=FeetLocation()+FVector(0,0,150); Direction=BotAim_.IsValid()?(BotAim_->GetActorLocation()-Start).GetSafeNormal():GetActorForwardVector(); }
 else if(Cursor) if(auto* PC=Cast<APlayerController>(GetController())) PC->DeprojectMousePositionToWorld(Start,Direction);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(CultCameraAim),true,this); FHitResult Hit;
 const FVector Far=Start+Direction*250000.f; GetWorld()->LineTraceSingleByChannel(Hit,Start,Far,ECC_Visibility,Q);
 FVector Point=Hit.bBlockingHit?Hit.ImpactPoint:Far;
 const float Reach=RangeCm+GetCapsuleComponent()->GetScaledCapsuleRadius();
 if(RangeCm<25000.f && FVector::Dist2D(FeetLocation(),Point)>Reach) { Point=FeetLocation()+(Point-FeetLocation()).GetSafeNormal()*Reach; Hit=FHitResult(); }
 Hit.TraceStart=Start; Hit.TraceEnd=Point; if(!Hit.bBlockingHit) Hit.ImpactPoint=Point; return Hit;
}
void ACultUlhuPlayerPawn::ConfigureBot(const FString& Id,int32 Faction) { CharacterId_=Id; Faction_=Faction; bBotControlled_=true; }
bool ACultUlhuPlayerPawn::IsRmbHeld() const { return bRmbHeld_; }
void ACultUlhuPlayerPawn::Notify(const FString& Text) { Notice_=Text; NoticeTime_=Text.Len()>80?6.f:3.f; if(!IsBot()) UE_LOG(LogTemp,Log,TEXT("[CultUlhu] %s"),*Text); }
void ACultUlhuPlayerPawn::AltPressed() { MeleeBuffer_=0; if(auto* PC=Cast<APlayerController>(GetController())) { PC->bShowMouseCursor=true; FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); PC->SetInputMode(M); } }
void ACultUlhuPlayerPawn::CommandReleased() { if(auto* PC=Cast<APlayerController>(GetController())) if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) HUD->CommandRelease(); }
void ACultUlhuPlayerPawn::AltReleased() { if(auto* PC=Cast<APlayerController>(GetController())) { if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) HUD->CloseCommands(); PC->bShowMouseCursor=false; FInputModeGameOnly M; M.SetConsumeCaptureMouseDown(false); PC->SetInputMode(M); } }
void ACultUlhuPlayerPawn::TryJump() { if(!Control_.Stop && !Control_.Fear && CanJump()) { Jump(); EvadeTime_=FMath::Max(EvadeTime_,.14f); } }
void ACultUlhuPlayerPawn::ToggleStats() { bStats_=!bStats_; }

void ACultUlhuPlayerPawn::Tick(float DeltaSeconds)
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, HeroTick);
	Super::Tick(DeltaSeconds);
 if(bMatchDead_) { if(Kit_) Kit_->AdvanceCooldowns(DeltaSeconds); return; }
 Control_.Tick(DeltaSeconds); NoticeTime_=FMath::Max(0.f,NoticeTime_-DeltaSeconds);
 DashCooldown_=FMath::Max(0.f,DashCooldown_-DeltaSeconds); EvadeTime_=FMath::Max(0.f,EvadeTime_-DeltaSeconds); SwingTime_=FMath::Max(0.f,SwingTime_-DeltaSeconds); HitConfirmTime_=FMath::Max(0.f,HitConfirmTime_-DeltaSeconds);
 if(IsBot()) GetCharacterMovement()->bRunPhysicsWithNoController=true;
 if(DashTime_>0) { DashTime_=FMath::Max(0.f,DashTime_-DeltaSeconds); if(Control_.Stop) { GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); DashTime_=0; } }
 if(Control_.Stop) GetCharacterMovement()->StopMovementImmediately();
 if(Control_.Stun||Control_.Fear) for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this&&It->Shape==ACultSkillAttack::EShape::Sweep) It->Destroy();

	if(Kit_) { Kit_->Tick(DeltaSeconds); Health_=Kit_->Hp(); }
 UpdateRecovery(DeltaSeconds);
 SmashCooldown_=FMath::Max(0.f,SmashCooldown_-DeltaSeconds);
 TickCombatFeel(DeltaSeconds);
 if(auto* M=ACultMatchDirector::Find(GetWorld())) if(!M->CanAct(this)) { GetCharacterMovement()->StopMovementImmediately(); return; }
 if(IsBot()) { if(Kit_) Kit_->Sprint(false,DeltaSeconds); SetSprinting(false); return; }
 APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;


	HeavyCooldown_ = FMath::Max(0.f, HeavyCooldown_ - DeltaSeconds);
	QCooldown_ = FMath::Max(0.f, QCooldown_ - DeltaSeconds);
	FCooldown_ = FMath::Max(0.f, FCooldown_ - DeltaSeconds);
	RCooldown_ = FMath::Max(0.f, RCooldown_ - DeltaSeconds);
	GroundCheckTime_ -= DeltaSeconds;
	if (GroundCheckTime_ <= 0.f) { GroundCheckTime_ = 0.05f; GuardTerrainBoundary(); }

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
	PollMoveKeys(DeltaSeconds);
}

void ACultUlhuPlayerPawn::PollMoveKeys(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	const bool Shift=PC->IsInputKeyDown(EKeys::LeftShift)||PC->IsInputKeyDown(EKeys::RightShift);

	if(Control_.Stop||PC->bShowMouseCursor) { ProcessShift(Shift,FVector::ZeroVector,DeltaSeconds,false); SetSprinting(false); return; }
 if(Control_.Fear) { ProcessShift(Shift,FVector::ZeroVector,DeltaSeconds,false); SetSprinting(false); AddMovementInput((GetActorLocation()-Control_.Origin).GetSafeNormal2D(),1); return; }
 FVector Move = FVector::ZeroVector;
	const FRotator Yaw(0.0f, PC->GetControlRotation().Yaw, 0.0f);
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))    Move += Yaw.Vector();
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))  Move -= Yaw.Vector();
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) Move += FRotationMatrix(Yaw).GetScaledAxis(EAxis::Y);
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))  Move -= FRotationMatrix(Yaw).GetScaledAxis(EAxis::Y);
 const bool Sprint=ProcessShift(Shift,Move,DeltaSeconds);
 SetSprinting(Kit_?Kit_->Sprint(Sprint&&Move.SizeSquared()>0&&DashTime_<=0,DeltaSeconds):false);
 if (Move.SizeSquared() > 0.0f)
  AddMovementInput(Move.GetSafeNormal(), 1.0f);
}




void ACultUlhuPlayerPawn::ToggleFaith() { if(auto* PC=Cast<APlayerController>(GetController())) if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) HUD->ToggleFaith(); }

void ACultUlhuPlayerPawn::FaithScrollUp() { if(auto* PC=Cast<APlayerController>(GetController())) if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) HUD->ScrollFaith(-3); }
void ACultUlhuPlayerPawn::FaithScrollDown() { if(auto* PC=Cast<APlayerController>(GetController())) if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) HUD->ScrollFaith(3); }

bool ACultUlhuPlayerPawn::ProcessShift(bool Down,FVector Direction,float Dt,bool Allowed) {
 constexpr float TapLimit=.18f;
 if(!Allowed) { PreviousShift_=Down; ShiftHeldSeconds_=TapLimit; ShiftDirection_=FVector::ZeroVector; return false; }
 if(Down) {
  if(!PreviousShift_) { ShiftHeldSeconds_=0; ShiftDirection_=FVector::ZeroVector; }
  ShiftHeldSeconds_+=Dt;
  if(!Direction.IsNearlyZero()) ShiftDirection_=Direction.GetSafeNormal2D();
 } else if(PreviousShift_&&ShiftHeldSeconds_<TapLimit) {
  FVector Dash=Direction.IsNearlyZero()?ShiftDirection_:Direction.GetSafeNormal2D();
  if(Dash.IsNearlyZero()) Dash=AimRotation().Vector().GetSafeNormal2D();
  TryDash(Dash);
 }
 PreviousShift_=Down;
 return Down&&ShiftHeldSeconds_>=TapLimit;
}
