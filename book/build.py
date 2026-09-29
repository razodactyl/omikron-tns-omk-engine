#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the book: chapters/*.md -> omk-book.md (one file, page order) ->
omk-book.html -> omk-book.pdf (headless Chrome).

Stdlib only, like the rest of tools/: a small Markdown subset is converted
here rather than pulling in a dependency. What it understands is exactly what
the chapters use - headings, paragraphs, **bold**, *italic*, `code`, links,
images, fenced code, block quotes, ordered and unordered lists, pipe tables,
horizontal rules - plus raw HTML lines passed through (a page break is
`<div class="pagebreak"></div>`).

    python3 book/build.py            # md + html + pdf
    python3 book/build.py --no-pdf   # md + html only
"""
import html
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHAPTERS = os.path.join(HERE, "chapters")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"


def inline(t):
    """Inline markup on already-escaped text; code spans are protected first."""
    codes = []

    def keep(m):
        codes.append("<code>" + m.group(1) + "</code>")
        return "\x00%d\x00" % (len(codes) - 1)

    t = re.sub(r"`([^`]+)`", keep, t)
    t = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)\)", r'<img src="\2" alt="\1">', t)
    t = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", r'<a href="\2">\1</a>', t)
    t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
    t = re.sub(r"(?<![*\w])\*([^*\s][^*]*?)\*(?![*\w])", r"<em>\1</em>", t)
    t = re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], t)
    return t


def esc(t):
    return html.escape(t, quote=False)


def slug(t):
    return re.sub(r"[^a-z0-9]+", "-", t.lower()).strip("-")


def md_to_html(md):
    lines = md.split("\n")
    out, i, toc = [], 0, []
    para = []

    def flush():
        if para:
            out.append("<p>" + inline(esc(" ".join(para))) + "</p>")
            para.clear()

    while i < len(lines):
        ln = lines[i]
        s = ln.strip()
        if s.startswith("```"):
            flush()
            j = i + 1
            buf = []
            while j < len(lines) and not lines[j].strip().startswith("```"):
                buf.append(lines[j])
                j += 1
            out.append("<pre><code>" + esc("\n".join(buf)) + "</code></pre>")
            i = j + 1
            continue
        if s.startswith("<"):
            flush()
            mp = re.match(r'<h1 class="part" id="([^"]+)">(.*)</h1>', s)
            if mp:
                toc.append((0, mp.group(2), mp.group(1)))
            out.append(ln)
            i += 1
            continue
        m = re.match(r"^(#{1,4})\s+(.*)$", s)
        if m:
            flush()
            lvl, text = len(m.group(1)), m.group(2)
            sid = slug(text)
            if lvl <= 2:
                toc.append((lvl, text, sid))
            out.append('<h%d id="%s">%s</h%d>' % (lvl, sid, inline(esc(text)), lvl))
            i += 1
            continue
        if re.match(r"^-{3,}$", s):
            flush()
            out.append("<hr>")
            i += 1
            continue
        if s.startswith(">"):
            flush()
            buf = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                buf.append(lines[i].strip()[1:].lstrip())
                i += 1
            inner = md_to_html("\n".join(buf))[0]
            cls = ' class="aside"' if buf and buf[0].startswith("**") else ""
            out.append("<blockquote%s>%s</blockquote>" % (cls, inner))
            continue
        if s.startswith("|") and i + 1 < len(lines) and re.match(r"^\|?\s*:?-{3,}", lines[i + 1].strip()):
            flush()
            head = [c.strip() for c in s.strip("|").split("|")]
            i += 2
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            t = ["<table><thead><tr>"] + ["<th>%s</th>" % inline(esc(c)) for c in head] + ["</tr></thead><tbody>"]
            for r in rows:
                t.append("<tr>" + "".join("<td>%s</td>" % inline(esc(c)) for c in r) + "</tr>")
            t.append("</tbody></table>")
            out.append("".join(t))
            continue
        m = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", ln)
        if m:
            flush()
            ordered = m.group(2)[0].isdigit()
            tag = "ol" if ordered else "ul"
            items = []
            while i < len(lines):
                mm = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", lines[i])
                if mm and len(mm.group(1)) < 2:
                    items.append(mm.group(3))
                    i += 1
                elif lines[i].startswith("  ") and lines[i].strip() and items:
                    items[-1] += " " + lines[i].strip()
                    i += 1
                else:
                    break
            out.append("<%s>%s</%s>" % (tag, "".join("<li>%s</li>" % inline(esc(x)) for x in items), tag))
            continue
        if not s:
            flush()
            i += 1
            continue
        para.append(s)
        i += 1
    flush()
    return "\n".join(out), toc


CSS = """
@page { size: A4; margin: 22mm 20mm 24mm 20mm; }
body { font-family: 'Iowan Old Style', 'Palatino', Georgia, serif; font-size: 10.6pt;
       line-height: 1.52; color: #1d1d1f; max-width: 46em; margin: 0 auto; }
h1 { font-size: 21pt; margin: 0 0 .6em; padding-top: .4em; break-before: page;
     border-bottom: 2px solid #1d1d1f; padding-bottom: .2em; }
h1.part { text-align: center; border: none; font-size: 26pt; margin-top: 30%; }
h2 { font-size: 14pt; margin: 1.6em 0 .5em; }
h3 { font-size: 11.5pt; margin: 1.3em 0 .4em; }
h4 { font-size: 10.6pt; margin: 1.1em 0 .3em; font-style: italic; }
p { margin: .5em 0 .7em; text-align: justify; hyphens: auto; }
code { font-family: Menlo, 'SF Mono', monospace; font-size: 8.8pt; background: #f2f2f4;
       padding: 0 .2em; border-radius: 2px; }
pre { background: #f5f5f7; border: 1px solid #e0e0e4; padding: .7em .9em; overflow: hidden;
      font-size: 8.4pt; line-height: 1.38; break-inside: avoid; white-space: pre-wrap; }
pre code { background: none; padding: 0; font-size: inherit; }
blockquote { margin: 1em 0; padding: .4em 1em; border-left: 3px solid #9aa0a6; color: #333; }
blockquote.aside { background: #eef4fb; border-left: 4px solid #3b73b9; break-inside: avoid; }
table { border-collapse: collapse; margin: .8em 0 1em; font-size: 9.3pt; width: 100%; break-inside: avoid; }
th, td { border: 1px solid #d0d0d6; padding: .3em .5em; vertical-align: top; text-align: left; }
th { background: #f0f0f3; }
img { display: block; max-width: 100%; margin: 1em auto .3em; break-inside: avoid; }
img[src$='.png'] { max-width: 82%; }
.caption { text-align: center; font-size: 9pt; color: #555; margin: 0 0 1.2em; font-style: italic; }
.pagebreak { break-after: page; }
.title { text-align: center; margin-top: 28%; }
.title h1 { break-before: auto; border: none; font-size: 30pt; }
.toc a { color: inherit; text-decoration: none; }
.toc ul { list-style: none; padding-left: 0; }
.toc li.part { font-weight: bold; margin: .9em 0 .2em; }
.toc li.chap { margin: .12em 0 .12em 1.4em; }
a { color: #2a5ca8; }
hr { border: none; border-top: 1px solid #ccc; margin: 1.5em 0; }
"""


def main():
    files = sorted(f for f in os.listdir(CHAPTERS) if f.endswith(".md"))
    md = "\n\n".join(open(os.path.join(CHAPTERS, f), encoding="utf-8").read() for f in files)
    # the one-file Markdown edition reads its figures from ./figures, as the chapters do
    open(os.path.join(HERE, "omk-book.md"), "w", encoding="utf-8").write(md)
    body, toc = md_to_html(md)
    toc_html = ['<div class="toc"><h1 id="contents">Contents</h1><ul>']
    for lvl, text, sid in toc:
        if lvl == 0:
            toc_html.append('<li class="part"><a href="#%s">%s</a></li>' % (sid, text))
        elif lvl == 1 and text not in ("Contents", "How Omikron Works"):
            toc_html.append('<li class="chap"><a href="#%s">%s</a></li>' % (sid, inline(esc(text))))
    toc_html.append("</ul></div>")
    body = body.replace("<!-- TOC -->", "\n".join(toc_html), 1)
    page = ("<!doctype html><html><head><meta charset='utf-8'><title>How Omikron Works</title>"
            "<style>%s</style></head><body>%s</body></html>" % (CSS, body))
    htmlp = os.path.join(HERE, "omk-book.html")
    open(htmlp, "w", encoding="utf-8").write(page)
    print("wrote", htmlp)
    if "--no-pdf" in sys.argv:
        return
    pdf = os.path.join(HERE, "omk-book.pdf")
    r = subprocess.run([CHROME, "--headless", "--disable-gpu", "--no-pdf-header-footer",
                        "--print-to-pdf=" + pdf, "file://" + htmlp],
                       capture_output=True, text=True, timeout=240)
    if not os.path.exists(pdf):
        print(r.stderr[-2000:])
        sys.exit(1)
    print("wrote", pdf, os.path.getsize(pdf), "bytes")


if __name__ == "__main__":
    main()
