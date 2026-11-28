#include "CultFirstPersonView.h"
#include "CultCharacterModel.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPrimaryProfile.h"
#include "CultAbilityFilaments.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace {
FString ViewPath(const FString& Id) {return FPaths::ProjectContentDir()/TEXT("CharacterModels/FirstPerson")/(Id+TEXT(".viewmesh"));}
}
UCultFirstPersonView::UCultFirstPersonView() {
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}
bool UCultFirstPersonView::HasVisibleAnatomy() const {
 for(const UProceduralMeshComponent* Mesh:Meshes) if(IsValid(Mesh)&&Mesh->IsVisible()) return true;
 return false;
}
void UCultFirstPersonView::TransferViewData(FArchive& Ar,TArray<FSection>& Data) {
 int32 Magic=0x43565032,Count=Data.Num();Ar<<Magic<<Count;
 if(Magic!=0x43565032||Count<0||Count>48) {Ar.SetError();return;}
 if(Ar.IsLoading()) Data.SetNum(Count);
 int32 Budget=0;
 for(auto& S:Data) {
  Ar<<S.Part<<S.Material<<S.SourceAsset<<S.Anchor<<S.Side;
  int32 Vertices=S.Source.Num(),Indices=S.Triangles.Num();Ar<<Vertices<<Indices;Budget+=Vertices;
  if(Vertices<0||Budget>24000||Indices<0||Indices>144000||S.Part<0||S.Part>128) {Ar.SetError();return;}
  if(Ar.IsLoading()) {S.Source.SetNum(Vertices);S.Triangles.SetNum(Indices);}
  for(auto& V:S.Source) {Ar<<V.Position<<V.Normal<<V.UV;for(int I=0;I<8;++I) Ar<<V.Bones[I]<<V.Weights[I];}
  for(auto& Index:S.Triangles) {Ar<<Index;if(Index<0||Index>=Vertices) Ar.SetError();}
 }
}
#if WITH_EDITOR
bool UCultFirstPersonView::Bake(const FString& Id) {
 UWorld* World=GWorld; if(!World) return false;
 auto* Art=World->SpawnActor<ACultCharacterModel>();
 if(!Art||!Art->Initialize(Id,true)) {if(Art) Art->Destroy();return false;}
 TArray<FSection> Output;int32 Budget=0;
 for(int32 Part=0;Part<Art->Parts().Num();++Part) if(auto* Skin=Cast<USkeletalMeshComponent>(Art->Parts()[Part])) {
  const auto* Asset=Skin->GetSkeletalMeshAsset();const auto* Render=Asset->GetResourceForRendering();if(!Render||Render->LODRenderData.IsEmpty()) continue;
  const int LODIndex=Render->LODRenderData.Num()>1?1:0;
  const auto& LOD=Render->LODRenderData[LODIndex];const auto* Weights=Skin->GetSkinWeightBuffer(LODIndex);if(!Weights) continue;
  const auto& VB=LOD.StaticVertexBuffers;const auto* IB=LOD.MultiSizeIndexContainer.GetIndexBuffer();if(!IB) continue;
  const auto& Ref=Asset->GetRefSkeleton();
  TArray<int32> Side;Side.SetNumZeroed(Ref.GetNum());
  for(int32 Bone=0;Bone<Ref.GetNum();++Bone) {
   FString Name=Ref.GetBoneName(Bone).ToString().ToLower();
   const bool Limb=Name.Contains(TEXT("forearm"))||Name.Contains(TEXT("hand"))||Name.Contains(TEXT("claw"))||Name.Contains(TEXT("pincher"))||Name.Contains(TEXT("scimitar"));
   if(Limb) Side[Bone]=(Name.Contains(TEXT("left"))||Name.EndsWith(TEXT("_l"))||Name.Contains(TEXT(".l"))||Name.Contains(TEXT("_l_")))?-1:1;
   else {int32 Parent=Ref.GetParentIndex(Bone);if(Parent>=0) Side[Bone]=Side[Parent];}
   if(Name.Contains(TEXT("scimitar"))) Side[Bone]=(Name.EndsWith(TEXT("005"))||Name.EndsWith(TEXT("008")))?-1:1;
  }
  for(const auto& SourceSection:LOD.RenderSections) for(int32 Hand:{-1,1}) {
   FSection Out;Out.Part=Part;Out.Material=SourceSection.MaterialIndex;Out.Side=Hand;Out.SourceAsset=Asset->GetPathName();
   TMap<int32,int32> Remap;TSet<int32> Selected;
   for(uint32 Local=0;Local<SourceSection.NumVertices;++Local) {
    const int32 V=SourceSection.BaseVertexIndex+Local;float Influence=0;
    for(uint32 W=0;W<Weights->GetMaxBoneInfluences();++W) {
     int B=SourceSection.BoneMap[Weights->GetBoneIndex(V,W)];if(Side.IsValidIndex(B)&&Side[B]==Hand) Influence+=Weights->GetBoneWeight(V,W)/65535.f;
    }
    if(Influence>.64f) Selected.Add(V);
   }
   for(uint32 T=0;T<SourceSection.NumTriangles;++T) {
    int32 IDs[3];bool All=true;
    for(int K=0;K<3;++K) {IDs[K]=IB->Get(SourceSection.BaseIndex+T*3+K);All&=Selected.Contains(IDs[K]);}
    if(!All||Budget>23000) continue;
    for(int K=0;K<3;++K) {
     const int32 ID=IDs[K];auto* Existing=Remap.Find(ID);
     if(!Existing) {
      const int32 Index=Out.Source.Num();FVertex V;V.Position=VB.PositionVertexBuffer.VertexPosition(ID);V.Normal=FVector3f(VB.StaticMeshVertexBuffer.VertexTangentZ(ID));V.UV=VB.StaticMeshVertexBuffer.GetVertexUV(ID,0);
      float Total=0;
      for(uint32 W=0;W<FMath::Min(8u,Weights->GetMaxBoneInfluences());++W) {V.Bones[W]=SourceSection.BoneMap[Weights->GetBoneIndex(ID,W)];V.Weights[W]=Weights->GetBoneWeight(ID,W)/65535.f;Total+=V.Weights[W];}
      for(float& Weight:V.Weights) Weight/=FMath::Max(.0001f,Total);
      Out.Source.Add(V);Remap.Add(ID,Index);++Budget;
     }
     Out.Triangles.Add(Remap[ID]);
    }
   }
   if(Out.Source.Num()<12) continue;
   FBox Bounds(ForceInit);for(const auto& V:Out.Source) Bounds+=Skin->GetRelativeTransform().TransformPosition(FVector(V.Position));
   Out.Anchor=Bounds.GetCenter();Output.Add(MoveTemp(Out));
  }
 }
 Art->Destroy();
 if(Output.IsEmpty()) return false;
 FBufferArchive Bytes;TransferViewData(Bytes,Output);IFileManager::Get().MakeDirectory(*FPaths::GetPath(ViewPath(Id)),true);
 return !Bytes.IsError()&&FFileHelper::SaveArrayToFile(Bytes,*ViewPath(Id));
}
#endif
void UCultFirstPersonView::LoadView(const FString& Id) {
 Loaded=true;TArray<uint8> Bytes;
 if(FPaths::FileExists(ViewPath(Id))&&FFileHelper::LoadFileToArray(Bytes,*ViewPath(Id))&&Bytes.Num()<=12*1024*1024) {
  FMemoryReader Reader(Bytes);TransferViewData(Reader,Sections);if(Reader.IsError()) {Sections.Reset();return;}
 }
 // Original, inexpensive contracting spit organs for non-humanoid casters.
 // These are view-only anatomy, never an extra world attack or collision.
 if(Sections.IsEmpty()&&(Id==TEXT("bokrug")||Id==TEXT("mother_hydra")||Id==TEXT("shub_niggurath")||Id==TEXT("nug_and_yeb"))) {
  const int Count=Id==TEXT("nug_and_yeb")?2:1;
  for(int Organ=0;Organ<Count;++Organ) {
   FSection S;S.Part=-1;S.Side=Organ?-1:1;
   for(int Ring=0;Ring<=10;++Ring) for(int Side=0;Side<16;++Side) {
    const float T=Ring/10.f,A=Side*2*PI/16;const float Radius=FMath::Lerp(10.f,5.f,T)*(1+.11f*FMath::Sin(T*6*PI));
    FVertex V;V.Position=FVector3f(T*26,Radius*FMath::Cos(A),Radius*FMath::Sin(A));V.Normal=FVector3f(-.1f,FMath::Cos(A),FMath::Sin(A)).GetSafeNormal();V.UV=FVector2f(Side/16.f,T);S.Source.Add(V);
    if(Ring<10) {const int A0=Ring*16+Side,B=Ring*16+(Side+1)%16;S.Triangles.Append({A0,B,A0+16,B,B+16,A0+16});}
   }
   Sections.Add(MoveTemp(S));
  }
 }
 auto* P=Cast<ACultUlhuPlayerPawn>(GetOwner());auto* Camera=P?P->FindComponentByClass<UCameraComponent>():nullptr;if(!Camera||!Model.IsValid()) return;
 Camera->SetEnableFirstPersonFieldOfView(true);Camera->SetFirstPersonFieldOfView(85);Camera->SetEnableFirstPersonScale(true);Camera->SetFirstPersonScale(.10f);
 for(auto& Section:Sections) {
  if(Section.Part>=0) {
   auto* Skin=Model->Parts().IsValidIndex(Section.Part)?Cast<USkeletalMeshComponent>(Model->Parts()[Section.Part]):nullptr;
   if(!Skin||!Skin->GetSkeletalMeshAsset()||Skin->GetSkeletalMeshAsset()->GetPathName()!=Section.SourceAsset) {Sections.Reset();return;}
  }
  auto* Mesh=NewObject<UProceduralMeshComponent>(GetOwner());Mesh->SetupAttachment(Camera);Mesh->SetAbsolute(false,false,true);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetGenerateOverlapEvents(false);Mesh->SetCanEverAffectNavigation(false);Mesh->SetCastShadow(false);Mesh->SetOnlyOwnerSee(true);
  Mesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);Mesh->ComponentTags.Add(TEXT("CultFirstPersonAnatomy"));
  auto* Surface=Section.Part<0?LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_VisceralOrgan")):Model->Parts()[Section.Part]->GetMaterial(Section.Material);
  if(!Surface&&Model->Parts().Num()) Surface=Model->Parts()[0]->GetMaterial(0);
  Mesh->SetMaterial(0,Surface);Mesh->SetMaterial(1,CultAbilityFX::FilamentMaterial());Mesh->RegisterComponent();Mesh->SetVisibility(false);Meshes.Add(Mesh);
  Section.Points.SetNum(Section.Source.Num());Section.Normals.SetNum(Section.Source.Num());for(const auto& V:Section.Source) Section.UVs.Add(FVector2D(V.UV));
 }
}
void UCultFirstPersonView::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function) {
 Super::TickComponent(Dt,Type,Function);auto* P=Cast<ACultUlhuPlayerPawn>(GetOwner());if(!P||!P->IsLocallyControlled()||GetNetMode()==NM_DedicatedServer) return;
 if(!Model.IsValid()) {TArray<AActor*> Children;P->GetAttachedActors(Children);for(auto* Child:Children) if(auto* Art=Cast<ACultCharacterModel>(Child)) {Model=Art;break;}}
 if(!Model.IsValid()) return;
 const bool Show=!P->IsThirdPerson()&&!P->IsIncapacitated()&&!P->IsHidden()&&!P->MatchDead();
 if(auto* Camera=P->FindComponentByClass<UCameraComponent>()) {
  Camera->PostProcessSettings.bOverride_MotionBlurAmount=Show;
  Camera->PostProcessSettings.MotionBlurAmount=0;
 }
 if(Show&&!Loaded) LoadView(P->GetCharacterId());
 for(UProceduralMeshComponent* Mesh:Meshes) if(IsValid(Mesh)) Mesh->SetVisibility(Show);
 if(!Show||Meshes.Num()!=Sections.Num()||Sections.IsEmpty()) return;
 Clock+=Dt;
 const FRotator Look=P->AimRotation(),Delta=(Look-PreviousLook).GetNormalized();PreviousLook=Look;
 Sway=FMath::VInterpTo(Sway,FVector(0,FMath::Clamp(-Delta.Yaw*.09f,-1.5f,1.5f),FMath::Clamp(Delta.Pitch*.06f,-1.f,1.f)),Dt,9.f);
 const auto& Motion=P->AttackPlayback();const float Age=GetWorld()->GetTimeSeconds()-Motion.Began;
 const bool Active=Age>=0&&Age<Motion.Duration;
 const bool Melee=Active&&Motion.Move==CultAttackMotion::EMove::Melee;
 const float Swing=Melee?FMath::SmoothStep(Motion.Contact-.16f,Motion.Contact+.15f,Age):0;
 const float Rebound=Active?1-FMath::SmoothStep(Motion.Duration*.64f,Motion.Duration,Age):0;
 const float Charge=Active?FMath::SmoothStep(0.f,FMath::Max(.05f,Motion.Contact),Age)*Rebound:0;
 TMap<int32,TArray<FMatrix44f>> Matrices;
 TSet<int32> TrailedHands;
 for(int32 Index=0;Index<Sections.Num();++Index) {
  auto& S=Sections[Index];auto* Skin=S.Part>=0?Cast<USkeletalMeshComponent>(Model->Parts()[S.Part]):nullptr;if(S.Part>=0&&!Skin) continue;
  TArray<FMatrix44f>* Cached=nullptr;
  if(Skin) {
   Skin->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
   Cached=Matrices.Find(S.Part);if(!Cached) {TArray<FMatrix44f> M;Skin->GetCurrentRefToLocalMatrices(M,0);Matrices.Add(S.Part,MoveTemp(M));Cached=Matrices.Find(S.Part);}
  }
  const bool Lead=(Motion.Variant&3)==3||S.Side==((Motion.Variant&3)==2?1:-1);
  const float Slash=Melee&&Lead?FMath::Sin(Swing*PI)*Rebound:0;
  const float Gait=FMath::Clamp(P->GetVelocity().Size2D()/FMath::Max(1.f,P->GetCharacterMovement()->MaxWalkSpeed),0.f,1.f);
  const FVector Base(50+Slash*11-Charge*2,S.Side*(26-Slash*36),-17+Slash*17+FMath::Sin(Clock*2.1f)*.4f);
  const FVector Bob(0,FMath::Sin(Clock*8.f)*Gait*.55f,FMath::Cos(Clock*16.f)*Gait*.65f);
  const FQuat Rotate(FVector::ForwardVector,S.Side*Slash*.28f);
  FBox PosedBounds(ForceInit);
  for(int32 V=0;V<S.Source.Num();++V) {
   const auto& In=S.Source[V];FVector3f Point=FVector3f::ZeroVector,Normal=FVector3f::ZeroVector;
   if(Skin) {
    for(int W=0;W<8;++W) if(In.Weights[W]>0&&Cached->IsValidIndex(In.Bones[W])) {const auto& M=(*Cached)[In.Bones[W]];Point+=M.TransformPosition(In.Position)*In.Weights[W];Normal+=M.TransformVector(In.Normal)*In.Weights[W];}
    const FVector Local=Skin->GetRelativeTransform().TransformPosition(FVector(Point));
    S.Points[V]=Local;PosedBounds+=Local;
    S.Normals[V]=Skin->GetRelativeRotation().RotateVector(FVector(Normal)).GetSafeNormal();
   } else {
    FVector Posed(In.Position);const float Pulse=FMath::Sin(Clock*2.3f+S.Side)*.035f+Charge*.24f*FMath::Sin(Posed.X*.2f-Age*15);
    Posed.Y*=1+Pulse;Posed.Z*=1+Pulse;Posed.X*=1-Charge*.16f;
    const float T=FMath::Clamp(In.Position.X/26.f,0.f,1.f);
    const float Swell=1+.23f*FMath::Sin(T*PI);
    S.Points[V]=FVector(12+48*T-Charge*4,S.Side*(38-16*T),-33+23*T+Charge*3)
     +FVector(0,Posed.Y*Swell,Posed.Z*Swell)+Bob+Sway;S.Normals[V]=FVector(In.Normal);
   }
  }
  if(Skin) {
   // Frame the actual idle pose, not the source's outstretched bind pose.
   if(!S.AnchorReady&&PosedBounds.IsValid) {
    if(const auto* Anchor=InitialAnchors.Find(S.Side)) S.Anchor=*Anchor;
    else {
     S.Anchor=PosedBounds.GetCenter();InitialAnchors.Add(S.Side,S.Anchor);
     FVector Elbow,Hand,Axis(0,0,-1);
     if(Model->StrikeLimb(S.Side<0?0:1,Elbow,Hand)) Axis=Model->GetActorTransform().InverseTransformVectorNoScale(Hand-Elbow).GetSafeNormal();
     InitialAnchors.Add(S.Side*10,Axis);
    }
    S.AnchorReady=true;
   }
   const FVector Axis=InitialAnchors.FindRef(S.Side*10);
   const FQuat Ready=FQuat::FindBetweenNormals(Axis.IsNearlyZero()?FVector(0,0,-1):Axis,FVector(1,0,.2f).GetSafeNormal());
   const FQuat Gesture=Rotate*Ready;
   for(FVector& Point:S.Points) Point=Base+Bob+Sway+Gesture.RotateVector((Point-S.Anchor)*.48f);
   for(FVector& Normal:S.Normals) Normal=Gesture.RotateVector(Normal);
  }
  if(!Meshes[Index]->GetProcMeshSection(0)) Meshes[Index]->CreateMeshSection_LinearColor(0,S.Points,S.Triangles,S.Normals,S.UVs,{}, {},false,false);
  else Meshes[Index]->UpdateMeshSection_LinearColor(0,S.Points,S.Normals,S.UVs,{}, {},false);
  const bool Blade=P->GetCharacterId()==TEXT("yig");
  if(Melee&&Lead&&Swing>.02f&&Swing<.98f&&!TrailedHands.Contains(S.Side)&&(!Blade||S.SourceAsset.Contains(TEXT("Sword")))) {
   TrailedHands.Add(S.Side);
   CultAbilityFX::FFilaments Trail;
   FVector Tip=Base;
   for(const FVector& Point:S.Points) if(Point.X>Tip.X) Tip=Point;
   auto PathAt=[&](float T) {
    const float Sweep=FMath::SmoothStep(Motion.Contact-.16f,Motion.Contact+.15f,T);
    const float Recovery=1-FMath::SmoothStep(Motion.Duration*.64f,Motion.Duration,T);
    const float Gain=FMath::Sin(Sweep*PI)*Recovery;
    const FVector Arc(50+Gain*11,S.Side*(26-Gain*36),-17+Gain*17);
    return Tip+(Arc-Base)*1.13f;
   };
   for(int Strand=Blade?0:-1;Strand<=(Blade?0:1);++Strand) for(int Segment=1;Segment<=10;++Segment) {
    const float Weight=Segment/10.f;
    const FVector Offset(0,0,Strand*1.6f);
    const FVector A=PathAt(Age-.11f+.11f*(Segment-1)/10.f)+Offset;
    const FVector B=PathAt(Age-.11f+.11f*Weight)+Offset;
    Trail.Ribbon(A,B,.25f+.35f*Weight,.3f+.45f*Weight,FLinearColor(.12f,.36f,.42f,.06f*Weight),FLinearColor(.48f,.76f,.81f,.45f*Weight));
   }
   const auto* Existing=Meshes[Index]->GetProcMeshSection(1);
   if(Existing&&Existing->ProcVertexBuffer.Num()==Trail.Vertices.Num()) Meshes[Index]->UpdateMeshSection_LinearColor(1,Trail.Vertices,{},Trail.UVs,Trail.Colors,{},false);
   else Meshes[Index]->CreateMeshSection_LinearColor(1,Trail.Vertices,Trail.Indices,{},Trail.UVs,Trail.Colors,{},false,false);
  } else if(Meshes[Index]->GetProcMeshSection(1)) Meshes[Index]->ClearMeshSection(1);
 }
}
