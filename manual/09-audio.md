# 9. Audio

← [Contents](README.md) · prev: [Rendering](08-rendering.md) · next: [The interface](10-the-interface.md)

---

## In short

The most useful thing to know about the game's audio is that **there is no
mixer in it**.

The engine opens one DirectSound primary buffer, sets it to 22 050 Hz stereo,
starts it looping, and after that every sound is a secondary buffer that
DirectSound itself sums. Nothing in the executable ever adds two samples
together. What the engine does is *decide*: which buffer, where in the world,
how loud, and when to stop it.

That changes what a port of it can even claim. The decisions are portable and
checkable — a bank of buffers, a pool of voices, a listener, a volume law. The
sound that comes out is the operating system's, and no rig in this repository
records audio, so the attenuation curve and the panning have **no reachable
tier at all**. They are written down as unverifiable rather than quietly
implemented as though they were established.

The one part that *is* exact is the decoder. The game's speech, its music and
its facial animation all use one variant of ADPCM, and the port's decoder is
sample-identical to an independent reader across all 777 dialogue files —
225 441 216 samples.

Since the last revision of this chapter the port's audio has been reshaped for
a small machine, the PlayStation Vita, without changing a sample: the music is
decoded **as it plays** from the open file rather than whole up front, sounds
are converted to the device format **once** and kept, and the ADPCM nibble law
goes through a **precomputed table**. Each of those is proved identical to what
it replaced.

## In detail

### What `Sound_Init` actually sets up

```
primary buffer:  PCM, 2 channels, 22050 Hz, 16 bits,
                 block align 4, 88200 bytes/sec
                 Play(0, 0, DSBPLAY_LOOPING)   — once, at startup
```

(`docs/ASSETS.md` §3c, `docs/PORTING.md` B6.) Everything after that is a
*secondary* buffer. The engine keeps a **160-buffer bank** and a **16-voice
pool** of 64-byte records; a buffer asks for `CTRL3D` when the 3D switch is on
and for `CTRLPAN` only when it is off, because a 3D buffer's pan belongs to the
listener. Freeing a buffer first stops every voice that is a duplicate of it.
The interface plays its sounds with no placement block at all, which is
`DS3DMODE_DISABLE` — flat.

The listener is handed a distance factor of **`0.0254`** — metres per inch —
which is the engine telling DirectSound, in its own units, that **the world
unit is an inch**. Its 48-byte struct is in the order position, top, front,
velocity, which is *not* DirectSound's order, so a straight copy would swap
the orientation for the velocity (`docs/ASSETS.md` §3c).

The volume law is an **attenuation**, not a gain: `-10000 * pct / 100`
hundredths of a dB, so 0 is full and 100 is silent.

Three wrappers — set frequency, get frequency, length in milliseconds — are
**dead code**: no direct callers and no address held anywhere in the image,
which is why they are in no decompilation. `Wav_LoadToBuffer` accepts all
**61** shipped interface `.wav`; two of its branches are never taken by the
shipped corpus (`docs/ASSETS.md` §3c, `verify.py: engine: audio`).

### Grades, stated once

`docs/PORTING.md` B6 grades the audio row several ways, and the grades matter
more than the list:

| part | tier |
|---|---|
| the loader, and the immediates and vtable offsets asserted against the image | **2** |
| `Sound_LengthMs` and the reference mixer's transparency | **3** — both sides written from one reading |
| the bookkeeping (bank, voices, listener, volume law) | **6** |
| the attenuation and pan law | **none** — DirectSound's, described nowhere in the image |

The one waveform claim that *is* met is transparency: a mono voice, not 3D, at
full volume and at the mix rate comes out of the reference mixer unchanged in
both channels (`men001.wav`), with `pause.wav` at 22 080 Hz as the control that
must differ (`verify.py: engine: audio`).

### Pausing is a sound decision too

Screen 31, the pause, keeps the world *drawn* but stops it ticking, and its
own open and close callbacks do the sound: the open calls `sub_46C290`, which
walks the sound bank, and three more suspend routines directly; the close
calls their partners (`docs/UI.md` §3b, "Which screens stop the world"). The
port's own reading of the four pairs, in the comment above its pause handling
in `engine/backends/sdl/play.cpp`, is that the two streaming handles restart
from a **saved position** — a suspend, not a stop — and the port reproduces
that by flushing what it has queued and not pulling while paused.

A second mechanism reaches the same routine: any screen that hides the world
calls `sub_466B30`, whose arms include the sound-bank suspend. That arm is
**not ported** — the mixer has no suspend API — and is filed as such
(`docs/UI.md` §3b).

### The decoder

OTNS ADPCM, transcribed from `sub_483200`. It decodes the audio embedded in
every `.3DM` morph file — the voice recording the facial animation was made
against — and it is checked against `tools/adp.py`, an independent Python
decoder, over the whole corpus: **777 of 777 files, sample-identical**
(`docs/PORTING.md` A5, `verify.py: engine: morph+ADPCM`).

**The nibble law is now a table** (`todo/optimization.md` step 16). A channel's
whole state is its predictor and its step index, so what one nibble does is a
function of (index, nibble) alone: a signed delta added before the clamp, and
the next index. The port builds those **89 × 16** entries once, and the voice
decoder and the music stream both look them up. The claim is exactness, and
`engine/tools/adpcm_equiv`, which keeps the old branching law verbatim, shows
it three ways: **all 93 323 264 channel states**, every one of the **145**
music tracks decoded whole and streamed, and every voice-over (17, in the
step's own count); and
`engine: morph+ADPCM` still holds 777/777 against `tools/adp.py`. The check is
`engine: adpcm table`, shown to fail — clamping the next index at 87 turns it
red on 3 145 728 states, 50 tracks and 5 voices.

What the step measured, on an M3: a whole-file decode of all 145 tracks
**1469 → 1012 ms**, and 120 s of music pulled in quarter seconds
**52.0 → 31.7 ms**. The step also records its own correction: the audit had
called the music "0.5–0.6 ms a frame" from an *uncapped* `--frames` run, which
does not pace audio in real time; paced at 30 fps the same machine reports it
at 0.0–0.1 ms. **A span read off an uncapped run measures the run, not the
game.** What the Vita gains from the table is not measured.

### Music

Music is `music.play`, opcode 103: `Music_PlayTrack` builds `TRACKS\<n>.ADP`
from the first field and streams it, the second field is the loop flag and the
third selects the stream buffer size; the handler skips a request for the track
already playing. **518 of its 521 sites** name a file that exists among the
**145** shipped tracks, and the other 3 are track 0, which the callee refuses.
The area header does the same from **`AREA +142`**, under the same guard —
**189 of 189** non-zero values name a real file (`docs/SCRIPT_VM.md` §103,
`verify.py: music.play`). `music.volume`, opcode 131, ramps the same
attenuation over a number of frames; 52 of 52 sites stay inside 0..100, and
36 of them fade the music out (`docs/SCRIPT_VM.md` §131,
`verify.py: music.volume`).

**How the port plays it** is the part that changed. A stereo `.ADP` has no
header and no blocks — one byte is one frame — so nothing forces a track to be
decoded ahead of time. It used to be: first the whole track resampled to floats
at the device rate, then (the first memory cut, 2026-09-13) the whole track
kept at its own 16-bit 22 050 Hz and resampled as it was pulled
(`todo/optimization.md` step 6). On a console that was fatal — the city's
three-minute track asked the heap for **16 MB contiguous**, twice, and the Vita
refused it as Anekbah loaded (`todo/vita-port.md`, 2026-09-21). Now:

* the player keeps the **file**, open, and decodes forward as the device pulls
  (`AdpcmStereoStream`), restarting the decoder on the loop's wrap; and since a
  later console log showed that even reading the whole 4 MB file at a music
  switch stalled a frame, it is read in 32 KB windows as it plays;
* the device is topped up **a quarter second at a time**, with a whole refill
  only when less than 0.3 s is queued — a whole second decoded at once had
  cost a console 25–34 ms on the frame it happened (`todo/vita-port.md`).

Each version is proved against the one before it by `engine/tools/music_equiv`,
which keeps the old player verbatim: **0 mismatches over 425 million samples**
for the rate change (tracks 2, 15, 29, 79, 80, 86), over 82 million for the
stream and over 56 million for the windowed read, looped and not
(`todo/optimization.md` step 6, `todo/vita-port.md`;
`verify.py: engine: music storage`, shown to fail by wrapping one frame early).

### The interface sounds

45 of them, ids 0..44, named by a table of 45 × 20 bytes that ends exactly on
the next label. All 45 resolve to a shipped `.wav` — 61 ship, so 16 are never
named by anything (`docs/UI.md` §3).

The **cache holds 32 slots**, so 13 of the 45 can never be resident together;
the loader takes the first free slot, does not check whether the id is already
cached, and returns silently when it is full (`docs/ASSETS.md` §3c).

Each screen names **twelve** sound ids. Which slot plays when is
`sub_482FE0`, which reads the live input word and picks by bit: **slot 0 on
confirm, slot 1 on back, slot 2 on a selection move, slot 3 on close** — and
there is no screen-opening sound at all. That order was corrected by a reader
**playing** the replica, who heard the validation sound on every move; the
names, the counts and the cache agreed with each other whichever meaning was
attached, so no check could have caught it (`docs/UI.md` §3,
`verify.py: ui sound slots`). The sound is not gated on the dispatch: a frame a
widget consumed still clicks (`docs/UI.md` §3c).

A screen's code also plays sounds **by id** into the 45-row table,
independently of its slots: the sneak's consumable arm plays id **13**
(`SNK012`) and its successful combine id **12** (`SNK009`)
(`docs/UI.md` §3b, the sneak's verbs).

### A sound in the world

`Script_PlaySound`, the scene programs' own sound call, positions its sound at
a node. Its sibling `Script_PlaySyncSound` looks like the same function and is
not: **parameter 1 is the frame to fire on** in the second (475 sync cues, 467
of them inside their editing's duration) and **a loop flag** in the first.
Decoding both alike invents a cue time for every call of the first
(`docs/CUTSCENES.md` §5, `verify.py: cutscene sound`).

A `.CTL` state's footstep and effect sounds name an **id**, which is searched
for in the resident scene's sound records — so the same id names different
sounds in different scenes, and a state's footstep is silent in a scene that
does not carry it, which is the engine's own behaviour (`docs/ASSETS.md` §7,
the `.CTL` effect records; `verify.py: engine: actor sounds`).

A fight loads **`fight.scx`** the way the boot loads `aventure.scx`: 21 sounds
(ids 402..423 — the punches, the kicks, the block, the fall, the cries) and 16
sprites that exist nowhere else. The port's missing fight sounds and sprites
were one cause, that load (`todo/fight-mode.md` §15.2,
`verify.py: engine: fight library`).

In shoot mode the hurt sound when a bolt hits the player is **flat, not
positional** (`todo/handoff-shoot-mode.md` §4). And the gunfire *noise*
(`sub_4246E0`) is not a sound at all: every shot and every impact alerts the
gunmen on that floor within a hearing range in map cells — the input shoot
mode's brains react to (`todo/shoot-mode.md`).

The slider's motor, `SOUNDS\sliderm01.wav`, is placed with a max distance of
**585.0** that doubles as its audibility cut-off. The directory's other file,
`pluie.wav`, **cannot be reached**: the executable holds no path template that
could name it (`docs/ASSETS.md` §3b and §3c, `verify.py: SOUNDS folder`).

**How the port plays an effect** (`todo/vita-port.md`, the city's frame).
Every effect and scene sound used to be decoded from its WAV and resampled
again on each play, which a console log showed as 66 ms frames. Converted
sounds are now **cached by their bytes' address and size** — the global,
fight and shoot libraries stay resident, and the scene's entries are cleared
when the resident scene changes — and `wavToDevice` writes an exact 1, 2, 4 or
8 times rate multiple without a per-sample multiply. Its **189 conversions**
(63 WAVs at three rates) are identical to the old loop, with a channel swap as
the mutation that is caught. A dialogue line's voice takes the same kind of
shortcut: its ratio to the device is exactly two.

### The voice-overs

Outside a conversation, `media.play` (op 92) names an `IAM\OBJECT` record whose
stem is a `VOICEOFF\*.ADP`, and plays it through the **same streamer** a
`.3DM` line uses — 22 080 Hz, mono, 30 fps, with no morph tracks — so only one
media voice sounds at a time (`docs/CUTSCENES.md` §5).

**Only 10 of the 561 such objects have their own file on the disc**, and that
is *not* a gap. The handler rewrites any name beginning `ZVOT` or `ZVOP` to
`JINGOFF3.ADP`, a 2.08 s jingle that ships, so the partition is **10 with their
own file, 520 that play the jingle, and 31 genuinely silent**
(`docs/CUTSCENES.md` §5; `verify.py: engine: voice over`, `cutscene music`).
The resolution is tier 4 against the golden traces' own media ids, the decode
tier 3, and loudness and placement have no tier (`engine/README.md` §Coverage).

### The movies

The three intro movies carry **44 100 Hz** audio and go straight to the device.
They never went through the game's 22 050 primary buffer — the original played
them through DirectShow, which had its own output — so a replica that fed them
through the ported path would be wrong about both the rate and the route
(`docs/PORTING.md` A5, `verify.py: engine: movies`).

## Where it lives

| | |
|---|---|
| the findings | `docs/ASSETS.md` §3b and §3c (the sound path, the loader, the cache), `docs/SCRIPT_VM.md` §103 and §131 (`music.play`, `music.volume`), `docs/CUTSCENES.md` §5 (sync cues, voice-overs), `docs/UI.md` §3 (the slots), `docs/PORTING.md` A5 and B6 |
| the port | `engine/src/audio/` — `mixer.*` (the bank, the voices, the listener), `voiceover.*`, `music.*`; `engine/src/formats/adpcm.h` (the table and `AdpcmStereoStream`); `engine/src/app/playhelpers.*` (`wavToDevice`) |
| the equivalence tools | `engine/tools/adpcm_equiv.cpp`, `engine/tools/music_equiv.cpp` |
| the reference decoder | `tools/adp.py` |
| the task records | `todo/optimization.md` steps 6 and 16, `todo/vita-port.md` (the city's load and frame) |
| the checks | `engine: audio`, `engine: morph+ADPCM`, `engine: adpcm table`, `engine: music storage`, `engine: audio queue bound`, `engine: voice over`, `engine: stop sound`, `engine: scene sounds`, `engine: actor sounds`, `engine: fight library`, `engine: movies`, `.3DM files`, `music.play`, `music.volume`, `cutscene sound`, `cutscene music`, `ui sound slots`, `SOUNDS folder` |

## What is not settled

* **The attenuation and pan law is DirectSound's**, is described nowhere in the
  image, and no rig here records sound. It has **no reachable tier**, and the
  port's `render()` says so in its own header.
* **The Vita audio changes are not measured on a console.** The table, the
  quarter-second pulls and the sound cache were measured on a Mac and proved
  equal there; whether they remove the console's 25–34 ms and 66 ms frames is
  what the next console log answers (`todo/handoff-vita-port.md` §4).
* **The converted-sound cache is unbounded** — listed as open in
  `todo/optimization.md` (the RAM row).
* **The per-screen slots are not fired by the headless widget walk**, per the
  coverage audit (`engine/README.md` §Coverage), and the special screens' own
  sounds are labelled unported in their sources (`docs/UI.md` §3k).
* **The sound-bank arm of `sub_466B30`**, the suspend every world-hiding screen
  triggers, is not ported.
* A state's footsteps re-arm on the clip's wrap in the port; what the engine's
  latch reduces to for an open-window record is **not traced**, and the port
  labels its rule a reconstruction (`docs/ASSETS.md` §7).
