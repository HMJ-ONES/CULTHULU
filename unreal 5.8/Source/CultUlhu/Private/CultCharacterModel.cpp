#include "CultCharacterModel.h"
#include "CultUlhuPlayerPawn.h"
#include "CultCanonicalKit.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "SkeletalRenderPublic.h"
#include "HAL/FileManager.h"
#endif

namespace {
TSharedPtr<FJsonObject> Binding(const FString& Id) {
 // Load once per process. Editor import is followed by a new play session/process.
 static TSharedPtr<FJsonObject> Registry=[]() {
  FString Text; TSharedPtr<FJsonObject> Result;
  if(FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("CharacterModels/registry.json"))))
   FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Result);
  return Result;
 }();
 if(!Registry.IsValid()||!CultCanonicalKit::IsReleased(Id)) return nullptr;
 const TSharedPtr<FJsonObject>* Characters=nullptr; const TSharedPtr<FJsonObject>* Entry=nullptr;
 if(!Registry->TryGetObjectField(TEXT("characters"),Characters)||!(*Characters)->TryGetObjectField(Id,Entry)) return nullptr;
 return *Entry;
}
}
ACultCharacterModel::ACultCharacterModel() {
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("ApprovedModel"));
 Tags.Add(TEXT("CultApprovedCharacterModel"));
}
FString ACultCharacterModel::PortraitPath(const FString& Id) {
 auto B=Binding(Id); FString Path; if(B) B->TryGetStringField(TEXT("portrait"),Path); return Path;
}
FString ACultCharacterModel::SourceCredit(const FString& Id) {
 auto B=Binding(Id); if(!B) return FString(); FString Author,License,Url;
 B->TryGetStringField(TEXT("author"),Author); B->TryGetStringField(TEXT("license"),License); B->TryGetStringField(TEXT("source_url"),Url);
 return Author+TEXT(" / ")+License+TEXT("\n")+Url;
}
bool ACultCharacterModel::HasSourceAnimation(const FString& Id) {
 auto B=Binding(Id); const TArray<TSharedPtr<FJsonValue>>* Clips=nullptr;
 return B&&B->TryGetArrayField(TEXT("clips"),Clips)&&Clips->Num()>0;
}
bool ACultCharacterModel::Initialize(const FString& Id,bool Preview) {
 auto B=Binding(Id); if(!B) return false;
 FString Procedural;
 if(B->TryGetStringField(TEXT("procedural"),Procedural)&&Id==TEXT("sghllor")&&Procedural==TEXT("sghllor_light_and_mist")) {
  FString CorePath,MistPath; B->TryGetStringField(TEXT("core_material"),CorePath); B->TryGetStringField(TEXT("mist_material"),MistPath);
  auto* Core=LoadObject<UMaterialInterface>(nullptr,*CorePath); auto* Mist=LoadObject<UMaterialInterface>(nullptr,*MistPath);
  if(!Core||!Mist) return false;
  for(int I=0;I<3;++I) {
   auto* Part=NewObject<UStaticMeshComponent>(this); Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,I?TEXT("/Engine/BasicShapes/Plane"):TEXT("/Engine/BasicShapes/Sphere")));
   Part->SetupAttachment(RootComponent); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetGenerateOverlapEvents(false); Part->SetCanEverAffectNavigation(false);
   Part->SetCastShadow(false); Part->SetMaterial(0,I?Mist:Core); Part->ComponentTags.Add(TEXT("CultCorpseBody"));
   Part->SetRelativeLocation(FVector(0,0,90)); Part->SetRelativeScale3D(I?FVector(1.8,1.8,1):FVector(.7));
   if(I) Part->SetRelativeRotation(FRotator(90,I==1?0:90,0));
   Part->RegisterComponent(); Parts_.Add(Part);
  }
  bPreview_=Preview; bFloating_=true; return true;
 }
 const TArray<TSharedPtr<FJsonValue>>* Meshes=nullptr;
 if(!B->TryGetArrayField(TEXT("meshes"),Meshes)) return false;
 bPreview_=Preview; B->TryGetBoolField(TEXT("floating"),bFloating_); FRotator Rotation=FRotator::ZeroRotator;
 const TArray<TSharedPtr<FJsonValue>>* Angles=nullptr;
 if(B->TryGetArrayField(TEXT("rotation"),Angles)&&Angles->Num()==3)
  Rotation=FRotator((*Angles)[0]->AsNumber(),(*Angles)[1]->AsNumber(),(*Angles)[2]->AsNumber());
 FBox Bounds(ForceInit);
 for(const auto& Value:*Meshes) {
  UObject* Asset=StaticLoadObject(UObject::StaticClass(),nullptr,*Value->AsString()); UMeshComponent* Part=nullptr;
  if(auto* Mesh=Cast<USkeletalMesh>(Asset)) {
   auto* S=NewObject<USkeletalMeshComponent>(this); S->SetSkeletalMesh(Mesh);
   S->SetAnimationMode(EAnimationMode::AnimationSingleNode);
   if(Preview) S->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
   Part=S;
  } else if(auto* StaticAsset=Cast<UStaticMesh>(Asset)) {
   auto* S=NewObject<UStaticMeshComponent>(this); S->SetStaticMesh(StaticAsset);
   double Lod=0; if(B->TryGetNumberField(TEXT("render_lod"),Lod)) S->SetForcedLodModel(int32(Lod)+1);
   Part=S;
  }
  if(!Part) continue;
  Part->SetupAttachment(RootComponent); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Part->SetGenerateOverlapEvents(false); Part->SetCanEverAffectNavigation(false);
  Part->ComponentTags.Add(TEXT("CultCorpseBody"));
  Part->SetRelativeRotation(Rotation); Part->RegisterComponent(); Part->UpdateBounds();
  // Initialize is called at the origin, before attaching or placing the actor.
  Bounds+=Part->Bounds.GetBox(); Parts_.Add(Part);
 }
 if(Parts_.IsEmpty()||!Bounds.IsValid||Bounds.GetSize().Z<.001f) return false;
 const TSharedPtr<FJsonObject>* Roles=nullptr;
 if(B->TryGetObjectField(TEXT("animation_roles"),Roles)) for(const FString ClipRole:{TEXT("idle"),TEXT("walk"),TEXT("run"),TEXT("attack"),TEXT("preview")}) {
  FString Path;
  if((*Roles)->TryGetStringField(ClipRole,Path)) if(auto* Clip=LoadObject<UAnimSequence>(nullptr,*Path)) Animations_.Add(ClipRole,Clip);
 }
 const TArray<TSharedPtr<FJsonValue>>* Sets=nullptr;
 if(B->TryGetArrayField(TEXT("animation_sets"),Sets)) for(const auto& Value:*Sets) {
  auto Set=Value->AsObject(); FString SkeletonPath;
  if(!Set||!Set->TryGetStringField(TEXT("skeleton"),SkeletonPath)||!Set->TryGetObjectField(TEXT("roles"),Roles)) continue;
  for(const FString ClipRole:{TEXT("idle"),TEXT("walk"),TEXT("run"),TEXT("attack"),TEXT("preview")}) {
   FString Path; if((*Roles)->TryGetStringField(ClipRole,Path)) if(auto* Clip=LoadObject<UAnimSequence>(nullptr,*Path)) Animations_.Add(SkeletonPath+TEXT("|")+ClipRole,Clip);
  }
 }
 // Establish an actual supplied first pose before measuring nonhuman rigs.
 // Imported reference bounds can include exporter transforms absent in the clip.
 PlayRole(TEXT("preview"));
 for(UMeshComponent* Part:Parts_) if(auto* S=Cast<USkeletalMeshComponent>(Part)) {
  S->SetPosition(0,false); S->TickAnimation(0,false); S->RefreshBoneTransforms(); S->UpdateBounds();
 }
 const TArray<TSharedPtr<FJsonValue>>* Reference=nullptr;
 if(B->TryGetArrayField(TEXT("reference_bounds"),Reference)&&Reference->Num()==6) {
  Bounds=FBox(FVector((*Reference)[0]->AsNumber(),(*Reference)[1]->AsNumber(),(*Reference)[2]->AsNumber()),FVector((*Reference)[3]->AsNumber(),(*Reference)[4]->AsNumber(),(*Reference)[5]->AsNumber()));
 }
#if WITH_EDITOR
 else {
  static TMap<FString,FBox> PosedBounds;
  FString Digest,Profile; B->TryGetStringField(TEXT("source_sha256"),Digest); B->TryGetStringField(TEXT("import_profile"),Profile); const FString CacheKey=Id+Digest+Profile+Rotation.ToString();
  if(const auto* Cached=PosedBounds.Find(CacheKey)) Bounds=*Cached;
  else {
   FBox Actual(ForceInit); bool Skinned=false;
   for(UMeshComponent* Part:Parts_) if(auto* S=Cast<USkeletalMeshComponent>(Part)) {
    TArray<FFinalSkinVertex> Vertices; S->GetCPUSkinnedVertices(Vertices,0);
    for(const auto& Vertex:Vertices) Actual+=Rotation.RotateVector(FVector(Vertex.Position));
    Skinned=true;
   } else { Part->UpdateBounds(); Actual+=Part->Bounds.GetBox(); }
   if(Skinned&&Actual.IsValid&&Actual.GetSize().Z>.001f) Bounds=Actual;
   PosedBounds.Add(CacheKey,Bounds);
   auto Record=MakeShared<FJsonObject>(); Record->SetStringField(TEXT("id"),Id); Record->SetStringField(TEXT("source_sha256"),Digest);
   Record->SetStringField(TEXT("import_profile"),Profile.IsEmpty()?TEXT("default"):Profile);
   TArray<TSharedPtr<FJsonValue>> Values;
   for(double V:{Bounds.Min.X,Bounds.Min.Y,Bounds.Min.Z,Bounds.Max.X,Bounds.Max.Y,Bounds.Max.Z}) Values.Add(MakeShared<FJsonValueNumber>(V));
   Record->SetArrayField(TEXT("reference_bounds"),Values);
   FString Text; FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Text));
   const auto Directory=FPaths::ProjectSavedDir()/TEXT("Reports/CharacterCalibration"); IFileManager::Get().MakeDirectory(*Directory,true);
   FFileHelper::SaveStringToFile(Text,*(Directory/Id+TEXT(".json")));
  }
 }
#endif
 if(!Bounds.IsValid||Bounds.GetSize().Z<.001f) return false;
 // 180 local cm, then the canonical pawn scale applies exactly once.
 const float Scale=180.f/Bounds.GetSize().Z;
 const FVector Offset(-Bounds.GetCenter().X*Scale,-Bounds.GetCenter().Y*Scale,-Bounds.Min.Z*Scale);
 for(UMeshComponent* Part:Parts_) { Part->SetRelativeScale3D(FVector(Scale)); Part->SetRelativeLocation(Offset); }
 if(!Preview) PlayRole(TEXT("idle"));
 return true;
}
void ACultCharacterModel::PlayRole(const FString& ClipRole) {
 // Missing clips remain visibly unanimated; never substitute unrelated attacks.
 if(ClipRole==CurrentRole_) return; CurrentRole_=ClipRole;
 for(UMeshComponent* Part:Parts_) if(auto* S=Cast<USkeletalMeshComponent>(Part)) {
  auto* Rig=S->GetSkeletalMeshAsset()->GetSkeleton();
  UAnimSequence* Desired=Animations_.FindRef(Rig->GetPathName()+TEXT("|")+ClipRole);
  if(!Desired) Desired=Animations_.FindRef(ClipRole);
  if(Desired&&Desired->GetSkeleton()!=Rig) Desired=nullptr;
  if(Desired) S->PlayAnimation(Desired,ClipRole!=TEXT("attack")); else S->Stop();
 }
}
ACultCharacterModel* ACultCharacterModel::AttachToPawn(ACultUlhuPlayerPawn* Pawn,const FString& Id) {
 if(!Pawn||!Binding(Id)) return nullptr;
 auto* Model=Pawn->GetWorld()->SpawnActor<ACultCharacterModel>();
 if(!Model||!Model->Initialize(Id)) { if(Model) Model->Destroy(); return nullptr; }
 Model->Pawn_=Pawn; Model->SetOwner(Pawn);
 Model->AttachToComponent(Pawn->GetCapsuleComponent(),FAttachmentTransformRules::KeepRelativeTransform);
 Model->SetActorRelativeLocation(FVector(0,0,-Pawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
 TArray<UStaticMeshComponent*> Old; Pawn->GetComponents(Old);
 for(auto* Part:Old) if(Part->ComponentHasTag(TEXT("CultCorpseBody"))||Part->ComponentHasTag(TEXT("VesselEye"))) {
  Part->SetVisibility(false); Part->ComponentTags.Add(TEXT("CultReplacedVessel"));
 }
 return Model;
}
void ACultCharacterModel::Tick(float DeltaSeconds) {
 Super::Tick(DeltaSeconds);
 if(bFloating_) { Elapsed_+=DeltaSeconds; const float Next=3.f+FMath::Sin(Elapsed_*1.4f)*2.5f;
  for(UMeshComponent* Part:Parts_) Part->SetRelativeLocation(Part->GetRelativeLocation()+FVector(0,0,Next-Hover_)); Hover_=Next;
 }
 if(Pawn_&&!IsValid(Pawn_)) { Destroy(); return; }
 if(Pawn_) {
  SetActorHiddenInGame(Pawn_->IsHidden()||Pawn_->MatchDead()||(!Pawn_->IsBot()&&!Pawn_->IsThirdPerson()));
  const bool Moving=Pawn_->GetVelocity().SizeSquared2D()>100;
  PlayRole(Pawn_->IsSwinging()?TEXT("attack"):Moving?TEXT("walk"):TEXT("idle"));
 } else if(!bPreview_&&GetOwner()) Destroy();
}
