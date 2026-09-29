# 3. The data

← [Contents](README.md) · prev: [Boot, and the frame](02-boot-and-frame.md) · next: [The script VM](04-the-script-vm.md)

---

## In short

The shipped game is about 1.7 GB in 2 812 files (`CLAUDE.md` §0, the inputs
table), and almost all of the game is in there rather than in the executable.
The directories that matter:

| | |
|---|---|
| `IAM/` | the **archives** and the plain state files: conversations, the world scripts, the areas and scenes, the object records, the interface text, the new-game state, the saved games |
| `MESHES/` | models, sets and their textures |
| `SCPTDATA/` | the scene scripts, their animation clips, paths and sound tables |
| `MORPH/` | facial animation with the voice recording inside it |
| `FONTS/`, `I2D/`, `IMAGES/`, `MAP2D/` | the interface and the maps |
| `FLIS/` | the three intro movies |
| `VOICEOFF/`, `SOUNDS/`, `TRACKS/` | speech, effects and music |

There are two shapes of file. Some are **archives**: one file holding hundreds
of numbered chunks, with a directory of offsets at the front. The rest are
ordinary files in a dozen small formats, each read by one loader in the engine.

The formats are *plain*: a record is a struct, an array is an array, a count
is usually right there, and many files are the loader's memory image of
themselves. They are not all raw — textures are LZ-packed and the audio is
ADPCM — but nothing is encrypted, and every packing scheme has been decoded
exactly. That is what made the whole project tractable.

A file is also often more than its name says. The `MAP2D` files are the maps
the player sees — and also the navigation grid shoot mode's gunmen walk by. A
mesh flag that makes a skyline shimmer also marks the bed of a canal you can
swim in. Nothing in the data announces that; each was found by reading the
code that uses it.

## In detail

### The IAM container

The archives — `IAM\DIALOG`, `IAM\AREA`, `IAM\SCENE` among them — are flat
files read by `Archive_ReadChunk` (0x0040FF90) (`docs/FILE_FORMATS.md` §1,
*verified*):

```
offset 0   directory: an array of 8-byte entries
             uint32 offset      absolute, from the start of the file
             uint32 size        in bytes
             (0,0) means "no chunk with this index"
offset N   payload
```

The directory ends where the first payload byte begins, so **its length is
implied rather than stored** — `IAM\DIALOG` has a 4 096-byte directory, so 512
entries, 420 of which hold data (§1). The loader computes an entry's address as
`(index >> 8) * 2048 + 8 * (index & 255)`, which is `8 * index` written the long
way: it reads a 2 048-byte sector and indexes inside it. Called with a positive
fourth argument, `Archive_ReadChunk` skips the directory and the caller
supplies the offset and the stride.

Because the offsets are **absolute**, a copy that gains even a few bytes shifts
every chunk after them while the directory still reads cleanly. The port met
exactly that on the PS Vita: an FTP client in ASCII mode treated every
extension-less IAM file as text and turned each CR LF into CR CR LF, the
console's `IAM\AREA` came out 1 253 386 bytes against the shipped 1 253 376,
and area 118 then read with no set and no startup script, so the game never
left its splash (`todo/handoff-vita-port.md` §3; `todo/vita-port.md`). The
archives must be copied in binary mode. The port's loaders now read a file
whole or report the shortfall (`readWholeFile`, `engine/src/platform/datafs.h`),
and the session logs the file's size and hash when an area chunk comes out
with neither a set nor a startup script, which separates a bad read from a bad
copy.

### Three files in that directory are not archives at all

This is the trap the repository's first ground rule was written for
(`CLAUDE.md` §1's table), and it cost real work twice:

* **`IAM\GLOBAL` parses plausibly as an archive** and is not one. `Global_Load`
  (0x0040DE60) opens it directly — a plain file with a fixed header. Reading it
  as an archive lost 2 of its 10 scripts and 86 trigger sites, silently
  (`docs/FILE_FORMATS.md` §5d). The header's walk lands exactly on the file
  size (6 584 + 44 × 4 = 6 760), and its `+32` is the weapon table: five
  ammunition types, then the guns that fire them, index-aligned (§5d;
  `verify.py: weapon table`).
* **`IAM\START` does not fit that header either**, and the conclusion "a
  different format" was also wrong: `Game_NewGame` hands it to `State_Apply`.
  It is **the new-game save** — the 8 192-byte game state as it stands before
  the first frame (`docs/GAME_STATE.md` §1) — and reading a section offset in
  it as a count once produced a confident "5 016 scripts".
* **`IAM\GAMES`** is created at run time and is not shipped: a 3 496-byte
  settings header and 256 slots of 32 808 bytes (`docs/GAME_STATE.md` §8).
  Chapter 5 has it.

`GLOBAL` and `START` were settled the same way: by finding the loader. A
layout that fits the shipped bytes looks exactly like a correct one.

### The format families

Each is one loader in the engine and one reader in `engine/src/formats/`. The
numbers are what the checks assert:

| format | what it is | established | source |
|---|---|---|---|
| `.3DO` | models, characters and sets — and the **lights** inside them | 635 models; 4 179 light records over 216 models; 99.9986% of 33 370 836 bytes claimed, 611 models byte for byte | `FILE_FORMATS` §5b; `verify.py: .3DO bytes`, `light record`, `engine: 3DO` |
| `.3DT` | textures: a palette, then LZ-packed pixels, in material order | 2 534 of 2 534 decode to exactly width × height; every file's size equals the sum over its materials | `ASSETS` §3; `textures`, `engine: 3DT` |
| `.ani` | animation libraries | 243 362 of 243 362 unit quaternions | `ASSETS` §7; `.ani quaternions` |
| `.CTL` | the actor state machines — saved memory images whose pointers the loader recomputes | 7 of 7 walks exact; 398 clips; all 2 044 graph edges resolve | `ASSETS` §7 `.CTL`; `.CTL walk`, `ctl transitions` |
| `.SCX` | scene scripts — objects, programs, camera editings, effect sprites | 220 of 220; 4 511 objects | `FILE_FORMATS` §5c; `.SCX scenes`, `engine: SCX` |
| `.3DA` / `.3DP` | scene animation clips and authored paths | 1 490 of 1 490 embedded clips parse; the header duration equals the last key's frame in all 6 756 paths | `ASSETS` §7 `.3DA`, `.3DP` |
| `.3DM` | facial animation with its voice audio inside | 777 of 777 files, sample-identical | `FILE_FORMATS` §5; `.3DM files`, `engine: morph+ADPCM` |
| `.SFX` | a scene's sounds, shot sprites and ambient effects | 67 of 67, a six-section walk exact | `verify.py: sfx files` (see *What is not settled*) |
| `.OPT` | the city's traffic circuits | 7 blocks, 6 of 6 files exact | `STREET_LIFE` §2; `opt tracks` |
| `.MPT` (`MAP2D/`) | a location's map, and the shoot AI's navigation grid | 16 files, 79 floors; the walk lands exactly | `FILE_FORMATS` §5b5; `map2d`, `map2d grid` |
| `.FNT` | the interface fonts | 2 899 glyphs, none outside its file, none overlapping | `docs/UI.md`; `engine: fonts` |
| `IAM\AREA`, `SCENE`, `GLOBAL` | the world scripts and trigger zones | 5 785 of 5 785 script slots decode; 4 558 zones, none malformed | `SCRIPT_VM`; `world scripts`, `zone records` |

Two audio formats sit under that: **OTNS ADPCM**, transcribed from
`sub_483200` and sample-identical across all 777 morph files
(`engine: morph+ADPCM`), and plain `.wav` for the interface and effect sounds.

### One file, two jobs

**`MAP2D/*.mpt`** is loaded with an area that names one at `+106` (16 areas
do, all shipped except the cut `ARCHIV04`; `FILE_FORMATS` §5b5). Per floor it
carries a grid of byte cells — the picture of the in-game map and its reveal
state — and it is also what shoot mode's gunmen think with: `Shoot_Think`
converts a gunman's world position into a cell of his floor and tests cells
before choosing a destination, and a gunman stamps 0x80 over the cell he
stands in, so the grid is also the engine's occupancy map. The data agrees 16
of 16 both ways: every area whose scripts carry a shoot opcode names a map,
and the two maps left over belong to the areas the Supermarket and Toits
gunfights are loaded over (§5b5; `verify.py: shoot arenas`). Another section
of the file holds the patrol routes — 53 shipped, 51 closed rings and 2
two-point sentry beats (`docs/RECONSTRUCTION.md`, 2026-09-12; `verify.py:
shoot patrol`). Chapter 6 has the gunmen.

**Mesh flags** do the same kind of double duty. `0x8000000` makes a mesh's
vertex colour oscillate on the frame clock — the shimmer, over 233 set meshes
(`ASSETS` §4c; `shimmer table`) — and in Jaunpur it is also what marks the
canal's bed and banks. `0x20000000` marks the water's *surface*, the same bit
the step refusal and the camera's see-through test name (`ASSETS` §7, "The
water"). And the vertex record carried a **normal** at `+12` that nobody had
read until the crowd's dynamic lighting needed it: twelve bytes skipped since
the format was first decoded, fixed as a normal because `sub_493E40` dots it
with the light direction (`FILE_FORMATS` §5b, "The vertex record").

### Case, and why it needed a class

The game shipped for Windows 95/98, whose filesystem ignores case, so the
authors typed whatever they liked: the executable asks for `FLIS\EIDOS.mpg`
and the disc holds `EIDOS.MPG` (`docs/BOOT.md`), and eight of the 67 scene
sound files spell the extension `.Sfx` or `.SfX`. Five checks once measured 59
of those 67 while reporting a total, because they globbed two spellings; a
`.3DM` sweep once reported 708 files where there are 777 (`verify.py:
extension case`, `sfx files`). A count that is quietly short looks exactly like
a count that is right.

So **every data access in the port goes through one class**, `DataFs`, which
resolves case-insensitively: every one of the 2 367 shipped files under `IAM`,
`SCPTDATA`, `ANIMS`, `MESHES` and `MORPH` is asked for again with its whole
path mangled — all upper, all lower, case-flipped — and all come back
(`verify.py: engine: DataFs`). It is also where the write guard lives:
`safeOutputPath` refuses any path inside the shipped tree or carrying a
shipped-data extension, because a tool whose second positional argument was
its output once truncated a 26 308-byte mesh from the 1999 disc to 8 bytes,
and the check that noticed ran afterwards (`CLAUDE.md` §1).

### What cannot be read out of the data

Some tables are compiled into the executable, and a replica cannot recover them
from any file: the VM's 153-entry opcode table and which operand each handler
announces, the interface widget tree, the four control schemes, the
camera-mode presets, the ADPCM coefficients, the 66 special-move rows, the
shoot AI's behaviour scripts and its weapons, the fight AI's eight built-in
moves, the city maps' rectangles. They are lifted to `tables/*.json` — twelve
files, each self-checking and regenerable by `tools/exetables.py --check`
(`tables/README.md`; `verify.py: exe tables`).

This is the one place where the port depends on this repository as well as on
your copy of the game.

## Where it lives

| | |
|---|---|
| the container and the asset formats | `docs/FILE_FORMATS.md` §1, §5–5d; `docs/ASSETS.md` §2–§7 |
| the readers | `engine/src/formats/` — one file per format, `map2d.*` among them |
| all data access | `engine/src/platform/datafs.*` (`DataFs`, `readWholeFile`, `safeOutputPath`) |
| the lifted tables | `tables/*.json`, regenerated by `tools/exetables.py` |
| the Python readers | `tools/omkdata.py` and one module per format |
| the checks | `.3DO bytes`, `textures`, `.ani quaternions`, `.CTL walk`, `.SCX scenes`, `sfx files`, `map2d`, `extension case`, `engine: DataFs`, `exe tables` |

## What is not settled

* **`.3DM` still has unread fields** (`FILE_FORMATS` §5, §7). A `float[3]`
  track whose integration is read and asm-confirmed, but read as root-motion
  deltas it walks nearly every character off the map, so that meaning is
  refuted and what neutralises the integral in the engine is untraced. Node
  slots 0 and 1 are uploaded with ids no drawn mesh binds — not rotations, and
  measurably not the voice envelope; eye-direction or blink channels are the
  shapes that survive.
* **The `.3DO` vertex's `+24` float** has no traced consumer, and 233 of the
  635 models mix unit and non-unit normals — very likely vertices of meshes
  that are never drawn, which is not established (`FILE_FORMATS` §5b).
* **The Dreamcast `.DDM` variant** is decoded only as far as its section tags.
  It is not this port's subject.
* **The 1999 spec sheet claims a BSP tree** in the models. Every `.3DO` is
  accounted for to 460 unexplained bytes across 33 MB, short ascending integers
  in 24 files (`FILE_FORMATS` §5b), so whatever the sheet meant, it is not a
  structure hiding in those files.
* **The `.SFX` count disagrees between two sources.** `docs/FILE_FORMATS.md`
  §5b6 still says 59 of 59; `verify.py: sfx files` walks 67 of 67 and records
  why the 59 was short (eight files spelled `.Sfx`). This chapter follows the
  check; the document needs the edit.
