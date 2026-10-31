# Send troops across the Free Roam world

1. Hold **ALT** and drag over the followers you want to send.
2. Click **Prepare for battle**. A gold **M — Map / Send prepared** prompt appears above the lower-right abilities. You can also click that prompt while using the ALT cursor.
3. Release ALT and press **M**. The map supplies its own mouse cursor.
4. Click a region, then choose **Explore region**, **Hunt monsters**, or **Raid city**.
5. Press **M**, **Esc**, or **Close** to return to play. Esc closes this map first; it does not leave Free Roam.

The map covers the existing 3.6 km world in 36 regions. Height shading follows the generated terrain. Gold markers show city structures, teal shows followers and temples, red shows wild monsters, C marks caves, and white marks your entity. Region details show current city structures, monsters and friendly followers. Orders and rejection messages appear inside the map.

Troops physically march; distant orders do not teleport troops or award imaginary off-screen kills. Explore sends the group to patrol the region and fight nearby threats. Groups defend themselves against nearby monsters and wartime enemies encountered on the journey, while retaining their regional assignment. Hunt actively seeks uncaptured wild monsters in that region, using the existing physical melee attacks and real damage. Raid selects an actual living enemy city and uses the existing structure-damage behavior. Raiding a cult-owned city declares war after troops are dispatched. A region without a living enemy city cannot produce a phantom raid.

Only the prepared group receives these orders, irrespective of loyalty, as with existing combat commands. To send another group, close the map, ALT-select other followers, prepare them, and choose another region. Earlier groups keep their assignments. An explicit new order redirects the selected group; **Global retreat** recalls all groups. Changing which troops are prepared does not recall existing expeditions.

Journeys take real time at follower marching speed. Coarse terrain routing chooses paths around steep ridges; native collision and ledge checks still govern each step. This is not a guarantee that every dynamically obstructed journey will complete unattended. If a group is blocked, redirect it through a neighboring region or recall it. Hunting and exploration persist until reassigned; this pass does not add fog of war or a separate expedition reward system.

Orders and their destinations use the existing save format and survive save/load. Terrain routes are rebuilt when needed after loading. In co-op, the server validates the prepared group and map bounds; these specific regional orders can exceed the usual 300m local order range. The cult and its preparation selection remain shared between players. A new two-machine/network playtest was not part of this change.

Beliefs and this troop map are available only in Free Roam. Domination and DOTGOO retain their existing controls and rules.

## Verification

The canonical `CultUlhuEditor Win64 Development` target was compiled after project validation. Native tests exercise actual M and mouse input, all 36 map coordinate round-trips, action button bounds, dispatch beyond 300m, selected-group isolation, physical marching, independent groups, a real monster kill, real-city raid dispatch, invalid-world rejection, global retreat, and save/load of an expedition activity and destination. Existing army command and constrained-view belief/ALT UI checks also pass. See [the verification record](RegionExpeditionsResults_2026-10-31.json). Local screenshots and build/test logs remain under `Saved/`; they are not included in the source publication.
