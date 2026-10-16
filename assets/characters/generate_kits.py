#!/usr/bin/env python3
"""CULT-ULHU wave 25: generate character.def kits for the 20 playable entities.

Each kit follows the shared power budget (see KITS.md):
  - HP 400-700, move speed 4.5-7.0
  - Q: bread-and-butter, 6-10s CD, 80-150 power
  - F: utility/CC/mobility, 10-18s CD
  - R: game-changer, 40-60s CD, <= 250 power
  - CC: single effect <= 2.5s, never chainable (CD >> duration)
  - Estimated kit DPS (Q + R direct damage, summons amortized) lands 10-40

Effect vocabulary (must stay in CharacterValidator::knownEffectKind):
  aoe_damage, projectile, fear_aura, summon, buff, heal, shield, debuff,
  dash (power = meters), pull (power = drag meters), stun (power = seconds)

Model slot: folders ship WITHOUT model files. The loader treats them as
logic-only and the engine uses the procedural placeholder until the owner
drops in model.fbx (or model.glb) + rig.map. See docs/bring_your_own_model.md.

Re-run: python3 assets/characters/generate_kits.py  (from the repo root)
"""

import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)))


def spell(d):
    return d


def kit(cid, display_name, flavor, hp, speed, combo,
        q, f, r, rc, passive):
    return {
        "id": cid, "display_name": display_name, "flavor": flavor,
        "max_hp": hp, "move_speed": speed, "max_stamina": 100,
        "melee_combo": combo, "q": q, "f": f, "r": r,
        "rightclick": rc, "passive": passive,
    }


def rc(kind, name, mult, rng, cc="", cc_s=0):
    return {"kind": kind, "name": name, "damage_mult": mult,
            "range": rng, "cc": cc, "cc_seconds": cc_s}


KITS = [
    # ------------------------------------------------------------------
    # Yog-Sothoth, the All-in-One — Controller (gates / space-time)
    # ------------------------------------------------------------------
    kit("yog_sothoth", "Yog-Sothoth, the All-in-One",
        "The gate is open. The gate was always open.",
        480, 5.5, "gatekeeper_flurry",
        spell({"id": "gate_lash", "name": "Gate Lash",
               "flavor": "A rift tears open and lashes through it.",
               "cooldown": 8, "stamina_cost": 25,
               "effect": "projectile", "power": 110, "range": 18}),
        spell({"id": "between_spaces", "name": "Between Spaces",
               "flavor": "Steps outside the world and back, elsewhere.",
               "cooldown": 14, "stamina_cost": 30,
               "effect": "dash", "power": 15, "range": 0}),
        spell({"id": "key_and_gate", "name": "The Key and the Gate",
               "flavor": "Every gate opens at once; all things fall toward it.",
               "cooldown": 50, "stamina_cost": 55,
               "effect": "pull", "power": 12, "range": 14}),
        rc("EldritchGrasp", "Gate Maw", 1.0, 20, "Slow", 2.0),
        ("all_in_one",
         "All-in-One: Between Spaces refunds half its cooldown when it "
         "carries Yog-Sothoth out of a lethal blow's path.")),

    # ------------------------------------------------------------------
    # Nyarlathotep, the Crawling Chaos — Assassin / trickster
    # ------------------------------------------------------------------
    kit("nyarlathotep", "Nyarlathotep, the Crawling Chaos",
        "A thousand forms, and every one of them is lying.",
        450, 7.0, "crawling_flurry",
        spell({"id": "thousand_lashes", "name": "Thousand Lashes",
               "flavor": "Every borrowed arm strikes at once.",
               "cooldown": 7, "stamina_cost": 25,
               "effect": "projectile", "power": 100, "range": 16}),
        spell({"id": "borrowed_face", "name": "Borrowed Face",
               "flavor": "Wears a stranger's shape; eyes slide off him.",
               "cooldown": 16, "stamina_cost": 30,
               "effect": "buff", "power": 6, "range": 0}),
        spell({"id": "unmasked_chaos", "name": "Unmasked Chaos",
               "flavor": "The masks fall. What looks back, breaks.",
               "cooldown": 55, "stamina_cost": 55,
               "effect": "fear_aura", "power": 3, "range": 16}),
        rc("MeleeHeavy", "Crawling Strike", 1.4, 3, "Fear", 1.5),
        ("thousand_forms",
         "Thousand Forms: Borrowed Face also cleanses slows and roots.")),

    # ------------------------------------------------------------------
    # Shub-Niggurath, the Black Goat — Summoner (broodmother)
    # ------------------------------------------------------------------
    kit("shub_niggurath", "Shub-Niggurath, the Black Goat",
        "The woods are full of her children. So is everywhere else.",
        600, 5.0, "broodmother_slam",
        spell({"id": "brood_surge", "name": "Brood Surge",
               "flavor": "The dark young pour from the treeline.",
               "cooldown": 10, "stamina_cost": 30,
               "effect": "summon", "power": 3, "range": 16}),
        spell({"id": "fertile_rot", "name": "Fertile Rot",
               "flavor": "The ground ripens; flesh softens for the harvest.",
               "cooldown": 14, "stamina_cost": 35,
               "effect": "debuff", "power": 5, "range": 12}),
        spell({"id": "woods_give_birth", "name": "The Woods Give Birth",
               "flavor": "Iä! The forest itself stands up and walks.",
               "cooldown": 55, "stamina_cost": 60,
               "effect": "summon", "power": 6, "range": 20}),
        rc("AcidSpit", "Brood Spit", 0.9, 22, "Slow", 2.0),
        ("thousand_young",
         "Thousand Young: summoned young last 25% longer and share 10% of "
         "the damage they deal back as healing.")),

    # ------------------------------------------------------------------
    # The Mi-Go, Fungi from Yuggoth — Controller / artillery
    # ------------------------------------------------------------------
    kit("mi_go", "The Mi-Go, Fungi from Yuggoth",
        "Surgery is just diplomacy with sharper tools.",
        470, 6.5, "surgical_flurry",
        spell({"id": "nerve_needle", "name": "Nerve Needle",
               "flavor": "A filament finds the spine. Precision hurts.",
               "cooldown": 7, "stamina_cost": 25,
               "effect": "projectile", "power": 95, "range": 20}),
        spell({"id": "cylinder_stasis", "name": "Cylinder Stasis",
               "flavor": "The specimen is filed. Specimens do not move.",
               "cooldown": 15, "stamina_cost": 35,
               "effect": "stun", "power": 1.5, "range": 14}),
        spell({"id": "yuggoth_harvest", "name": "Yuggoth Harvest",
               "flavor": "The wings unfold; the sky becomes a scalpel.",
               "cooldown": 50, "stamina_cost": 55,
               "effect": "aoe_damage", "power": 200, "range": 10}),
        rc("AcidSpit", "Bio-Acid Jet", 1.0, 20, "Slow", 2.5),
        ("alien_anatomy",
         "Alien Anatomy: crowd control durations against the Mi-Go are "
         "reduced by 25%.")),

    # ------------------------------------------------------------------
    # The Shoggoths, the Shapeless Ones — Bruiser
    # ------------------------------------------------------------------
    kit("shoggoths", "The Shoggoths, the Shapeless Ones",
        "Tekeli-li! The mass remembers every shape it ever hated.",
        650, 5.5, "shapeless_flurry",
        spell({"id": "protoplasm_slam", "name": "Protoplasm Slam",
               "flavor": "Several tons of spite arrive at once.",
               "cooldown": 8, "stamina_cost": 30,
               "effect": "aoe_damage", "power": 130, "range": 8}),
        spell({"id": "formless_rush", "name": "Formless Rush",
               "flavor": "It pours itself forward faster than it should.",
               "cooldown": 13, "stamina_cost": 30,
               "effect": "dash", "power": 12, "range": 0}),
        spell({"id": "tekelili", "name": "Tekeli-li!",
               "flavor": "The ancient cry. Even stone wants to run.",
               "cooldown": 50, "stamina_cost": 55,
               "effect": "fear_aura", "power": 3, "range": 14}),
        rc("MeleeHeavy", "Amorphous Crush", 1.5, 3),
        ("amorphous",
         "Amorphous: regenerates 2% of max HP per second while out of "
         "combat.")),

    # ------------------------------------------------------------------
    # Bokrug, the Great Water Lizard — Bruiser (doom)
    # ------------------------------------------------------------------
    kit("bokrug", "Bokrug, the Great Water Lizard",
        "Sarnath fell in a single night. He remembers the address.",
        620, 5.0, "doom_flurry",
        spell({"id": "doomfall_stomp", "name": "Doomfall Stomp",
               "flavor": "The lake-bed cracks; the old doom climbs out.",
               "cooldown": 8, "stamina_cost": 30,
               "effect": "aoe_damage", "power": 120, "range": 8}),
        spell({"id": "curse_of_sarnath", "name": "Curse of Sarnath",
               "flavor": "Marked, as that drowned city was marked.",
               "cooldown": 15, "stamina_cost": 35,
               "effect": "debuff", "power": 5, "range": 12}),
        spell({"id": "wrath_of_the_lizard", "name": "Wrath of the Water-Lizard",
               "flavor": "The waters rise and do not forgive.",
               "cooldown": 55, "stamina_cost": 60,
               "effect": "aoe_damage", "power": 230, "range": 12}),
        rc("MeleeHeavy", "Lizard Rend", 1.3, 3, "Stun", 1.0),
        ("doom_of_sarnath",
         "Doom of Sarnath: deals +30% damage to debuffed targets.")),

    # ------------------------------------------------------------------
    # Nodens, Lord of the Great Abyss — Hunter / skirmisher
    # ------------------------------------------------------------------
    kit("nodens", "Nodens, Lord of the Great Abyss",
        "The abyss does not chase. It simply arrives where you are.",
        520, 6.5, "hunter_flurry",
        spell({"id": "abyssal_harpoon", "name": "Abyssal Harpoon",
               "flavor": "The deep throws one spear. It does not miss twice.",
               "cooldown": 7, "stamina_cost": 25,
               "effect": "projectile", "power": 105, "range": 18}),
        spell({"id": "hunters_wake", "name": "Hunter's Wake",
               "flavor": "The hunt does not pause for distance.",
               "cooldown": 12, "stamina_cost": 30,
               "effect": "dash", "power": 14, "range": 0}),
        spell({"id": "abyss_opens", "name": "The Abyss Opens",
               "flavor": "The seafloor splits; everything slides down.",
               "cooldown": 50, "stamina_cost": 55,
               "effect": "pull", "power": 12, "range": 14}),
        rc("EldritchGrasp", "Abyssal Drag", 1.1, 18, "Root", 1.5),
        ("lord_of_the_hunt",
         "Lord of the Hunt: +15% move speed while pursuing damaged foes.")),

    # ------------------------------------------------------------------
    # Dagon, the Deep Father — Bruiser / summoner
    # ------------------------------------------------------------------
    kit("dagon", "Dagon, the Deep Father",
        "The tide brings his children. The tide always brings them.",
        600, 5.0, "deep_flurry",
        spell({"id": "crushing_maw", "name": "Crushing Maw",
               "flavor": "The reef-teeth close like a harbor gate.",
               "cooldown": 8, "stamina_cost": 30,
               "effect": "aoe_damage", "power": 125, "range": 8}),
        spell({"id": "tidal_surge", "name": "Tidal Surge",
               "flavor": "The sea itself shoves him forward.",
               "cooldown": 14, "stamina_cost": 30,
               "effect": "dash", "power": 12, "range": 0}),
        spell({"id": "rise_deep_ones", "name": "Rise, Deep Ones",
               "flavor": "From the trenches they come, singing.",
               "cooldown": 55, "stamina_cost": 60,
               "effect": "summon", "power": 4, "range": 20}),
        rc("MeleeHeavy", "Father's Wrath", 1.4, 3),
        ("deep_patriarch",
         "Deep Patriarch: nearby summons deal +20% damage.")),

    # ------------------------------------------------------------------
    # Mother Hydra, the Deep Mother — Support / summoner
    # ------------------------------------------------------------------
    kit("mother_hydra", "Mother Hydra, the Deep Mother",
        "She does not fight her wars. She mothers them.",
        580, 5.0, "matriarch_flurry",
        spell({"id": "matriarchs_call", "name": "Matriarch's Call",
               "flavor": "Her voice carries through miles of black water.",
               "cooldown": 10, "stamina_cost": 30,
               "effect": "summon", "power": 2, "range": 16}),
        spell({"id": "brine_blessing", "name": "Brine Blessing",
               "flavor": "The old waters knit flesh back together.",
               "cooldown": 16, "stamina_cost": 35,
               "effect": "heal", "power": 150, "range": 14}),
        spell({"id": "tide_of_the_brood", "name": "Tide of the Brood",
               "flavor": "The ocean stands up and walks inland.",
               "cooldown": 55, "stamina_cost": 60,
               "effect": "summon", "power": 5, "range": 20}),
        rc("AcidSpit", "Brine Spit", 0.8, 20, "Slow", 2.0),
        ("broodmothers_vigor",
         "Broodmother's Vigor: her summons regenerate 1% of max HP per "
         "second.")),

    # ------------------------------------------------------------------
    # Ghatanothoa, Lord of the Volcano — Controller (petrification)
    # ------------------------------------------------------------------
    kit("ghatanothoa", "Ghatanothoa, Lord of the Volcano",
        "Look upon him and be still. Forever is optional.",
        540, 5.0, "volcano_flurry",
        spell({"id": "petrifying_gaze", "name": "Petrifying Gaze",
               "flavor": "Stone is just flesh that stopped arguing.",
               "cooldown": 9, "stamina_cost": 25,
               "effect": "stun", "power": 1.25, "range": 12}),
        spell({"id": "obsidian_shards", "name": "Obsidian Shards",
               "flavor": "The mountain throws its broken teeth.",
               "cooldown": 10, "stamina_cost": 30,
               "effect": "projectile", "power": 110, "range": 16}),
        spell({"id": "behold", "name": "Behold!",
               "flavor": "The gaze made manifest; stone cracks like glass.",
               "cooldown": 55, "stamina_cost": 55,
               "effect": "aoe_damage", "power": 190, "range": 12}),
        rc("MeleeHeavy", "Volcanic Fist", 1.2, 3, "Slow", 2.0),
        ("medusas_lesson",
         "Medusa's Lesson: enemies struck while stunned take +40% damage.")),

    # ------------------------------------------------------------------
    # Nug and Yeb, the Twin Blasphemies — Fighter (duo)
    # ------------------------------------------------------------------
    kit("nug_and_yeb", "Nug and Yeb, the Twin Blasphemies",
        "Two hungers, one shadow. Do not count them twice.",
        560, 6.0, "twin_flurry",
        spell({"id": "twin_fangs", "name": "Twin Fangs",
               "flavor": "Both mouths speak at once. Both bite.",
               "cooldown": 8, "stamina_cost": 25,
               "effect": "projectile", "power": 110, "range": 16}),
        spell({"id": "blasphemous_pact", "name": "Blasphemous Pact",
               "flavor": "The twins agree with each other, loudly.",
               "cooldown": 16, "stamina_cost": 35,
               "effect": "buff", "power": 6, "range": 0}),
        spell({"id": "twins_unite", "name": "The Twins Unite",
               "flavor": "For one moment they are a single appetite.",
               "cooldown": 50, "stamina_cost": 60,
               "effect": "aoe_damage", "power": 220, "range": 10}),
        rc("MeleeHeavy", "Twin Rend", 1.3, 3),
        ("two_bodies",
         "Two Bodies, One Hunger: +15% damage while above 50% HP.")),

    # ------------------------------------------------------------------
    # Rhan-Tegoth, Terror of the Museum — Tank
    # ------------------------------------------------------------------
    kit("rhan_tegoth", "Rhan-Tegoth, Terror of the Museum",
        "The exhibit walked out. The building did not survive.",
        680, 4.5, "crab_flurry",
        spell({"id": "crusher_claw", "name": "Crusher Claw",
               "flavor": "The pincer closes like a vault door.",
               "cooldown": 8, "stamina_cost": 30,
               "effect": "aoe_damage", "power": 135, "range": 8}),
        spell({"id": "chitin_wall", "name": "Chitin Wall",
               "flavor": "Six hundred million years of saying 'no'.",
               "cooldown": 16, "stamina_cost": 35,
               "effect": "shield", "power": 150, "range": 0}),
        spell({"id": "museum_wakes", "name": "The Museum Wakes",
               "flavor": "Every fossil in the hall remembers dying.",
               "cooldown": 50, "stamina_cost": 55,
               "effect": "fear_aura", "power": 2.5, "range": 14}),
        rc("MeleeHeavy", "Fossil Slam", 1.5, 3, "Root", 1.0),
        ("living_fossil",
         "Living Fossil: damage taken reduced by 15%.")),

    # ------------------------------------------------------------------
    # Sghllor, the Colour Out of Space — Artillery (blight / DoT)
    # ------------------------------------------------------------------
    kit("sghllor", "Sghllor, the Colour Out of Space",
        "It fell from the sky and the farm was never right again.",
        480, 5.5, "colour_flurry",
        spell({"id": "blight_ray", "name": "Blight Ray",
               "flavor": "A colour with no name eats the light.",
               "cooldown": 7, "stamina_cost": 25,
               "effect": "projectile", "power": 90, "range": 20}),
        spell({"id": "withering_field", "name": "Withering Field",
               "flavor": "The ground greys; things stop growing right.",
               "cooldown": 15, "stamina_cost": 35,
               "effect": "debuff", "power": 6, "range": 12}),
        spell({"id": "colour_falls", "name": "The Colour Falls",
               "flavor": "The sky opens and pours itself down.",
               "cooldown": 55, "stamina_cost": 60,
               "effect": "aoe_damage", "power": 240, "range": 14}),
        rc("AcidSpit", "Prismatic Beam", 1.1, 24),
        ("blight_zone",
         "Blight Zone: enemies damaged by Sghllor deal -15% damage for 4s.")),

    # ------------------------------------------------------------------
    # Hastur, the King in Yellow — Controller (madness / fear)
    # ------------------------------------------------------------------
    kit("hastur", "Hastur, the King in Yellow",
        "Have you seen the Yellow Sign? You have now.",
        500, 6.0, "yellow_flurry",
        spell({"id": "tattered_shriek", "name": "Tattered Shriek",
               "flavor": "The play's second act, performed at full volume.",
               "cooldown": 8, "stamina_cost": 25,
               "effect": "projectile", "power": 100, "range": 16}),
        spell({"id": "play_continues", "name": "The Play Continues",
               "flavor": "No one leaves before the final bow. No one.",
               "cooldown": 15, "stamina_cost": 35,
               "effect": "fear_aura", "power": 2, "range": 12}),
        spell({"id": "carcosa_manifest", "name": "Carcosa Manifest",
               "flavor": "Black stars rise over a lake that isn't there.",
               "cooldown": 55, "stamina_cost": 60,
               "effect": "aoe_damage", "power": 220, "range": 14}),
        rc("MindControl", "The King's Whisper", 0.8, 20, "Fear", 2.0),
        ("king_in_yellow",
         "The King in Yellow: feared enemies take +25% damage from Hastur.")),

    # ------------------------------------------------------------------
    # Yig, Father of Serpents — Skirmisher (venom / curse)
    # ------------------------------------------------------------------
    kit("yig", "Yig, Father of Serpents",
        "He does not hate. He remembers. That is worse.",
        520, 6.5, "serpent_flurry",
        spell({"id": "serpent_strike", "name": "Serpent Strike",
               "flavor": "Autumn rites end with a single bite.",
               "cooldown": 7, "stamina_cost": 25,
               "effect": "projectile", "power": 100, "range": 16}),
        spell({"id": "coil", "name": "Coil",
               "flavor": "The coils were already around you.",
               "cooldown": 13, "stamina_cost": 30,
               "effect": "dash", "power": 10, "range": 0}),
        spell({"id": "curse_of_yig", "name": "Curse of Yig",
               "flavor": "The mark spreads. The change is already done.",
               "cooldown": 50, "stamina_cost": 55,
               "effect": "aoe_damage", "power": 180, "range": 12}),
        rc("MeleeHeavy", "Father's Bite", 1.2, 3, "Slow", 2.0),
        ("serpents_patience",
         "Serpent's Patience: +20% damage to enemies below 30% HP.")),

    # ------------------------------------------------------------------
    # Bast, Queen of the Cats of Ulthar — Skirmisher (pack hunter)
    # ------------------------------------------------------------------
    kit("bast", "Bast, Queen of the Cats of Ulthar",
        "The cats remember every kindness. And every debt.",
        490, 7.0, "cat_flurry",
        spell({"id": "claw_of_the_sphinx", "name": "Claw of the Sphinx",
               "flavor": "The vessel moves; the desert remembers.",
               "cooldown": 7, "stamina_cost": 25,
               "effect": "aoe_damage", "power": 110, "range": 8}),
        spell({"id": "pounce", "name": "Pounce",
               "flavor": "Nine lives, all of them faster than you.",
               "cooldown": 12, "stamina_cost": 30,
               "effect": "dash", "power": 14, "range": 0}),
        spell({"id": "cats_of_ulthar", "name": "The Cats of Ulthar",
               "flavor": "They come over the rooftops, silent as dusk.",
               "cooldown": 50, "stamina_cost": 60,
               "effect": "summon", "power": 5, "range": 18}),
        rc("MeleeHeavy", "Sphinx Swipe", 1.2, 3),
        ("queen_of_cats",
         "Queen of Cats: +10% move speed; summoned cats prioritize her "
         "target.")),

    # ------------------------------------------------------------------
    # The Elder Mind, Protector of the Yith — Support / controller
    # ------------------------------------------------------------------
    kit("elder_mind", "The Elder Mind, Protector of the Yith",
        "It has read tomorrow. It disapproves of your plans.",
        500, 5.5, "yith_flurry",
        spell({"id": "temporal_lash", "name": "Temporal Lash",
               "flavor": "A yesterday strikes a tomorrow.",
               "cooldown": 8, "stamina_cost": 25,
               "effect": "projectile", "power": 105, "range": 18}),
        spell({"id": "between_seconds", "name": "Between Seconds",
               "flavor": "The Library has a door behind every moment.",
               "cooldown": 14, "stamina_cost": 30,
               "effect": "dash", "power": 14, "range": 0}),
        spell({"id": "library_opens", "name": "The Library Opens",
               "flavor": "Every Yith mind turns the same page at once.",
               "cooldown": 55, "stamina_cost": 55,
               "effect": "buff", "power": 8, "range": 16}),
        rc("MindControl", "Yith Rebuke", 0.9, 22, "Slow", 2.5),
        ("great_race_memory",
         "Great Race Memory: ability cooldowns are 10% shorter "
         "(precognition).")),

    # ------------------------------------------------------------------
    # The Howling Eye, Heart of the Polyp Scourge — Artillery (storm)
    # ------------------------------------------------------------------
    kit("howling_eye", "The Howling Eye, Heart of the Polyp Scourge",
        "The storm never stops screaming. Neither will you.",
        470, 6.5, "storm_flurry",
        spell({"id": "screaming_gale", "name": "Screaming Gale",
               "flavor": "The wind itself has gone mad and it is armed.",
               "cooldown": 8, "stamina_cost": 30,
               "effect": "aoe_damage", "power": 115, "range": 12}),
        spell({"id": "storm_surge", "name": "Storm Surge",
               "flavor": "Ride the scream; arrive before the sound.",
               "cooldown": 14, "stamina_cost": 30,
               "effect": "dash", "power": 16, "range": 0}),
        spell({"id": "eye_of_the_maelstrom", "name": "Eye of the Maelstrom",
               "flavor": "At the center of the storm, the Eye opens.",
               "cooldown": 55, "stamina_cost": 60,
               "effect": "aoe_damage", "power": 230, "range": 16}),
        rc("AcidSpit", "Polyp Shriek", 1.0, 22, "Slow", 2.0),
        ("storm_never_stops",
         "The Storm That Never Stops: +10% damage while moving.")),

    # ------------------------------------------------------------------
    # Hypnos, Lord of Sleep — Controller (dreams / sleep)
    # ------------------------------------------------------------------
    kit("hypnos", "Hypnos, Lord of Sleep",
        "He owns the world behind your eyes. Visit often.",
        460, 6.0, "dream_flurry",
        spell({"id": "dream_pierce", "name": "Dream Pierce",
               "flavor": "A nightmare with a sharp point.",
               "cooldown": 7, "stamina_cost": 25,
               "effect": "projectile", "power": 100, "range": 18}),
        spell({"id": "slumber", "name": "Slumber",
               "flavor": "Sleep now. The dreams are already waiting.",
               "cooldown": 16, "stamina_cost": 35,
               "effect": "stun", "power": 2, "range": 12}),
        spell({"id": "dreamlands_open", "name": "The Dreamlands Open",
               "flavor": "The sky becomes a eyelid, and it closes.",
               "cooldown": 55, "stamina_cost": 55,
               "effect": "aoe_damage", "power": 190, "range": 14}),
        rc("MindControl", "Nightmare Touch", 0.7, 20, "Stun", 2.0),
        ("lord_of_sleep",
         "Lord of Sleep: +20% damage to stunned, feared, or sleeping "
         "enemies. The dream build is real.")),
]



def render(k):
    L = []
    A = L.append
    A(f"# {k['display_name']}.")
    A("# Generated by assets/characters/generate_kits.py (wave 25) — do not hand-edit;")
    A("# edit the KITS table in the generator and re-run.")
    A("#")
    A("# MODEL SLOT: drop model.fbx (or model.glb) + rig.map into this folder to")
    A("# replace the procedural placeholder. See docs/bring_your_own_model.md.")
    A("")
    A(f"id = {k['id']}")
    A(f"display_name = {k['display_name']}")
    A(f"flavor = {k['flavor']}")
    A(f"max_hp = {k['max_hp']}")
    A(f"move_speed = {k['move_speed']}")
    A(f"max_stamina = {k['max_stamina']}")
    A(f"melee_combo = {k['melee_combo']}")
    A("")
    for slot in ("q", "f", "r"):
        s = k[slot]
        A(f"[{slot}]")
        A(f"id = {s['id']}")
        A(f"name = {s['name']}")
        A(f"flavor = {s['flavor']}")
        A(f"cooldown = {s['cooldown']}")
        A(f"stamina_cost = {s['stamina_cost']}")
        A("mana_cost = 0")
        A(f"effect = {s['effect']}")
        A(f"power = {s['power']}")
        A(f"range = {s['range']}")
        A("")
    r = k["rightclick"]
    A("[rightclick]")
    A(f"kind = {r['kind']}")
    A(f"name = {r['name']}")
    A(f"damage_mult = {r['damage_mult']}")
    A(f"range = {r['range']}")
    if r["cc"]:
        A(f"cc = {r['cc']}")
        A(f"cc_seconds = {r['cc_seconds']}")
    A("")
    p = k["passive"]
    A("[passive]")
    A(f"id = {p[0]}")
    A(f"desc = {p[1]}")
    A("")
    return "\n".join(L)


def main():
    made = 0
    for k in KITS:
        folder = os.path.join(ROOT, k["id"])
        os.makedirs(folder, exist_ok=True)
        path = os.path.join(folder, "character.def")
        with open(path, "w") as fh:
            fh.write(render(k))
        made += 1
    print(f"wrote {made} character.def files")


if __name__ == "__main__":
    main()
