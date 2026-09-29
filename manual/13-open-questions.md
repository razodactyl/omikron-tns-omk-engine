# 13. Open questions

← [Contents](README.md) · prev: [Evidence](12-evidence.md)

---

## In short

This chapter lists what is not known. It also lists what has already been
ruled out, which is just as useful, because it stops anyone repeating a search.

The largest question turned out to have a different kind of answer from the one
everyone was looking for. A third of the game's conversations are launched by
no script anywhere in the shipped data. After a long search for a hidden
launcher, the evidence says there isn't one. They are **content that was cut**,
and the data shows it.

What remains is smaller, and each item is stated plainly:

* fields whose meaning has been narrowed but not settled;
* two dark or blocked camera views that no reading explains yet: the security
  centre's lift and a crane shot in a restaurant;
* a street fault nobody has reproduced;
* a few reconstructions, each labelled as one;
* several subsystems whose behaviour no instrument here can reach.

A newer group of questions comes from the PS Vita port. They are about
performance, not correctness, and the main one is whether a console can hold
the original's 30 frames a second. Nobody has measured that on a console since
the latest changes.

## In detail

### The 105 conversations nothing launches

The game ships 321 conversations. Following every path a script can take to
open one reaches **216** of them. The other 105 cannot be reached (`CLAUDE.md`
§6, `verify.py: unlaunched dialogs`).

**What was ruled out.** Each item on this list cost real work:

* opcode 61 is the only way into the loader, and all **1 246** of its operands
  are direct literals. The handler's indirect mode is never used;
* the conversation scripts themselves contain no opcode 61;
* every relocated pointer array in the area and scene chunks is accounted for;
* the object archive holds exactly **one** launch site in 1 002 records;
* case 0 of the event dispatcher calls the loader directly, but **nothing in
  the binary raises event 0**. It is a dead entry, not a hidden launcher;
* the message-subscription scripts are among the 5 785 already scanned;
* the French text directory is a **byte-identical duplicate** of the main one,
  not a second corpus;
* the startup scripts at chunk `+4` add **one** conversation to the reachable
  set, taking it from 215 to 216.

**What settled it** was a measurement, not another search. A conversation
reached through some undiscovered launcher would be as *finished* as the rest.
Cut content would not be:

| | count | has facial animation | mean nodes | first line empty |
|---|---|---|---|---|
| launched | 216 | **68%** | 4.1 | 26% |
| unlaunched | 105 | **14%** | 2.7 | **26%** |

The facial animation (the `.3DM`) is the expensive asset made late in
production. The unlaunched set lacks it nearly five times as often, while the
*text* is written to the same extent in both. That points to content cut
**late**, not content never written. It is a correlation over the shipped
corpus and not a proof, but it is the reading that predicts the data, and the
search for a launcher can stop.

The game has other cut content of the same kind, each piece found by reading
the code or data rather than guessed:

* **six spell recipes** whose gate is never met, which leaves five spell items
  unobtainable (`CLAUDE.md` §4);
* **fight-AI profile 4**, which no shipped fight can reach, because
  `Niveau Combat` is clamped to 0..2 and selects profiles 1..3 only
  (`docs/ASSETS.md`);
* two interface pages that are **built but unreachable**: options page 12
  (`CLAUDE.md` §4) and the sneak's quit page (`todo/sneak.md` §5, step 6).

### Closed or corrected since the last edition

* **The "dirty upload bug" on the Vita renderer was not a bug.** The different
  street came from an uncommitted change that assumed the dirty list was
  sorted. The partial upload is exact (`todo/optimization.md` step 25; chapter
  12 tells the story).
* **"The original did this at a fraction of the detail" was withdrawn**
  (`todo/optimization.md` §7, corrected 2026-09-29). The crowd's density follows
  the engine's own rule, on the same models, and the original posed every body
  on the CPU. What the port adds on top of that is its own driver calls,
  uploads and depth tie, not extra detail.
* **The city on a console is slow, not frozen.** Three console logs from
  2026-09-21/22 reach Anekbah at about 200 ms a frame
  (`todo/handoff-vita-port.md` §1).

### The Vita port's open questions

These concern speed and memory, not what the game does. Chapter 11 covers
the port itself.

* **Can a console hold 30 frames a second?** `todo/optimization.md` §7 decided
  on 2026-09-14 that one core doing this CPU work was **not** in reach, by a
  factor of several. Since then the bodies have moved onto the GPU (see
  below), the GLES backend has stopped re-setting state that has not changed
  (1 809 → 92 state calls a street frame, measured on a Mac), the music is
  decoded through a table, and the frame makes about 160 heap allocations
  outside the software rasterizer where it used to make about 724. **None of
  this has been measured on a console** (`todo/handoff-vita-port.md` §4). The
  next city log from a console is what answers it.
* **The depth tie baked at load** (`todo/optimization.md` step 27, planned and
  not started). The original shows the first-drawn of two coincident faces, and
  the port decides which face wins **every frame**. The plan argues that the
  answer depends only on which set is loaded, so it can be computed once per
  load. A **corpus census** has to come first: coincident pairs within one mesh
  against pairs across meshes, and any pair on geometry that deforms
  non-rigidly. A pair spanning two independently moving meshes is the one case
  a load-time answer could get wrong.
* **Do NPCs cast a shadow at detail 0?** (`todo/optimization.md` step 18.)
  `shadowBonesFor(-1)` returns nothing, because the chest's minimum level is 0
  and the test is `detail >= minLevel`. The function's own comment, though,
  reads `Actor_DrawShadow`'s switch as "-1 and 0 fall to the chest", and an NPC
  takes the level minus one. The engine's switch decides it, and it has not
  been re-read. It was noted and not changed.
* **The player is not yet posed on the GPU** (`todo/gpu-skinning.md` step 5,
  open). The walkers and the staged bodies are (steps 3–4). The exception is a
  speaker whose line morphs the face: that body stays on the CPU, and a small
  dynamic buffer for the face is the refinement left. Step 6, the new shader
  programs on the console and a city log, is also open.
* **GPU buffers for geometry that is gone are never released**
  (`todo/optimization.md` step 26, open).

### Fields whose meaning is narrowed, not settled

* **The morph files' `float[3]` track.** The parser's handling of it has been
  read and confirmed against the assembly. "Root-motion deltas" is refuted as a
  playable meaning: the track is near-constant and near-unit in 57 of 60 files,
  which would integrate into drift everywhere. Whatever cancels it in the
  engine has not been traced (`CLAUDE.md` §6).
* **Node slots 0 and 1 in the same files.** They are uploaded with ids that no
  drawn mesh binds. They are measurably not rotations, and measurably **not**
  the voice envelope either (|r| < 0.2 against per-frame RMS in four files).
  Eye-direction or blink channels are the shapes that remain (`CLAUDE.md` §6).
* **Four of the second render bank's six swapped pointers.** No capture tells
  them apart (`PORTING.md` §B6).
* **The zone scan's height radius.** It is a float in the zone-space record
  that has not been read. The port uses the quad's own height plus one metre,
  labelled as a reconstruction (`todo/missing-ui.md` §2).

### Things with no account yet

* **The lift's dark arrival.** At each level of the security centre, the
  arrival camera sits inside the lift car's own mesh, `CSPont04`, and the frame
  is almost black. Displacing that one mesh takes it from 96.7% dark to 16.9%
  (`todo/missing-ui.md` §6b). The slot bookkeeping was measured and ruled out
  as the cause. The camera obstruction pass is ruled out too: the camera is
  absolute (both subjects −1), an absolute camera never carries the flag that
  arms the pass, and only the 406 world cameras with eye subject 0 ever take it
  (`todo/camera-obstruction.md` §6). The candidate left standing is that the
  engine does not draw the car that is not in use. That is a candidate, not a
  finding. Related, and also unfixed: **the lift door opens and then shuts
  itself**, because the port drops the motion patch (`todo/play-test.md`).
* **The restaurant crane shot.** In dialog 387, the reply pair 4194 → 4195 is
  an authored crane shot, with the eye 5.7 m up and descending. In the port it
  has the ceiling in front of it, and in the original it does not. The port
  places and moves that camera exactly as `Dialog_ApplyLineCameras` says.
  `todo/missing-ui.md` §7 read the difference as the obstruction pass, and three
  ports of that pass were reverted after the reader played each one.
  `todo/camera-obstruction.md` §6 later **refuted that reading**. All 44 of the
  dialogue's cameras are absolute, and the issuer explicitly clears the section
  flag, so the pass cannot reach the crane. The cause is open, and it may be
  the same as the lift's.
* **Street NPCs that stop and T-pose** (`todo/next-tasks.md` item 6). The
  walkers are cleared: stopping is the engine's own action-point rule, and
  every crowd clip binds all 19 of its tracks. The authored extras remain a
  candidate. A staged body rebuilt after dropping out of view for a frame would
  start with no last pose. None of this has been reproduced, and it needs the
  place where the reader saw it.
* **The released spectres' attack.** Every data-side lead closed with no answer
  (`todo/released-spectres.md`). The fire test's bit is set only for character
  type 3, and none of the catacomb's actors is type 3.
* **Three placement jumps at cutscene beat starts** in the Impasse. The check
  pins the count, so none of them can change unnoticed
  (`verify.py: engine: beat handover`).
* **ACTOR_STATE 13, the swimmer's surface, has not been reached by any run**
  (`todo/swimming.md`). The canal walk-in keeps him under the water mesh the
  whole time, so finding the edge where he surfaces is left for the play test.

### Reconstructions, labelled

These work in the port and are **not** transcriptions. Each one says so in its
source:

* the body sweep's **edges and corners**, and the height the sweep starts at.
  The engine's 930-line polygon kernel has not been read (`docs/ASSETS.md`);
* the mirror's **confinement** to its own area, and its **plane normal**
  (`CLAUDE.md` §6);
* the **dither matrix**, which belongs to the driver, not the engine
  (`CLAUDE.md` §4);
* the zone scan's height radius (above);
* the lift's description box, which takes the text file's string directly
  instead of running the unread function that builds it (`todo/missing-ui.md`
  §2b).

### Things no instrument here can reach

These are limits of the apparatus, each recorded with its reason:

* **The actor runtime has no oracle**: the channel, melee and shoot mode. The
  opcodes that start a fight or a shoot phase announce nothing the logger keeps
  (chapter 12). The fight AI's priority gate has a threshold that is read and
  asserted, but it has never been *seen* refusing a move. An instrument
  counting the candidates it turned away reads 0 in the supermarket fights
  (`todo/fight-mode.md` 15.14). All of it stays at tier 5.
* **The audio attenuation and pan law belongs to DirectSound.** Nothing in the
  image describes it, and no rig here records sound (`PORTING.md` §B6).
* **The Vulkan backend is unverifiable by construction** (`PORTING.md` §B6).
* **No pixel value in 3D** reaches any tier (`PORTING.md` §B5).

### Smaller open ends

* **24 VM opcodes have no mnemonic.** They are the ones no shipped script
  executes. Five more are read but deliberately left unnamed: 17 instructions,
  0.029% of the corpus (`docs/SCRIPT_VM.md`).
* **Transition status 5 is read and not resolved.** It parks the *previous*
  caller when a second `area.goto` arrives while one is in flight, and nothing
  in this reading resumes it. It may never be reached, since it needs two
  transitions in flight, and a claim either way needs a trace this repository
  cannot take (`docs/SCRIPT_VM.md`).
* **The movie player's decoder and parameters** have not been traced, and
  nothing needs them. **What the engine's external clock reads** is inferred
  from where the call sits, not established (`docs/BOOT.md`).
* **551 of the 561 voice-over files are not on the disc.** This is explained,
  not missing (`CLAUDE.md` §6, `verify.py: cutscene music`).
* **The 1999 press sheet's claimed BSP tree** is not in the model files. Every
  byte of them is accounted for, apart from 460 unexplained bytes across 33 MB
  (`CLAUDE.md` §2, `tools/domap.py`).
* **The web viewer still stages dialog 401 from a clip root** where the engine
  uses an authored path. This is a known viewer TODO, not a finding in doubt
  (`CLAUDE.md` §6).
* **Interface pieces not yet ported or not yet right**: screen 35, the options
  screen that the sneak's Options tab hosts, is blocked because the port models
  it but does not draw it (`todo/sneak.md` §5). Tab strips switch page on the
  confirm instead of on the move. The overwrite dialog does not name the file,
  and the start menu's `Quitter` is not the real exit. These last two are
  decoded and waiting to be transcribed (`todo/play-test.md`).
* **The interface clocks read the wall in a frame-bounded run.** No frame shows
  it, so it has not been changed (`todo/optimization.md` step 25).

## Where it lives

| | |
|---|---|
| the standing list, with what has been ruled out | `CLAUDE.md` §6 |
| the roadmap and the running log | `docs/RECONSTRUCTION.md`; grep it by date or subsystem, never read it whole |
| what is committed and not yet played | `todo/play-test.md` |
| the reader's list of what to do next | `todo/next-tasks.md` |
| the Vita's performance questions | `todo/optimization.md` (steps table, §7, steps 18, 25, 26, 27), `todo/handoff-vita-port.md` §4, `todo/gpu-skinning.md` |
| the lift and the crane shot | `todo/missing-ui.md` §6–§7, `todo/camera-obstruction.md` §6 |
| the per-subsystem plans and records | `todo/*.md` |
