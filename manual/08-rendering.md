# 8. Rendering

← [Contents](README.md) · prev: [Conversations and cutscenes](07-conversations-and-cutscenes.md) · next: [Audio](09-audio.md)

---

## In short

The original draws through Direct3D in its 1999 fixed-function form: it hands
the driver vertices it has already transformed and lit, and a texture, and the
driver fills the triangles. A set is *pre-lit* — every vertex carries a colour
baked into the file — and only the street's moving population is lit at run
time, by a table of lights that ships inside the set's model.

It is a particular picture, and the port has to draw that picture rather than a
better one. The engine turns anti-aliasing **off**, samples textures **point**
with no mipmaps, and turns the driver's **dither on**. A character's shadow is
not a shadow pass but one soft blob copied under a handful of bones. The far
skyline of a city shimmers, because a mesh flag oscillates its vertex colour on
the frame clock. The framebuffer is **16-bit RGB565**, and that decides what a
comparison against a captured frame is allowed to mean.

The interface is a separate world — a 16-layer display list blitted with a
colour key. A blit is a memory copy, so the 2D half of this chapter is
reproduced pixel for pixel; the 3D half is judged only on **silhouette and
coverage**, for exactly **one camera** so far, and no 3D pixel's value is
claimed at all.

The port draws through one boundary at the level of **decisions**, with three
backends behind it: a software rasterizer that the checks measure, a Vulkan
backend for playing on a desktop, and a GLES2 backend that now runs the game on
a real PS Vita into the city — slowly. Everything the original did not do
(anti-aliasing, filtering, real shadow maps, per-pixel light, supersampling) is
an **enhancement**, off by default.

## In detail

### What is ported is decisions

The interface between the engine and a backend (`docs/PORTING.md` A2):

```
begin(view)                        the camera and its frustum
submit(draw)                       {bucketKey, mesh, range, blend, cutout}
submit2d(I2dList)                  the display list, already ordered
end() -> Frame                     RGB565, 640x480
```

A backend receives decisions and turns them into API calls; it never makes one.
The decisions are the port:

* **the drawable mask** — one test, `flags & 0x800043`, which replaced three
  viewer heuristics and disagrees with them in both directions
  (`docs/ASSETS.md` §4; `drawable mask`);
* **the 14-bit bucket key** that orders every draw (§4b; `render bucket key`);
* **the texture as the key's low six bits**, and nothing else;
* **two blend modes** — `0x1000|0x2000` additive (211 meshes) and
  `0x1000|0x4000` multiply (6 meshes), the multiply being `dst × (1 − src)` —
  with `0x800` a separate cutout path (§4);
* **the 58-slot texture cache**, matched on a 19-character name (§4b);
* **the visible-set walk** against the engine's own frustum (`sub_48D0D0`).

PORTING A2 gives the reason the line sits there: at the API level the ported
decisions would leak into backend code, and a second backend could not be
added without extracting them again. `engine: renderer` checks that the
boundary itself is not a renderer — the software backend behind it moves 0
pixels against the direct call (`docs/RECONSTRUCTION.md`, 2026-09-01).

### The render states the original sets

`sub_4638C0`, the hardware arm of the device set-up, writes the render states
once (`docs/ASSETS.md` §4b): **ANTIALIAS explicitly off**, magnification and
minification **POINT**, **no mipmap** filter, perspective-correct texturing,
Gouraud, `CULLMODE = NONE`, specular on — and `DITHERENABLE` **on**.

That splits cleanly into two kinds of work:

* anti-aliasing and filtering are **enhancements**: the original turned them
  off, so a replica judged against it must draw without them unless told
  otherwise;
* the dither is a **fidelity fix**: the port had drawn hard 565 bands where the
  original drew dithered noise. It is on by default in every backend, applied
  at the single 888 → 565 quantisation point (`engine/src/ui/surface.h`). What
  is ported is the *decision*; the 4 × 4 ordered matrix belongs to the player's
  graphics card and is a **labelled reconstruction**. `engine: dither` holds it
  to what can be checked — a 4 × 4 block reconstructs its input to 0.491 of 255
  against 2.047 for plain rounding, at a bias of −0.249.

The same shape of defect turned up once more. Mesh flag `0x8000000` oscillates
the vertex colour of **233 set meshes** — distant scenery, 132 of them in
Lahoreh — through a 32-entry signed table read with `movsx`, indexed by the
frame clock and the vertex's own position, a cycle of about 2.1 s
(`docs/ASSETS.md` §4c). The port had decoded the phase and read it nowhere; it
is now drawn by default. `shimmer table` asserts the table's three copies —
the header's, the shader's and the executable's bytes — against each other,
because a table duplicated for a shader drifts.

**Before building an enhancement, check whether the game already does the
thing and the port dropped it.** That is cheaper than any enhancement, and it
is what the original looks like.

### RGB565, and how a comparison has to be done

Measured from a captured frame (`docs/PORTING.md` A3, `traces/frames/menu-22.png`):
red carries 32 distinct levels, green 63, blue 25 of a possible 32, with the
gaps of a 5/6-bit value expanded by bit replication.

So comparisons are made **in 565, not in 888**. A captured frame's 8-bit values
are the *host's* expansion, not the game's data. Expanding the reference up to
888 matches the menu's title region 94.9%; quantising the capture back down to
565 matches **66 560 of 66 560**. Bring the capture into the framebuffer's
space, never the other way.

### The set is lit before it ships

A set's vertices carry a **colour**, not a brightness: the engine copies the
whole dword at vertex `+28` into the vertex it hands the driver and declares
`D3DFVF_DIFFUSE` in both its draw calls. Reading only the green byte renders
every set in monochrome, and 38.9% of set vertices are not grey
(`docs/ASSETS.md` §4c).

The engine *does* contain a luma conversion, in the **second render bank** —
six two-entry arrays swapped by `sub_42FA00`, whose scene renderer is the same
bucket walk with every vertex colour converted to grey. VM opcode 150 installs
it at 14 shipped sites against 15 restores, and 74 of the 82 instructions
between them are camera and fade opcodes: the game has **black-and-white
cutscenes** (`docs/PORTING.md` B6; `render back ends`).

### The lights inside a model

The 1999 press sheet advertised "Multilights", and the table is real: **4 179
records of 304 bytes across 216 models**, each naming itself `LIGHT` and
carrying two radii in round metres, an RGB colour, a position and a footprint
(`docs/FILE_FORMATS.md` §5b; `mesh lights`). The count is at `desc+240`, not
the `+232` a first reading used — the loader overwrites `+232` from it, and
only `+240` lands the table on the file size in all 216 files.

A decor supplies the lights and the **street's moving population receives
them**: `sub_4380B0`'s eight call sites are all street life
(`docs/STREET_LIFE.md`; `light consumers`). Per vertex it is `−(N·L)` over a
linear falloff between the two radii, through a `(t × c) >> 8` ramp, which
needed the vertex **normal** at `.3DO` vertex `+12`. A lit instance starts from
**black** — the crowd models ship pure white — so the lights are the crowd's
whole illumination (`engine vertex light`).

### The shadows

There is **no shadow pass** (`docs/ASSETS.md` §4d). The engine has one shipped
quad, `MESHES\MISC\shadows.3DO` — a soft white disc on black, in the multiply
bucket — and `Actor_DrawShadow` writes copies of it into the frame's own pools
under a fixed set of **bones** each frame. The options menu's detail level says
how many: arms at 2, head and legs at 1, the chest always; an NPC gets one level
less, and a level of 3 or more draws nothing. The reaches are 3.6 m for the
chest, 1.8 m for the arms and head, 1.4 m for the legs; the size is the bone's
own bounding sphere over 10 (chest), 12 (head and legs) or 14 (arms), clamped
at 1.5; and the blob fades by its
vertex colour, `255 − dist × 255 / reach`, as the bone rises. The crowd's shadow
is a separate whole node at the midpoint of the two feet.

The trap is the lookup: **bones are found by `strstr`, keeping the last
match**, because every character bone carries a prefix. An equality test draws
nothing at all. And a crowd model carries **four LOD skeletons side by side**,
so each bone name matches four times and the last-match rule lands on the
lowest-detail skeleton — the worst bone sat 252.7 units from its body until
the search was scoped to the skeleton the animation poses, and 22.8 after
(`shadow model`, `engine: character shadow`).

### What one option sizes

The clip distance (options row 3) is in **metres** against an inch world unit
and sizes **four things at once** (`docs/ASSETS.md` §4b): the visible-set
radius, the near split at a quarter of it, the far split at 0.95 of it, and the
fog. The fog is **linear**, from a quarter of the clip distance to exactly the
clip distance, and its colour ships as **black** — the scene's `+336` is
zeroed and nothing else writes it — so the city darkens toward the horizon
rather than hazing (`engine fog`).

The sky (row 4) is a flat painted **ceiling**, not a dome: the area chunk's
`+133` names one of six models, scaled 12.5×, lifted 2 250 units and moved to
the camera's x and z every frame. 17 of the 259 areas name one (`the sky`). The
engine keeps **one** sky, not one per resident slot, so whichever arriving area
names one replaces it (`docs/RECONSTRUCTION.md`, 2026-09-08).

### The mirrors

Mesh flag `0x100000` is carried by **6 of 12 203** meshes, one is live at a
time, and the engine reflects the camera through the mirror's plane and **draws
the whole scene again** — a second full pass, gated on the display driver, so
hardware mode only (`docs/ASSETS.md` §4). The blend mode is the compositing
operator over that reflection, which is why one mirror is additive and another
multiply.

The port's version sits on the renderer boundary, so every backend gets it
(0.998 coverage agreement; `mirror pass`). Two parts are **reconstruction** and
say so: how the engine confines the reflection to the mirror's area, and the
plane's normal. Both were confirmed by **flying a camera across viewpoints**,
which is the only thing that can settle a plane, a normal's sign or a flip.

### The texture cache

The cache hands out 58 slots from a **global** pool, matching on the texture's
19-character name alone; on a hit it points the material at whatever is already
there (`docs/ASSETS.md` §4b). Two decor sets are resident at once — hidden is
not unloaded — so an arriving set cache-hits against the one you walked out of.
**182 texture names ship with different pixels in different model files**, and
rendered with only the resident neighbour changing, `AImpasse` moves 6 920
pixels of an Anekbah frame and `AToit` **121 588** (`anekbah rendered`). The
location draws differently depending on where you walked in from — in the
original too. Which panel of the reported shot this accounts for is narrowed,
not confirmed.

### The depth tie — a flicker that was the port's

Beside those panels a reader saw a **flicker**, and it turned out to be the
port's (`docs/ASSETS.md` §4). Anekbah's 18 coincident shop-sign pairs are the
two **sides** of a sign — the same four vertices in opposite winding, one advert
a side. With culling off the engine submits both, and its strict depth test on
a quantised z-buffer shows whichever was drawn **first**. A float comparison
cannot reproduce that: the two windings split the quad on different diagonals,
their depths differ by up to 2 × 10⁻⁷, and last-bit noise picked the face pixel
by pixel — dots of the other advert, re-rolled by every movement of the camera.

The backends settle it two ways:

* the **software** rasterizer uses a 2⁻¹⁶ relative tie band, a reconstruction
  of the buffer's quantisation rather than its bit depth (`engine: sign tie`);
* the **GPU** backends cannot read their depth buffer, so they settle the tie at
  submit — a face whose positions an earlier depth-writing face already claimed
  is degenerated in the vertex buffer (`o3de/depthtie.*`). A 16-bit depth buffer
  with a strict test is not enough on its own: the GLES backend has exactly
  that, and without the tie the GPU gave the second face the whole sign
  (`todo/optimization.md` §27).

The tie is decided every frame, and much of the performance work went into
making that cheap without changing an answer: hashed and flat tables
(`engine: tie equivalence`), a **replay** of the previous revision's losers when
only some corners moved, a rigid body's losers replayed unchanged between
poses, and — on GLES — only the triangles the tie **newly** marks written back
to the buffer. That last change took the tie's buffer patches on Anekbah's
street from **~150 a frame to 0**, with the frame byte-identical over 400 frames
(`todo/vita-port.md`, the patch fix).

### The 2D layer

A 16-layer display list with seven primitives and their pools, ending in
`IDirectDrawSurface::Blt` with a colour key (`docs/UI.md` §1). The per-layer
cache is a **head** cache, so within one layer the rest draw in **reverse**
submission order; and the blits do not test the source Y.

Because a blit is a memory copy with no filtering, this half is exactly
reproducible, and it is (`docs/PORTING.md` B1): the menu title comes out
**66 560 of 66 560** pixels identical to the engine's own framebuffer, the load
panel's selection outline 1 518 of 1 518, and the ported text 6 132 of 6 132
(`engine: I2D blit`, `engine: text draw`).

<p align="center">
  <img src="images/menu-text-port.png" width="440" alt="The start menu's labels drawn by the port">
  <br><em>The four start-menu labels, drawn by the ported text renderer out of the<br>game's own fonts at the coordinates its widget tree gives them.</em>
</p>

### The 3D path, and what a captured frame can prove

PORTING B5: a captured 3D frame is exact about **geometry and ordering** and not
about a pixel's low bits — filtering, dithering and the fog table belong to the
driver, and the capture was taken under Wine rather than on a 1999 card.
Between two captures of the same parked scene, 42% of pixels differ by ≤ 8.

So the criterion is **silhouette and coverage**, and per-pixel equality is
refuted by the capture itself. Rendered through dialog 402's camera 4555, the
set scores 0.73 / 0.83 on edge alignment against the two parked captures on a
chance floor of 0.27 / 0.30, and 92% / 99% of the holes a set-only render leaves
fall where the capture is black, against 33% frame-wide (`docs/PORTING.md` B6;
`engine: silhouette`). That is **tier 4, for one camera in one set**.

<p align="center">
  <img src="images/dlg402-port-render.png" width="440" alt="The port's render of the apartment through camera 4555">
  <br><em>The same set through the same camera as <code>traces/frames/dlg402-47.png</code>.<br>The capture also carries a character, the props and a subtitle; the render<br>draws the set alone, which is the asymmetry the metric is built around.</em>
</p>

The one-camera limit bit within hours: a player flying the viewer found the
rasterizer had no near-plane clipping at all. Through 4555 the fix changes **0
pixels**; one step into the room it changes 12 710 (`engine: near clip`).

### The backends

* **Software** (`o3de/raster.*`) — the reference, and what every frame check
  measures. It is not a port — the engine has no software 3D rasterizer — but a
  reference implementation standing where Direct3D stood.
* **Vulkan** — MoltenVK on macOS, presenting directly. PORTING B6 gives it **no
  tier**: it is explicitly unverifiable, its correctness inherited from the
  software backend it mirrors, held to it by coverage (0.995 on the first set).
* **GLES2** — the PS Vita's (vitaGL), also built for macOS's GL. On a Mac it
  draws dialog 402's camera at 0.996 coverage against software, with its GPU
  dither and present exact to the CPU's, and the Anekbah street at 0.99
  (`todo/vita-port.md`; `engine: gles backend`). PORTING has no row for it; it
  is held the same way as Vulkan. On a **real console** it runs the films,
  the menu, the flat, the intro cutscene and the city. The console has no
  shader compiler a homebrew may ship, so the VPK carries vitaGL's shader
  cache, made once where the compiler exists; a console **without** the
  compiler running the game confirmed that on hardware (`todo/vita-port.md`,
  2026-09-22).

Two things the GLES backend does that the others do not, both for the Vita:

* **it poses bodies in its vertex shader.** A character is rigid per mesh, so
  the GPU needs its rest geometry once and one 3 × 4 matrix per mesh a frame,
  instead of a whole posed vertex buffer (`todo/gpu-skinning.md`). Steps 1–4
  are done: the posing program, the crowd's light in the shader (the same law,
  the same lights, one function choosing them), the walkers, and the staged
  bodies — all but a speaker whose line is animating his face. **The player is
  not yet** (step 5). The proof is on the Mac, CPU path against GPU path through
  the same backend: 0–17 pixels of 307 200 differ (`engine: gles pose`), and
  street frames differ by a handful of pixels. The software and Vulkan backends
  still pose on the CPU, so every software check is untouched by construction.
* **it skips GL state that has not changed** — blend, depth mask, texture,
  uniforms, attributes. On an Apple **M3**, over 248 draws of a street frame,
  1 809 state calls became 92, the pictures byte-identical
  (`todo/optimization.md` §17; `engine: gles state cache`).

Beside those, the texture upload's keyed RGB → RGBA conversion has a NEON loop
next to a generic one, the two exact against each other (`todo/vita-port.md`,
2026-09-25).

### The enhancements

Everything the original did not do is **off by default** and says so
(`todo/enhancements.md`): multisample anti-aliasing, bilinear and trilinear
filtering with anisotropy, filtered interface scaling, an unlimited clip
distance (which measures as buying nothing at the option's 200 m maximum),
**fitted** shadows (the same blobs laid on the surface under them), **mapped**
shadows (a 1024 × 1024 shadow map from the set's own lights — the one render
pass the port has that the original never had), per-pixel lighting by the
engine's own law, and supersampling. `--enhance-all` turns them all to their
top. Most are Vulkan-only. The software reference still draws exactly what the
original drew.

### Speed, and the machines it was measured on

The figures come from three machines and **are not comparable across them**:

* **Apple M1**, 2026-09-14, Vulkan, the Anekbah street capped at 30 fps: about
  **6.0 ms** of main-thread work a frame (`todo/optimization.md` §7). The
  decision recorded then: a 30 fps Vita build doing this CPU work on one core
  was not in reach, by an estimated factor of several — the CPU factor itself
  was framed by a break-even, not measured.
* **PS Vita**, the console's city log of 2026-09-27, on the CPU posing path:
  ~150–160 ms of work a frame, the GL submit the largest share at 42–47 ms
  (`todo/vita-port.md`, 2026-09-27; `todo/optimization.md` §15). The city runs
  slowly rather than freezing.
* **Apple M3**, 2026-09-29, the audit that ranked what is left for the Vita;
  its counts (allocations, patches, state calls) carry across machines, its
  milliseconds do not (§15).

One correction recorded with those numbers: the street's crowd density is the
engine's own rule — `Slider_Init` spawns `39 × (5 − density) × h[3]` walkers
from the same circuit, on the same models (`docs/STREET_LIFE.md`) — so the port
draws the **same characters** the original did. What it adds is its own: GL
driver calls, uploads and the depth tie (`todo/optimization.md` §7, corrected
2026-09-29).

## Where it lives

| | |
|---|---|
| the findings | `docs/ASSETS.md` §4 (the mirrors, the blend modes, the sign pairs), §4b (the walk, the key, the sky, the fog, the render states, the texture cache), §4c (colour, the shimmer, the dynamic lights), §4d (the shadows); `docs/FILE_FORMATS.md` §5b (the light record); `docs/PORTING.md` A2, A3, B5, B6 |
| the boundary | `engine/src/o3de/renderer.h` |
| the backends | `engine/src/o3de/raster.*` (software), `engine/backends/vulkan/`, `engine/backends/gles/glesrender.cpp`, `engine/backends/vita/` |
| the depth tie, lights, shadows, shimmer | `engine/src/o3de/depthtie.*`, `vertexlight.*`, `shadow.*`, `shimmer.h` |
| the 2D layer | `engine/src/ui/i2d.*`, `surface.*` |
| the records | `todo/enhancements.md`; `todo/optimization.md` (steps table, §15, §17, §27); `todo/gpu-skinning.md`; `todo/vita-port.md` and `todo/handoff-vita-port.md` |
| the checks | `drawable mask`, `render bucket key`, `texture name cache`, `anekbah rendered`, `engine: sign tie`, `engine: tie equivalence`, `mirror pass`, `render back ends`, `shimmer table`, `engine: shimmer`, `engine: dither`, `engine fog`, `the sky`, `mesh lights`, `light consumers`, `engine vertex light`, `shadow model`, `engine: character shadow`, `engine: I2D blit`, `engine: text draw`, `engine: raster`, `engine: silhouette`, `engine: near clip`, `engine: renderer`, `engine: gles backend`, `engine: gles pose`, `engine: gles state cache`, `engine: mapped shadows` — of 484 checks in all, 257 of them behind `--slow` (`verify.py --list`, 2026-09-29) |

## What is not settled

* **No 3D pixel's *value* has a reachable tier.** Filtering, dither, the fog
  arithmetic and the blend maths are the driver's, and no rig here can
  distinguish them. The dither matrix is a labelled reconstruction.
* **Tier 4 covers one camera in one set.** The silhouette metric sees no
  characters or props, by construction.
* **The mirror's confinement and its plane normal** are reconstruction,
  confirmed by play rather than traced.
* **Four of the second render bank's six swapped pointers** are unread, with no
  oracle to read them against.
* **A latent backend disagreement is recorded, not fixed**: with the dither off,
  the Vulkan readback truncates to 565 where the software path rounds, so the
  boundary that should move 0 pixels moves some (`docs/RECONSTRUCTION.md`,
  2026-09-09). With the dither on — the default — the two agree.
* **The depth tie is per frame; a load-time version is planned, not done.** The
  original's answer to a coincident pair is fixed once a set loads — the pairs
  sit in one mesh and are ordered by texture slot, a moving mesh moves both
  faces alike, and the slots change only at a load — so computing the winner
  once per load would reproduce it, texture-cache quirk included, at no
  per-frame cost. A census of all 635 models comes first, because a pair across
  two independently moving meshes is the one case a load-time answer could get
  wrong (`todo/optimization.md` §27).
* **GPU posing is proven on the Mac, not yet on the console.** The console's
  posing self-test failed on the odd matrix slots (a Vita GPU wants an even
  computed uniform index); the layout was changed to four rows a slot, and the
  next console log is the proof (`todo/vita-port.md`, 2026-09-27). The player is
  still posed on the CPU (`todo/gpu-skinning.md` step 5), as is a speaker's
  face while his line plays.
* **What the console gains from the state cache and the tie-patch cut is not
  measured** — both were measured on Macs. The Vita is not at 30 fps.
* **The lift's dark arrival.** At the security centre the arrival camera sits
  inside the lift car's own mesh (`CSPont04`) and the frame is almost black;
  displacing that one mesh takes it from 96.7% dark to 16.9%. What the
  original does about the car is open — it is not the obstruction pass, which
  an absolute camera does not take (`docs/ASSETS.md` §7; `todo/missing-ui.md`
  §6).
* **No capture in this tree shows a shipped shadow**, so the blob's size is
  data-constrained.
