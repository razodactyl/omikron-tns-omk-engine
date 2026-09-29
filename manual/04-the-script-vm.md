# 4. The script VM

← [Contents](README.md) · prev: [The data](03-the-data.md) · next: [The world](05-the-world.md)

---

## In short

The game's behaviour is a program, and the program is in the data files.

Walk into a doorway and a script runs. It is bytecode — one byte per
instruction, operands following — interpreted through a 153-entry table
compiled into the executable. There are 5 785 world-script slots in the shipped
game, plus 612 small scripts inside the conversations, and they are what says
*this conversation starts here*, *this door opens*, *this camera watches*,
*this variable is now 1*, *this gunman patrols that corridor*.

The one idea worth carrying out of this chapter is how a script **waits**. It
does not block, and nothing polls. A script that opens a menu, or starts a
cutscene, or starts a fight, simply **parks**: it writes a number into its own
status word and stops, with its program counter and stack intact. Later
something else — a screen closing, an animation finishing, a fight ending —
writes 1 back and it carries on from the next instruction.

That is why a conversation can interrupt a cutscene, why an area transition can
take several frames of streaming without the game stopping, and why the whole
engine is one loop with no threads in it. It is also why the VM costs almost
nothing on an ordinary street: the scripts run when something happens, not
every frame.

The second idea cost more to learn: **a script belongs to a place.** When it
names a scene object by number, the number means something only inside the
scene of the slot the script came from — and two places are loaded at once.

## In detail

### The interpreter

`sub_4060B0` is the whole of it (`docs/SCRIPT_VM.md`, "Execution model" — one
of the four handlers Hex-Rays decompiled; the other 149 were read out of the
listing):

```c
pc = ctx->code;                     /* ctx+4  */
op = *pc++;
while (op != 3 && !(ctx->flags & 0x10)) {
    handlers[op](ctx);              /* table at 0x004C0140 */
    pc = ctx->pc;                   /* handlers advance it themselves */
    op = *pc++;
}
```

An opcode is one byte; **opcode 3 ends the script**; bit `0x10` of `ctx+40`
stops the scan. The context is small (SCRIPT_VM, "The two entry points"):

```
+4   uint8 *code       the script, as passed in
+12  uint8 *pc         each handler advances it itself
+16  int32 *stack
+20  uint16 sp         one past the top
+22  int16  status     the park word — see below
+36  int16 *params     the indirect-operand block (operand bit 14)
+40  uint8  flags      bit 0x10 stops a Script_Run scan
```

The table at `0x004C0140` is 153 entries of `{handler, operandBytes}`, lifted
to `tables/vm_opcodes.json` because a replica cannot read it out of any data
file (`verify.py: vm table sources`, `opcode table fresh`).

There are **two entry points on one context**. `Script_Execute` (0x00406460)
is the real interpreter. `Script_Run` evaluates: it sets a dry-run flag, and
every handler that tests it consumes its operands and returns without acting
or announcing — 111 references across the listing, and 22 of the 153 handlers
open with the identical five-instruction test (SCRIPT_VM §62). It is how the
engine asks a question of a script without running it. This is also why an
operand length can be wrong without anything visibly failing: in the evaluate
pass a handler does nothing *but* consume operands.

Binary operators take the **left operand from `stack[sp-1]`** and the right
from `stack[sp-2]`, so the compiler emits the right-hand side first
(SCRIPT_VM, "Stack convention").

### Operands, and a lesson about believing a table

Most operands are 16-bit literals following the opcode; `0xFFFF` means
"none". With bit `0x4000` set, the rest indexes the block at `ctx+36` instead
— and the fetch reads it **from its second word**, so operand `0x4000` names
`args[1]`. That block has exactly one filler: `Message_RunHandlers` writes
`{message id, sender}` into it. So the only parameter a script can reach is
the **sender** — and all **59** indirect operands in the world scripts are
`push.i16 0x4000` inside a message handler, comparing the sender with an actor
or object id (SCRIPT_VM, "Operand encoding"; `verify.py: engine: vm probe`).
**91** handlers run that shared fetch. The `scx.play` family's object word
does not — it is read raw, an unsigned scene-object id — and reading it
through the indirect rule once turned Anekbah's whole startup crowd, whose ids
carry bit 14, into `param[718]` (same section; `engine: city crowd`).

The operand *lengths* in the table were checked against the handlers
themselves, and the exercise is worth recording because it is the shape of
mistake this repository is built to catch (`CLAUDE.md` §1). Recovering lengths
from handler assembly produced **21 disagreements** with the table. Applying
all 21 made the corpus decode *worse* — 53 failures became 58. Tested one at a
time, **6 were real**. The table had been checked only against the
conversations, which exercise 25 opcodes; the world scripts exercise far more,
and 53 slots desynchronised onto an opcode above 152 when they were first read
(SCRIPT_VM, "The world scripts use far more of the VM").

Then the reverse: opcode 103's assembly reads 6 bytes in a straight line, and
it was recorded as *wrong* — left at the table's 2 — because a corpus test
showed a script breaking at 6. That test had been run while three other lengths
were still wrong, and it was those that were breaking. Corrected, op 103 at 6
decodes 5 785 of 5 785 slots with **2 056 fewer instructions**: exactly the
four surplus bytes per site, which at 2 had been decoding as phantom
instructions (`CLAUDE.md` §1).

And one correction whose consequence waited two weeks. `fight.begin`, opcode
62, takes three 2-byte fields where the table said 4 bytes; at 4 the corpus
decodes 216 phantom instructions, two per site over its 108 sites, all inert,
so nothing observable broke (SCRIPT_VM, "The ten opcodes that write the status
word"). Read under the corrected length, **the third field is the AI level**,
pushed as `Fight_Engage`'s second argument: every site is a `case` on
`VARIABLES[175] 'Niveau Combat'` with three copies of the call, 36 / 36 / 36
over levels 0, 1 and 2 (SCRIPT_VM §62). The scripts raise that variable when
the last fight cost the player under 30 life and lower it at 70 or more,
clamped to 0..2 — **the game's fight difficulty is adaptive**, and the fourth
fight-AI profile, a sparring partner, can never be selected
(`docs/ASSETS.md` §7, "The `.CTL` fight AI"; `verify.py: fight & become`).

So a corpus verdict is only as good as the rest of the table, and a conclusion
drawn from a table that has since changed is worth re-running rather than
re-reading.

### The status word — how a script waits

`Script_Execute` runs `while (status == 1)`. A handler that writes anything
else parks its caller, and something else must write 1 back. **Nothing polls**:
every resume is an event or a step of the pump (SCRIPT_VM, "The context status
word", *read from the code*).

| status | what it means | resumed by |
|---|---|---|
| 0 | idle; the action queue may be armed | the pump, when it dequeues an action |
| 1 | running | — |
| 3 | in a fight | event case 2 — the **first** context at 3 |
| 4 | waiting for a scene object or a player move | case 3, when the object or the move finishes |
| 5 | a transition caller a *second* transition superseded | **nothing found** |
| 6 | waiting for a screen | case 5, from the answer or either close key |
| 7 | waiting for a camera move | case 4, when the move ends |
| 8 | waiting for `area.preload` | the pump's tail, once the load reports |
| 9 | retry the transition next frame | the action processor's head, unconditionally |
| 10 | waiting on the area transition | the pump's tail |
| 11 | set by the transition's 60-second watchdog | case 3's `else` arm |

Exactly **ten opcodes** write the word — `end`, `area.preload`, the three
waiting `scx.play` variants, `fight.begin`, `ui.open`, `player.move.wait`,
`camera.set.wait` and `camera.set.at_address` — and the transition machine
writes the rest (same section; `verify.py: engine: parking ops`). Details of
the plumbing that a reader reconstructs wrongly otherwise:

* **Queued actions do not overtake a parked script.** The action processor
  refuses to arm anything while the status is non-zero, so an action waits
  behind the script that is parked, and an action is over only when `end` runs.
* **Starting a conversation stops the world on the next frame, not this one.**
  `Script_Execute` returns after `dialog.start` for that one context; what
  stops the others is the pump's first line on the following frame. A port
  that ended the frame there skipped every later context (same section,
  corrected 2026-09-02).
* **A handler can retry itself.** A refused `area.goto` rewinds its pc by 7 —
  its own size — and parks at 9, so it runs again next tick until the
  transition accepts it.
* **`camera.set.wait` parks only when the travel is non-zero; the camera
  call on an address parks always**, so a 0-frame cut parks under one and not
  the other. Every shipped site of the latter travels 20 frames, so the
  corpus cannot show the difference; it is asserted from the handler.
* **A fight's result is dropped.** Case 2 releases the first context at
  status 3 and ignores the result code it is handed; the scripts learn what
  happened by re-reading the player's life (`todo/fight-mode.md` §5).
* **A screen can answer its own question.** `ui.open` parks its caller at 6 —
  but the videophone's open ends in a call to `UI_SendAnswer`, which hands back
  the −1 the screen was seeded with while the device stays up, so the next
  instruction runs at once and the call looks like it never waited
  (`docs/UI.md` §3i).

### Which pool a script plays objects in

`scx.play` and its variants name a scene object by a small number that is
**scene-local** — `Scene_FindScriptObject` matches it against the 100-byte
object records of the loaded `.SCX` — and object numbers are reused in every
scene file (SCRIPT_VM, "The four `scx.play` opcodes"). `scx.play.wait`
(0x004031E0) resolves the number through the object container of **the slot
the running context belongs to**, not the scene loaded last (SCRIPT_VM, "…and
a script plays objects in ITS OWN slot's pool", 2026-09-18).

The shipped case that shows it is a lift door after a phone call. SCENE 45,
loaded over the security centre's shaft, takes the lift over with a zone that
does `ui.open 4`, then `area.goto 181`, `area.arrive -1`, and only then
`scx.play.wait obj 0x0012` — the door. By that line the destination's scene is
resident in the other slot; the script is the shaft's, so `0x12` is the shaft's
door. A replica that hands every `scx.play*` to the newest scene opens an
object of the wrong file, and the door stays shut. Every other ride worked
**by order** — the car zone had started the door a frame before the swap —
which is why no ride without the call could see it. `verify.py: engine: slot
pool` reads the door's drawn vertices: moved 0.0 without the rule, 87 units
open with it.

Two more rules from the same family, both learned from play reports:

* **A waiting `scx.play` is not a cutscene.** Opcode 58 leaves its caller
  parked while a door slides, exactly as a cutscene beat does; asking "is some
  context parked on some program" treated the flat's doors as cutscenes, put
  the letterbox up and dropped the player for the length of each. **A beat
  poses a body and a door does not** (`docs/RECONSTRUCTION.md`, 2026-09-09).
* **`player.move 100` is the stop.** The operand is a group of the player's
  `.CTL` bank, and group 100's default entry is the stand, so a walking player
  stops dead with no root motion left to play out. The scripts put it in front
  of 60 of the 312 `player.move` sites, and it is why a cutscene trigger is not
  re-fired by the player's next step (SCRIPT_VM, "63 / 89"; `verify.py:
  engine: player move`).

### What the corpus exercises

**129 of the 153 opcodes are named.** The world scripts exercise **119** of
them, **114** named, which covers **99.97%** of executed instructions; the
other five are read and deliberately left unnamed, 17 instructions between
them; and the 24 opcodes with no name at all are ones no shipped script
executes (SCRIPT_VM, "Naming the opcodes"). Names are added only with the
handler traced, and the table in the document is generated from the
disassembler's own name map, so the two cannot drift.

Some of the names came out of the game rather than out of a guess. Opcodes 150
and 151 install and remove the engine's **second render bank**, the same
bucket walk converting every vertex colour to luma: all 14 shipped installs
are closed by a restore, and 74 of the 82 instructions between them are camera
and fade opcodes, in eight chunks including the Morgue and the Bowie concert.
So the game has **black-and-white cutscenes**, and the opcodes are
`render.grey.on` / `render.grey.off` (SCRIPT_VM, "150 / 151").

Shoot mode's six opcodes carry most of its content, and the handler of one of
them names the subsystem in its own error string (SCRIPT_VM, "Shoot mode";
`verify.py: shoot mode`). `shoot.actor.enter` brings a gunman in and
`shoot.actor.action` gives him something to do: 315 of its 319 sites are
preceded in the same script by an enter on the same character, which is the
handler's own precondition showing up in the authored data. **116 of the 319
are action 1, a patrol** along a route in the level's `MAP2D` file, ahead of
action 3's 113 (`docs/RECONSTRUCTION.md`, 2026-09-12; `verify.py: shoot
patrol`).

On a steady street almost none of this runs: the port measured about 50
instructions in 150 frames, which is why the VM's per-instruction cost only
matters in cutscenes and transitions (`todo/optimization.md`, item C8).

### The accident that makes the original observable

**51 handlers** announce an operand by domain name through
`GetPrivateProfileStringA` on the `IAM\*.TAG` tables — and that call sits
*before* the debug window's `if (hWnd)`. So the shipped, unmodified executable
narrates what it is doing to anyone watching the Win32 profile-string API, with
no patch, no shim and no debugger (`CLAUDE.md` §6, "Running the original").

Which operand a handler announces is not always its first: that mattered five
times, each found by a golden trace disagreeing with a prediction, so the
announce is now read out of every handler's assembly — 49 read, 0
disagreements (SCRIPT_VM, "Which operand a handler announces"; `verify.py: vm
announce fields`).

That is the whole basis of the golden traces in chapter 12, and it is luck
rather than design. It is also why fights and shoot phases have no such oracle:
`fight.begin` announces nothing, and `player.become` announces to a domain the
logger filters (`CLAUDE.md` §2, `traces/`).

## Where it lives

| | |
|---|---|
| the finding | `docs/SCRIPT_VM.md` — the table, the operands, the status word, the naming, which pool a script plays in |
| the table | `tables/vm_opcodes.json`, `tables/vm_announce.json` |
| the port | `engine/src/script/interp.cpp` (the handlers), `area.cpp` (the pump, the contexts, `poolForSlot`) |
| the readers | `tools/dialog_disasm.py`, `tools/script_dump.py`, `tools/vm_oplen.py`, and `/world` in the web viewer |
| the checks | `vm table sources`, `dialogue scripts`, `world scripts`, `vm announce fields`, `engine: vm probe`, `engine: parking ops`, `engine: slot pool`, `engine: player move`, `fight & become`, `shoot patrol` |

## What is not settled

* **Status 5 is read and not resolved.** A transition caller that a second
  `area.goto` supersedes is parked, and nothing in the reading writes it back
  to 1. The shipped scripts may never reach it — it needs two transitions in
  flight — and a claim either way needs a trace the rig cannot take
  (SCRIPT_VM, "The context status word").
* **Five exercised opcodes are read and not named**: 137 jumps to a target not
  yet located; 134/135 save and restore a mesh pointer in an object slot
  nothing identifies; 53 and 101 are no-ops (SCRIPT_VM, "Read but not named").
  **24 opcodes have no name at all**, identified only by their table entry,
  because no shipped script executes them.
* **Site counts depend on the corpus, and three enumerations are not
  reconciled**: the 5 785 slots of `IAM\AREA` + `SCENE` + `GLOBAL` that
  `docs/` counts over, the same without `GLOBAL`, and the port's own sweep of
  5 958 slots, which reports higher figures again (SCRIPT_VM, "A note on site
  counts").
* **`docs/SCRIPT_VM.md` carries two stale passages** this chapter does not
  follow: the status-word section still says op 62's table length "has not
  been corrected yet", which §62 supersedes, and its closing *Open* list still
  names ops 92, 93, 52 and 76 as unnamed.
