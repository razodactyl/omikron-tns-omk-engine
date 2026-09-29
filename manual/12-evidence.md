# 12. Evidence

← [Contents](README.md) · prev: [The port](11-the-port.md) · next: [Open questions](13-open-questions.md)

---

## In short

This chapter is about the difference between *a check passes* and *this is
true*. They are not the same thing, and a project like this one fails in the
gap between them.

There are **484 checks** (`python3 tools/verify.py --list`, 2026-09-29). 228 of
them run by default in seconds. 256 more sit behind `--slow`, and most of those
are engine runs that build the port and play part of the game. Every number
quoted in the documents is meant to be asserted by one of these checks, so the
prose and the code cannot drift apart.

A passing check means only as much as the thing it compares against, and there
are **six grades** of that (`docs/PORTING.md` §B1). The strongest is
byte-identical to the shipped data. Next comes reproducing the original
engine's own output. Agreeing with a second implementation is weaker than it
looks: **two implementations of one reading agreeing is not evidence**, and the
project has paid for that more than once. At the bottom is a transcription that
is internally consistent and says nothing about the original at all.

Each claim carries its grade in three separate places, so a green tick never
implies more than was established.

The evidence comes from three sources. The first is the data, which is fixed
and can be tested exhaustively. The second is a surprise: **the original game
narrates itself.** Every script instruction announces its operand through a
Windows call that sits before the check for whether the debug window exists.
That means the unmodified 1999 executable, running under CrossOver, can be made
to say what it is doing without a patch, a shim or a debugger. This gave a
project that could only compare itself to itself an oracle. The third source is
**a person playing the port**. Most of the faults fixed in recent weeks were
found that way, often with the original running beside it, and not by any
check.

## In detail

### The six tiers

From `docs/PORTING.md` §B1, with its examples:

| tier | means | example that holds | what it does **not** license |
|---|---|---|---|
| **1 exact** | byte-identical to the shipped data | `.3DT` textures 2 534/2 534; ADPCM 777/777 sample-identical | anything about how the data is *used* |
| **2 corpus-constrained** | an invariant the shipped data could fail | `.CTL` 7/7 walks landing exactly on the file size; `.SCX` 220/220 | that the runtime reading it behaves correctly |
| **3 differential** | agrees with an independent implementation | `engine/` against `tools/sim`; `uitext.py` against `fnt.py` | anything both got wrong. They were often written from one reading |
| **4 behavioural** | reproduces the original's own output | the golden trace, 42/42 in order; the menu title 66 560/66 560, the selection outline 1 518/1 518, `Text_DrawRun` 6 132/6 132 | more than the capture actually contains |
| **5 data-constrained** | only the data it reads is checkable; the logic is not | `ACTOR_STATE` 0..17; the `.CTL` channel | that the machine *behaves* like the original |
| **6 read and explained** | a transcription with internal invariants only | the I2D display list's ordering; Astaroth's and the generic shooter's state graphs | anything at all about the original's behaviour |

Tier 3's warning, spelled out: it catches offsets, signedness and off-by-one
errors, and it **cannot** catch a wrong reading applied consistently, because
both sides were written from that reading. Only tiers 1, 2 and 4 can (§B1).

### The rules that make a tier mean something

`docs/PORTING.md` §B2–§B3 set four requirements for every slice of the port:

* **The tier is declared in three places**: the source header, the check's
  docstring and the coverage row. Each is read by a different person. The rule
  exists because of a specific failure. "The shoot AI has no data at all" was
  true of the dispatch, was written as if it were true of the subsystem, and
  survived in three documents until something contradicted it.
* **An oracle is named, or the slice says why none is possible.** The actor
  runtime's entry is the model to follow. It says the trace rig cannot reach
  it, names the two opcodes that make it invisible, and cites the capture that
  proved it.
* **At least one invariant the shipped data could fail**, or an explicit
  statement that there is none. I2D has exactly one (139 flag constants
  resolving), and it says so instead of letting six transcription invariants
  suggest more.
* **A falsification record.** Every check is *shown to fail* (§B4). A check
  that has never been broken has never been tested. Break it deliberately,
  record which mutation moves which number, and put that in the docstring.

`verify.py: porting standard` (`PORTING.md`, Enforcement) keeps the standard
and the coverage table from drifting apart. It asserts that the coverage counts
sum to the row total, that every tier on the ladder is used by at least one
check, and that every §B6 item is one the coverage table still calls
unfinished.

### The ways a falsification test lies

The show-it-fail rule has earned the most in practice, and its mechanics have
also failed in more ways than any other part of the apparatus. Each of these
happened, and each is recorded in `CLAUDE.md` §1 or `PORTING.md` §B4:

* **A mutation that did not apply.** A string replacement whose anchor did not
  match (wrong whitespace, or an anchor that appeared three times) left the
  code untouched, and the check "passed the mutation". Before believing the
  result, assert that the file changed.
* **A stale object file, in both directions.** If the rebuilt object and the
  old binary land in the same second, `make` relinks nothing and the mutation
  looks harmless. This happened to `engine: input`'s initialiser mutation, and
  a forced relink then moved it from 18/19 to 28/57 (§B4). It is the worst
  direction to be wrong in, because the tidy response to a harmless-looking
  mutation is to delete a working check. The reverse also happened: macOS ships
  GNU Make 3.81, which compares times to the **whole second**. A mutation
  restored in the same second as its compile leaves the *mutated* object in
  place, and for an hour the fight checks measured an AI with no guard. After
  restoring a mutation, `touch` the file and rebuild.
* **A check that did not build what it measured.** One check built its probe
  and then ran the viewer, which it had not built, so a mutation left it green.
  Name every binary a check invokes in its `make` line.
* **A log line printed where a value is handed over.** Such a line reports the
  intention, not the output. Three checks read lines like that, and two of them
  passed a mutation that cut the consumer off entirely. Print the line from the
  value the *consumer* produced.
* **A scan that enumerates call sites.** Three checks counted guarded call
  sites by the names of the renderer variables that existed when they were
  written. A third renderer arrived with its calls guarded exactly like the
  others, the pattern matched none of them, and all three checks went red
  against correct code and stayed red for three days. Match the invariant, not
  the callers.
* **A parse of another tool's output that reads nothing.** A regex written
  against an order the tool never printed matched zero lines. One sibling
  assertion went red. Another passed *for free* because it compared empty maps,
  and that vacuous pass was the worse of the two. A check that cannot read its
  input has to fail as a parse, not give an answer.

### A wrong turn, and what caught it (2026-09-29)

The newest entry in that list is also the clearest case of the apparatus
working. It is recorded in `todo/optimization.md` step 25, and two traps were
added to `CLAUDE.md` §1 the same day.

The claim was that the Vita renderer's *partial* vertex upload drew a different
street from a whole upload, differing by 537 pixels at frame 300. It went wrong
twice:

1. **A comparison across builds was taken for one within a build.** The
   differing pictures seemed to group by *when* they were made, which pointed
   at a timing fault. They actually grouped by *which build was on disk* at
   that moment. Every "partial" frame had been drawn by a build carrying an
   uncommitted run-merge. That merge assumed the dirty-corner list was sorted,
   and it is not: the list is built mesh by mesh, and until then no consumer
   had needed it sorted.
2. **Four agreeing dumps were not four frames.** A GLES run blocks at frame 0
   while the display sleeps, and a watchdog that kills it still writes its
   `--dump`, which is then frame **one**. Four such dumps agreed with each
   other, that agreement was read as a confirmed fix, and an interface-clock
   change was committed as the cause (`446beb7`).

§B4 is what exposed it. The check written for the clock change **could not be
shown to fail** in any scenario tried, which gave it away, and the change was
reverted (`63e357d`). An audit mode kept in the GLES backend
(`OMK_DIRTY_AUDIT=1`) then found 0 corners that had changed without the dirty
list naming them, and 0 corners that differed from a whole upload's bytes. The
partial upload had been exact the whole time.

The rules taken from it (`CLAUDE.md` §1): **a dump is evidence only together
with the frame count it came from**, so read `N frames presented` in the log
before comparing hashes, and keep the display awake for any GL run. A comment
that claims an ordering ("sorted by construction") is a hypothesis, not a
property.

### The traces: the original as an oracle

`tools/goldentrace.py` runs the game's own executable under CrossOver and
records what it announces (`CLAUDE.md` §6; the rig is `docs/RECONSTRUCTION.md`
§4.6). Nothing is patched. The announcement is a `GetPrivateProfileStringA` call
the engine makes to look up its own operand names in `IAM\*.TAG`. It sits before
the debug window's `if (hWnd)`, so it happens whether or not anybody is
watching.

There are five captures of play (`CLAUDE.md` §2, `traces/`): the intro (58
events), a walk-in (76), the walk out of the Impasse (286, over 592 seconds), a
conversation started from a save (55), and the largest, 840 events, which runs
out to a restaurant lunch with nine conversations. Together that is **1 315
events, all attributable, with 64 scripts replayed**. The single disagreement
with the model is event interleaving inside an anchor window, not a different
decision (`verify.py: trace agreement`). Starting cold, the port reproduces the
intro capture **42 of 42 in order**.

A sixth capture, of a fight, is outside that total. It is the instructive one,
because it produced **nothing**. The logger sees only what a VM handler
narrates, and combat has two opcodes: `fight.begin` announces nothing, and
`player.become` announces to a domain the logger filters out. The capture *did*
reach combat, so the silence comes from the mechanism, not from how the game
was played. That is why melee and shoot mode, both of which now run in the
port, can never get above tier 5. **Check whether a subsystem announces before
asking anyone to capture it.**

### The frame oracle, and what it licenses

The rig also grabs the engine's **own framebuffer**, and the recovery step
**refuses rather than degrades** (`PORTING.md` §B5): a 2× Retina grab is
accepted only if every 2×2 block is uniform. A frame that is only *nearly* the
framebuffer would make every diff built on it quietly wrong while it still
passed.

* **2D is exact.** Every interface primitive ends in a blit, which is a memory
  copy with an optional colour key, so there is no filtering to differ.
* **3D is exact about geometry and ordering, but not about a pixel's low
  bits.** Filtering, dithering and the fog table belong to the driver, and
  Wine's driver is not a Voodoo's.
* **A frame is deterministic only in parts.** Across three captures of the same
  screen, the title bitmap and all four labels' glyph pixels are identical,
  mask *and* values, while the frame as a whole differs by **52%** because the
  tile-map background animates. A check may assert the text. It must not
  assert the scene.

The 3D rasterizer is **tier 4 for one camera** (`PORTING.md` §B6,
`verify.py: engine silhouette`). The set seen through dialog 402's camera 4555
scores 0.73 / 0.83 on edge alignment against the two parked captures, against a
chance floor of 0.27 / 0.30. That is a claim about one camera, not about the
renderer. The metric cannot see characters, and it checks no pixel's value.

### Things that look like evidence and are not

From `PORTING.md` §B7 and `CLAUDE.md` §1:

* two implementations of one reading agreeing;
* asking the code that made a decision whether it made it correctly (the first
  dispatch check read the same flag it asserted);
* a corpus test that cannot separate the rule from a simpler one. Of 9 103
  gated `.CTL` transitions, 120 are a real priority contest, and in **none** of
  them is the first match anything but a match of maximal priority. So the rule
  stands on the code, the corpus says nothing, and the check says so;
* a count that is quietly short. A sweep reported 708 morph files where 777
  ship, and five checks missed the eight `.SFX` files spelled `.Sfx`. A total
  that is too small looks exactly like a total that is right;
* generalising a negative beyond where it was measured;
* taking a name in the rename map as evidence. `Perso_SetInputEnabled` is a
  *block*, not an enable, and trusting the name disabled the action button
  after every conversation;
* agreeing outputs whose provenance nobody checked: several dumps of one
  stalled frame, or frames from two builds (the 2026-09-29 case above);
* a number in the prose that nothing asserts.

### What the suite cannot see

A suite that only compares this repository with itself cannot see a **wrong
reading applied consistently** (`CLAUDE.md` §6). The dialogue staging stayed
wrong through two successive "fixes" while every check passed and every number
agreed with every other one. One frame of the running game showed the error in
a second.

That is why "look at it" is a rule, why the viewer exists, and why the play
report is now the main source of work. What has been committed but not yet
confirmed by a person is listed separately (`todo/play-test.md`). Nothing on
that list counts as confirmed, however green its checks are.

### Running it

`--only` is the normal way to run the suite (`CLAUDE.md` §4): name the checks a
change could plausibly break and run those. **A red check is fixed alone.**
Change the code, rerun that one check, and rerun its neighbours only after it
is green. Running a section around a red check measures nothing that survives
the next edit.

The counts from `verify.py --list` on 2026-09-29 are **485 entries**, **257**
of them marked `--slow`. `engine: airlock walk` is registered twice, both times
in the slow list, which leaves **484 distinct checks**: 256 slow and **228
fast** (485 − 257, with no duplicate among the fast ones). Of the 245 `engine:`
entries, 222 are slow and 23 are fast. That disagrees with `CLAUDE.md` §4, which
says the fast list runs no engine check at all. As the list stands, 23 engine
checks run in the fast pass, and the other 222 run only under `--slow` or
`--only`.

A full sweep means `--slow`. It runs **every five to ten finished tasks**, where
a task is one thing the reader asked for, not a commit, and the count is kept in
`todo/sweep-log.md`. The first fully green run of the whole suite was on
2026-09-09 (384 checks, 0 failed). The last sweep recorded in that file's table
was on 2026-09-14: 427 checks, 3 failures, each attributed and none caused by
the work it covered. After it, the counter climbed to **15 tasks** against the
five-to-ten cadence. A full sweep was run on 2026-09-29. Its result belongs in
`todo/sweep-log.md`, and this chapter does not report it.

## Where it lives

| | |
|---|---|
| the standard: tiers, the three places, show-it-fail, what a frame licenses | `docs/PORTING.md` Part B and its Enforcement section |
| the working practice, and its traps | `CLAUDE.md` §1 |
| the checks | `tools/verify.py`; `--list` names every one and the document that quotes it |
| the rig | `tools/goldentrace.py`, `tools/frame.py`; `docs/RECONSTRUCTION.md` §4.6 |
| the captures | `traces/`: the operand logs, the framebuffer grabs, and the saves they anchor to |
| the 2026-09-29 wrong turn | `todo/optimization.md` step 25 |
| the cadence | `todo/sweep-log.md`; what awaits a person, `todo/play-test.md` |

## What is not settled

* **The captures are CrossOver's rasterisation**, not a Voodoo's. Everything
  built on a 3D frame says which half of §B5 it relies on.
* **Whole subsystems have no reachable oracle**, and say so: the actor channel,
  melee, shoot mode, the audio attenuation and pan law, the Vulkan backend
  (§B6).
* **Tier 4 has been reached for exactly one camera in one set.**
* **The result of the 2026-09-29 sweep is not in this chapter.** The sweep was
  still running when this was written, and its outcome belongs in
  `todo/sweep-log.md`.
* **The interface clocks read the wall in a frame-bounded run**, against the
  rule the frame delta follows. No frame tried shows it, so it is noted
  (`todo/optimization.md` step 25) and not changed.
* **The fast/slow split is stated two ways.** `CLAUDE.md` §4 says the fast list
  holds no engine check, while `--list` shows 23. This chapter follows `--list`,
  and the prose is the one due for correction.
