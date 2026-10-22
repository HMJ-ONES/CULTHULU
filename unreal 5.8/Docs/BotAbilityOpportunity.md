# Useful area casts

The generic bot distance triggers allowed Cthulhu's 12 m Nightmare Veil aura
and Dagon's 8 m Crushing Maw cone to spend resources without any reachable
victim. Native baseline probes in both modes reproduce a false warning plus
35 stamina / 15 seconds for Veil and 30 stamina / 8 seconds for Maw. Advancing
those far deliveries produces no damage or fear. The source kit values are
retained; the change is when a bot commits an area cast.

Bot sphere and cone casts now look for a living, hostile, vulnerable body in
their actual committed shape with clear cover. They use the same AreaContact
calculation as the physical attack and the same Clear trace. Self-centred
auras consider every eligible nearby victim rather than only the selected aim
target. An aura can therefore remain useful against nearby acolytes when a
selected hero is out of range. Structures that already ignore control effects
do not count as valid victims of those effects. Solid cover can invalidate an
otherwise close opportunity.

If no useful victim is present, the delivery returns before a warning actor,
stamina deduction or ability cooldown is committed. These checks occur after
normal availability, existing cooldown and stamina checks; they are not an
additional per-frame world poll. No navigation, enemy selection, future-input
prediction or homing behavior is added. An enemy may still move or evade during
the original windup, causing a correctly committed cast to miss.

The actual area-contact code is extracted without changing its math. A source
identity check compares sphere/cone/ground-disk calculations with the accepted
prior implementation, normalizing only iterator/output names and whitespace.
Enemy checks, cover and once-per-victim contact remain at the physical call
site. Spell shapes, source ranges, powers, cooldowns, stamina, delays, projectile
delivery and original kit files are unchanged. Human casting is not subject to
the new bot opportunity gate. Healing, buffs, shields, dashes, summons, ground
impacts and heavy sweeps retain their existing cast paths.

## Evidence and limits

BotAbilityOpportunityResults.json records baseline and final native outcomes,
source identity and input hashes. The real BotThink probes check wasted versus
useful casts, exact resource preservation and actual near control/damage in both
modes. Additional probes cover a different nearby minion, real fear projection,
cover blocking and an isolated immune structure. The fear control uses the
normal projection update before inspecting aggregate state.

Two initial fixture assumptions are explicitly excluded from the production
defect count: reading fear before its projection update, and calling a scene
immune-only while another vulnerable hero was still nearby. The final fixture
corrects both. The valid far resource/warning failures remain the baseline.

Separate attack-shape, physical combat, melee approach, defensive dodge and
Domination/MOBA integration checks provide regression coverage. Integration
fixtures contain short live windows and scripted transitions, not uninterrupted
human matches. The report retains interruptions and any isolated reruns; a green
subset is not described as a clean original batch.

The opportunity scan is not a complete tactical model. It does not choose the
best victim, coordinate cooldowns, conserve resources for planned combos or
solve role positioning. Its additional CPU cost has not been benchmarked. Some
spell/RMB triggers still use broad shared distances, and the specialized heavy,
summon and ground-impact paths need further decision work. No all-kit tactical
quality, long-session balance, human fun, multiplayer, low-spec performance or
fresh standalone package is established. The full production goal remains open.
