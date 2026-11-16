#include "CultCombatAudio.h"
#include "Engine/World.h"
#include "AudioDevice.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"

// Small original synthesis cues, local to the human player. No downloaded sound packs.
void PlayCultCombatCue(UWorld* World,ECultCombatCue Kind,float Volume)
{
 if(!World||!World->GetAudioDevice().IsValid()||IsRunningDedicatedServer()) return;
 constexpr int Rate=22050;
 const bool Whoosh=Kind==ECultCombatCue::Dash||Kind==ECultCombatCue::Evade;
 const bool Cannon=Kind==ECultCombatCue::GateCannon;
 const float Duration=Cannon?.24f:Whoosh?.13f:Kind==ECultCombatCue::Kill?.16f:.085f;
 const int Count=FMath::RoundToInt(Duration*Rate);
 TArray<int16> Samples; Samples.SetNumUninitialized(Count);
 FRandomStream Noise(9247); float Phase=0,Filtered=0;
 for(int I=0;I<Count;++I) {
  const float T=float(I)/Rate,Progress=T/Duration;
  const float Envelope=FMath::Min(1.f,T/.003f)*FMath::Pow(1.f-Progress,2.f);
  Filtered=FMath::Lerp(Filtered,Noise.FRandRange(-1.f,1.f),Whoosh?.35f:.12f);
  const float Pitch=Cannon?FMath::Lerp(135.f,47.f,FMath::Sqrt(Progress)):Whoosh?FMath::Lerp(700.f,230.f,Progress):Kind==ECultCombatCue::Kill?660.f:Kind==ECultCombatCue::Hurt?85.f:FMath::Lerp(180.f,65.f,Progress);
  Phase+=2*PI*Pitch/Rate;
  const float Tone=FMath::Sin(Phase);
  const float Value=Cannon?Tone*.68f+FMath::Sin(Phase*2.08f)*.12f+Filtered*.40f*(1.f-Progress):
   Whoosh?Filtered*.8f:Kind==ECultCombatCue::Kill?(Tone*.35f+FMath::Sin(Phase*1.5f)*.22f):Tone*.65f+Filtered*.25f;
  Samples[I]=int16(FMath::Clamp(Value*Envelope,-1.f,1.f)*24000);
 }
 auto* Sound=NewObject<USoundWaveProcedural>(World);
 Sound->SetSampleRate(Rate); Sound->NumChannels=1; Sound->Duration=Duration; Sound->bLooping=false;
 Sound->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));
 UGameplayStatics::PlaySound2D(World,Sound,Volume,1.f,0.f,nullptr,nullptr,true);
}
