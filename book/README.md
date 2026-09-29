# How Omikron Works — the book

A book for developers about the engine of *Omikron: The Nomad Soul* (1999) and
about OMK, the replica that runs it again. It is meant to be **read in order**,
first page to last: each chapter leans on the ones before it, it explains
mechanisms with diagrams and analogies (Unity, OpenGL), and it keeps numbers to
the ones that carry an idea.

| file | what |
|---|---|
| [`omk-book.pdf`](omk-book.pdf) | the book, typeset |
| [`omk-book.md`](omk-book.md) | the same text as one Markdown file (its figures are in `figures/`, its pictures in `../traces/frames/` and `../manual/images/`) |
| `chapters/*.md` | the source, one file per part, in page order |
| `figures/*.svg` | the diagrams; `figures/make_figures.py` draws them |
| `build.py` | builds `omk-book.md`, `omk-book.html` and `omk-book.pdf` |

## How it relates to the manual and the docs

The **manual** (`../manual/`) is the reference: chapter by chapter, with the
addresses, the offsets and the check behind every number. This book is the
companion read before it. Both are **derivative**: they retell findings whose
evidence lives in `../docs/`, and if either disagrees with `docs/`, **`docs/`
is right**. Neither is ever cited as evidence.

Unlike the manual, the book is not a snapshot regenerated only on request: it
is a draft that is edited like any other document. When a finding it retells
changes, the book should change with it.

## Building it

```sh
python3 book/figures/make_figures.py   # the diagrams, if a figure changed
python3 book/build.py                  # omk-book.md + .html + .pdf
python3 book/build.py --no-pdf         # without the PDF
```

Standard library only, like the rest of `tools/`: the Markdown subset the
chapters use is converted by `build.py` itself, and the PDF is printed by a
headless Google Chrome (`/Applications/Google Chrome.app` on macOS). The PDF and
the one-file Markdown are **build outputs committed on purpose**, so the book
can be read without building it; `omk-book.html` is the intermediate and is
not committed. Rebuild both after editing a chapter, and commit them together.

## Licence

The prose and the diagrams are **CC-BY-4.0**, like `docs/` (`../docs/LICENSE`);
`build.py` and `figures/make_figures.py` are code, **GPL-3.0-or-later**
(`../LICENSE`). The pictures embedded in the PDF come from two places:
`../manual/images/` holds **renders by the port** (output of GPL code reading
data you supply), and `../traces/frames/` holds **frames of the original
game's own framebuffer**, which are not OMK's to relicense.
