#include "CultFluidAnimInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/AnimSequence.h"

namespace {
struct FCultFluidProxy final : FAnimSingleNodeInstanceProxy {
 explicit FCultFluidProxy(UAnimInstance* Instance):FAnimSingleNodeInstanceProxy(Instance) {}
 TArray<FTransform> LastPose,FromPose;
 float BlendAge=1.f,BlendLength=.14f,LastTime=0;
 TWeakObjectPtr<UAnimationAsset> LastAsset;
 CultAttackMotion::FFrame Attack;
 float Lean=0;
 uint8 GestureRig=0;
 uint8 MeleeStage=0;
 float MeleeProgress=0, MeleePitch=0;
 FTransform GripTargets[4];
 uint8 GripMask=0;
 float TailMotion=0, TailPhase=0;
 FVector Right=FVector::RightVector,Up=FVector::UpVector,Forward=FVector::ForwardVector;

 void PreUpdate(UAnimInstance* Instance,float Delta) override {
  FAnimSingleNodeInstanceProxy::PreUpdate(Instance,Delta);
  const auto* Motion=CastChecked<UCultFluidAnimInstance>(Instance);
  Attack=Motion->Attack;
  Lean=Motion->LateralLean;
  GestureRig=Motion->GestureRig;
  MeleeStage=Motion->MeleeStage;
  MeleeProgress=Motion->MeleeProgress;
  MeleePitch=Motion->MeleePitch;
  GripMask=Motion->GripMask;
  for(int Index=0;Index<4;++Index) if(GripMask&(1<<Index)) GripTargets[Index]=Motion->GripTargets[Index];
  TailMotion=Motion->TailMotion;
  TailPhase=Motion->TailPhase;
  const auto* Mesh=Instance->GetSkelMeshComponent();
  const FQuat Relative=IsValid(Mesh)?Mesh->GetRelativeRotation().Quaternion():FQuat::Identity;
  Right=Relative.UnrotateVector(FVector::RightVector);
  Up=Relative.UnrotateVector(FVector::UpVector);
  Forward=Relative.UnrotateVector(FVector::ForwardVector);
 }

 void UpdateYigAttachments(FCompactPose& Pose) {
  if(!GripMask&&TailMotion<=0) return;
  const auto& Bones=Pose.GetBoneContainer();
  auto Find=[&](FName Name) {
   const int Index=Bones.GetReferenceSkeleton().FindBoneIndex(Name);
   return Index==INDEX_NONE?FCompactPoseBoneIndex(INDEX_NONE):Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
  };
  if(GripMask) {
   static const FName Swords[]={TEXT("sword_scimitar_lp__005"),TEXT("sword_scimitar_lp__006"),TEXT("sword_scimitar_lp__008"),TEXT("sword_scimitar_lp__009")};
   FCSPose<FCompactPose> Component;
   Component.InitPose(Pose);
   for(int Index=0;Index<4;++Index) if(GripMask&(1<<Index)) {
    const auto Bone=Find(Swords[Index]);
    if(Bone.GetInt()==INDEX_NONE) continue;
    const auto Parent=Pose.GetParentBoneIndex(Bone);
    if(Parent.GetInt()!=INDEX_NONE) {
     const FTransform Base=Component.GetComponentSpaceTransform(Parent);
     const FTransform Delta=GripTargets[Index];
     FTransform Bind=Pose.GetRefPose(Bone);
     for(auto Ancestor=Parent;Ancestor.GetInt()!=INDEX_NONE;Ancestor=Pose.GetParentBoneIndex(Ancestor))
      Bind=Bind*Pose.GetRefPose(Ancestor);
     const FTransform Posed((Delta.GetRotation()*Bind.GetRotation()).GetNormalized(),
      Delta.TransformPosition(Bind.GetLocation()),Bind.GetScale3D());
     Pose[Bone]=Posed.GetRelativeTransform(Base);
    }
   }
  }
  if(TailMotion>0) for(int Index=2;Index<=16;++Index) {
   const auto Bone=Find(FName(*FString::Printf(TEXT("tail%02d"),Index)));
   if(Bone.GetInt()==INDEX_NONE) continue;
   const auto Parent=Pose.GetParentBoneIndex(Bone);
   if(Parent.GetInt()==INDEX_NONE) continue;
   FCSPose<FCompactPose> Component;
   Component.InitPose(Pose);
   const auto Axis=Component.GetComponentSpaceTransform(Parent).GetRotation().UnrotateVector(Up);
   const float Degrees=FMath::Sin(TailPhase-Index*.49f)*TailMotion*FMath::Lerp(3.f,10.f,Index/16.f);
   Pose[Bone].SetRotation((FQuat(Axis,FMath::DegreesToRadians(Degrees))*Pose[Bone].GetRotation()).GetNormalized());
  }
 }

 void AimStrike(FCompactPose& Pose) {
  if(!MeleeStage||(GestureRig!=1&&GestureRig!=4&&GestureRig!=5&&GestureRig!=6)) return;
  const float Weight=FMath::SmoothStep(-1.55f,-.72f,MeleeProgress)*(1-FMath::SmoothStep(.65f,1.65f,MeleeProgress));
  if(Weight<=0) return;
  const auto& Bones=Pose.GetBoneContainer();
  for(int Side=0;Side<2;++Side) {
   if(MeleeStage!=3&&Side!=MeleeStage-1) continue;
   const auto Index=[&](FName Name) { return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(Name))); };
   const auto Arm=Index(Side?TEXT("upperarm_r"):TEXT("upperarm_l"));
   const auto Hand=Index(Side?TEXT("hand_r"):TEXT("hand_l"));
   if(Arm.GetInt()==INDEX_NONE||Hand.GetInt()==INDEX_NONE) continue;
   FCSPose<FCompactPose> ComponentPose;
   ComponentPose.InitPose(Pose);
   const auto ArmTransform=ComponentPose.GetComponentSpaceTransform(Arm);
   const auto HandTransform=ComponentPose.GetComponentSpaceTransform(Hand);
   const auto Parent=ComponentPose.GetComponentSpaceTransform(Pose.GetParentBoneIndex(Arm));
   const float SideSign=Side?1.f:-1.f;
   const float Swing=FMath::SmoothStep(-1.f,1.f,MeleeProgress);
   const float Yaw=FMath::DegreesToRadians(SideSign*FMath::Lerp(58.f,-32.f,Swing));
   const float Pitch=FMath::DegreesToRadians(MeleePitch);
   const FVector Desired=(Forward*FMath::Cos(Yaw)+Right*FMath::Sin(Yaw))*FMath::Cos(Pitch)+Up*FMath::Sin(Pitch);
   const FVector Current=(HandTransform.GetLocation()-ArmTransform.GetLocation()).GetSafeNormal();
   if(Current.IsNearlyZero()) continue;
   const FQuat Turn=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(Current,Desired.GetSafeNormal()),Weight);
   Pose[Arm].SetRotation((Parent.GetRotation().Inverse()*Turn*ArmTransform.GetRotation()).GetNormalized());
  }
 }

 void ApplyGesture(FCompactPose& Pose) {
  constexpr double MinimumRotation=.02;
  const FVector4 Zero(0,0,0,0);
  if(!GestureRig||(Attack.Body==Zero&&Attack.Limbs==Zero&&Attack.Details==Zero&&FMath::Abs(Lean)<.01f)) return;
  const auto& Bones=Pose.GetBoneContainer();
  FCSPose<FCompactPose> ComponentPose;
  ComponentPose.InitPose(Pose);
  auto Rotate=[&](FName Name,FVector Axis,double Degrees) {
   if(FMath::Abs(Degrees)<MinimumRotation) return;
   const int32 MeshIndex=Bones.GetReferenceSkeleton().FindBoneIndex(Name);
   if(MeshIndex==INDEX_NONE) return;
   const auto Bone=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
   if(Bone.GetInt()==INDEX_NONE) return;
   const auto Parent=Pose.GetParentBoneIndex(Bone);
   if(Parent.GetInt()!=INDEX_NONE) {
    Axis=ComponentPose.GetComponentSpaceTransform(Parent).GetRotation().UnrotateVector(Axis);
   }
   Pose[Bone].SetRotation((FQuat(Axis,FMath::DegreesToRadians(Degrees))*Pose[Bone].GetRotation()).GetNormalized());
  };

  const auto& B=Attack.Body;
  const auto& L=Attack.Limbs;
  const auto& D=Attack.Details;
  auto Arms=[&](FName ShoulderL,FName ShoulderR,FName ForearmL,FName ForearmR,double Gain) {
   const double Left=1-.23*Attack.Lead;
   const double RightSide=1+.23*Attack.Lead;
   Rotate(ShoulderL,Right,L.X*Gain*Left);
   Rotate(ShoulderR,Right,L.X*Gain*RightSide);
   Rotate(ShoulderL,Up,-L.Y*Gain*Left);
   Rotate(ShoulderR,Up,L.Y*Gain*RightSide);
   Rotate(ForearmL,Right,L.Z*Gain*Left);
   Rotate(ForearmR,Right,L.Z*Gain*RightSide);
  };
  auto Torso=[&](FName Bone,double Gain) {
   Rotate(Bone,Right,B.X*Gain);
   Rotate(Bone,Up,B.Y*Gain);
   Rotate(Bone,Forward,Lean*6+B.W*.24);
  };
  if(GestureRig==1) {
   // Shoulders take the load before the forearms extend. Wing spars and
   // facial tendrils carry the momentum beyond the release.
   Torso(TEXT("chest"),.75);
   Arms(TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("forearm_l"),TEXT("forearm_r"),.91);
   Rotate(TEXT("neck"),Right,D.X*.67);
   Rotate(TEXT("head"),Up,D.Z*.22);
   Rotate(TEXT("hand_l"),Right,L.Z*.26-D.X*.14);
   Rotate(TEXT("hand_r"),Right,L.Z*.26-D.X*.14);
   Rotate(TEXT("wingbase_l"),Up,-D.Y*.83);
   Rotate(TEXT("wingbase_r"),Up,D.Y*.83);
   Rotate(TEXT("wingspar_l"),Right,D.X*.26+D.Y*.25);
   Rotate(TEXT("wingspar_r"),Right,D.X*.26+D.Y*.25);
   Rotate(TEXT("tendril_inner1_l"),Right,L.W*.36+D.X*.3);
   Rotate(TEXT("tendril_inner1_r"),Right,L.W*.36+D.X*.3);
   Rotate(TEXT("tendril_outer1_l"),Up,-D.Y*.42);
   Rotate(TEXT("tendril_outer1_r"),Up,D.Y*.42);
  } else if(GestureRig==2) {
   Arms(TEXT("bicep.l"),TEXT("bicep.r"),TEXT("forearm.l"),TEXT("forearm.r"),.69);
   Rotate(TEXT("claw.l"),Right,L.W*.37-D.X*.14);
   Rotate(TEXT("claw.r"),Right,L.W*.37-D.X*.14);
   Rotate(TEXT("minipincer.l"),Up,-L.Y*.45);
   Rotate(TEXT("minipincer.r"),Up,L.Y*.45);
   Rotate(TEXT("wingbase1.l"),Up,-D.Y*.72);
   Rotate(TEXT("wingbase1.r"),Up,D.Y*.72);
   Rotate(TEXT("wingtip1.l"),Right,D.X*.42);
   Rotate(TEXT("wingtip1.r"),Right,D.X*.42);
   Rotate(TEXT("wingbase2.l"),Up,-D.Y*.47);
   Rotate(TEXT("wingbase2.r"),Up,D.Y*.47);
  } else if(GestureRig==3) {
   Torso(TEXT("core_mid"),.73);
   Rotate(TEXT("core_top"),Up,B.Y*.47+D.Z*.42);
   Rotate(TEXT("core_top"),Right,D.X*.65);
   static const FName Orbs[]={TEXT("orb_00"),TEXT("orb_01"),TEXT("orb_02"),TEXT("orb_03"),TEXT("orb_04"),TEXT("orb_05"),TEXT("orb_06"),TEXT("orb_07")};
   static const FName Columns[]={TEXT("tendril_00_01"),TEXT("tendril_01_01"),TEXT("tendril_02_01"),TEXT("tendril_03_01"),TEXT("tendril_04_01"),TEXT("tendril_05_01"),TEXT("tendril_06_01"),TEXT("tendril_07_01")};
   for(int32 Index=0;Index<UE_ARRAY_COUNT(Orbs);++Index) {
    const double Phase=Index*PI*.25;
    Rotate(Orbs[Index],Right,FMath::Sin(Phase)*D.Y*.74);
    Rotate(Columns[Index],Forward,FMath::Cos(Phase)*D.Y*.53+D.Z*.18);
   }
  } else if(GestureRig==4) {
   Torso(TEXT("chest"),.66);
   Arms(TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("forearm_l"),TEXT("forearm_r"),.76);
   Rotate(TEXT("neck"),Forward,-3*Lean);
   Rotate(TEXT("head"),Right,D.X*.8);
   Rotate(TEXT("jaw"),Right,L.W*.85);
   Rotate(TEXT("crest"),Up,D.Z*.27);
   Rotate(TEXT("tail_02"),Up,-B.Y*.36);
  } else if(GestureRig==5) {
   Torso(TEXT("chest"),.58);
   Arms(TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("forearm_l"),TEXT("forearm_r"),.67);
   Rotate(TEXT("head_00"),Right,D.X*.73);
   Rotate(TEXT("head_02"),Up,6*Lean+D.Z*.56);
   Rotate(TEXT("head_04"),Up,D.Z*.42);
  } else if(GestureRig==6) {
   Torso(TEXT("chest"),.56);
   Arms(TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("arm_mid_l"),TEXT("arm_mid_r"),.63);
   Rotate(TEXT("neck"),Right,D.X*.51);
   Rotate(TEXT("head"),Right,L.W*.37);
   Rotate(TEXT("horn0_l"),Up,-D.Y*.37);
   Rotate(TEXT("horn0_r"),Up,D.Y*.37);
   Rotate(TEXT("forearm_roll_l"),Right,L.Z*.18);
   Rotate(TEXT("forearm_roll_r"),Right,L.Z*.18);
  }
 }
 virtual void SetAnimationAsset(UAnimationAsset* Asset,USkeletalMeshComponent* Mesh,bool Loop,float Rate) override {
  if(Asset!=LastAsset.Get()) { FromPose=LastPose; BlendAge=0; BlendLength=.14f; LastTime=0; LastAsset=Asset; }
  FAnimSingleNodeInstanceProxy::SetAnimationAsset(Asset,Mesh,Loop,Rate);
 }
 virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override {
  BlendAge+=Context.GetDeltaTime(); FAnimSingleNodeInstanceProxy::UpdateAnimationNode(Context);
 }
 virtual bool Evaluate(FPoseContext& Output) override {
  const bool Result=FAnimSingleNodeInstanceProxy::Evaluate(Output);
  const float Time=GetCurrentTime();
  if(auto* Clip=Cast<UAnimSequence>(LastAsset.Get())) if(IsLooping()&&LastPose.Num()>0&&Clip->GetPlayLength()>0&&FMath::Abs(Time-LastTime)>Clip->GetPlayLength()*.5f) {
   FromPose=LastPose; BlendAge=0; BlendLength=.1f;
  }
  LastTime=Time;
  const int32 Count=Output.Pose.GetNumBones();
  if(FromPose.Num()==Count&&BlendAge<BlendLength) {
   const float T=FMath::Clamp(BlendAge/BlendLength,0.f,1.f),Alpha=T*T*T*(T*(T*6-15)+10);
   for(int32 I=0;I<Count;++I) { const FCompactPoseBoneIndex Bone(I); FTransform Blended; Blended.Blend(FromPose[I],Output.Pose[Bone],Alpha); Output.Pose[Bone]=Blended; }
  }
  LastPose.SetNumUninitialized(Count);
  for(int32 I=0;I<Count;++I) LastPose[I]=Output.Pose[FCompactPoseBoneIndex(I)];
  ApplyGesture(Output.Pose);
  AimStrike(Output.Pose);
  UpdateYigAttachments(Output.Pose);
  return Result;
 }
};
}
FAnimInstanceProxy* UCultFluidAnimInstance::CreateAnimInstanceProxy() { return new FCultFluidProxy(this); }
