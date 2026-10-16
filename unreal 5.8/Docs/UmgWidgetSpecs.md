# UMG Widget Specs — CommandMenu + StatsPanel

The core (`src/ui`) is data-only by design; everything below is authored
in Widget Blueprints parented on the C++ classes in
`Source/CultUlhuCore/Public/UI/`. Precise `BindWidget` names are in the
headers — **they must match the Blueprint exactly** (case-sensitive).

## WBP_CommandMenu (parents `UCultCommandMenuWidget`)

**Purpose:** Alt+left-click radial/context menu. Buttons are generated
contextually by the core; the widget only renders and routes clicks.

**Required named widgets:**

| Blueprint name | Type | Notes |
|---|---|---|
| `MenuRoot` | `UCanvasPanel` | Full-screen, hit-test invisible except buttons. `BindWidget` (see header). |

**Runtime-built buttons** (created in C++ `RebuildButtons`, not pre-placed):

- Button widget class: `WBP_CommandMenuButton` (make it): contains one
  `UTextBlock` named `LabelText` and exposes `SetLabel(FString)`.
  // VERIFY IN EDITOR: `WBP_CommandMenuButton` exists and the C++ can
  // reference it (TSubclassOf property if you prefer it configurable).
- Layout: buttons evenly spaced on a circle, radius ~120 px, centered on
  the click screen position. Clamp the circle inside the viewport.
- Each button shows `MenuButton.label`; disabled buttons (`enabled ==
  false`) render greyed and ignore clicks.
- Click → C++ `OnMenuButtonClicked(Index)` → core `dispatch()` → close.

**Contexts → buttons** (core `generateMenu` decides; the widget is dumb):

| Clicked | Buttons |
|---|---|
| Own cultist | Follow / Attack / Build / Sacrifice / Convert |
| Location | Move / Raid / Build altar / Scout |
| Enemy | Attack / Capture |
| Altar | Ritual choices (tier-scaled) |

**Open/close rules:** open on `IA_CommandMenu` (Alt+LMB); close on
button click, RMB, or Esc. While open, the game may keep running
(single-player pause is a design decision — default: no pause).

## WBP_StatsPanel (parents `UCultStatsPanelWidget`)

**Purpose:** Tab overlay — cult state, kills, belief gauges, power,
insurrection risk; KDA table in multiplayer.

**Required named widgets:**

| Blueprint name | Type | Notes |
|---|---|---|
| `PowerText` | `UTextBlock` | Total power value |
| `KillsText` | `UTextBlock` | Total kills |
| `InsurrectionBar` | `UProgressBar` | 0–100 %; color-shift to red above 70 % |
| `BeliefGaugeList` | `UVerticalBox` | 12 rows built at runtime: belief name + `UProgressBar` (0–100 exertion) |
| `KdaList` | `UVerticalBox` | One row per player (name, K/D/A); `Collapsed` when empty (single-player) |

**Behavior:**

- Toggled by `IA_StatsOverlay` (Tab). Refresh on show + ~4 Hz while visible
  (a timer in the widget or the PlayerController; not every tick).
- Data source: `UCultStatsPanelWidget::Refresh()` → core
  `StatsPanel::gather()`. KDA rows come from the game module's
  `PlayerStatsTracker` wrapper (see `NetcodeDecision.md`).
  // VERIFY IN EDITOR: the tracker is actually wired — empty table in
  // multiplayer is a bug, not a feature.

## WBP_CommandMenuButton (helper, referenced above)

- Root: `UButton`; child `UTextBlock` named `LabelText`.
- Exposes `SetLabel(FString)` (BlueprintCallable or native).
- Disabled visual state: implement via a bool `SetEnabled` that swaps the
  button style / text color.

## General UMG rules for this project

- Widgets never simulate: they read core data and call core dispatch.
- No game logic in Blueprint graphs beyond layout/animation. If a Blueprint
  starts computing, that logic belongs in `../src` + a C++ wrapper.
- Font/art direction: cosmic horror — dark, serif display font for headers,
  thin sans for numbers. (Art pass, not blocking.)
