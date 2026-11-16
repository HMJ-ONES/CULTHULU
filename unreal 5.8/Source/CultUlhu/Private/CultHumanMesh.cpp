#include "CultHumanMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "SkeletalMeshMerge.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/IConsoleManager.h"

namespace {
TAutoConsoleVariable<int32> MergeEnabled(TEXT("cult.Population.MergeHumanMeshes"),1,TEXT("Share a runtime merge of the six original humanoid pieces. Zero retains modular components for comparison."));
struct FEntry {
 TStrongObjectPtr<USkeletalMesh> Mesh;
 bool Attempted=false; int32 Parts=0; FString Details;
};
TMap<FString,FEntry>& Cache() { static TMap<FString,FEntry> Entries; return Entries; }

TArray<USkeletalMesh*> Sources(const FString& Skin) {
 TArray<USkeletalMesh*> Out;
 if(Skin!=TEXT("civilian_villager")&&Skin!=TEXT("cultist_hooded")&&Skin!=TEXT("cultist_magus")) return Out;
 const FString Prefix=Skin==TEXT("cultist_magus")?TEXT("Mage_"):TEXT("Rogue_");
 const FString Folder=TEXT("/Game/CultUlhu/People/")+Skin+TEXT("/")+Skin+TEXT("/SkeletalMeshes");
 for(const FString Piece:{FString(TEXT("Body")),Skin==TEXT("cultist_hooded")?FString(TEXT("Head_Hooded")):FString(TEXT("Head")),FString(TEXT("ArmLeft")),FString(TEXT("ArmRight")),FString(TEXT("LegLeft")),FString(TEXT("LegRight"))}) {
  const FString Name=Prefix+Piece;
  if(auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(Folder/Name+TEXT(".")+Name))) Out.Add(Mesh);
 }
 return Out;
}

int32 TriangleCount(const FSkeletalMeshLODRenderData& LOD) {
 int32 Count=0; for(const auto& Section:LOD.RenderSections) Count+=Section.NumTriangles; return Count;
}

bool Validate(const TArray<USkeletalMesh*>& Inputs,USkeletalMesh* Merged,FString& Why) {
 if(Inputs.Num()!=6||!Merged||!Merged->GetResourceForRendering()||Merged->GetResourceForRendering()->LODRenderData.IsEmpty()) { Why=TEXT("Missing source/result geometry"); return false; }
 int32 Vertices=0,Triangles=0; FBox Bounds(ForceInit);
 struct FVertex { FVector3f Position; FVector2f UV; };
 TArray<FVertex> Positions;
 for(auto* Input:Inputs) {
  auto* Data=Input->GetResourceForRendering(); if(!Data||Data->LODRenderData.IsEmpty()) { Why=TEXT("Source render data missing"); return false; }
  const auto& LOD=Data->LODRenderData[0]; Vertices+=LOD.GetNumVertices(); Triangles+=TriangleCount(LOD); Bounds+=Input->GetBounds().GetBox();
  for(uint32 V=0;V<LOD.GetNumVertices();++V) Positions.Add({LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(V),LOD.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V,0)});
  const auto& Ref=Input->GetRefSkeleton();
  for(int B=0;B<Ref.GetNum();++B) {
   const int Destination=Merged->GetRefSkeleton().FindBoneIndex(Ref.GetBoneName(B));
   if(Destination==INDEX_NONE||!Ref.GetRefBonePose()[B].Equals(Merged->GetRefSkeleton().GetRefBonePose()[Destination],.001f)) { Why=TEXT("A source bone/reference pose changed"); return false; }
  }
  for(const auto& Material:Input->GetMaterials()) {
   bool Found=false; for(const auto& Candidate:Merged->GetMaterials()) Found|=Material.MaterialInterface==Candidate.MaterialInterface;
   if(!Found) { Why=TEXT("A source material was lost"); return false; }
  }
 }
 const auto& LOD=Merged->GetResourceForRendering()->LODRenderData[0];
 if(Vertices!=int32(LOD.GetNumVertices())||Triangles!=TriangleCount(LOD)) { Why=TEXT("Source vertex/triangle totals changed"); return false; }
 TArray<FVertex> Result;
 for(uint32 V=0;V<LOD.GetNumVertices();++V) Result.Add({LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(V),LOD.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V,0)});
 auto Less=[](const FVertex& A,const FVertex& B) { if(A.Position.X!=B.Position.X) return A.Position.X<B.Position.X; if(A.Position.Y!=B.Position.Y) return A.Position.Y<B.Position.Y; if(A.Position.Z!=B.Position.Z) return A.Position.Z<B.Position.Z; return A.UV.X!=B.UV.X?A.UV.X<B.UV.X:A.UV.Y<B.UV.Y; };
 Positions.Sort(Less); Result.Sort(Less);
 for(int V=0;V<Positions.Num();++V) if((!Positions[V].Position.Equals(Result[V].Position,.001f)||!Positions[V].UV.Equals(Result[V].UV,.001f))) { Why=TEXT("Original rest geometry/primary UVs changed"); return false; }
 const auto Box=Merged->GetBounds().GetBox();
 if(!Box.Min.Equals(Bounds.Min,1.f)||!Box.Max.Equals(Bounds.Max,1.f)) { Why=TEXT("Combined model bounds changed"); return false; }
 Why=FString::Printf(TEXT("Six source pieces, %d vertices, %d triangles, original positions/primary UVs/materials/reference bones retained"),Vertices,Triangles);
 return true;
}
}

bool CultHumanMesh::Enabled() { return MergeEnabled.GetValueOnGameThread()!=0; }
void CultHumanMesh::Reset() { Cache().Reset(); }
USkeletalMesh* CultHumanMesh::Find(const FString& Skin) {
 auto& Entry=Cache().FindOrAdd(Skin); if(Entry.Attempted) return Entry.Mesh.Get(); Entry.Attempted=true;
 const auto Inputs=Sources(Skin); if(Inputs.Num()!=6) { Entry.Details=TEXT("Six original body meshes are required"); return nullptr; }
 for(auto* Input:Inputs) {
  if(!Input->GetMorphTargets().IsEmpty()) { Entry.Details=TEXT("Preserving modular morph targets; this skin is not mergeable"); return nullptr; }
  auto* Data=Input->GetResourceForRendering();
  if(!Data||Data->LODRenderData.IsEmpty()||!Data->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer.GetVertexData()) { Entry.Details=TEXT("CPU source positions unavailable"); return nullptr; }
#if !WITH_EDITOR
  if(!Input->GetLODInfo(0)||!Input->GetLODInfo(0)->bAllowCPUAccess) { Entry.Details=TEXT("Cooked source LOD must retain CPU data"); return nullptr; }
#endif
 }
 auto* Merged=NewObject<USkeletalMesh>(GetTransientPackage()); Entry.Mesh.Reset(Merged);
 TArray<FSkelMeshMergeSectionMapping> Mapping;
 FSkeletalMeshMerge Merge(Merged,Inputs,Mapping,0,EMeshBufferAccess::ForceCPUAndGPU);
 if(!Merge.DoMerge()) { Entry.Details=TEXT("Engine mesh merge failed"); Entry.Mesh.Reset(); return nullptr; }
 Merged->SetSkeleton(Inputs[0]->GetSkeleton());
 if(!Validate(Inputs,Merged,Entry.Details)) { UE_LOG(LogTemp,Warning,TEXT("[CultHumanMesh] %s: %s; preserving modular art"),*Skin,*Entry.Details); Entry.Mesh.Reset(); return nullptr; }
 Entry.Parts=Inputs.Num();
 UE_LOG(LogTemp,Log,TEXT("[CultHumanMesh] %s: %s"),*Skin,*Entry.Details); return Merged;
}
int32 CultHumanMesh::SourceParts(const USkeletalMesh* Mesh) { for(const auto& Pair:Cache()) if(Pair.Value.Mesh.Get()==Mesh) return Pair.Value.Parts; return 0; }
bool CultHumanMesh::Verify(const FString& Skin,FString& Details) {
 auto* Merged=Find(Skin); auto& Entry=Cache().FindChecked(Skin); Details=Entry.Details;
 return Merged&&Validate(Sources(Skin),Merged,Details);
}
