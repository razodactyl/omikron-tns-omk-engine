# 1. The original

← [Contents](README.md) · next: [Boot, and the frame](02-boot-and-frame.md)

---

## In short

*Omikron: The Nomad Soul* shipped in 1999 for Windows. It is three games
wearing one coat: you walk around a city and talk to people (adventure), you
fight people hand to hand (a beat-'em-up), and you shoot people (a first-person
shooter). Now and then you swim. The conceit is that you are a soul that moves
between bodies, so the game has to be able to hand you a different character
and keep going.

The engine that carries all that is a single executable of under a megabyte —
the game's `Runtime.exe` — plus about 1.7 GB of data files. Almost nothing
about how the game behaves is compiled into that executable. What is compiled
in is a **machine**: a bytecode interpreter, a state-machine runner, a
renderer, a set of file readers. What the game *does* — which conversation
starts when you walk through a doorway, which camera watches it, which
animation a character plays when it turns around, which gunman patrols which
corridor — is data, sitting in the files beside it.

That split is why this project is possible at all. Read the machine once, and
the content reads itself.

<p align="center">
  <img src="../traces/frames/menu-22.png" width="440" alt="The original engine's start menu, captured at 640x480">
  <br><em>The original engine's own framebuffer — the start menu, in the game's<br>invented alphabet. Not a screenshot of a window: the framebuffer itself.</em>
</p>

**OMK** re-implements that machine in C++20 and reads your copy of the data. It
does not ship any game content and never will. It runs on a Mac, and — slowly —
on a PS Vita.

## In detail

### What the executable is

The engine is the game's `Runtime.exe`, in the build that does not ask for the
CD. Two builds of it shipped — one that checks for the disc and one that does
not — and both are called `Runtime.exe`, so a tree holding both has to rename
one of them. **That renamed name is a local convention and means nothing about
the game**: every address in this repository refers to the no-CD build,
whatever the file is called on the disk it was read from (`CLAUDE.md`, its
opening paragraph). This manual names it `Runtime.exe` throughout for that
reason; `CLAUDE.md` and `docs/` carry the local name, `Runtime 2.exe`.

The tools do not hard-code either name: `omkpaths.exe_path()` takes the
largest candidate present, because the two builds are not close — 984 576
bytes against the launcher's 280 290, as measured and recorded in
`tools/omkpaths.py` — and size survives a rename in either direction where a
name does not.

It is a Win32 binary against the 1999 Microsoft stack:

| subsystem | API | what the port does with it |
|---|---|---|
| video | Direct3D fixed function, fed a `D3DTLVERTEX` stream carrying a diffuse colour per vertex (`docs/ASSETS.md` §4c) | reimplemented behind a decision-level boundary (`docs/PORTING.md` A2): a software rasterizer, a Vulkan backend, and a GLES2 backend — the last is what the PS Vita draws with |
| 2D / blitting | DirectDraw, `Blt` with colour keys (`docs/UI.md` §1) | ported exactly; a blit is a memory copy, so it is reproducible pixel for pixel |
| audio | DirectSound — a primary buffer at 22 050 Hz, 16-bit, stereo, into which the system mixes (`docs/PORTING.md` A5) | the *decisions* are ported; there is no mixer in the engine to port |
| input | DirectInput, polled into a key-state array, plus a joystick read as `DIJOYSTATE` | ported, including the edge filter and `Input_Poll`'s own fix-ups (`engine: input poll`); a gamepad reaches it as the engine's own joystick device |
| video playback | DirectShow (`CoCreateInstance`) and MCI (`mciSendCommandA`), for three MPEG-1 files — the image carries no MPEG decoder of its own (`engine: movies`) | replaced by a vendored decoder, and on the Vita by the console's hardware decoder; not a port of anything (chapter 2) |

The engine's whole frame is gated behind a Win32 idle loop — see chapter 2.

### The three modes, and why they share a spine

Adventure, fight and shoot are not three engines. They are three **input
context groups** over one actor runtime: 4 context groups × 14 actions × 3
devices, installed by whichever code takes over (`Fight_Begin` installs group
3, `Shoot_Enter` group 2, swimming group 1) — `CLAUDE.md` §4, "the four control
schemes". Underneath, every character — player and NPC alike — is driven by
the same thing: a `.CTL` state machine read out of a data file, matching the
current input bitfield against the transitions its author wrote.

So "the player draws a gun", "the player dives" and "the player takes a step"
are the same kind of event: a transition in a graph that shipped on the disc.
What differs between the modes is who presses the buttons — the player, the
fight AI injecting button combinations into the opponent's own input queue, or
a gunman's brain — and which camera watches.

### The scale of the content

Measured, from `CLAUDE.md` §4 and the checks that assert each figure:

| | |
|---|---|
| textures (`.3DT`) | 2 534, all byte-identical (`textures`) |
| models and sets (`.3DO`) | 635 models, 16 188 meshes, 666 cameras (`engine: 3DO`) |
| lights inside those models | 4 179 records across 216 models (`CLAUDE.md` §4, the `.3DO` light table) |
| animation quaternions (`.ani`) | 243 362, every one a unit quaternion (`.ani quaternions`) |
| morph/voice files (`.3DM`) | 777, sample-identical audio (`.3DM files`) |
| scene scripts (`.SCX`) | 220 |
| world script slots | 5 785, all decoding |
| trigger zones | 4 558, none malformed |
| conversations | 321, of which 105 no script launches (see below) |
| VM opcodes | 153, of which 129 are named |
| interface screens | 37, over a widget tree of 60 panels, 176 lists and 728 items (counted in `tables/ui_widgets.json` at this snapshot) |

### What "reimplementation" means here

Not emulation, and not a rewrite from a design document. The unit of work is:
read a function in the disassembly, establish what it does, write a check the
shipped data could fail, then write the C++ that makes the same decision. The
standard for when that counts as done is `docs/PORTING.md`, and chapter 12 of
this manual is its summary. The checks live in `tools/verify.py`: **484 of
them** at this snapshot (485 entries in `verify.py --list`, 257 of them behind
`--slow`).

The one thing that cannot be read out of the data files is the set of tables
**compiled into the executable** — the VM opcode table and its announce map,
the widget tree and the interface tables, the key bindings, the camera
presets, the special moves, the ADPCM coefficients, the fight AI's built-in
moves, the shoot AI and the shoot weapons, the four city maps. Those are lifted
to JSON in `tables/` (12 files), which is why a replica needs both your data
directory *and* this repository's `tables/`.

### Where the replica has got to

From a cold start it steps the three intro movies, shows the splash, draws the
start menu and takes an answer, plays the Kay'l intro conversation with its
dialogue cameras and voice-over, flies the Impasse's camera editings, and hands
over the player: adventure mode with a follow camera, a walkable floor that
stops at walls and rails, the jump and the fall, swimming, and area
transitions that keep two sets resident and play the doors between them
(`README.md`). Its announcement stream matches a capture of the original
**42 of 42, in order**, from a cold start (`engine: boot`; chapter 2).

From there, each of these runs through the game's own data (`README.md`):

* the **sneak** — Kay'l's device — with its inventory and verbs, the memo
  journal, the identity page and the city map;
* the **slider**: called, boarded through its door, flown;
* **shops** and the **MULTIPLAN** storage kiosk; the security centre's **lift**
  and **terminals**;
* **saving and loading** through the game's own panels; the **pause screen**
  on Escape;
* **melee**, with the game's own adaptive fight AI;
* **shoot mode** — first person, with gunmen who patrol and fight over the
  level's navigation grid.

The city's crowd and its road traffic walk and drive their circuit around you,
lit by the lights baked into the set and casting the engine's own blob
shadows. How many walkers there are is the engine's own rule, not the port's:
`Slider_Init`'s spawner places one pedestrian every `39 × (5 − density) × h[3]`
units along each lane of the shipped circuit, `density` being the options
menu's street-activity row (`docs/STREET_LIFE.md`, "`Slider_Init` — the pools
and the spawn"). So the port populates the street the original populated, on
the same models — not a denser one.

Much of this has been confirmed by a person playing it; what has not is listed
in `todo/play-test.md`, and nothing is claimed as confirmed beyond what a
reader has confirmed.

The coverage audit in `engine/README.md` §Coverage counts, against the 41
content rows of `CLAUDE.md` §4: **31 fully ported, 7 partly, 0 lifted but
unconsumed, 0 unported**, and 3 that are this project's own instruments. That
table has been wrong twice, so read it rather than this paragraph.

**The PS Vita.** A separate backend — GLES2 through vitaGL, the pad as the
engine's joystick, a VitaSDK build — runs the game **on a real console into
the city**: the intro films, the menu, the flat, the intro cutscene, and
Anekbah's streets (`todo/handoff-vita-port.md` §1). It is slow. The most
recent console log, of 2026-09-27, puts the city at about 150–160 ms of work a
frame on the CPU path (`todo/vita-port.md`, the entry of that date) — six or
seven frames a second, which under the engine's own clamp is a game running in
slow motion rather than one taking bigger steps (chapter 2). The optimisation
work committed since that log has not been measured on a console.

<p align="center">
  <img src="images/anekbah-street.png" width="560" alt="Kay'l standing in Anekbah's main street, drawn by the port">
  <br><em>The port, in adventure mode: Anekbah's main street with its procedural<br>crowd, its ambient fire and neon, the set's own lights and the characters'<br>blob shadows. The command that produced this frame is in<br><code>manual/images/README.md</code>.</em>
</p>

## Where it lives

| | |
|---|---|
| the original | the game's `Runtime.exe`, the no-CD build (yours; never in this repo) |
| the disassembly | `Runtime.exe.asm` / `Runtime.exe.c` — optional, not distributed (it is a derivative work), relocatable via `$OMK_ASM` / `$OMK_DECOMP` |
| the hand-cleaned reading | `readable/src/*.c` — 33 modules, every function carrying a status banner; `readable/INDEX.md` is the index |
| the findings | `docs/` — 10 documents |
| the port | `engine/` — C++20, no required dependencies. `engine/src/` is 9 directories and ~57 300 lines; with the four backends (`sdl`, `vulkan`, `gles`, `vita`) and the 198 probe tools in `engine/tools/`, ~112 900 (counted with `wc -l` at commit `71188b4`, vendored code excluded) |
| the Vita | `todo/handoff-vita-port.md` for the state and the recipes; `todo/vita-port.md` for the plan and its running record |
| the lifted tables | `tables/*.json` — 12 files, each self-checking |
| the readers and viewers | `tools/` — 77 Python files, stdlib only |
| the checks | `tools/verify.py` — 484 checks; `--list` is the index |

## What is not settled

* **105 of the game's 321 conversations are launched by no script**, and the
  reading is that they are **cut content** rather than evidence of a launcher
  nobody has found: the unlaunched ones lack their facial-animation `.3DM`
  nearly five times as often as the launched ones (14% have one, against 68%),
  while their text is as complete (`verify.py: unlaunched dialogs`;
  `CLAUDE.md` §6). It is a correlation over the shipped corpus, not a proof.
  Chapter 13 has the measurement.
* **Two modes run whose behaviour no instrument here can check against the
  original.** Melee and the actor runtime under it are *data-constrained*: the
  golden-trace rig sees only what a script handler announces, and combat
  announces nothing, so a fight's outcome has no oracle (`CLAUDE.md` §2, on
  `traces/fight.log`). Shoot mode is in the same position (chapter 6).
* **The Vita's frame rate is far from playable**, and what the newest builds
  do on the console — the films, the GPU posing, the frame — waits on the next
  console log (`todo/handoff-vita-port.md` §3b–§4). The Vita is recorded in its
  own task files and sits outside `engine/README.md`'s coverage audit.
* **The joystick's coverage row lags the code.** `engine/README.md` §Coverage
  still says the joystick axes are "carried but nothing steers with them yet",
  while `engine: input poll` (2026-09-18) asserts the axes reaching slots 0..3
  by `Input_Poll`'s own rule. The audit is the authority and has not been
  brought up to date; until it is, the two disagree.
* Whole subsystems of the original are **read but not exercised**, because the
  port has not reached the part of the game that uses them.
