#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "CultCharacterModel.h"
#include "CultFluidAnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "SkeletalRenderPublic.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultRigGeometryTest,"CultUlhu.Art.RigGeometryAudit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultRigGeometryTest::RunTest(const FString&) {
 auto* W=GEditor->GetEditorWorldContext().World(); if(!W) { AddError(TEXT("No editor world")); return false; }
 auto* Model=W->SpawnActor<ACultCharacterModel>(); if(!Model->Initialize(TEXT("yig"),true)) { AddError(TEXT("Yig model unavailable")); Model->Destroy(); return false; }
 auto Record=MakeShared<FJsonObject>();
 for(const auto& Part:Model->Parts()) if(auto* S=Cast<USkeletalMeshComponent>(Part.Get())) if(S->GetSkeletalMeshAsset()->GetName().Contains(TEXT("Sword"))) {
  S->SetPosition(0,false); S->TickAnimation(0,false); S->RefreshBoneTransforms();
  TArray<FFinalSkinVertex> Vertices; S->GetCPUSkinnedVertices(Vertices,0);
  const auto& LOD=S->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0]; const auto* Weights=LOD.GetSkinWeightVertexBuffer();
  TMap<int,FVector> Sum; TMap<int,double> Mass;
  for(const auto& Section:LOD.RenderSections) for(uint32 I=Section.BaseVertexIndex;I<Section.BaseVertexIndex+Section.NumVertices&&I<uint32(Vertices.Num());++I) {
   for(uint32 K=0;K<Weights->GetMaxBoneInfluences();++K) { const auto Weight=Weights->GetBoneWeight(I,K); if(!Weight) continue; const int Bone=Section.BoneMap[Weights->GetBoneIndex(I,K)]; Sum.FindOrAdd(Bone)+=FVector(Vertices[I].Position)*Weight; Mass.FindOrAdd(Bone)+=Weight; }
  }
  for(const auto& Pair:Sum) { const FVector Centre=Pair.Value/Mass[Pair.Key]; TArray<TSharedPtr<FJsonValue>> XYZ; for(double V:{Centre.X,Centre.Y,Centre.Z}) XYZ.Add(MakeShared<FJsonValueNumber>(V)); Record->SetArrayField(S->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(Pair.Key).ToString().ToLower(),XYZ); }
 }
 TestEqual(TEXT("All four rigid source swords have independently weighted geometry"),Record->Values.Num(),4);
 FString Text; FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Text));
 TestTrue(TEXT("Posed weapon geometry audit saved"),FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("CombatTactics20261030/WeaponGeometry.json"))));
 Model->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultFluidMotionTest,"CultUlhu.Art.FluidMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultFluidMotionTest::RunTest(const FString&) {
 auto* W=GEditor->GetEditorWorldContext().World(); if(!W) return false;
 for(const FString Id:{TEXT("mi_go"),TEXT("yig")}) {
  auto* Model=W->SpawnActor<ACultCharacterModel>(); if(!Model->Initialize(Id,true)) { AddError(TEXT("No model for ")+Id); Model->Destroy(); continue; }
  Model->PreviewMotion(TEXT("walk"));
  TArray<TArray<FTransform>> Before;
  for(const auto& Part:Model->Parts()) if(auto* S=Cast<USkeletalMeshComponent>(Part.Get())) {
   TestNotNull(TEXT("Motion uses the actual native blending instance"),Cast<UCultFluidAnimInstance>(S->GetAnimInstance()));
   S->SetPosition(.2f,false); S->TickAnimation(.2f,false); S->RefreshBoneTransforms(); Before.Add(S->GetComponentSpaceTransforms());
  }
  Model->PreviewMotion(TEXT("attack")); int Index=0;
  for(const auto& Part:Model->Parts()) if(auto* S=Cast<USkeletalMeshComponent>(Part.Get())) {
   S->TickAnimation(0,false); S->RefreshBoneTransforms(); const auto& After=S->GetComponentSpaceTransforms();
   bool Continuous=Before.IsValidIndex(Index)&&Before[Index].Num()==After.Num();
   if(Continuous) for(int Bone=0;Bone<After.Num();++Bone) Continuous&=After[Bone].Equals(Before[Index][Bone],.01f);
   TestTrue(TEXT("The first switched pose is continuous rather than snapping: ")+Id,Continuous);
   S->TickAnimation(.15f,false); S->RefreshBoneTransforms(); bool Advanced=false;
   const auto& Blended=S->GetComponentSpaceTransforms();
   if(Before.IsValidIndex(Index)) for(int Bone=0;Bone<FMath::Min(Before[Index].Num(),Blended.Num());++Bone) Advanced|=!Blended[Bone].Equals(Before[Index][Bone],.001f);
   TestTrue(TEXT("The transition advances into real articulated motion: ")+Id,Advanced); ++Index;
  }
  Model->Destroy();
 }
 return true;
}
#endif
