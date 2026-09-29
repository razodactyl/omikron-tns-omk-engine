# 11. The port

← [Contents](README.md) · prev: [The interface](10-the-interface.md) · next: [Evidence](12-evidence.md)

---

## In short

`engine/` is the re-implementation: C++20, about 57 300 lines of engine source
in nine directories and 28 000 more in four backends, and **no required
dependencies at all**. `make` on a machine with nothing installed builds every
probe and passes the test suite. SDL, the Vulkan loader, a GL context and the
PS Vita SDK are optional, and each buys a window on some machine.

The shape of it comes from one decision. Every output subsystem — picture,
sound, input — has **a reference implementation and a live one behind one
boundary**:

| | the reference | the live ones |
|---|---|---|
| render | a software rasterizer into an RGB565 framebuffer | Vulkan (MoltenVK on macOS), and GLES2 — the PS Vita's |
| audio | PCM buffers | an audio device |
| input | a replayable event stream | a keyboard, or a pad read as the engine's own joystick |

The reference is what the checks measure. The live ones are what make the
replica playable. Neither may be required to build the other.

Beside it sits a second rule, about what the port may add: **anything the
original did not do is an enhancement, and every enhancement is off by
default.** And a third, about speed: an optimisation may change *how* an answer
is computed, **never the answer** — every speed step is proven by the same
frame, byte for byte, or by an exhaustive comparison.

Since the last edition of this manual the port has left the desktop: **it runs
on a real PS Vita, into the city** — slowly. The films, the menu, the flat and
the intro cutscene play on the console, and Anekbah's street loads and runs, at
well over a hundred milliseconds a frame against a budget of 33. Whether 30 fps
is reachable there is **still open**, and the console log that should say how
far off it now is has not yet been taken.

## In detail

### The layout

Counted 2026-09-29 with `wc -l` over every `.cpp` and `.h` under `engine/src`
(57 314 lines) and `engine/backends` (27 986); vendored code lives in
`engine/third_party/` and is not counted. The probes in `engine/tools/` add
another 27 569.

| directory | what is in it |
|---|---|
| `formats/` | one reader per file format — iam, scx, sfx, ctl, anim, morph, mesh3do, tex3dt, fnt, adpcm, addresses, opt, map2d, the `.3DO` light table |
| `script/` | the world-script runtime: the Session with its two resident slots and its frame, the VM handlers, the zone registry, conversations, the game state and the save file, the scene objects, the inventory |
| `actor/` | the `.CTL` channel, the player controller and its cameras, skinning, the walker, the slider, melee, shoot mode's brains, weapons and projectiles, the street crowd and its vehicles, the spatial index |
| `o3de/` | the renderer boundary and the software rasterizer, the draw buckets, geometry, the texture cache, the depth tie, lights, shadows, the shimmer, the point placement, world cameras, camera editings, particles, collision |
| `ui/` | the I2D layer, the widget walk, screen drawing, text and its layout, surfaces, the options tree, the city map, the HUD bars, the radar |
| `audio/` | the voice bank and pool, the voice-over path, music |
| `input/` | the four control schemes, `Input_Poll`'s rules, the pad |
| `platform/` | all data access, the boot path, the frontend interface, the movie decoder, the settings and the ini, the thread pool, JSON |
| `app/` | **new**: the first pieces lifted out of the viewer's single source file (below) |

Four backends: `sdl/` (the game and viewer, `omk-play`), `vulkan/`, `gles/`,
and `vita/` (the console's entry point, the hardware film player, the on-screen
keyboard for the name field, a bench and a smoke test). And **198** small
command-line probes in `engine/tools/`, one per check, mostly (`ls | wc -l`,
2026-09-29).

### The boundary is at the decision level

This is the load-bearing choice (`docs/PORTING.md` §A2), and getting it wrong is
expensive in a way that is hard to undo. What is ported is not triangles but
**decisions** — the drawable mask, the bucket key, the blend modes, the texture
cache, the visible-set walk (chapter 8). The interface takes those:

```
begin(view) · submit(draw) · submit2d(list) · end() -> Frame
```

A backend receives decisions and turns them into API calls; it never makes one.
Put the boundary at the API level instead and those decisions leak into
API-specific code; a second backend then cannot be added without extracting
them again, and the frame oracle becomes unusable.

The boundary has since carried three backends. It grew for the enhancements by
fields whose default changes nothing, and for the handheld by one question —
`Renderer::posesBodies()`, *can this backend pose a body itself?* — which is
false on every backend but GLES (`todo/gpu-skinning.md` step 1). So the
software reference still poses and draws exactly as before, by construction.

### What "dependency-free" actually means

It is a property of the **verification path**, not of the program
(`docs/PORTING.md` §A8). Everything the suite touches builds with nothing
installed. The playable frontends need dependencies and should have them —
hand-rolling Cocoa, Win32 and X11 to avoid SDL, or writing an MPEG decoder,
would be more code, no verification benefit, and a port of nothing in the
original.

Three rules keep that honest:

1. **`make` with nothing installed builds every probe and passes the suite.** A
   dependency that breaks this is rejected, not worked around.
2. **No ported source includes a system dependency's header.** They appear only
   in backend files, behind the boundary. A *vendored* dependency — the MPEG-1
   decoder, checked in — is always present, so it may sit in ported code.
3. **No dependency may do work a reference implementation is supposed to be a
   port of.** Using a library's blitter or BMP loader instead of the ported one
   means the check tests the library — and it passes exactly as green while
   establishing nothing. PORTING calls this the one rule that is not hygiene.

### Settings, and the enhancements

The game's settings resolve from three sources, in order: the engine's own
defaults, the `[Preferences]` section of the game's ini (the 65 keys the binary
spells), and the **save file's header**, which carries all 74 option rows
(`engine/README.md` §Coverage, the graphical-options paragraph). The port
records which source supplied each field.

Everything else lives under `[Enhancements]`, a section the original never had.
`todo/enhancements.md` lists eleven rows: anti-aliasing, bilinear filtering,
mipmaps and anisotropy, filtered interface scaling, an unlimited draw distance,
fitted shadows, mapped shadows, per-pixel lighting, the sets receiving the
lights (held back — it overrides authored art — and marked not recommended),
supersampling, and shoot mode's radar in every arena. Each is off by default;
`all = max` turns every one to its top. The two fidelity fixes that came out of
the same reading — the **dither** and the **shimmer** — are deliberately not in
that list: the original does them, so they are on.

### Speed: the rule, and what it has bought

`todo/optimization.md` is the record, and its principle is the one above: a
step that moves a pixel or a position is a bug in the step. Every row of its
steps table ends in an equivalence — frames byte-identical, a hash unchanged,
or an exhaustive comparison — and most in a check named after it
(`engine: probe grid`, `tie equivalence`, `pixel tables`, `gpu present`,
`pose equivalence`, `adpcm table`, `gles state cache`).

**Every millisecond in that file carries its machine, and the machines are not
comparable.** Steps 1–14 were measured on an **M1**; step 15 onward, from
2026-09-29, on an **M3** (`todo/optimization.md` §15). Counts — allocations,
state calls, patches — compare across them; milliseconds do not.

What the M1 steps did, in the file's order: grids for the ground probe, the
walker and the body and camera sweeps (the shadows' probe had been scanning all
15 137 walkable triangles of Anekbah per shadow bone, hot spot H1); the depth
tie on hashed sets and then stored flat; the interface's pixel conversions by
table, and a frame with nothing drawn over it dithered and presented on the
GPU; the moving set meshes patched through an index; a revision that says
*which* corners moved. Step 7 then took stock (below).

What the M3 steps did, all on 2026-09-29:

* **§15, an audit** ranking what is left for the console. Its one correction to
  itself is instructive: the music was called "the largest engine-owned span,
  0.5–0.6 ms a frame", read off an *uncapped* run that does not pace audio;
  paced at 30 fps it is ~0 on the M3 (§16). **A span read off an uncapped run
  measures the run, not the game.**
* **§16, the music decoded through a table** — a nibble's effect depends only on
  (index, nibble), so 89 × 16 entries replace the branches. Exact over all
  93 323 264 channel states, every track and every voice line; whole-file
  decode 1469 → 1012 ms, a 120 s pull 52.0 → 31.7 ms (M3).
  `engine: adpcm table`.
* **§17, the GLES draw-state cache** — state a draw would set to the value it
  already has is not set again. On the Anekbah street, **1809 → 92 state calls
  over 248 draws a frame** (M3), the pictures identical.
  `engine: gles state cache`. Its other half, merging dirty upload runs, was
  **refuted and not committed**: it saved almost nothing, and it was not exact.
* **§25, a suspected bug closed as not a bug.** A street frame that drew 537
  pixels differently with whole and partial uploads looked like a GLES
  dirty-upload fault. It was §17's own uncommitted merge, which assumed the
  dirty list sorted; it is not, and the committed upload loop is exact
  (0 corners missed and 0 different, by an audit mode kept in the backend). A
  wrong turn on the way — dumps taken at frame one while the display slept,
  read as a fix and committed as `446beb7`, then reverted — is recorded there
  as a trap.
* **§18, the frame's heap churn**, counted with a malloc interposer in the
  software build: **1214 → 651 allocations a frame** on the street, and outside
  the software rasterizer (the reference, and out of scope) **~724 → ~160**.
  Byte-identical at every step.

### The Vita decision, and its correction

Step 7 (`todo/optimization.md` §7, 2026-09-14, **M1**) measured the main thread
at **6.0 ms** of work a frame, capped at 30 on the Anekbah street, with 137 MB
of live heap. It could not measure the console's CPU factor — no sourced figure
puts the M1 and a Cortex-A9 on one scale — so it framed the question by the
break-even instead: 6.0 ms fits a 33 ms frame on a core up to ~5.5× slower, and
the gap is, in the file's words, several times that — "a reading of the gap,
not a measurement". Its decision: **a 30 fps Vita build doing this CPU work on
one core is not in reach**, and the levers are GPU skinning and lighting, the
other cores, and measuring on the device.

**Corrected 2026-09-29, and the correction changes what the cost *is*.** §7 had
called the remaining work per-vertex detail that the 1999 engine did "at a
fraction of this detail". That is not established, and the crowd says
otherwise: its density is the engine's own rule, spawned from the same circuit
on the same models (`docs/STREET_LIFE.md`), so **the port draws the same
characters the original did** — and the original posed and transformed every
one of them on the CPU, handing Direct3D vertices it had already transformed.
What the port pays extra is **its own**: GL driver calls, uploads, and the
depth tie — not more detail.

### The PS Vita port

It began on 2026-09-17 as its own line of work — `todo/vita-port.md` is the plan
and the record, `todo/handoff-vita-port.md` the state — and it is **not part of
the coverage audit** (`engine/README.md` §Coverage says so).

Where it stands, from the handoff (§1, §3) and the record's entries through
2026-09-27:

* **It runs on a real console into the city.** The films, the menu, the flat,
  the intro cutscene, then Anekbah — which first died of the music's 16 MB (now
  streamed from the file as it plays, 0 mismatches over 82 million samples),
  then took 8.4 s a frame (a whole-buffer copy on every tie patch, fixed), then
  shook (the port's own delta clamp, replaced by the engine's three-frame cap).
  The handoff's summary of 2026-09-22: ~200 ms a frame, all CPU, "a uniform
  ~40× the M1" — a **console** figure over an **M1** one, and labelled so.
* **The last console city log on the CPU path** (2026-09-27): ~150–160 ms of
  work a frame — submit 42–47, staged bodies 29, pedestrians 20, meshes placed
  6.8, grids 7, lights 5, with music top-ups of 25–34 ms in the frames that ran
  one (since split into quarter-second pulls).
* **The films** play through SceAvPlayer from hardware-decoded H.264 copies,
  falling back to the MPEG-1 path when those are absent. On the console only
  GAME has been seen to play, with its picture running fast against its sound;
  the fixes for both (one memory strategy for all three films, the sound taken
  from the decoder more slowly) are not yet confirmed there.
* **Precompiled shaders travel in the VPK**: a console log of 2026-09-22 ran
  with the runtime shader compiler absent and the game started — confirmed on
  hardware.
* **The heap is 192 MB**; 300 was refused by the console. The log is written by
  its own thread, off the frame, and a run that dies still carries its last
  line.
* **The crowd is posed over several cores, by default** (P4): the pass is split
  into a serial resolve, a parallel per-body pass writing only its own body, and
  an in-order merge, and the frame is byte-identical threaded and inline
  (`engine: threaded bodies`). The console's bench said `threads: EXACT`,
  **2.71×** on three runners (2026-09-22), and 2.75× on 2026-09-27.
* **Bodies posed on the GPU**, GLES only (`todo/gpu-skinning.md`): steps 1–4 are
  done — the posing program, the crowd's light in the shader, the walkers, and
  the staged bodies except a speaker whose line is morphing his face. The
  exactness story is not "same bytes" but PORTING's coverage standard, on the
  Mac: 0–6 pixels of 307 200 differing for a posed body, 0–17 with lights, 1–5
  on the street at density 4. **Step 5, the player, is not done**, and step 6,
  a console city log with the new programs, is owed.
* **A fault only the console could show**: GPU-posed cutscene limbs flung from
  their bodies. A start-up self-test now proves the posing program on the device
  and falls back to the CPU if it fails. It failed on exactly the odd mesh
  slots, and the fix (four rows a slot, every computed base a multiple of four)
  has **not yet been confirmed by a console log** (`vita-port.md`, 2026-09-27).
* **NEON is allowed, beside a generic path** (the reader's rule, 2026-09-25),
  chosen at compile time and measured against it. Two kernels so far, each
  bit-identical to its generic loop: the scripted motion's point placement
  (EXACT, 1.17× on the A9) and the texture upload's keyed-RGBA conversion
  (EXACT, 1.74× on the A9) — both from the console bench of 2026-09-27.

**What the next console log is expected to answer**
(`handoff-vita-port.md` §4, added 2026-09-29). None of the M3 work of
2026-09-29 — the state cache, the music table, the allocation cut — has been
measured on a console. The next city log's per-section lines, set against
2026-09-27's (submit 42–47 ms), and its new `gles state` line every 60 frames
(draws, state calls made and skipped) are what will say how much of the submit
was state-setting. After that, the largest single item left is **§27, the depth
tie computed once per set load**: a coincident pair's winner is fixed by the
texture slots, which change only at a load, so the original's answer — its
texture-cache quirk included — can be baked in. That is **planned, not done**,
and it starts with a census across all 635 models. And one run without the tie
has already said it is not the whole lag: the reader found it "a little
smoother, [but it] does not make a great difference".

### The viewer's one source file, and its split

`engine/backends/sdl/play.cpp` is the game and the viewer at once — 21 267
lines on 2026-09-29 (`wc -l`). `todo/play-split.md` is the plan for breaking it
up, and the reason is not the line count: its `main` declares 468 names that 95
lambdas capture, so nothing in the frame loop has a parameter list. Done so
far: **S0**, a golden record of six scenes (framebuffer and filtered stdout)
that each step must reproduce, and **S1/S1b**, the pure helpers — the subtitle
box, the WAV conversion, the text transcoding, the free-look camera and the
light — moved to `src/app/playhelpers.*`. The rest, S1c to S9, is planned.

### Two rules that shaped the code more than any design

**Everything counts in frames.** The simulation is fed thirtieths of a second,
never seconds, however fast the frontend presents (`docs/PORTING.md` §A7;
chapter 2 has why).

**All data access goes through one class.** `DataFs` resolves case-insensitively
because Win95 did, and on the Vita it is also what stands in for a standard
library that turned out not to be the platform (`std::filesystem` and the
standard mutex broke there; `handoff-vita-port.md` §5). It is where the write
guard lives too: `safeOutputPath` refuses any output path inside the shipped
tree or carrying a shipped-data extension. That guard exists because a tool
whose second positional argument was its output once truncated a file from the
1999 disc, and the check that noticed ran afterwards — the wrong side of the
event.

### The instruments

Three things exist to be looked at rather than measured:

* **`omk-play`** — the game, and also a free-look viewer for one set, on the
  software, Vulkan or GLES renderer. It draws through the same boundary the
  checks measure, so a fault you can see in it is a fault in the thing they
  check. Its harness flags reach a flow without the playthrough that would.
* **four web viewers** that read the data directly rather than through the port
  — a conversation player, a cutscene player, the menus, and the world scripts
  as annotated listings.
* **`tools/sim`** — a Python model of the runtime that the C++ is compared
  against.

### Where the port stands

The coverage audit in `engine/README.md` §Coverage is the authority, written out
row by row because it has been wrong twice. As of 2026-09-29 it still reads:
**31 rows fully ported**, **7 partly**, **0 lifted as a table but never
consumed**, **0 unported**, and **3 that are not portable subjects** — the
simulator, the UI model and the trace rig, which are this project's instruments.
The 41 rows predate most of chapter 6; melee, shoot mode's runtime, the slider,
the water and the falls, the shadows, the lights and the street life are
recorded in dated paragraphs above the table rather than as new rows. What is
left on the table's own terms is essentially **device** and **native code
outside the decompilation**: DirectSound's mix, and the screen callbacks that
were never recognised as functions.

`verify.py --list` counts 485 entries on 2026-09-29: 484 checks, 257 of them
behind `--slow`.

## Where it lives

| | |
|---|---|
| the standard | `docs/PORTING.md` — Part A is this chapter, Part B is chapter 12 |
| the audit | `engine/README.md` §Coverage |
| the enhancements | `todo/enhancements.md` |
| the speed record | `todo/optimization.md` — the steps table, §7 (the decision and its correction), §15–§18, §25, §27 |
| the handheld | `todo/handoff-vita-port.md` (the state), `todo/vita-port.md` (the plan and the record), `todo/gpu-skinning.md`, and the older `todo/handoff-vita.md` (the pre-port decision) |
| the viewer's split | `todo/play-split.md` |
| the build | `engine/Makefile`; `engine/backends/vita/CMakeLists.txt` for the console |
| the enforcement | `verify.py: porting standard`, which asserts the audit's counts sum and that every item the standard calls unfinished is still called unfinished |

## What is not settled

* **30 fps on the Vita is open.** Nothing since the 2026-09-27 CPU-path log has
  been measured on a console as a whole frame in the city: not the GPU posing
  with its odd-slot fix, not the state cache, the music table or the allocation
  cut. The 2026-09-14 decision's numbers are the M1's, and its CPU factor was
  never measured, only framed.
* **The depth tie baked at load (§27) is a plan.** Its premise — that a
  coincident pair keeps its winner for the life of a load — is argued from the
  engine's draw order and from the pairs sitting inside one rigidly moving
  mesh; the census that would find a pair spanning two independently moving
  meshes has not been run.
* **GPU skinning has no `verify.py` check for the walkers and the staged
  bodies** (steps 3–4): `omk-play-gles` needs a real GL window, which a check
  must not open, so those proofs are recorded runs; `engine: gles pose` covers
  the maths through a windowless probe. The player (step 5) is still posed on
  the CPU.
* **The films on the console**: only GAME has been seen to play, too fast
  against its sound; the fixes since are unconfirmed there.
* **The port has been built and run on macOS on Apple Silicon, in the Vita3K
  emulator and on a PS Vita.** Its core has no operating-system conditional, so
  it is portable by construction, but that is a property of the source, not a
  tested fact on Windows or Linux. Case-sensitive filesystems will break some of
  the *Python* tools, which let the host do the resolving.
* **The Vulkan backend has no reachable tier**, by construction: its correctness
  is inherited from the software backend it mirrors (`docs/PORTING.md` §B6).
  The GLES one is measured by coverage against the software reference (0.9961
  plain, 0.9977 dithered, with the 565 present exact, on an M3 —
  `todo/vita-port.md` §0), with one recorded blind spot: the probe's camera does
  not exercise the cutout rule.
* **A question found on the way and not changed**: `shadowBonesFor(-1)` returns
  no bones, where its own comment reads the engine's switch as giving an npc at
  detail 0 the chest's shadow. The engine's switch decides it, and it has not
  been re-read (`todo/optimization.md` §18).
* **The trace rig is macOS-only** — it drives the original under CrossOver and
  captures with a platform screenshot tool.
