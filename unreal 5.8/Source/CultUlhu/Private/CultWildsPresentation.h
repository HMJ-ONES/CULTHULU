#pragma once
class UWorld;

namespace CultWildsPresentation {
// Runtime-only lighting for Free Roam. Authored maps and imported art stay intact.
void Apply(UWorld* World);
}
