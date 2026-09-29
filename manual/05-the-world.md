# 5. The world

← [Contents](README.md) · prev: [The script VM](04-the-script-vm.md) · next: [Actors](06-actors.md)

---

## In short

The world is a set of **places**, and a place is a chunk in an archive: its
set, its characters, its props, its scripts, its shop stock, and the invisible
boxes that make things happen when you walk into them.

Four ideas carry the whole chapter.

**A place runs a script the moment it loads.** Nothing has to name it — arriving
*is* the trigger. That is how a new game starts talking to you.

**Everything else is a trigger zone.** A quad on the floor, plus an arc saying
which way you have to be facing, plus up to three scripts: one for entering,
one for pressing the action button inside it, one for leaving. Each zone has
one bit in the saved game, which is how a one-shot stays shot. A zone has a
height too, though not where you would look for it.

**Two places are loaded at once.** When you walk out of a street into a
building, the street is not thrown away — it stays resident in the other slot,
its animations still running, and walking back is not a reload.

**Hidden is not gone.** A place that is loaded but not shown is still solid:
its walls still stop you, and a step onto its floor is undone. A whole
building in the game is laid out around that.

Under all of it sits one 8 192-byte block of memory that is both the live game
state and the save file, and a calendar of thirteen 41-day months.

## In detail

### Areas and scenes

`IAM\AREA` and `IAM\SCENE` are archives of chunks. An AREA chunk is a location:
its header names the set (`+88`), the scene scripts (`+97`), the 2D map
(`+106`), the traffic circuit (`+115`), an animation library (`+124`), a sky
model (`+133`) and a music track (`+142`) (`docs/FILE_FORMATS.md`, "The AREA
header's asset manifest"); its `+8` is the shop stock, sixteen object ids
ending at `0xFFFF` (`docs/UI.md`, "…its rows are the AREA's STOCK"). A SCENE
chunk is a *layer* loaded over an area — a cutscene's cast and its own zones —
and `scene.load` brings one in without disturbing the area under it.

Both kinds carry the same nine tables, and **AREA's offsets are SCENE's plus 32
throughout**, except the shared `+4`: objects, props, zones, prop assets,
characters, addresses, world cameras and message subscriptions. With the
strides taken from the loaders, the tables abut with 0 overlaps in 330 chunks,
and every byte of every chunk is claimed but three unreferenced runs of
bytecode, 97 bytes in all (FILE_FORMATS, "Every byte of an AREA / SCENE chunk,
accounted for"; `verify.py: chunk accounting`).

Both carry a **startup script at `+4`**. `Area_TickLoad` hands it to
`Script_NewContext` and queues it the moment the chunk loads, so entering the
place is what runs it. **173 of the 330 chunks carry one** — 117 of 259 AREA,
56 of 71 SCENE — and all 173 disassemble clean, 1 968 instructions in total
(FILE_FORMATS, "`AREA +4` / `SCENE +4`"; `verify.py: startup scripts`).

That field is worth a paragraph because of how long it stayed hidden. The
question "what starts a cutscene's beats?" was open while every route that
*was* checked came back empty — and the enumeration was the problem, not the
reasoning: the 5 785 script slots come from the zone records and the message
subscriptions, and nothing in that walk reaches `+4`. So "no shipped script
starts them" really meant "no script I enumerate". The golden trace had been
announcing scenes no slot could emit the whole time (`CLAUDE.md` §6).

### The trigger zones

68 bytes, and the whole surface through which the world reacts to you
(FILE_FORMATS §5b2b, *solved, both ways*):

```
+0/+4/+8  three script slots — enter, activate, leave
+12       four corners × {int32 x, y, z} — the quad on the floor
+60       facing-arc centre  } 4096ths of a turn on disk; the loader
+62       facing-arc width   } converts both to degrees as it relocates
+64       int16 id — one save-game bit each; bit 15 is a flag
+66       int16 world camera, -1 = none
```

`Zones_RegisterAll` puts each into a sweep-and-prune index, and only zones
whose save bit is set are registered — which is how `zone.disable` retires a
one-shot permanently. Every frame the actor scan tests containment and raises
event 8 on touch, and event 7 — the 16-slot "what can I press the button on"
table — when the facing matches too. A zero arc width accepts any facing.

The three scripts run through one shared context as a small action queue: the
enter script when the zone arms, the activate script on the button (conversation
launches live in this slot), the leave script and a free on leaving (§5b2c).
A press nothing handles posts **message 26**, the game's "nothing here"
(SCRIPT_VM, "The unconsumed press"). A second press is refused only while the
first activate is still running; the **one-shot bit**, carried by 37 of the
zones, latches the slot out of the press cycle until the player leaves rather
than freeing it early (SCRIPT_VM, "The 16 prompt slots"; `verify.py: engine:
zone pump`).

**The camera is asked for on touch, before the facing test.** 54 zones carry
one, over 40 distinct camera ids, and walking into one forces it: the game's
walk-into-a-room auto-cut (§5b2b).

**The containment test has no height, and the scan does.** `Zone_ContainsPoint`
(0x0048C880) takes a y and never reads it; the filter is the iterator, which
builds its search box around the actor in all three axes, so only zones at his
height are yielded. The security centre's lift shows why it matters: one pair
of lift zones per level, stacked over a single footprint up the shaft.
Scanning by the quad alone armed every level's lift at once, five scripts
parked on five copies of the lift screen, and the one answer went to the wrong
one. The port bands by the quad's own height plus a metre — **a labelled
reconstruction**, since the engine's box size comes from a field that is
unread; stacked zones sit a median 389.7 units apart, so the choice decides
nothing shipped (`docs/UI.md` §3f, "…why it never arrived"; `verify.py:
engine: lift`).

**Verified**: 4 558 zones, 0 invalid script offsets, 0 arcs past 4096, 0
duplicate ids (`verify.py: zone records`).

### Messages

Scripts also subscribe to **messages**, which is how one script tells another
that something happened without either knowing about the other. The shipped
data carries **154 subscriptions, 138 with a script**, over message ids 0..32
(FILE_FORMATS §5b3; `verify.py: message tables`). A posted message is searched
for **scene, then area, then `IAM\GLOBAL`**, first match wins, and runs in a
fresh context whose one readable parameter is the sender (chapter 4); message
25 runs inline and the rest are queued (SCRIPT_VM, "Messages, and the
handler's parameter block").

The engine is the usual sender. Examining an object posts message 4
(`docs/UI.md`, "MULTIPLAN"); a hard landing 10 or 11, a vehicle hitting the
player 17, running out of breath underwater 12 and surfacing 21
(`docs/ASSETS.md` §7, "The fall reaction" and "The water") — each answered by
some area's or `IAM\GLOBAL`'s handler.

### Two resident slots, one pool each

The engine keeps **two** areas loaded, in a two-row table with one row active.
Everything that walks the world walks both rows: the zone registry, the camera
search, the message handlers. The outgoing area stays live — zones armed,
scripts running — for as long as it is resident (SCRIPT_VM, "Two resident
slots").

The object pool belongs to the **slot**, not to the game. `Area_LoadScx` walks
the decor slots for the one holding the area, fills *that slot's* container,
and binds *that slot's* sound file, and `Game_Frame` plays **both** pools every
frame, which is why the place you are not standing in goes on animating. A
script plays objects in its own slot's pool, too (chapter 4).

**So walking back out of a building reloads nothing.** `Area_LoadIntoSlot`
opens by testing whether the slot already holds that area, and if it does it
refreshes the fog block and returns — no `.SCX` reload and no startup script.
The street's container, its sound binding and its running programs are exactly
as you left them. The port once rebuilt the street from the file on the way
back, and a city that had 32 programs running and 153 ambient emitters bound
came back with 0 and 0 (SCRIPT_VM, "One OBJECT POOL per slot"; `verify.py:
engine: city return`).

**Which row is active is decided by the player's feet.** The transition only
shows and hides sets; what switches the active row is event 9, raised by the
ground probe when the floor under the actor belongs to a set in the other slot
that is **shown**. The transition's completion then hides "the non-active
row's" set — which is the one you left, because your feet already moved the
row. When a script has no `area.arrive`, both sets stay shown and the player
walks from one onto the other with both drawn, as in the Impasse's airlock
(SCRIPT_VM, "Which row is ACTIVE"; `verify.py: engine: airlock walk`).

The sky is the exception to one-per-slot: the engine keeps **one** sky, and an
area that names one replaces it while one that names none leaves it standing;
17 of the 259 areas name one (`docs/RECONSTRUCTION.md`, 2026-09-08;
`verify.py: the sky`).

### Hidden is not unloaded

Show and hide (`sub_419AF0` / `sub_419A90`) link a set into and out of the
**render list**, and that is all they do. The **collision array** is a separate
list: a set joins it when it finishes loading and leaves it only when it is
unloaded or evicted. So a hidden set keeps its walls.

And its floor refuses you. The tail of `Walk_ProbeGround` (0x00467030), read
whole: when the floor under the actor belongs to another scene, a slot in
state 2 relinks him and raises event 9; a slot in **state 1** — loaded and
hidden — gets his move for this frame subtracted again. The step is undone
(SCRIPT_VM, "A HIDDEN set is still SOLID", 2026-09-18).

The security centre is built on that. Each level's corridor is part of the
**shaft's** set — the landings, the lift cars, the shaft doors, 99 meshes in
one model — and each level's own set holds only its offices' furniture and
doors, no floor at all, loaded and hidden until a door zone's `area.goto`
brings it in. Its doors and furniture stay solid while it is hidden, and the
corridor under the player is always the shaft's. The port had dropped a set
from collision when it hid it, and a player went through the barriers; it now
keeps every loaded set solid and draws only the shown ones. The rails also
needed the body's sweep to meet a triangle's edges, which is a labelled
reconstruction (`docs/ASSETS.md` §7; chapter 6; `verify.py: engine: security
rail`).

### The transition

`area.goto` names a destination and up to two **door objects** of the
outgoing scene: a departure animation played before the switch and an arrival
animation after it. 448 of its 756 sites carry both, and resolved against the
scene being left 416 of those 448 land, against the destination's 84 (SCRIPT_VM,
"Opcode 47 `area.goto`"; `verify.py: area.goto objects`); 441 of the 448 are
zone enter scripts, and the objects are linked open/close pairs of a door set
piece (SCRIPT_VM, "The objects are doors").

The destination loads into the other slot while the game keeps running. Only
the set streams, in 128 KiB slices, one a frame, and everything after it loads
in one pump tail — so a transition takes as many frames as the set has slices:
17 for Anekbah's 2 099 056-byte set, 1 for the Impasse's (SCRIPT_VM, "The load
takes frames"; `verify.py: engine: area transition`). The caller parks at 10
and is handed back through the pump's tail. A 60-second watchdog unwedges a
load that never reports.

A door a program opens **stays open**: `Script_MoveObjectOnPath` ends by
setting the node's position and restores nothing. Over the corpus, 1 277 of the
2 315 objects that move a node leave it displaced, and 1 239 of those have a
linked partner state — an *open* and a *closed* — which is the data's own
argument that nothing puts the node back (SCRIPT_VM, "…and a script plays
objects in ITS OWN slot's pool"; `verify.py: engine: node rest`).

### The saved game is one block

There is one 8 192-byte structure, and it is both the live game state and the
save file's payload (`docs/GAME_STATE.md` §1):

```
IAM\START ────► the 8192-byte DB ────► State_Apply ────► playing
   5686 bytes                                ▲
IAM\GAMES slot ─────────────────────────────┘
```

`IAM\START` **is the new-game save** — not a script archive, which is what it
was first read as. The file is 5 686 bytes and the block 8 192; everything past
the file is zero and stays zero, and the save writes all 8 192 back. The walk
over it lands exactly on the file size, and its six arrays — 694 variables,
259 per-area resident-scene ids, 670 two-bit prop states, 1 032 shown bits
for characters and objects, 791 address bits, 4 558 zone bits — are each
counted against an independent source (GAME_STATE §2;
`verify.py: game state`). A new game begins in AREA 118, *Introduction Kay'l*,
with no player body, every variable zero, and 3 834 of the 4 558 zones set
(GAME_STATE §7).

`IAM\GAMES` is a 3 496-byte header and 256 slots of 32 808 bytes (GAME_STATE
§8; `verify.py: save file`). The header is the settings block — all 74 option
rows, the three key-binding tables verbatim, and the shooting range's
high-score table at `+724`, four pages of five, a name and a time in
milliseconds — so saving a game saves your options and your best times, once
for all 256 slots (§8a; `verify.py: settings block`). The directory record's
fourth field, once recorded as "not a string", is slot `+108`: the
character's name, which is what the load panel labels a row with; the wrong
offset had landed in the state's array counts (§8).

Saves are not free and not anywhere. Screen 30 is opened only by the world, at
37 zones, all in the activate slot (GAME_STATE §8c; `verify.py: save points`).
36 of them are the same program: test the player's rings — his *anneaux* — and
refuse with a voice line when there are none. The price is charged by the
panel, not the script: all four call sites of `Game_WriteSave` are preceded by
the same run that takes exactly one ring, so an overwrite costs the same as a
new slot (§8c; `verify.py: save price`). The screen's *Sauvegarde* item also
turns to a different page when the count reads zero (`docs/UI.md` §3g). One
save point, in *Ix Astaroth*, has no gate at all. A slot carries a 128 × 96
thumbnail, and loading is a *request*, served at the top of the next script
pump, fading in from white (§8).

A dossier read on a terminal can **open a place**: `address.enable` sets a bit
of the 791-bit address map — one dossier enables 'Anekbah - Bar Zone 52' —
and the sneak's city map draws a marker for each enabled destination
(`docs/RECONSTRUCTION.md`, 2026-09-17; `docs/UI.md` §3g-bis).

### The calendar

Time is two globals saved beside the block, and every constant is a `dd` in the
data segment: **41 days per month, 13 months**, year zero 7216, 3 600 000
milliseconds per day divided into 21 hours of 15 minutes of 33 seconds, with
the months named Aqed, Nadim, Andar, Xenep, Nevod, Ganevat, Osmydep, Qomivo,
Taznevet, Ustanevat, Nivat, Mozkanep, Primevat. A game day is an hour of real
time (GAME_STATE §6).

A new game begins on **12 Nadim 7216 at 11:10:00**. The shipped content
agrees without knowing about the code: all eight dated newspapers in
`IAM\OBJECT` are legal dates in this calendar, and the first is dated the day
before the game starts (§6; `verify.py: game clock`).

## Where it lives

| | |
|---|---|
| the findings | `docs/FILE_FORMATS.md` §5b2b–5b3, §5c; `docs/SCRIPT_VM.md` "The area transition" and "The 16 prompt slots"; `docs/GAME_STATE.md` |
| the port | `engine/src/script/area.*` (the Session: slots, transitions, the frame), `zones.*`, `gamestate.*`, `savefile.*`; `engine/src/o3de/collision.*` |
| the checks | `zone records`, `startup scripts`, `chunk accounting`, `message tables`, `engine: live zones`, `engine: zone pump`, `engine: area transition`, `engine: airlock walk`, `engine: city return`, `engine: lift`, `engine: slot pool`, `engine: node rest`, `engine: security rail`, `game state`, `save file`, `game clock` |

## What is not settled

* **The zone scan's height** in the port is a labelled reconstruction: the
  engine's search box size comes from a field of the zone-space record that
  has not been read (`docs/UI.md` §3f).
* **Status 5** — a superseded transition caller — is parked with no resumer
  found anywhere in the reading. The shipped scripts may never reach it, and
  settling it needs a trace this repository cannot take (chapter 4).
* **How many sites post a message is not counted.** `docs/SCRIPT_VM.md` says
  28 and `docs/FILE_FORMATS.md` §5b3 says 27; the second is a floor over the
  decompilation, which misses every function nothing calls, and neither is
  asserted by a check (SCRIPT_VM, "Messages").
* **The fog block** a resident return refreshes is read as a refresh and its
  contents are not traced here.
* **Whether the panel stops a save with no rings is read two ways.**
  `docs/GAME_STATE.md` §8c reads the confirm and finds that zero rings skips
  the decrement and saves anyway, so only the save point's script stops you;
  `docs/UI.md` §3g reads the screen's *Sauvegarde* item and finds it routes to
  another page when the count is zero. They read different functions, and
  `docs/` has not reconciled them into one statement.
* **What the engine does about the lift car around the camera** at a level's
  arrival is open: the frame is dark because an absolute camera sits inside
  the car's mesh, and the obstruction pass does not run for absolute cameras
  (`docs/ASSETS.md` §7, "The obstruction pass"; chapter 8).
* `docs/SCRIPT_VM.md` "87 / 88" still says no reader of the address bits has
  been traced; `docs/UI.md` §3g-bis has since traced two, the slider page and
  the city map. This chapter follows `UI.md`.
