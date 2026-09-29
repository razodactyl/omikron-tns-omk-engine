# 2. Boot, and the frame

← [Contents](README.md) · prev: [The original](01-the-original.md) · next: [The data](03-the-data.md)

---

## In short

Double-click the icon and the game does five things: it starts its subsystems,
plays three MPEG films (`EIDOS`, `QUANTIC` and `GAME`), loads a scene file
called `aventure.scx`, puts up a splash bitmap, and enters a loop that runs one
frame at a time until you quit.

Three things about that are worth knowing before anything else.

**There is no "menu state".** The start menu is not a special mode the engine
enters. `aventure.scx` is the game's global library of effect sprites and
sounds, and the menu is a screen that the first area's own startup script
opens, the way any script asks the player a question. What you read as the
game's front end is the ordinary script and screen machinery.

**The game counts in frames, not seconds.** Every clock in the engine —
animations, camera moves, the flicker of a neon sign — is measured in units of
one-thirtieth of a second, because the frame delta is computed as `30 / fps`.
At 30 fps that is exactly 1.0. Nobody chose this as a convention; it falls out
of a single division, and it is why every duration in this repository is
quoted in frames.

One consequence is a gameplay fact rather than an implementation detail: the
delta is **capped at 3.0**, so below 10 fps the game slows down rather than
taking bigger steps. A replica that integrates real elapsed time on a slow
machine is not being more accurate — it is being wrong. That rule turned out
to matter in practice: the port's first console runs on a PS Vita, at a few
frames a second, "shook" until the port adopted it.

**And Escape is not a key the game binds.** The loop reads it by hand, one
instruction before the frame, and opens the pause screen directly. Pausing
also silences the sound, because that screen's own open and close callbacks
suspend it.

## In detail

Everything here is read from `Game_Main` (0x00439470), `Game_RunLoop`
(0x00439310) and `Game_Frame` (0x0041F740), with the constants read out of the
executable itself rather than off the decompilation (`docs/BOOT.md`).
`verify.py: boot sequence` asserts them.

### The chain

```
WinMain               parse the command line: WINDOW, NOFMV, CONFIG
  Game_Main
    Game_Init                 subsystems
    Movie_Play  FLIS\EIDOS.MPG      | each guarded by the skip latch
    Movie_Play  FLIS\QUANTIC.MPG    |
    Movie_Play  FLIS\GAME.MPG       |
    Game_Start("aventure.scx")      the boot scene
    sub_420A20("IMAGES\OMIKRON.BMP")
    Game_RunLoop                    ... until WM_QUIT
    Game_Shutdown
```

`Game_Main` opens with `setjmp3`, and the `if` arm is the failure path: any
subsystem that longjmps out lands there, shuts down, and puts up
`"Can't initialize"` in a `MessageBoxA`. The whole boot is one guarded block,
which is why nothing inside it checks a return value (`docs/BOOT.md` §1).

`aventure.scx` is the game's **global effect and sound library** — 20 effect
sprites and 53 sounds — and not a menu (`engine/README.md`, the boot walk; the
correction is logged in `docs/RECONSTRUCTION.md` on 2026-08-31). The start
menu is `ui.open(29, …)` in AREA 118's own startup script, where that script
parks until the screen answers (`CLAUDE.md` §2, on `engine/`; chapter 5) —
which is why the port reaches the menu with nothing hand-wired.

### The three films, and the two different skips

`gamedata/FLIS/` holds exactly three files, and they are plain MPEG-1 Program
Streams — there is nothing proprietary to decode (`docs/BOOT.md` §2). Each is
320×240 at 29.97 fps with one 44 100 Hz audio stream (`engine: movies`):

| file | bytes | dated | length (`engine: movies`) |
|---|---|---|---|
| `EIDOS.MPG` | 5 714 716 | 28 Sept 1999 | 13 s |
| `QUANTIC.MPG` | 10 265 108 | 28 Sept 1999 | 23 s |
| `GAME.MPG` | 46 359 152 | 5 Oct 1999 | 107 s |

They are also the first thing in the game that cannot work on a
case-sensitive filesystem, before any asset and before any archive: the
**executable spells them `FLIS\EIDOS.mpg` in lower case and the disc ships
`EIDOS.MPG` in upper**, and not one of the three uppercase forms appears
anywhere in the image.

The whole block is skipped when the `NOFMV` command-line word is present, and
also when the machine reports no movie playback. Each call is separately
guarded, and the poll callback handed to every `Movie_Play` is what makes the
skip work:

```c
if (!dword_52DD54) {
    Input_Poll(&v2, &v1);
    if (v1) {
        sub_43B7D0();              /* stop the movie that is playing */
        if (v1 == 2)
            dword_52DD54 = 1;      /* ... and every one after it */
    }
}
```

So **there are two skips, not one**: any key ends the film that is playing,
and **Alt ends all three** — `Input_Poll`'s second out-parameter is 1 for any
key held and 2 for scan code 56, `DIK_LMENU`, the left Alt. Nothing in the
decompilation ever clears the latch.

Two details survive being reproduced. The scan loop does not break on a hit, so
a key with a code **above** 56 held together with Alt overwrites the 2 back to
a 1 — Alt+Shift skips one film, Alt alone skips three. And the same key that
stopped one film is read again by the next, so holding a key down skips all
three by a different route from the latch.

`Movie_Play` itself (0x0043B4E0) is **not read**: which API decodes and what
its eight parameters are is untraced. Nothing needs it — a replica hands three
MPEG-1 files to a decoder.

### `Game_RunLoop` — a Win32 idle loop, and what gates a frame

```c
while (1) {
    while (!PeekMessageA(&Msg, 0, 0, 0, 0)) {
        if (word_4E7694 && dword_52DD58 && !dword_52DD4C) {
            if (GetAsyncKeyState(27) & 0x8000 && !dword_4E9728)
                UI_LoadScreen(31, -1, -1);
            Game_Frame(dword_4C5944, word_90EF2E);
            dword_4C5944 = 0;
        } else {
            dword_4C5944 = 1;
            WaitMessage();
        }
    }
    if (!GetMessageA(&Msg, 0, 0, 0)) break;
    TranslateMessage(&Msg); DispatchMessageA(&Msg);
}
```

The frame runs only in the message pump's **idle** path, behind three gates
(`docs/BOOT.md` §3): the game is running, the app is foreground
(`WM_ACTIVATEAPP`, stored straight from `wParam`), and a third flag. Fail any
of them and it calls `WaitMessage()`, which **blocks** — alt-tab away and the
process genuinely stops burning CPU rather than spinning.

`dword_4C5944` is what makes that safe. It is set on the way into
`WaitMessage` and cleared after every frame, and `Game_Frame`'s first act when
it is set is to re-baseline all three timers. **The idle gap is discarded, not
integrated** — otherwise coming back from a two-minute alt-tab would hand the
simulation a 120-second delta.

### Escape, and the pause

Everything in this section is `docs/UI.md` §3h. Escape is read with
`GetAsyncKeyState` directly in the loop, so the input system never sees it. It
is **level-triggered** — bit 15, down *now*, not an edge — and guarded only by
`dword_4E9728`, the pause flag, whose only two writes in the whole image are
the pause screen's own open and close callbacks. Holding Escape therefore opens
the screen once; holding it *through* the close reopens it at once.

It calls **`UI_LoadScreen`, not `UI_OpenScreen`**: no answer variable is written
and no script is parked, which is the difference between a screen the world
asks a question with and one the player brings up over it. `UI_LoadScreen` also
refuses the pause while a slot holds a screen carrying `0x20000400` — the start
menu and the save screen.

**What the pause flag does is force the frame delta to 0.0, and that is all**:
every subsystem still runs, on nothing. The sound is a separate mechanism:
screen 31's open callback calls the routine that suspends every buffer in the
sound bank and three more suspend routines, and its close calls their partners
(`docs/UI.md`, "Which screens stop the world"). *Quitter le jeu* on it does
**not** quit the program: its *Oui* sets a request that the next script pump
serves by tearing the session down and running `Game_NewGame`, which walks
back out through AREA 118 to the start menu.

### `Input_Poll`'s own rules

Before any binding is matched, `Input_Poll` (0x0043E0D0) fixes up the keyboard
state: left and right Shift set each other, left Control sets right Control
(one way), and TAB is dropped while Alt is held, so alt-tab never opens the
sneak. The adventure scheme binds *run* to scan code 54, right Shift — so
without the first rule left Shift would reach no binding at all.

A joystick binding code is a byte offset into `DIJOYSTATE` — the X axis at 0,
Y at 4, button *k* at 48 + *k* — and the axes are **hardwired** to the first
four slots, compared against a threshold that no instruction stores to, over a
range of −1000..1000, with no dead zone set by the engine. This section is
`engine: input poll`'s docstring and `engine/src/input/bindings.h`; the check
asserts sixteen cases.

### `Game_Frame`, and where "one frame" comes from

```c
flt_90E174 = 1000.0 / raw_ms;        /* this frame's fps               */
flt_90E170 = 1000.0 / smoothed_ms;   /* smoothed: (prev + raw) >> 1    */
switch ((int16_t)dword_4E972C) {
case 0: flt_4C30D8 = 30.0 / flt_90E170;
        if (flt_4C30D8 > 3.0) flt_4C30D8 = 3.0;   break;
case 1: flt_4C30D8 = 1.0;   break;
case 2: flt_4C30D8 = 0.5;   break;
case 3: flt_4C30D8 = 0.1;   break;
case 4: flt_4C30D8 = 2.0;   break;
}
```

Case 0 is the answer to "why does everything in this repository count frames"
(`docs/BOOT.md` §4). The delta is `30.0 / fps` — thirtieths of a second, one
unit *is* one frame at 30 Hz. At 60 fps it is 0.5, at 15 fps 2.0. Every clock
downstream is in those units: the `.3DA` scene clips, the camera editings, the
ambient-effect periods, an object program's own clock. An ambient cadence once
read as *seconds* ran a whole city 30× too slow, and that is how this constant
was found from the other end (`docs/PORTING.md` A7).

**The clamp is a gameplay fact, not a guard.** At 3.0 the step is at most three
frames, a tenth of a second, so below 10 fps the game slows down.

Cases 1..4 are fixed deltas read as immediates — frame-step and slow-motion
modes. Three overrides sit after the switch: a forced delta (shipped off), a
sync path where the delta becomes the advance of an **external clock** rather
than the wall clock — whatever drives it pulls the simulation along, which is
how a cutscene cannot drift from its soundtrack — and the pause, which forces
the delta to **0.0** and is what the function returns for `Game_RunLoop` to
gate Escape on.

The first half of the function is shorter: re-baseline the timers if resuming,
poll the input, compute the **edge-filtered** input word `a & (a ^ (prev &
prev2))`, and call `Game_Tick()`.

| symbol | value | what |
|---|---|---|
| `flt_4BC1CC` | 30.0 | the numerator — the frame rate the delta is *in* |
| `flt_4BC1EC` | 1000.0 | ms → fps |
| `flt_4BC1F4` | 3.0 | the clamp |
| `flt_4C30D8` | 1.0 | the delta as shipped, i.e. 30 fps |
| `flt_4C30E8` | −1.0 | the forced-delta override, off |

### What the port does with all this

**The chain.** `engine/`'s `platform/boot.*` walks the same chain, and its
announcement stream matches a capture of the original **42 events of 42, in
order, from a cold start** (`engine: boot`, against `traces/intro.log`) —
which is what makes "it boots" a measurement rather than a screenshot
(chapter 12).

**The films, on the desktop.** They are decoded by a **vendored** copy of
pl_mpeg (MIT), which is not a port of anything: the original handed the files
to DirectShow and MCI, and its import table says so (`engine: movies`). Their
44 100 Hz sound goes straight to the device, because the game's own primary
buffer is 22 050 Hz and the films never went through it; routing them through
the ported audio path would be wrong about both the rate and the route
(`docs/PORTING.md` A5). Four rules of the port's own, none of them the
engine's, keep the picture in step with that sound on a slow machine (commits
`71a54b2`, `2340f36`, `c893c13`, `9a3a7e4`):

* the film is fitted to the display at the largest scale that fits both axes,
  keeping its 4:3 ratio;
* a frame that is **late** against the sound is decoded — MPEG cannot skip
  one — and **dropped**, but never more than **three in a row**, so a machine
  that cannot decode a frame in a frame's time still shows one in four, a
  slower picture still in step with its sound;
* the sound is decoded **half a second ahead** of the picture rather than as
  fast as the decoder will go — which, on a run with no hardware copies, had
  decoded all 107 s of `GAME`'s sound on its first frame and run the Vita
  emulator out of memory;
* any **pad button** skips a film, as any key does, and the button must come
  up before the next film (and before the menu), so one press does not skip
  them all or confirm the menu behind them.

**The films, on the PS Vita.** pl_mpeg on the console's CPU showed 41 of
`EIDOS`'s 386 frames (`todo/vita-port.md`, "The films are decoded in
HARDWARE"). So the Vita plays **H.264 copies** of the three films, made once on
the desktop by `scripts/vita-movies.sh` (Baseline profile, AAC sound, 320×240),
through the console's hardware decoder, **SceAvPlayer**, converting its NV12
frames to RGB565 — the chroma is U first, and read the other way `GAME`'s red
cap drew blue (commit `223eac4`). The MPEG-1 path is the fallback when the
copies are absent. This is a platform substitute for a platform service, like
pl_mpeg, and carries no claim about the original.

Its state on the console is **partly established**. The decoder at first
reported the films stopped with nothing shown, because vitaGL had taken every
byte of the memory SceAvPlayer needs; with the port's own vitaGL build leaving
16 MB to the system, the reader saw `GAME` play on 2026-09-23 — its picture
running fast against its sound — while `EIDOS` and `QUANTIC` were still
skipped (`todo/handoff-vita-port.md` §3b–3b2). Since then the strategy that
played is the default for all three and the loop takes sound only as it plays,
which paces the picture (`todo/vita-port.md`, 2026-09-23 midday; commit
`f11e058`). No console log in the record yet confirms all three films playing
at the right speed.

**The frame delta.** The port takes the engine's rule as it is: `30 / fps`,
capped at three frames. Until 2026-09-22 it did something else — a frame over
0.25 s was treated as a hitch and fed the game 1/30 — and a console whose
frames straddled 0.25 s then alternated six frames of motion with one, which
the reader saw as the camera "shaking". The engine's cap replaced it
(`todo/vita-port.md`, 2026-09-22; `engine/backends/sdl/play.cpp`, "THE
ENGINE'S OWN CLAMP"). A frame-bounded headless run keeps a fixed 1/30 instead,
so the checks stay deterministic.

**The pacing.** A frame is paced to a **deadline**: each ends on a 1/30 s grid
of the performance counter rather than sleeping "33 ms minus what was spent",
which had held the capped street at a flat 28.8 fps; with the deadline it holds
30.0. A late frame shortens the next, and a stall of more than a frame
resynchronises instead of bursting (`todo/optimization.md`, "Capped at 30").
The simulation steps on the measured delta either way; the pacer only decides
when the frame is presented.

**Escape and the pause.** Escape opens screen 31 by the engine's own route —
polled beside the frame, not bound to an action — and the pause is a zero
delta rather than a mode, with the sound bank and both streams suspended the
way screen 31's callbacks suspend them. The route and the zero delta were
**confirmed in play** on 2026-09-07 (`todo/omk-play.md` 82; `engine: pause`).
*Oui* on *Quitter le jeu* ends the run and says so, because the restart it
should perform is not ported (below).

## Where it lives

| | |
|---|---|
| the finding | `docs/BOOT.md`; the pause in `docs/UI.md` §3h; `Input_Poll`'s rules in `engine: input poll` and `engine/src/input/bindings.h` |
| the checks | `verify.py: boot sequence`, `menu open site`, `engine: boot`, `engine: movies`, `engine: pause`, `engine: input poll` |
| the port | `engine/src/platform/boot.*`, `movie.*`; the film loop, the frame delta and the pacer in `engine/backends/sdl/play.cpp`; `engine/src/input/` for the poll's rules |
| the Vita films | `engine/backends/vita/avmovie.*`; `scripts/vita-movies.sh` makes the H.264 copies |
| the frame oracle | `traces/intro.log` — the original's own announcements |

## What is not settled

* **`Movie_Play`'s decoder and its eight parameters** are untraced. Nothing
  here needs them, but the row is open rather than closed.
* **What the external clock actually reads.** It is sampled where an audio
  position would be, and that is an inference from where the call sits, not a
  fact (`docs/BOOT.md`, "What this does not settle").
* **The fixed-delta modes** (`dword_4E972C` cases 1..4) are clearly debug
  modes, and nothing observed has ever been seen to set them.
* **Two sentences in `docs/` disagree with later readings**, and a reader of
  the documents should know which is current. `docs/BOOT.md` §1 still says the
  main menu *is* `aventure.scx`; the correction — a global effect and sound
  library, the menu opened by AREA 118's script — is in `engine/README.md` and
  the 2026-08-31 log row. And `docs/UI.md` §3c says the joystick's axes are
  tested "against a deadzone", where `engine: input poll`'s later reading of
  the same function finds a threshold nothing stores to and no dead zone of
  the engine's own. This chapter follows the later readings.
* **The pause's *Quitter le jeu* does not restart the game in the port.** The
  engine serves it as a new game; `omk-play`'s boot is not yet a function it
  can call again, so the run ends instead (`docs/UI.md` §3h).
* **The Vita's films are not confirmed at speed on a console** — one of the
  three was seen playing, too fast, before the pacing fix (above).
* **A faithful clamp is not a playable rate.** At the Vita's measured frame
  times the engine's rule makes the game run slowly but correctly; the port is
  right to do so, and the console is still far from 30 fps (chapter 1).
