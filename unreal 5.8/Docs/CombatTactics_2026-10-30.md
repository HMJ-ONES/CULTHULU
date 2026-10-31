# Tower fire and competitive combat movement — 2026-10-30

Ward tower and nexus shots keep their existing damage, firing cadence, windup and fast visible projectile. Each shot now retains the selected target and tracks it through ordinary movement. Starting a dash after the windup breaks that shot permanently. Jump evasion does not block tower damage. Death, a lost target or a destroyed firing tower still cleans up its shot. Tower minion priority and outer/inner/nexus protection remain unchanged. Hero abilities and minion bolts retain physical, nonhoming delivery.

The shared Shift tap dash has an eight-second cooldown in DOTGOO, for humans and bots. Other modes retain 1.15 seconds. Holding Shift remains sprint, with no repeated dash.

Competitive bots prioritize nearby opponents who threaten teammates, keep their assigned MOBA route, and select spacing from their actual authored kit. Ranged support can stand just outside the Domination capture area when a teammate is capturing and the firing position is within reach; otherwise it returns to the seal. Bots spread out their approach and firing angles, use supported dash paths to escape hero pressure or close for a melee ambush, and use occasional jumps while repositioning. Existing threat reactions retain observation/reaction time. No spell names, kit statistics, score rules or free-roam beliefs changed.

Native regression coverage includes real fired tower shots against circling movement, jump-evade damage, dash cancellation/cooldown, teammate target selection, ranged support and returning to capture, safe tactical escape, existing physical projectile defense and melee/ability opportunity checks. Final run results are recorded alongside the completed build verification.

Next presentation pass requested by the owner: correct Mi-Go/Yig facing; connect available animation clips to running, jumping, attacks and abilities; use a minimal white crosshair; preserve crosshair aim when backing up and changing camera view.

## Verification

Canonical editor build succeeded. All 13 distinct native checks have passing final results, including both live matches and seven focused checks rerun after refinements. Actual tactical escape choices passed in both modes, with 1.15 / 8.00 second cooldowns. Earlier failures and corrections are retained in Saved/CombatTactics20261030/Results.json and the original logs.
