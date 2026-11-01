#pragma once
class UWorld;
class UMaterialInterface;
namespace CultMatchSurfaces {
 bool Enabled();
 bool RefinedDomination();
 UMaterialInterface* StoneMaterial(bool Domination);
 int Apply(UWorld* World,bool Stone,bool Domination=false);
}
