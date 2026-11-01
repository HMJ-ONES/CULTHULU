#include "CultFluidAnimInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/AnimSequence.h"

namespace {
struct FCultFluidProxy final : FAnimSingleNodeInstanceProxy {
 explicit FCultFluidProxy(UAnimInstance* Instance):FAnimSingleNodeInstanceProxy(Instance) {}
 TArray<FTransform> LastPose,FromPose;
 float BlendAge=1.f,BlendLength=.14f,LastTime=0;
 UAnimationAsset* LastAsset=nullptr;
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
  return Result;
 }
};
}
FAnimInstanceProxy* UCultFluidAnimInstance::CreateAnimInstanceProxy() { return new FCultFluidProxy(this); }
