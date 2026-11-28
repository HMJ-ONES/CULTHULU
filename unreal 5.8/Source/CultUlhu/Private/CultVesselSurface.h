#pragma once
class ACultUlhuPlayerPawn;
class UStaticMeshComponent;
namespace CultVesselSurface {
// Applies to the owned placeholder only; authored art materials remain intact.
void Apply(ACultUlhuPlayerPawn* Hero,UStaticMeshComponent* Body,bool Legacy=false);
}
