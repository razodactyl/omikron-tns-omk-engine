# 6. Actors

← [Contents](README.md) · prev: [The world](05-the-world.md) · next: [Conversations and cutscenes](07-conversations-and-cutscenes.md)

---

## In short

Every character in the game — the one you steer and the ones you do not — is
driven by the same machine: a **state graph that shipped on the disc**.

A `.CTL` file is a list of states, each naming an animation, and a list of
transitions between them, each naming the buttons that take it. Press forward
and the graph moves from standing to walking, because an author drew that edge.
Press the action button next to an object and the graph walks its chain of
states for the same reason; press it beside a parked slider and a different arm
of the same handler opens the door and seats you.

So "the player draws a gun", "the player dives" and "the player takes a step"
are the same kind of event, and the fight and shoot modes are not separate
engines. They are different **control schemes** feeding the same graph — and
different things pressing the buttons: in a fight, the opponent's AI presses
combinations into his own input queue exactly as your keyboard presses into
yours; in a shoot phase, each gunman has a small brain that walks the level's
navigation grid.

Around that sits a walker that decides whether a step is possible — how steep
is too steep, how high a ledge you can climb, what happens when you jump or
fall, when the floor is water — and, in the cities, a crowd: pedestrians and
vehicles following a circuit authored as lanes and routes, as dense as the
options menu asks and by the engine's own rule.

One thing to carry through the whole chapter: **none of this has an oracle.**
The trace rig that checks the port against the original (chapter 12) cannot
see the actor runtime at all, so everything here is judged against the data and
against what a person sees when they play it — never against the original's
own output.

## In detail

### The `.CTL` channel

Seven files, and all seven parse to the byte: the walk lands exactly on the
file size, 398 clips, and **every one of 2 044 graph edges resolves** — the
loader refuses to start otherwise, which is what makes that a test the data
could fail (`CLAUDE.md` §4; `verify.py: ctl groups`, `ctl transitions`). A
`.CTL` is a saved memory image whose pointers are stale authoring addresses;
the loader never follows them, it walks the file in a fixed order and
overwrites each one (`docs/ASSETS.md` §7, ".CTL").

A state's entry carries flags, and every flag-gated block has its traced
consumer (`docs/ASSETS.md` §7; `verify.py: ctl flag blocks`):

| block | what it is |
|---|---|
| the combat block (`0x2000000`) | damage, the hit window, the reaction keyed by the low 16 bits of an entry id, and knockback — consumed by `Fight_ResolveHit`, present only in the three combat files; all 232 reaction references resolve (`ctl combat block`) |
| turn and root-shift (`0x140`, `0x280`) | each with an over-the-window mode and an on-transition mode — the two bits of each pair |
| the move name (`0x10`) | resolves into the executable's own 66-row `tab_special_move[]` of engine callbacks; all 54 distinct names at 209 shipped sites resolve (`ctl special moves`) |
| bit `0x20` | the group's default entry — exactly one in each of the 202 groups |
| `0x8002` / bit 2 | no clip of its own; bit 2 redirects through the entry's GoTo chain |

The `+28` sub-records are the states' **effect records**: bone-attached sprites
and frame-triggered sounds, footsteps among them. All 590 decode
(`ctl effects`). A sound id there is an *id*, searched in the resident scene,
not an index — so a state's footstep is silent in a scene that does not carry
it, which is the engine's own behaviour (`docs/ASSETS.md` §7).

Transitions are matched against the current input bitfield, with cancel
windows, priorities and group-global edges. How a press gets *into* the queue
is read to the instruction — a lone idle word is dropped so the first press of
a bank acts on the tick it arrives, one flag cuts the queue to a single word, a
push dedupes against the back — and an entry's GoTo can be **rewritten at
run time** into a dynamic return edge, which is the only way two `Sham.CTL`
entries are reachable at all (`docs/ASSETS.md` §7, the walker section).

The port runs all 202 groups of the seven banks, landing in 34 056 states and
re-deriving all **12 063 edges** from the file (`docs/ASSETS.md` §7,
`ACTOR_STATE`; `verify.py: engine: actor states`). Its standard is
**data-constrained, not engine-verified**, and the reason is structural rather
than a gap in effort: see *What is not settled*.

Some special moves are the **most consequential code in the actor runtime**,
because they are where a data file hands control to the engine: `MDACTION` is
the action button (the take, the slider door, a talk), `MDJUMP0A..03` the jump,
and `MDDIVEND`, `MDSW2SD`, `RSTAVNT` and `RSTNAGE` move a swimmer between
states. None of the last four is in the decompilation — nothing calls them
directly, they are dwords in a table — so their addresses were fixed by
aligning each block onto the next table row (`docs/ASSETS.md` §7).

### `ACTOR_STATE`

Eighteen states, 0..17, mapped **and run** (`docs/ASSETS.md` §7, the
`ACTOR_STATE` table). Several repay the reading:

* **11 is entering the water** — it was read as "the ladder" from the
  decompiler's own comment until 2026-09-17, and the floor that triggers it is
  the canal's bed; **14 is underwater**. Special moves write both.
* **7 and 8 are the mount and the ride of one slider**, and the dismount move
  refuses to leave anything but 8 — "bad mode getting out of the slider !" is
  its own error string. State 7 has **no case at all** in the per-frame actor
  tick; the slider's own tick drives the actor there.
* **2 is melee**, and `Fight_Engage` is its only writer — through an alias a
  search for the obvious store misses. A state the dispatch names with no
  writer is the tell that the *enumeration* is short, not the data.
* A placed NPC has **no** `.CTL` machine at all: every caller of the bank
  loader is the player, so a spawned character sits inert in state 0 until a
  scene program, the shoot AI or a fight owns his body (`docs/STREET_LIFE.md`
  §0).

### The walker

A step is refused when the face is steeper than **30°**, the rise higher than
**30 cm** (`dword_910340`, 11.81 world units), or the mesh carries the
"you may not step onto this" bit `0x20000000` — three arms, not two
(`docs/ASSETS.md` §7). A drop under **20 cm** is taken silently; anything
larger goes through the fall path. The game's own staircases, measured, have
risers of 6.17 to 8.30 units, so no staircase can be refused for its height
(`docs/ASSETS.md` §7; `verify.py: engine: stairs`).

The world unit is an **inch** — the engine tells the audio listener so
(`docs/ASSETS.md` §3c) — and the studio still authored in metres, which is why
the constants are round numbers in one system and not the other.

**The forward motion comes from the clip.** `Anim_RootDelta` sums the root's
position keys over the frames advanced and turns the sum by the actor's facing
matrix; the walker then undoes that delta and hands it to the collide-and-slide
— *the clip proposes and the collision disposes*. Turning is the `.CTL` too: a
held left or right is a 5°-a-frame turn edge (`docs/ASSETS.md` §7).

**The narrow phase is partly a reconstruction.** The engine's own sweep
(`Sweep_ActorMove` over the 930-line `Sweep_PolygonKernel`) is deliberately
not transcribed. The port's version — three passes, the model's own collision
spheres, one unit short of the contact, the remainder slid along the wall —
stops the player 13.0 units in front of a wall (`verify.py: engine: narrow
phase`; `todo/next-tasks.md`, the bank stairs). Two parts of it are labelled
reconstructions rather than readings:

* **the sweep starts a step-height above the feet**, because the ground probe
  already claims everything inside that window — without it the lowest sphere
  met every riser and the player stopped one step short of a bank's door;
* **the sweep meets edges and corners**, only from outside. The first stand-in
  tested a triangle's interior only, and on the security centre's handrails —
  8-unit bars at waist height — a player ran through the rail and down the
  shaft: *"not possible in the original game"*. A first fix that also counted
  an edge the sphere already overlapped threw a player round a bar stool
  (`docs/ASSETS.md` §7; `verify.py: engine: security rail`).

**The jump.** `Actor_MoveBy` applies X and Z only, so the vertical is the
walker's alone, and a walk that floated turned out to be the port's own mix of
two origins rather than anything in the clips — their pelvis drop and leg lift
cancel to 0.03 (`CLAUDE.md` §4; `todo/player-vertical.md` §4). The jump is
`MDJUMP0A` and `MDJUMP01`: 2.5 m forward over a number of frames the entry
carries, with a vertical impulse that brings him back down at the end —
measured at 14 frames and 22.9 cm of lift, a flat leap rather than a hop. A
global turns the ground probe's absorption off for its duration
(`todo/player-vertical.md` §5; `verify.py: engine: player jump`).

**The fall.** Stepping off a ledge is banded once per fall by the clearance
below — 1.5, 3 and 5 m — into the falling group and a camera request; the
landing by the distance fallen: back to walking, a stumble, the stumble and
**message 10**, or lying flat with **message 11** and an overhead camera. The
"fall states" a first reading found were camera requests. A vehicle still fast
when it touches a player on the road raises **message 17**, and Anekbah's
handler knocks him flat. And every script that costs health outside a fight or
a shoot phase floors it at 5, so **nothing in adventure mode kills the
player** — there is no adventure death to port (`docs/ASSETS.md` §7, "The fall
reaction"; `todo/falls.md`; `verify.py: engine: fall reaction`, `engine: run
over`).

### The water

`Actor_ApplyMotion` hands states 11..14 to `sub_4A8F30` instead of gravity. In
Jaunpur, where it was settled from the loader, mesh flag `0x8000000` is the
canal's bed and banks and `0x20000000` the surface. The surface holds a
swimmer 11.81 under it; underwater he drifts up over the bed with his pitch
turning and clamped, and a **40-second breath** runs from the first underwater
tick, drawn as the horizontal gauge of `Hud_DrawBar` — the one mode of it the
port had never needed — ending in **message 12**, which the city's own script
answers with a drowning and a rescue to the bank (`docs/ASSETS.md` §7, "The
water").

Underwater the **dive** key swims: the graph's edge from treading to swimming
matches the input bit that, in the swimming control scheme, is *Plonger*. The
body is drawn through the actor's whole Euler rather than its yaw, so the swim
pitch lies him down. **Played and confirmed** on 2026-09-17: nine reports,
each a port fault that a green check had passed (`todo/swimming.md` §8;
`verify.py: engine: water entry`).

### The slider

Kay'l's sneak can **call a slider** — a hover taxi — and the whole ride is
ported and was **played to the end and confirmed** by a reader on 2026-09-08
(*"ok, it works"*; `todo/slider.md`, "PLAYED AND CONFIRMED"):

* the call finds a **vehicle lane** of the city's traffic circuit, and the
  mover starts at that lane's origin set back 39 units, the body 30.75 under it
  — the ride's hover height;
* it drives in on its own camera (mode 8), and within **117 units** (2.97 m) of
  the pickup point — measured against the lane point, which is why it stops on
  the road — hands the camera back to the player;
* **boarding** is the action button's slider arm: a dot product against the
  slider's side and a 4.00 m reach, then the door clip — bound by name to the
  cockpit's doors — and he is seated;
* the ride's **flight model** steers at ±5° a frame on the interface's own input
  bits, banks at 1.75° a frame to a hard 11°, thrusts from a six-value ladder
  and drags quadratically; every state of the ride's machine watches at a field
  of view of **90** against the 75 of every other camera (`todo/slider.md`).

The destination is an **address**, possibly in another area: the engine loads
the new city, relinks a vehicle at the lane nearest the destination, and only
the drive itself is skipped under the load's fade (`todo/slider.md`;
`verify.py: engine: slider journey`, `engine: slider journey area`).

### Melee

A fight is `fight.begin` (op 62) from the chunk's own script. Its third operand
is the **AI level**, and every one of the 108 shipped sites sits in a `case` on
the variable *Niveau Combat* as three copies of the call, one per level 0, 1
and 2 — so the fourth profile, a sparring partner, is unreachable
(`todo/fight-mode.md` §2). The level is **adaptive**: the scripts raise or
lower it by how much life your last fight cost (chapter 4). The options menu's
*Difficulté des combats* is a different thing — a flat bonus on the player's
dodge multiplier.

`Fight_Begin` builds two combat contexts from the fighters' properties — life,
attack, dodge, and fight experience, which scales the channel rate so an
experienced fighter literally animates faster — loads melee's own sprite and
sound library `fight.SCX`, and installs control scheme 3 (`docs/ASSETS.md` §7,
the combat block; `todo/fight-mode.md` §3). Every frame runs both fighters'
steps, the knock-out trigger, two distance-gated bank switches at 3 m and
1.5 m, the separation push, and the hit resolution both ways, throws included.

**The AI presses buttons.** Four profiles of combinations live in each `.CTL`,
and each slot also carries a **percentage** that every choice rolls against;
the AI also presses **eight moves compiled into the executable**
(`tables/fight_ai_moves.json`), which is why porting it found content no data
file holds. The harder the level, the shorter the wait. Its defensive arm —
raising and dropping the guard — is ported (`todo/fight-mode.md` 15.11).

**Its dice are now the engine's generator.** Until 2026-09-24 the port's AI
rolled the host's `std::rand()`, which is neither the original's generator nor
private to the port: something else in the process drew from it once,
sometimes, and a headless fight came out two ways. It now draws from the
port's private copy of the CRT generator, the stream the gunmen already used,
so a fight is one result on every run and every host; six fight checks were
re-baselined, each first shown green on the old dice (commit `f11e058`;
`todo/fight-mode.md` §8).

The **priority gate** is read and ported: the player's threshold is his
experience scaled by 0.024390243 and truncated, so combat moves marked
priority 1 or 2 **unlock with Kay'l's rank** (`todo/fight-mode.md` 15.14).

The camera is **mode 14**, computed rather than authored, and the options
menu's fight-camera row chooses between two camera rigs. A knock-out is a
**60-frame replay shown twice** from two further angles, between the
letterbox bands. At the end the engine releases the parked script and drops
the result code; the player leaves every fight in state 1, win or lose, and the
**script** decides what a loss means by reading the life written back
(`todo/fight-mode.md` §4, §5, 15.12, 15.13).

All of it runs in the port (`verify.py: engine: melee` — 3 banks, 9 profiles,
9 probe fights) and was **played and confirmed** through one lost fight and two
won ones, the wins with the `--fight-health 200` harness
(`todo/handoff-fight-mode.md` §8).

### Shoot mode

A first-person mode: `Shoot_Enter` hides the player's own body, and the eye
sits at **0.7 × a height taken from the model's body-sphere table** — which
lands within 0.9% of the crown, not at the pelvis the camera preset's zero
offset would give. That rule was read from the original after three guessed
heights (`docs/RECONSTRUCTION.md`, 2026-09-10).

**The gunmen think with the map.** Each gunman's brain converts his position
into a cell of his floor's `MAP2D` grid, and the generic brain — all sixteen
states of `sub_424DE0`, transcribed — navigates, engages, patrols and retreats
over it. An ordinary gunman *sees* by walking a line across the grid; the
engage's sight, a spectre and a cross-floor watcher cast a ray. How far he can
shoot is a **character property**, not a weapon field (`docs/ASSETS.md` §7,
"What a gunman can SEE"). A patrol is a route written into the map file, and
the way to another floor is one hop to the nearest **link** — a staircase
written once per direction; there is no path search anywhere
(`todo/handoff-shoot-mode.md` §4).

The brain is picked by character type from the executable's own 14-name type
table: the generic one, Astaroth's, Gandhar's — three compiled behaviour
scripts, healthy, wounded and critical — and X-Tech's; a fifth callback serves
a type no character has. Over the 306 resolved entry sites the split is **302
generic, 3 Astaroth, 1 Gandhar, 0 X-Tech** (`docs/ASSETS.md` §7, the shoot AI).

**A shot is a projectile.** Each actor has four weapon slots drawing on one
projectile pool; a bolt flies, hits a wall or a body, and damages in a fixed
order (`docs/ASSETS.md` §7, "What a shot IS"). The player's shot, the gunmen's
fire and aim, their fall when killed, their walk and steering, the push, the
hurt reaction, the HUD with its radar and the **player's death** are all ported.

The state of each is recorded in `todo/handoff-shoot-mode.md` §2–3b, and it is
mixed. **Confirmed in play**: the entry, camera, mouse look, eye height and
move; the arm and gun; the HUD and radar; the phase's end and the return; the
robbers' fall and held height; a push that cannot carry you through a wall.
The patrol has been **watched**. **Measured only, not yet played**: dying, the
robbers' steering, the actions' advance-then-stop, the hurt reaction and the
gunmen's sight; the nav edge is proved at unit level and not watched.

### The street

Three mechanisms, all ported and all drawn (`docs/STREET_LIFE.md`):

**The `.OPT` traffic circuit.** Seven blocks, six of six files exact
(`verify.py: opt tracks`). Pedestrians are spawned by `Slider_Init` every
`39 × (5 − density) × h[3]` units along each lane, `h[3]` being the file's
pedestrian spacing, and walked by `Sliders_Tick` over lanes and routes, with
following, overtaking, reservation groups and action points (`docs/STREET_LIFE.md`
§2; `verify.py: engine: pedestrians`). The density is options row 6, default 3.
**The port draws the same crowd the original did** — the same rule on the same
circuit and models — so a street's cost in the port is not extra detail
(`docs/STREET_LIFE.md` §2; `todo/optimization.md`, the 2026-09-29 correction
to step 7).

**The road traffic** rides the same circuit's vehicle lanes, behind two masks in
the AREA chunk that are non-zero in exactly the three areas that have such
lanes. Vehicles spawn at `39 × h[4]` with **no** density factor, capped by the
40-slot ride pool. They share a pool with the walkers because they share the
**reservation groups**: 70 of Anekbah's groups are reached by both classes,
and a vehicle waits on a walker 2 197 times in 1 800 frames
(`docs/STREET_LIFE.md` §2b; `verify.py: engine: road traffic`).

**The authored extras** are ordinary scene programs — 621 `scx.play.actor`
sites in AREA startup scripts — plus the spatial index's push, the bump and
talk messages, and the head look that turns an NPC's head toward you, clamped
to ±40° of pitch and ±70° of yaw (`docs/STREET_LIFE.md` §1, §3 and "The head
look"; `verify.py: engine: city crowd`, `engine: crowd push`, `engine: head
look`). The voices the talk scripts play are among the voice-over files that do
not ship, so the talk runs and stays silent here.

The moving population is **lit** by the `.3DO` light table the set supplies —
all 446 of a walker model's vertices ship pure white, so those lights are its
whole illumination — and casts the engine's blob **shadows** (chapter 8;
`docs/STREET_LIFE.md` §2, "The crowd is LIT").

<p align="center">
  <img src="images/anekbah-street.png" width="560" alt="Anekbah's street with its crowd">
  <br><em>The crowd, the traffic, the ambient fire, the set's baked lights and<br>the characters' shadows, drawn by the port in adventure mode.</em>
</p>

### How the port poses the bodies

None of this is a finding about the game; it is how the port spends its frame
on the bodies above, and each item was held to *the picture does not change*.

* **The crowd is posed over several cores by default** since 2026-09-23: the
  walkers' pose pass is split into a serial resolve, parallel bodies and a
  merge. The frame is byte-identical either way, and the console's own bench
  measured the threads *EXACT* at 2.71× on three runners. The staged bodies are
  still serial (`todo/handoff-vita-port.md`, P4; `verify.py: engine: threaded
  bodies`).
* **A staged body beyond the clip distance is not skinned** — its program,
  placement and facing still run; only the skinning, lighting and upload are
  skipped, the way the engine skins only what its visible set draws. At the
  default 200 m clip, Anekbah's street start holds 9 of 25 extras beyond it,
  and a 700-frame software render is byte-identical with and without the skip
  (`todo/vita-port.md`, the entry "A staged body beyond the clip distance").
* **On the GLES backend — the Vita's — the walkers and the staged bodies are
  posed and lit in the vertex shader**: a character is rigid per mesh, so the
  GPU needs its rest geometry once and one matrix per mesh per frame. The
  software reference and Vulkan keep posing on the CPU. Two exceptions stay on
  the CPU: a speaker whose line is morphing his face, and **the player, whose
  step is not done** (`todo/gpu-skinning.md`, steps 1–5).

### A misnamed function, and why it is in this chapter

`tools/renames.json` is a map of hypotheses, and one of them had its sense
backwards. `Perso_SetInputEnabled` reads as an enable and is a **block**: flag
`0x80` makes the channel tick *skip* the input search, argument 1 sets it and
argument 0 clears it. The port trusted the name, asserted the flag on the way
out of every conversation, and the player's action button was dead from the
first conversation onward (`docs/ASSETS.md` §7; `CLAUDE.md` §1).

The rule that follows is the chapter's, not an aside: **before a rename decides
a behaviour, read the handler rather than the map.** A boolean argument is
where this bites hardest, because a wrong name inverts it silently and both
values look plausible at the call site.

## Where it lives

| | |
|---|---|
| the findings | `docs/ASSETS.md` §7 (the `.CTL` format, the walker, the fall, `ACTOR_STATE`, the water, the fight and shoot AI, the projectile pool), `docs/STREET_LIFE.md`, `docs/FILE_FORMATS.md` §5b5 (`MAP2D`) |
| the plans and records | `todo/fight-mode.md`, `todo/handoff-fight-mode.md`, `todo/handoff-shoot-mode.md`, `todo/slider.md`, `todo/swimming.md`, `todo/falls.md`, `todo/player-vertical.md`, `todo/gpu-skinning.md`, `todo/handoff-vita-port.md` |
| the port | `engine/src/actor/` — `channel.*`, `state.*`, `walk.*`, `player.*`, `moves.*`, `fight.*`, `shoot*.*`, `projectile.*`, `slider.*`, `sliders.h` + `pedestrians.cpp`, `vehicles.cpp`, `spatial.*`, `pose.*`; `engine/src/o3de/collision.cpp` (the narrow phase), `engine/src/formats/opt.*`, `engine/src/formats/map2d.h`, `engine/src/platform/threads.h`, `engine/backends/gles/glesrender.cpp` |
| the tables | `tables/special_moves.json`, `shoot_ai.json`, `shoot_weapons.json`, `fight_ai_moves.json`, `key_bindings.json` |
| the checks | `ctl groups`, `ctl transitions`, `ctl flag blocks`, `ctl combat block`, `ctl effects`, `engine: actor states`, `engine: narrow phase`, `engine: security rail`, `engine: stairs`, `engine: player jump`, `engine: fall reaction`, `engine: run over`, `engine: water entry`, `engine: slider ride`, `engine: slider journey`, `engine: melee`, `engine: fight AI`, `engine: shoot brain`, `engine: shoot death`, `engine: shoot hit`, `engine: pedestrians`, `engine: road traffic`, `engine: city crowd`, `engine: crowd push`, `engine: head look`, `engine: threaded bodies`, `engine: gles pose` — 484 checks in all, 257 of them behind `--slow` (`python3 tools/verify.py --list`, 2026-09-29) |

## What is not settled

* **The actor runtime has no oracle and cannot have one from this rig.** The
  trace logger sees only what a VM handler narrates; `fight.begin` announces
  nothing and `player.become` announces to a domain the logger filters out. A
  capture *did* reach combat — 32 of its anchored scripts carry `fight.begin` —
  so the silence is the mechanism, not the play (`docs/ASSETS.md` §7;
  `todo/fight-mode.md`). The `.CTL` channel, melee and shoot mode are
  therefore **data-constrained**, and a fight's outcome has no oracle.
* **The priority rule cannot be decided by the corpus**: it is
  indistinguishable from a first-match rule on all 120 contested decisions
  (`docs/ASSETS.md` §7), and the ported gate has never been *seen* refusing a
  move — nothing pressed in any run reaches a priority-2 move
  (`todo/fight-mode.md` 15.14).
* **Melee played but not judged**: the fight opening 1.5 m apart, the robber's
  wall collision, the camera held in front of crates, the stat card, the AI's
  guard and the KO replay between bars were played without a verdict; one
  reported "colliders issue" is still to be attributed
  (`todo/handoff-fight-mode.md` §8).
* **Shoot mode's measured-only items** — dying, the steering, the actions, the
  hurt reaction and the gunmen's sight — have not been played, and the nav
  edge has not been watched in a room; AREA 232, the one arena that would show
  it, cannot be walked in (`todo/handoff-shoot-mode.md` §3, §3b).
* **`engine: shoot hit` is red on purpose.** Its empty lists are a behavioural
  result rather than a parse artefact, and baselining them would be worse than
  the red (`todo/sweep-log.md`; `todo/shoot-patrol.md`).
* **The released spectres' attack is unresolved** and parked after every
  data-side lead closed; the close attack (dogs bite, robbers strike) is ported
  beside it (`todo/released-spectres.md`).
* **The body sweep's edges and its start height** are reconstructions;
  reading `Sweep_ActorMove` would settle both. The step refusal's third arm
  (mesh bit `0x20000000`) is **not** applied to the walkable floor, because
  doing it faithfully means carrying the flag per triangle — dropped per mesh it
  would take 3 606 of Lahoreh's 17 658 floor triangles away (`docs/ASSETS.md`
  §7).
* **The footstep latch's re-arm** on a looping clip is a reconstruction: what
  the engine's tail reduces to for open-window records is not traced
  (`docs/ASSETS.md` §7, the effect records).
* **Water state 13, the surface, has not been reached by a run**, and whether
  the engine keeps a placed body standing on a water surface is unread
  (`docs/ASSETS.md` §7, "The water").
* **The crowd's pace, facing over a turn, following distance and push** are
  still to be watched against the original by a person
  (`docs/STREET_LIFE.md` §2, "Still to be watched").
* **The GPU-posed bodies have no `verify.py` check**: the GLES window a check
  must not open is what they need, so their proof is recorded pixel counts on
  the Mac, and `engine: gles pose` covers only the arithmetic through a
  windowless probe (`todo/gpu-skinning.md` steps 3–4). The far-body skip and
  the other console-side savings had not been seen on a console when recorded
  (`todo/handoff-vita-port.md`).
* **An NPC's shadow at detail level 0** is an open question, found and not
  changed: the port's `shadowBonesFor(-1)` returns no bones, while its own
  comment reads the engine's switch as letting −1 fall to the chest — and an
  NPC takes the level minus one. The engine's switch decides it and has not
  been re-read (`todo/optimization.md` step 18; chapter 8).
