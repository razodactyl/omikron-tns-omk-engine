# 10. The interface

← [Contents](README.md) · prev: [Audio](09-audio.md) · next: [The port](11-the-port.md)

---

## In short

The game's menus are data too — but data compiled into the executable, not
shipped beside it. There are **37 screens** — the start menu, the options, the
save and load panels, the shops, the terminals, the lift, a few one-off
puzzles, and Kay'l's handheld device, the *sneak* — and each is a row in a
table naming its background bitmap, its text file, its twelve sounds and the
callbacks that open, drive and close it.

What a screen *contains* is a tree: a screen owns a panel, a panel owns lists,
a list owns items, and an item is 72 bytes saying where it sits, what it draws
and which flags it carries. **60 panels, 176 lists and 728 items** in all,
lifted to JSON because none of it is in any data file.

Most of what makes a screen *work* is not in that tree but in small native
functions it points at — a list's input hook, an item's draw hook, the
callback a press runs. Many of them are called from nowhere but a table, so
the disassembler never recognised them as code, and nearly every screen the
port brought up meant finding them in the raw image first. The start menu's
own OPTIONS and QUIT are the latest example: both descend correctly and both
come up empty, because neither half of either is in the widget records.

Text is drawn from the game's own fonts — 13 of them, 2 899 glyphs — with a
small markup language, laid out by the engine's own block layout. And the whole
interface is driven by fourteen input bits shared with the actor runtime, so a
menu and a character are reading the same word.

## In detail

### The screen table

92 bytes a row, 37 rows, and the order is confirmed from the *code* rather than
assumed — `UI_LoadScreen` scans the field at `+4`, which runs 0..36 in table
order, so the row index is the `ui.open` operand. Eleven screens name a
background bitmap and all eleven resolve; eighteen name a text file and all
eighteen resolve (`docs/UI.md` §2, `verify.py: ui tables`). A fixed parameter
at `+8` is what lets **ten shops share one screen definition** — they differ in
nothing else — and the same trick runs the sneak, the slider page and the
videophone off one text file. Only **three** screens can be open at once.

Each has an open/run/close state machine, and two flags in it have visible
consequences (`docs/UI.md` §2 and §3b):

* a screen **hides the world** behind it unless its record says otherwise, and
  only **three of the 37** keep it — the pause and the two shoot HUDs. A panel
  carrying `0x800` is drawn over a **dimmed** world by `Ui_DrawPanelDim`, which
  is why a shop, the save screen, the pause and the high-score board show the
  street behind them (`verify.py: engine: screen world`);
* `0x20000400`, on the start menu and the save screen only, makes those two and
  the pause **mutually exclusive, with the pause losing**: the pause is refused
  while either is up, and opening either closes it.

**No screen stops the world ticking except the pause.** `Game_Tick` has no test
for an open screen anywhere in it; the only brake is the pause flag, which has
exactly two writes in the image — the pause screen's open and close — and works
by forcing the frame delta to zero (`docs/UI.md` §3b). This refuted a `NAMED`
banner in `readable/` that said any screen short-circuits the tick.

The per-screen callbacks are the awkward part: of the 30 opens and closes,
**26 are absent from the decompilation entirely**. Not because they are unusual
code — because nothing *calls* them. They are dwords in a table, and the
disassembler's auto-analysis makes a function where it sees a call. Over the 33
addresses the table names, predicting a label from *starts with a push* is
right 22 times and from *has at least one direct caller* 29 (`CLAUDE.md` §1,
`verify.py: ui input`). The same is true one level down, of list hooks and item
callbacks, and it is why this chapter keeps saying "read from the raw image".

### The widget tree

A screen's panel owns up to ten lists; a list owns items; an item is a bag of
flag bits rather than a type. Two things about it were learnt the hard way
(`docs/UI.md` §3b):

* **the records' positions are not what a screen shows.** Each screen's open
  callback re-lays its lists through three helpers; the start menu's labels go
  to y 120 step 80 where the records say 150 step 60, and the engine's own
  capture has them where the callback puts them. Without the layout pass **0 of
  4** rows land near the capture's, with it 4 of 4 (`verify.py: menu layout`);
* **list and panel state is static data with process lifetime** — a list's
  selection, an item's colour, a panel's current list. Builders write it and
  nothing resets it, so an interface that "remembers" where you were is the
  engine's behaviour, not a bug (`todo/sneak.md`, its opening warning).

The whole tree is walked by `tools/sim/ui.py` and by the port with the engine's
own input words; `engine: UI` compares the two.

### The answer layer

A screen returns a value through **one global**, and it has **17 writers**,
enumerated into the lifted table and attributed to the screens whose tree
serves them. The cross-check is against the corpus: of the **242** `ui.open`
sites over 25 screens — one of them in a startup script, which is why a
slot-only walk sees no start menu — 15 screens keep the answer and 10 discard
it, and **no site is attributed only to screens that discard** (0 of 16). That
is tier 2, corpus-constrained: it establishes the set of values each screen can
write, not that any screen behaves correctly (`docs/UI.md` §3d-bis,
`docs/PORTING.md` B6, `verify.py: ui answers`).

The terminal family is a second shop-style dispatch — seven screens on one
panel, one activate callback, a seven-case jump table on the screen's own
parameter, whose cases are exactly the parameters 0..6 — in which **case 4
falls through into case 6**, which reading the arms independently would miss.

### Input

One shared callback serves all 32 live screens. It reads the same 14-slot
binding word the `.CTL` runtime reads, edge-filtered by mask `0x203F` so a menu
does not auto-repeat, and dispatches the back and close bits first, then the
panel's hook, then the current list's hook, then the default selection mover
(`docs/UI.md` §3c). The exits are **gated on the panel's own flags**: TAB
closes 26 of the 31 top screens and none of the start menu, the save screen,
the pause, the options, the videophone or the shoot HUD; BACK pops a child or
closes a top panel carrying `0x10`, so SPACE does not leave the start menu, the
pause or the options (`verify.py: engine: screen close`).

A list with its own hook **never confirms** — the hook replaces the default
mover, and the confirm is only reached by falling through it. And an item whose
child panel carries `0x20000040` is entered on the **move**, not the confirm: a
tab strip.

`0x203F` is expressed in *adventure*'s slots. In the shoot scheme slot 4 is the
trigger and the action has moved to slot 8, which the mask does not contain —
and the port hit both halves of that in play (`docs/UI.md` §3c). The interface
bits' documented "defaults" of E and R are a static initialiser that
`Game_Init` overwrites before the first frame; slots 4 and 5 are ENTER and
SPACE (`docs/UI.md` §3c, `docs/PORTING.md` B6). Mouse *motion* is not a binding
at all, and nothing shipped governs its sensitivity.

### The sneak

Kay'l's device is screen 9, and **no script opens it**: TAB is action 13 of
the adventure scheme, which the player's `.CTL` routes through an alias entry
to group 6 and a special move, `MDSNEAK0` (`docs/UI.md` §3d, "What opens the
sneak"). It is most of the interface a player touches, and all of the following
runs in the port:

* **the verbs.** *Utiliser* announces the object to the world as a **message**
  — which is why it works near a place and nowhere else: proximity is *which
  scene is resident*, and the handler belongs to that scene — and then decides
  by one bit of the record whether the object goes **in hand** (a key) or is
  **applied** (a medkit adds its amount to *Vie*, clamped, and leaves the bag).
  *Utiliser sur* is not a use at all but a **combine** of two carried rows —
  box and key give the open box — with its success sound and the verb's flash
  put out when it closes. *Examiner* shows either a 3D model or a document
  bitmap by the record's kind (83 of 83 and 17 of 17 resolve), over the object's
  description (`docs/UI.md` §3b; `verify.py: engine: sneak verbs`,
  `sneak examine`);
* **the echo bar** at the foot of every page, which shows the *selected*
  item's text and is the **only** place in the interface the player's seteks
  and anneaux appear, read from the player record's `+172` and `+174`
  (`verify.py: engine: sneak echo bar`);
* **the memory page, which is the memo journal**: 76 `inventory.add` sites
  fill it with memo objects, and a reader page shows the **memo** section of
  each description and not its **clue** — the two bracketed sections 37 of the
  1 002 object records carry. It had been recorded as empty *by the code*; a
  reader who had played the original said it was not, and the count it reads
  turned out to be a list's own field (`verify.py: sneak memory page`,
  `engine: sneak memos`);
* **the identity page**, two tabs: the character's details from the save, laid
  out by its own draw hook; and the characteristics as bars with a combat rank
  word. Beside them, the player's model in a still pose from a literal bank,
  turning. Confirmed in play (`todo/sneak.md` §5e);
* **`Lire plan`, the city map**: a panel nothing in the widget table points at,
  found through the one instruction that installs it. It names its bitmap from
  the resident decor, so it bounces back outside the four cities that ship one,
  and draws the player's pin and a marker per destination (`docs/UI.md`
  §3g-bis, `verify.py: engine: sneak map`);
* **the slider page** (chapter 6), with a hand-written list mover of its own,
  and **the call**: the videophone screen answers its own question the moment
  it opens, so the script that parked on it runs straight into its
  conversation, with the caller a real actor parked off-stage (`docs/UI.md` §3i).

The **options tab hosts screen 35**, which the port models as a page tree but
does not have as a drawn screen, so that step is blocked (`todo/sneak.md` §5e).
The **quit tab's page is built and unreachable** — its icon carries a callback
and a child, the callback wins, and the child's value appears once in the whole
image — the same shape as options page 12. The callback's *Oui* is the pause's
quit request: a new game at the next pump, not an exit
(`verify.py: engine: sneak quit`).

### The screens the world opens

* **The start menu answers for itself**: type a name, walk to *Confirmer*, and
  the answer is derived rather than supplied — the engine's own callback
  refuses an empty field (`verify.py: ui confirm gate`). Its **OPTIONS** and
  **QUIT** items both descend, and both come up empty: OPTIONS draws its
  heading and nothing else, faithfully, because its panel has one item and no
  hook; QUIT draws its confirm, and neither button acts. Neither half is in the
  widget records, so both are native code still to be read at the address the
  data names (commit `b429a50`, `todo/start-menu.md`).
* **The shops** — ten screens, one dispatch, and the titles name their own
  screen 8 times of 10, which a linear scan gets *wrong* by binding all ten to
  one string (`verify.py: ui shop titles`). The stock is the area's own `+8`; a
  row buys, sells at half, or examines depending on the selected button, and
  *Analyser* shows the item in 3D. Confirmed in play
  (`docs/UI.md` §3d, `verify.py: engine: shop open`).
* **MULTIPLAN**, opened by **82** zone scripts: a storage locker shared by
  every terminal, moving objects between the sneak and it. Confirmed in play
  (`docs/UI.md` §3f, `verify.py: engine: multiplan transfers`).
* **The save screen** holds a second page, the **hint shop**: it sells the clue
  section of your memos, and the price is not a constant in the executable —
  the event broadcasts a message, and `IAM\GLOBAL`'s one handler for it sets
  the price to **three anneaux** (`docs/UI.md` §3g-bis,
  `verify.py: engine: hint shop`).
* **The load and save panels**, the thumbnail, the profile wheel, and the ring
  a save costs (`docs/UI.md` §3g, `docs/GAME_STATE.md` §8).
* **The pause screen**, opened by ESC, which `Game_RunLoop` polls itself — it
  is not an input binding (`docs/UI.md` §3h, `verify.py: engine: pause`).
* **The security centre's lift**: a 7-slot grid answering `slot − 1` (slot 0,
  the only floor above ground, answers 6), with a box listing the hovered
  level's offices. Its zones stack up one shaft, and the engine's zone scan has
  a *height*; the port's band for it is a labelled reconstruction
  (`docs/UI.md` §3f, `verify.py: engine: lift`).
* **The terminals**: a keypad walked by its own hook, a display that names the
  screen's text, a header showing the label of the cell under the cursor, and
  an answer on the way **out** that reports which dossiers were read — the
  office's script uses it to set a mission and enable an address
  (`verify.py: engine: terminal family`).
* **The one-off screens** (`docs/UI.md` §3k): **Gandhar's door**, a 6 × 6 grid
  where four cells must be marked in any order; **Den's locker**, four wheels
  with the combination **7 2 1 3** compiled in; the **cartridges** before
  Xendar's door, symbols stepped along a ring *out of numerical order* to the
  code 10 14 7 9; and the shooting range's **high-score board**, whose rows
  live in the save file's settings header.

  Gandhar's door gained two corrections a reader saw on screen (commit
  `70eb3a8`): its move and press write the cell into the widget's **unlit
  sprite source** as well as its position, so an unlit cursor cuts out the
  artwork under itself — the port had moved only the position and dragged a
  rectangle of the wrong artwork about; and the screen's open callback
  **re-hides the four markers**, which the port never did, so a marker from a
  previous visit stood at (0, 0). The cursor's own position is static data and
  survives a revisit, as in the original. Confirmed by the reader.

<p align="center">
  <img src="../traces/frames/loadpanel-mode0.png" width="440" alt="The original engine's load panel, captured at 640x480">
  <br><em>The original engine's load panel. Its selection box and the connector<br>
  to the thumbnail are quads the port reproduces at 1518/1518 and 138/138<br>
  covered pixels (<code>docs/PORTING.md</code> B6).</em>
</p>

### The text

Thirteen fonts, each keyed by an ASCII letter, all thirteen shipping and none
extra; 2 899 glyphs, none outside its file and none overlapping. A pixel is
coverage 0..31 into a colour ramp rather than a colour, and the markup is a
small vocabulary — `{f}` picks a font, `{I…}` a colour as nine decimal digits,
`{X}` a position, `{B}` a blink that flashes **red**, and `{C}`/`{D}`/`{F}`/`{G}`
the alignments. `{P}` is inert: every shipped occurrence sits beside a real
paragraph break (`docs/UI.md` §5, `verify.py: ui fonts`).

The start menu's labels, drawn by the port, come out **6 132 of 6 132 glyph
pixels** identical to the engine's own framebuffer — tier 4 (`docs/PORTING.md`
B1; `verify.py: engine: text draw`).

What turns a string and a box into positioned lines is `Text_LayOutBlock`
(0x0043F3E0, 577 lines), and it is **ported**: the wrap, a line pitch of 120%
of the font's height, the alignment, the vertical placement, the counted spans,
and the origin that makes a box scroll. The same function measures and draws,
which is how a scroll is bounded (`docs/UI.md` §5,
`verify.py: engine: text block`).

### The options, and what they do

74 rows over a 13-page tree, every label resolving in the game's own text
archive, with the read and apply hooks paired from the code. **Page 12 is
built and unreachable** (`docs/UI.md` §4).

They are not cosmetic: chapter 8 has the clip distance sizing four things at
once, and the settings live in the **save file's 3 496-byte header** — so
saving a game saves your options, including all three binding tables verbatim
(`docs/GAME_STATE.md` §8a, `todo/options-config.md`).

### On the Vita: the interface over the GPU picture

The Vita port first composed every held frame on the CPU from a readback of
the world. Its G6 work draws the interface as an **overlay** over the GPU's
picture instead, using the fact that every pass that *reads* the picture — the
dialogue boxes, the fades, the gauges' blends, the screens' dim and 50% quad —
is affine in it (`todo/vita-port.md`, G6):

* step 3 moved the **fight gauges and the breath gauge** onto it; measured on
  the Mac against the CPU compose, 230 and 197 pixels of 480 000 differ in a
  fight, **none by more than one 565 level**, and 0 underwater;
* step 4 moved the **shoot HUD** and **open screens** — with three exceptions
  that still read back, because each reads the picture in a way the overlay
  cannot carry: a screen with a viewport item, the **save screen** (its
  thumbnail is taken from the frame itself) and a panel with the monitors'
  interference. In the pharmacy's shop, the dim's rounding differs by one level
  over the whole picture, the same law as the conversation box.

Both steps were shown on the Mac; the console logs since report the city's
frames coming straight from the GPU, which is the world path rather than a
test of each screen.

## Where it lives

| | |
|---|---|
| the findings | `docs/UI.md` — §2 the screens, §3 the sounds, §3b the widget tree and the sneak, §3c input, §3d the callbacks and the shops, §3d-bis the answers, §3f–3h MULTIPLAN, the lift, the load panel, the hint shop and the pause, §3i the call, §3k the special screens, §4 the options, §5 the text |
| the per-task records | `todo/sneak.md`, `todo/shops.md`, `todo/multiplan.md`, `todo/missing-ui.md`, `todo/start-menu.md`, `todo/text-layout.md`, `todo/vita-port.md` (G6) |
| the tables | `tables/ui.json` (screens, sounds, option rows, fonts), `tables/ui_widgets.json` (60 panels, 176 lists, 728 items, the answer sites, the option pages), `tables/city_maps.json` |
| the port | `engine/src/ui/` — `i2d.*`, `widgets.*`, `screendraw.*`, `text.*`, `options.*`, `citymap.*`, `iamtext.*`, `surface.*`, `overlay.h` |
| the model | `tools/sim/ui.py`, and `/ui` in the web viewer |
| the checks | `ui tables`, `ui answers`, `ui input`, `ui geometry`, `ui page`, `ui shop titles`, `menu layout`, `ui confirm gate`, `engine: UI`, `engine: screen world`, `engine: screen close`, `engine: text draw`, `engine: text block`, `engine: sneak verbs`, `engine: sneak memos`, `engine: sneak echo bar`, `engine: sneak map`, `engine: sneak quit`, `engine: shop open`, `engine: multiplan transfers`, `engine: hint shop`, `engine: lift`, `engine: terminal family`, `engine: gandhar door`, `engine: den locker`, `engine: xachen`, `engine: high score`, `engine: pause` |

## What is not settled

* **The start menu's OPTIONS and QUIT are not read.** The rows and the quit
  action are native code outside the widget records. Two task records also
  **disagree** on one address: `todo/ui-save-confirm-and-quit.md` (2026-09-18)
  reads `0x0047BC10` as the real exit, a `PostQuitMessage`;
  `todo/start-menu.md` (2026-09-22) reads the same address as a text function
  that quotes a name. Neither is in `docs/`, and this manual does not settle it.
* **Screen 35**, the options screen the sneak's tab hosts, is not ported as a
  screen; the port has the options page tree as a model instead.
* **Fourteen screens share one result variable**, so no per-screen comparison
  test exists for the answer layer; and the per-screen callbacks' own
  bookkeeping is recovered only in part (`docs/PORTING.md` B6).
* **Not yet played**, per `todo/play-test.md`: Den's locker, the cartridges,
  the terminals' header, the high-score board, the echo bar, the city map and
  the hint shop are committed and checked but await a person.
* **Filed and not done**: a tab strip should switch pages on the move rather
  than the confirm (`todo/ui-child-on-move.md`); the overwrite and destroy
  confirms should name the file; the combine's failure text on the echo bar has
  no writer; the special screens' sounds and their success timers are
  unported, so Den's wheels and the cartridge lamps close at once instead of
  blinking first (`docs/UI.md` §3k); the videophone is the one screen of the
  special survey left (`todo/missing-ui.md` §5).
* **The Vita overlay** is shown on the Mac for the gauges, the shoot HUD and the
  screens; what it is worth is a console number, and the save screen, a
  viewport item and the interference panel still read back.
* **The line and triangle 2D primitives cannot be raised from the interface at
  all**: the item vocabulary has no line, and exactly one item in the whole
  lifted tree carries the triangle bits — on a child panel no screen reaches
  (`docs/PORTING.md` B6).
