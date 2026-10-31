#include "CultReplicatedBody.h"
#include "CultFreeRoamSave.h"
#include "CultCorpse.h"
#include "Net/UnrealNetwork.h"

UCultReplicatedBody::UCultReplicatedBody() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickInterval=.2f; }
void UCultReplicatedBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UCultReplicatedBody,State_); }
void UCultReplicatedBody::OnRep_State() { Pending_=true; }
void UCultReplicatedBody::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function) {
 Super::TickComponent(Dt,Type,Function);
 auto* Owner=GetOwner(); if(!Owner||!Owner->HasActorBegunPlay()) return;
 if(Owner->HasAuthority()&&GetWorld()->GetNetMode()!=NM_Standalone) CultFreeRoamSave::CaptureBody(Owner,State_);
 else if(Pending_) { CultFreeRoamSave::ApplyBody(Owner,State_); Pending_=false; }
}
void UCultReplicatedBody::LeaveCorpse_Implementation() { if(!GetOwner()->HasAuthority()) ACultCorpse::Leave(GetOwner()); }
