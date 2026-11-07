#include "CultFluidAnimInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/AnimSequence.h"

namespace {
struct FCultFluidProxy final : FAnimSingleNodeInstanceProxy {
 explicit FCultFluidProxy(UAnimInstance* Instance):FAnimSingleNodeInstanceProxy(Instance) {}
 TArray<FTransform> LastPose,FromPose;
 float BlendAge=1.f,BlendLength=.14f,LastTime=0;
 UAnimationAsset* LastAsset=nullptr;
 float Gather=0,Snap=0,Guard=0;
 uint8 GestureRig=0;
 FVector Right=FVector::RightVector,Up=FVector::UpVector,Forward=FVector::ForwardVector;
 virtual void PreUpdate(UAnimInstance* Instance,float Delta) override {
  FAnimSingleNodeInstanceProxy::PreUpdate(Instance,Delta);
  const auto* Motion=CastChecked<UCultFluidAnimInstance>(Instance);
  Gather=Motion->Gather;
  Snap=Motion->Snap;
  Guard=Motion->Guard;
  GestureRig=Motion->GestureRig;
  const auto Relative=Instance->GetSkelMeshComponent()->GetRelativeRotation().Quaternion();
  Right=Relative.UnrotateVector(FVector::RightVector);
  Up=Relative.UnrotateVector(FVector::UpVector);
  Forward=Relative.UnrotateVector(FVector::ForwardVector);
 }
 void ApplyGesture(FCompactPose& Pose) {
  if(!GestureRig||Gather+Snap+FMath::Abs(Guard)<.001f) return;
  const auto& Bones=Pose.GetBoneContainer();
  FCSPose<FCompactPose> ComponentPose;
  ComponentPose.InitPose(Pose);
  auto Rotate=[&](FName Name,FVector Axis,float Degrees) {
   const int32 MeshIndex=Bones.GetReferenceSkeleton().FindBoneIndex(Name);
   if(MeshIndex==INDEX_NONE) return;
   const auto Bone=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
   if(Bone.GetInt()==INDEX_NONE) return;
   const auto Parent=Pose.GetParentBoneIndex(Bone);
   if(Parent.GetInt()!=INDEX_NONE) Axis=ComponentPose.GetComponentSpaceTransform(Parent).GetRotation().UnrotateVector(Axis);
   Pose[Bone].SetRotation((FQuat(Axis,FMath::DegreesToRadians(Degrees))*Pose[Bone].GetRotation()).GetNormalized());
  };
  if(GestureRig==1) {
   // The shoulder leads the lift, then the elbows pull in to brace. The
   // forward release opens the wings to counter the upper-body impulse.
   Rotate(TEXT("chest"),Right,-9*Guard+17*Snap);
   Rotate(TEXT("upperarm_l"),Right,-49*Guard-22*Snap);
   Rotate(TEXT("upperarm_r"),Right,-49*Guard-22*Snap);
   Rotate(TEXT("forearm_l"),Right,-27*Guard+20*Snap);
   Rotate(TEXT("forearm_r"),Right,-27*Guard+20*Snap);
   Rotate(TEXT("wingbase_l"),Up,-11*Guard-13*Snap);
   Rotate(TEXT("wingbase_r"),Up,11*Guard+13*Snap);
  } else if(GestureRig==2) {
   // An insectoid draws both scythes inward to brace a sharp head recoil.
   Rotate(TEXT("bicep.l"),Right,-21*Gather+13*Snap);
   Rotate(TEXT("bicep.r"),Right,-21*Gather+13*Snap);
   Rotate(TEXT("forearm.l"),Up,13*Gather-17*Snap);
   Rotate(TEXT("forearm.r"),Up,-13*Gather+17*Snap);
   Rotate(TEXT("wingbase1.l"),Up,10*Gather-18*Snap);
   Rotate(TEXT("wingbase1.r"),Up,-10*Gather+18*Snap);
  } else if(GestureRig==3) {
   Rotate(TEXT("core_mid"),Right,-13*Gather+18*Snap);
   Rotate(TEXT("core_top"),Up,18*Gather-25*Snap);
   for(int I=0;I<8;++I) {
    const FName Orb(*FString::Printf(TEXT("orb_%02d"),I));
    Rotate(Orb,Right,FMath::Sin(I*PI*.25f)*(17*Gather-22*Snap));
   }
  } else if(GestureRig==4) {
   Rotate(TEXT("chest"),Forward,5*Guard);
   Rotate(TEXT("neck"),Forward,-3*Guard);
   Rotate(TEXT("jaw"),Right,12*Snap);
  } else if(GestureRig==5) {
   Rotate(TEXT("chest"),Forward,5*Guard);
   Rotate(TEXT("head_00"),Right,-5*Gather+8*Snap);
   Rotate(TEXT("head_02"),Up,6*Guard-7*Snap);
  } else if(GestureRig==6) {
   Rotate(TEXT("chest"),Forward,4*Guard);
   Rotate(TEXT("neck"),Right,-4*Gather+7*Snap);
   Rotate(TEXT("head"),Right,5*Snap);
  }
 }
 virtual void SetAnimationAsset(UAnimationAsset* Asset,USkeletalMeshComponent* Mesh,bool Loop,float Rate) override {
  if(Asset!=LastAsset) { FromPose=LastPose; BlendAge=0; BlendLength=.14f; LastTime=0; LastAsset=Asset; }
  FAnimSingleNodeInstanceProxy::SetAnimationAsset(Asset,Mesh,Loop,Rate);
 }
 virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override {
  BlendAge+=Context.GetDeltaTime(); FAnimSingleNodeInstanceProxy::UpdateAnimationNode(Context);
 }
 virtual bool Evaluate(FPoseContext& Output) override {
  const bool Result=FAnimSingleNodeInstanceProxy::Evaluate(Output);
  const float Time=GetCurrentTime();
  if(auto* Clip=Cast<UAnimSequence>(LastAsset)) if(IsLooping()&&LastPose.Num()>0&&Clip->GetPlayLength()>0&&FMath::Abs(Time-LastTime)>Clip->GetPlayLength()*.5f) {
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
  return Result;
 }
};
}
FAnimInstanceProxy* UCultFluidAnimInstance::CreateAnimInstanceProxy() { return new FCultFluidProxy(this); }
