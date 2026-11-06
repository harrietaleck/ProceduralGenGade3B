# -*- coding: utf-8 -*-
"""Generate the GADE7322 Part 3 individual planning document: Evolving Blight (editable .docx).

Brief: an individual document of 400-500 words giving a detailed explanation of the planned procedural feature,
supported by drawings and diagrams. The body text is kept inside 400-500 words; the extra detail is carried by the
figures, tables and pseudo-code, which do not count towards the limit. References follow the IIE Harvard Anglia style.
"""

import re
import sys
from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Pt, Cm, RGBColor

sys.path.insert(0, str(Path(__file__).resolve().parent))
import part3_figures as figs  # noqa: E402

ROOT = Path(r"C:\Users\TUF\Documents\Unreal\ProceduralGenGade3B")
OUT = ROOT / "Docs" / "GADE7322_Part3_Procedural_Feature_Planning.docx"
SCREENSHOT = Path(r"C:\Users\TUF\AppData\Local\Temp\claude\70f6a6041cb1018b\3678ed87-64c1-4729-bb57-8746edba6a25\scratchpad\evo_3.png")

AUTHOR = "Harriet Manda"
STUDENT_NO = "st10252836"
GITHUB = "https://github.com/harrietaleck/ProceduralGenGade3B"
BRANCH = "part3"
ACCESSED = "6 November 2026"

DEEP = RGBColor(0x1F, 0x5F, 0x4A)
GOLD = RGBColor(0xD9, 0xA4, 0x41)
BARK = RGBColor(0x6B, 0x4A, 0x2E)
INK = RGBColor(0x24, 0x33, 0x2D)
GREY = RGBColor(0x6B, 0x7A, 0x72)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)

BODY_FONT = "Cambria"
HEAD_FONT = "Calibri"

BODY = []          # every counted body paragraph
FIG_N = [0]
TAB_N = [0]


# ---------------------------------------------------------------------------------------------
# Low-level helpers
# ---------------------------------------------------------------------------------------------

def set_font(run, name=BODY_FONT, size=11, bold=False, italic=False, color=None):
    run.font.name = name
    rpr = run._element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    for attr in ("w:ascii", "w:hAnsi", "w:eastAsia", "w:cs"):
        rfonts.set(qn(attr), name)
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic
    if color is not None:
        run.font.color.rgb = color


def shade(cell, hex_fill):
    tc_pr = cell._element.get_or_add_tcPr()
    for old in tc_pr.findall(qn("w:shd")):
        tc_pr.remove(old)
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), hex_fill)
    tc_pr.append(shd)


def cell_borders(cell, **edges):
    """edges: top/bottom/left/right = (size_eighths_of_pt, hex_colour) or None for no border."""
    tc_pr = cell._element.get_or_add_tcPr()
    borders = tc_pr.find(qn("w:tcBorders"))
    if borders is None:
        borders = OxmlElement("w:tcBorders")
        tc_pr.append(borders)
    for edge in ("top", "left", "bottom", "right"):
        spec = edges.get(edge, "keep")
        if spec == "keep":
            continue
        el = borders.find(qn(f"w:{edge}"))
        if el is None:
            el = OxmlElement(f"w:{edge}")
            borders.append(el)
        if spec is None:
            el.set(qn("w:val"), "nil")
        else:
            el.set(qn("w:val"), "single")
            el.set(qn("w:sz"), str(spec[0]))
            el.set(qn("w:color"), spec[1])
            el.set(qn("w:space"), "0")


def cell_margins(cell, top=80, bottom=80, left=120, right=120):
    tc_pr = cell._element.get_or_add_tcPr()
    mar = OxmlElement("w:tcMar")
    for name, val in (("top", top), ("left", left), ("bottom", bottom), ("right", right)):
        el = OxmlElement(f"w:{name}")
        el.set(qn("w:w"), str(val))
        el.set(qn("w:type"), "dxa")
        mar.append(el)
    tc_pr.append(mar)


def para_border(paragraph, edge="bottom", size=12, colour="D9A441", space=2):
    p_pr = paragraph._p.get_or_add_pPr()
    borders = p_pr.find(qn("w:pBdr"))
    if borders is None:
        borders = OxmlElement("w:pBdr")
        p_pr.append(borders)
    el = OxmlElement(f"w:{edge}")
    el.set(qn("w:val"), "single")
    el.set(qn("w:sz"), str(size))
    el.set(qn("w:space"), str(space))
    el.set(qn("w:color"), colour)
    borders.append(el)


def set_col_widths(table, widths_cm):
    table.autofit = False
    tbl_pr = table._tbl.tblPr
    layout = OxmlElement("w:tblLayout")
    layout.set(qn("w:type"), "fixed")
    tbl_pr.append(layout)
    for row in table.rows:
        for cell, w in zip(row.cells, widths_cm):
            cell.width = Cm(w)


def add_field(paragraph, instruction, size=9, color=GREY):
    run = paragraph.add_run()
    for kind, text in (("begin", None), (None, instruction), ("separate", None), (None, "1"), ("end", None)):
        if kind:
            el = OxmlElement("w:fldChar")
            el.set(qn("w:fldCharType"), kind)
        elif text == instruction:
            el = OxmlElement("w:instrText")
            el.set(qn("xml:space"), "preserve")
            el.text = instruction
        else:
            el = OxmlElement("w:t")
            el.text = text
        run._r.append(el)
    set_font(run, HEAD_FONT, size, color=color)


def para(doc, text="", *, font=BODY_FONT, size=11, bold=False, italic=False, color=None, align="justify",
         before=0, after=6, line=1.32, keep_next=False, indent_first=False):
    p = doc.add_paragraph()
    p.alignment = {"left": WD_ALIGN_PARAGRAPH.LEFT, "center": WD_ALIGN_PARAGRAPH.CENTER,
                   "right": WD_ALIGN_PARAGRAPH.RIGHT, "justify": WD_ALIGN_PARAGRAPH.JUSTIFY}[align]
    pf = p.paragraph_format
    pf.space_before, pf.space_after, pf.line_spacing = Pt(before), Pt(after), line
    pf.keep_with_next = keep_next
    if text:
        set_font(p.add_run(text), font, size, bold, italic, color)
    return p


def body(doc, text):
    """A counted body paragraph."""
    BODY.append(text)
    return para(doc, text, size=11, align="justify", after=7, line=1.38)


def heading(doc, number, text):
    p = doc.add_paragraph(style="Heading 1")
    p.paragraph_format.space_before = Pt(16)
    p.paragraph_format.space_after = Pt(6)
    p.paragraph_format.keep_with_next = True
    run = p.add_run(f"{number}  ")
    set_font(run, HEAD_FONT, 17, True, color=GOLD)
    run = p.add_run(text)
    set_font(run, HEAD_FONT, 17, True, color=DEEP)
    para_border(p, "bottom", 8, "1F5F4A", 3)
    return p


def caption(doc, kind, text, source=None):
    if kind == "Figure":
        FIG_N[0] += 1
        n = FIG_N[0]
    else:
        TAB_N[0] += 1
        n = TAB_N[0]
    p = para(doc, "", align="center", before=2, after=12 if kind == "Figure" else 4, keep_next=(kind == "Table"))
    set_font(p.add_run(f"{kind} {n}: "), HEAD_FONT, 9.5, True, color=DEEP)
    run = p.add_run(text + (f" ({source})" if source else ""))
    set_font(run, HEAD_FONT, 9.5, False, True, GREY)
    return n


def figure(doc, key, text, source, width=16.2):
    p = para(doc, "", align="center", before=6, after=0, keep_next=True)
    p.add_run().add_picture(str(figs.PATHS[key]), width=Cm(width))
    caption(doc, "Figure", text, source)


def table(doc, header, rows, widths, caption_text, font_size=9):
    # caption_text=None gives an unnumbered summary table, so the numbered tables match the numbers cited in the text
    if caption_text:
        caption(doc, "Table", caption_text)
    tbl = doc.add_table(rows=1, cols=len(header))
    tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, text in enumerate(header):
        cell = tbl.rows[0].cells[i]
        cell.text = ""
        shade(cell, "1F5F4A")
        cell_margins(cell)
        cell_borders(cell, top=(8, "1F5F4A"), bottom=(8, "1F5F4A"), left=(4, "1F5F4A"), right=(4, "1F5F4A"))
        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
        set_font(cell.paragraphs[0].add_run(text), HEAD_FONT, font_size + 0.5, True, color=WHITE)
    for r, row in enumerate(rows):
        cells = tbl.add_row().cells
        for i, text in enumerate(row):
            cells[i].text = ""
            shade(cells[i], "EEF6F0" if r % 2 == 0 else "FFFFFF")
            cell_margins(cells[i])
            cell_borders(cells[i], top=(4, "C9D8CE"), bottom=(4, "C9D8CE"), left=(4, "C9D8CE"), right=(4, "C9D8CE"))
            cells[i].vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            p = cells[i].paragraphs[0]
            p.paragraph_format.space_after = Pt(0)
            set_font(p.add_run(text), HEAD_FONT, font_size, i == 0, color=INK)
    set_col_widths(tbl, widths)
    # Keep each table on one page: stop rows splitting and chain every row to the next one.
    for r_i, row in enumerate(tbl.rows):
        tr_pr = row._tr.get_or_add_trPr()
        cant_split = OxmlElement("w:cantSplit")
        tr_pr.append(cant_split)
        if r_i < len(tbl.rows) - 1:
            for c in row.cells:
                for p in c.paragraphs:
                    p.paragraph_format.keep_with_next = True
    para(doc, "", after=8)
    return tbl


def callout(doc, title, lines, fill="EEF6F0", bar="D9A441", mono=False):
    tbl = doc.add_table(rows=1, cols=1)
    tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
    cell = tbl.rows[0].cells[0]
    shade(cell, fill)
    cell_margins(cell, 140, 140, 220, 160)
    cell_borders(cell, left=(36, bar), top=None, bottom=None, right=None)
    cell.text = ""
    p = cell.paragraphs[0]
    p.paragraph_format.space_after = Pt(4)
    set_font(p.add_run(title), HEAD_FONT, 10.5, True, color=DEEP)
    for line in lines:
        q = cell.add_paragraph()
        q.paragraph_format.space_after = Pt(1.5 if mono else 3)
        q.paragraph_format.line_spacing = 1.15
        if mono:
            set_font(q.add_run(line), "Consolas", 8.8, color=INK)
        else:
            set_font(q.add_run(line), HEAD_FONT, 10, color=INK)
    set_col_widths(tbl, [16.2])
    para(doc, "", after=8)


# ---------------------------------------------------------------------------------------------
# The document
# ---------------------------------------------------------------------------------------------

def build():
    figs.draw_all(SCREENSHOT)

    doc = Document()
    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21.0), Cm(29.7)
    sec.left_margin = sec.right_margin = Cm(2.4)
    sec.top_margin, sec.bottom_margin = Cm(2.3), Cm(2.2)
    sec.different_first_page_header_footer = True

    normal = doc.styles["Normal"]
    normal.font.name = BODY_FONT
    normal.font.size = Pt(11)
    normal.element.rPr.rFonts.set(qn("w:eastAsia"), BODY_FONT)
    for name in ("Heading 1",):
        st = doc.styles[name]
        st.font.name = HEAD_FONT
        st.font.color.rgb = DEEP

    # ---- header and footer (not on the cover) ----
    hp = sec.header.paragraphs[0]
    hp.alignment = WD_ALIGN_PARAGRAPH.LEFT
    set_font(hp.add_run("Evolving Blight  |  Procedural generation planning  |  GADE7322 Part 3"), HEAD_FONT, 8.5, color=GREY)
    para_border(hp, "bottom", 6, "C9D8CE", 4)
    fp = sec.footer.paragraphs[0]
    fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_font(fp.add_run(f"{AUTHOR}  |  {STUDENT_NO}  |  Page "), HEAD_FONT, 8.5, color=GREY)
    add_field(fp, "PAGE", 8.5)

    # ================= COVER =================
    band = doc.add_table(rows=1, cols=1)
    band.alignment = WD_TABLE_ALIGNMENT.CENTER
    cell = band.rows[0].cells[0]
    shade(cell, "1F5F4A")
    cell_margins(cell, 420, 420, 480, 480)
    cell_borders(cell, top=None, left=None, right=None, bottom=(36, "D9A441"))
    cell.text = ""
    p = cell.paragraphs[0]
    p.paragraph_format.space_after = Pt(18)
    set_font(p.add_run("THE INDEPENDENT INSTITUTE OF EDUCATION  \u00b7  EMERIS VEGA SCHOOL"), HEAD_FONT, 8.5, True, color=GOLD)
    q = cell.add_paragraph()
    q.paragraph_format.space_after = Pt(2)
    set_font(q.add_run("GADE7322  \u00b7  Game Development 3B  \u00b7  Part 3 Portfolio of Evidence"), HEAD_FONT, 10, color=WHITE)
    q = cell.add_paragraph()
    q.paragraph_format.space_before = Pt(26)
    q.paragraph_format.space_after = Pt(2)
    set_font(q.add_run("Evolving Blight"), HEAD_FONT, 40, True, color=WHITE)
    q = cell.add_paragraph()
    q.paragraph_format.space_after = Pt(10)
    set_font(q.add_run("A procedurally generated enemy ecosystem that evolves from wave to wave"), HEAD_FONT, 14, False, True, WHITE)
    set_col_widths(band, [16.2])

    para(doc, "", after=10)
    para(doc, "Individual planning document: custom procedural generation feature", font=HEAD_FONT, size=13, bold=True,
         color=DEEP, align="left", after=10)

    info = doc.add_table(rows=0, cols=2)
    info.alignment = WD_TABLE_ALIGNMENT.CENTER
    wc_cell = {}
    rows = [("Student", f"{AUTHOR}  ({STUDENT_NO})"), ("Module", "GADE7322 Game Development 3B, 2026"),
            ("Repository", GITHUB), ("Branch", BRANCH), ("Body word count", "[calculated]")]
    for label, value in rows:
        r = info.add_row().cells
        r[0].text = r[1].text = ""
        shade(r[0], "EEF6F0")
        for c in r:
            cell_margins(c, 70, 70, 140, 140)
            cell_borders(c, top=(4, "D6E3DA"), bottom=(4, "D6E3DA"), left=None, right=None)
        set_font(r[0].paragraphs[0].add_run(label), HEAD_FONT, 9.5, True, color=DEEP)
        set_font(r[1].paragraphs[0].add_run(value), HEAD_FONT, 9.5, color=INK)
        r[0].paragraphs[0].paragraph_format.space_after = Pt(0)
        r[1].paragraphs[0].paragraph_format.space_after = Pt(0)
        wc_cell[label] = r[1].paragraphs[0]
    set_col_widths(info, [4.0, 12.2])

    para(doc, "", after=6)
    p = para(doc, "", align="center", after=4)
    p.add_run().add_picture(str(figs.PATHS["gallery"]), width=Cm(16.2))

    doc.add_page_break()

    # ================= AT A GLANCE =================
    para(doc, "The feature at a glance", font=HEAD_FONT, size=20, bold=True, color=DEEP, align="left", after=2)
    p = para(doc, "", after=10)
    para_border(p, "bottom", 18, "D9A441", 2)

    callout(doc, "Design goal", [
        "Replayable variety that always feels fair. Each generation should look and play differently from the last, never "
        "exceed its biomass budget, and always be explainable to the player.",
    ])

    table(doc, ["Aspect", "Decision"], [
        ["Technique", "Genetic algorithm: a form of search-based procedural content generation"],
        ["What is generated", "Enemy stats, size, colour, shader motion and a name, all derived from a six-gene genome"],
        ["When it runs", "Once per wave: breed after the wave, hand out genomes as the next wave spawns"],
        ["What drives selection", "How far each enemy travelled along its path before dying (bonus for reaching the tower)"],
        ["Keeps it fair", "Shared biomass budget, capped growth, gene limits and a protected champion"],
        ["Reproducible", "Seeded by the map seed: same map and same play give the same lineages"],
        ["Where it lives", "UEnemyEvolution, FEnemyGenome, AEnemy::ApplyGenome, the Wave Manager and the HUD"],
        ["How it is checked", "Four automated tests plus play-testing across several waves"],
    ], [4.2, 12.0], None)

    # ================= 1 CONCEPT =================
    heading(doc, "1", "Concept and research basis")
    body(doc,
        "Evolving Blight replaces fixed enemy types with a population that evolves while the player plays. Every enemy "
        "carries a genome of six genes, and after each wave the enemies that travelled furthest breed the next "
        "generation, so the Blight adapts to the player's defence (Figure 1). Evolutionary search is a recognised "
        "family of procedural content generation (Maleki and Zhao, 2024; Togelius, et al., 2024). It has generated "
        "tower defence waves by scoring candidates on damage to the base and distance walked (Wattanapornprom, et "
        "al., 2024), while neural waves left players reporting no discernible difference from human-designed ones "
        "(Hind and Harvey, 2024). A genetic algorithm was chosen for readable, testable, tunable rules. Adapting challenge often improves engagement (Mortazavi, Moradi and Vahabie, 2024), yet outcomes "
        "are mixed unless it serves a clear design goal (Guo, Thawonmas and Ren, 2024), stated above as fair, "
        "replayable variety.")
    figure(doc, "loop", "The evolution loop that runs once per wave", "Manda, 2026a")

    # ================= 2 GENOME =================
    heading(doc, "2", "The genome and its biomass budget")
    body(doc,
        "Six genes lie between 0 and 1. Hardiness, Swiftness and Fury set health, speed and damage and share a "
        "biomass budget, so strength in one gene costs the others (Table 1, Figure 2). Girth, Pulse and Hue only "
        "change appearance. Every multiplier equals 1.0 at a gene value of 0.5, so an average genome behaves like the "
        "original enemy. The budget grows 3% per generation up to a cap, raising pressure gently without creating "
        "unbeatable enemies.")
    table(doc, ["Gene", "Controls", "Gameplay effect", "Visible as"], [
        ["Hardiness", "Health", "Max health x (0.5 + Hardiness)", "none (a tank looks heavy through Girth)"],
        ["Swiftness", "Speed", "Move speed x (0.6 + 0.8 Swiftness)", "faster shader ripple"],
        ["Fury", "Damage", "Attack damage x (0.5 + Fury)", "brighter emissive glow"],
        ["Girth", "Size", "Body scale x (0.75 + 0.5 Girth)", "bigger or smaller body"],
        ["Pulse", "Ripple", "none (looks only)", "vertex displacement amplitude 3 to 14 units"],
        ["Hue", "Colour", "none (looks only)", "type colour rotated by up to 30 degrees"],
    ], [2.4, 2.2, 5.6, 6.0], "Genome specification: what each gene does")
    figure(doc, "budget", "Equal biomass can buy very different builds, and every gene value maps to a clear multiplier", "Manda, 2026b")

    # ================= 3 SELECTION =================
    heading(doc, "3", "Selection, crossover and mutation")
    body(doc,
        "Fitness is path progress before death, plus 0.5 for reaching the tower. A genetic algorithm applies "
        "selection, crossover and mutation to a population, and keeping diversity is a recognised concern "
        "(Alhijawi and Awajan, 2024). Therefore the best 40% become parents, the champion is "
        "copied unchanged, and every other child uses uniform crossover with a 25% chance of mutating each gene "
        "(Table 2, Figure 3). Generated enemies should differ yet stay good (Viana, Pereira and Toledo, 2022), so "
        "mutation never stops and generation zero is random. Moderate populations performed best in earlier tower "
        "defence work, which suits waves of six to fourteen enemies (Wattanapornprom, et al., 2024).")
    table(doc, ["Parameter", "Value", "Reason"], [
        ["Starting biomass", "1.5", "an even 0.5 / 0.5 / 0.5 split of the three power genes"],
        ["Biomass growth", "+3% per generation, capped at 2.1", "rising pressure that can never run away"],
        ["Fitness", "progress 0 to 1, +0.5 if it reached the tower", "rewards getting further and finishing the job"],
        ["Parents", "best 40% of the wave, at least two", "enough selection pressure while keeping a gene pool"],
        ["Champion", "best enemy copied unchanged", "a bad mutation can never lose the best design"],
        ["Crossover", "uniform: each gene from either parent", "mixes builds without needing gene order"],
        ["Mutation", "25% per gene, change of up to 0.15", "steady variety without destroying a good build"],
        ["Loot", "x (0.7 + 0.5 x power rating)", "danger pays, so tougher mutants are worth fighting"],
    ], [3.4, 5.8, 7.0], "Evolution parameters, all editable in the editor without recompiling")
    callout(doc, "Pseudo-code for the two moments evolution runs", [
        "ON SPAWN (for each enemy):",
        "    if first enemy of this generation:  genome = champion (unchanged)",
        "    else:  A, B = two random parents",
        "           gene = A.gene or B.gene           # uniform crossover",
        "           with 25% chance per gene:  gene += random(-0.15, +0.15)",
        "           if Hardiness + Swiftness + Fury > biomass:  scale all three down",
        "           name = start(strongest gene) + ending + epithet(second strongest)",
        "",
        "ON WAVE END:",
        "    fitness = progress + (0.5 if it reached the tower)",
        "    sort enemies by fitness;  parents = top 40% (at least 2);  champion = best",
        "    generation += 1;  biomass = min(2.1, 1.5 x 1.03 ^ generation)",
    ], fill="F3F4F2", bar="1F5F4A", mono=True)
    figure(doc, "lineage", "How a lineage develops: the champion survives, parents cross, and mutations add variety", "Manda, 2026c")

    # ================= 4 APPEARANCE =================
    heading(doc, "4", "Appearance and player feedback")
    body(doc,
        "Genes are made visible. Girth scales the body, while Pulse and Swiftness drive the amplitude and speed of a "
        "vertex displacement shader through World Position Offset (Epic Games, [s.a.]). Hue rotates the type colour by "
        "up to 30 degrees, so Wolves and Bears stay recognisable (Figure 4). Each genome is named from its two "
        "strongest genes, and the HUD announces the generation and champion (Figure 5).")
    figure(doc, "gallery", "Four example mutants: genes decide size, ripple, colour and strength", "Manda, 2026d")
    figure(doc, "game", "The existing game with the two places the feature will appear (own screenshot, annotated)", "Manda, 2026e", width=15.4)

    # ================= 5 INTEGRATION =================
    heading(doc, "5", "Integration, testing and balance")
    body(doc,
        "A new EnemyEvolution object sits beside the Wave Director, the Wave Manager requests a genome at every spawn "
        "and reports every outcome, and the enemy applies it (Figures 6 and 7). The director's threat budget stays "
        "valid because the biomass budget keeps average power stable, and stronger genomes drop more loot. Evolution is "
        "seeded by the map seed, so identical maps and play reproduce identical lineages. Four automated tests check "
        "budget limits, reproducibility, champion preservation and selection pressure (Table 3), and play-testing "
        "judges fairness against the design goal. Table 4 lists risks. Known limit: save files do not store "
        "lineages (Manda, 2026h).")
    figure(doc, "architecture", "How the feature plugs into the existing code", "Manda, 2026f")
    figure(doc, "swimlane", "Sequence of events for one wave", "Manda, 2026g")
    table(doc, ["Automated test", "What it proves", "Pass condition", "Prototype result"], [
        ["Genomes respect the biomass budget", "no genome can exceed the shared budget", "25 seeds x 12 generations x 14 enemies stay within budget and gene limits", "Passed"],
        ["Same seed gives the same Blight", "runs are reproducible and testable", "identical genomes for equal seeds, different for different seeds", "Passed"],
        ["Evolution climbs towards what survives", "selection really changes the population", "mean Swiftness rises by more than 0.08 over 8 generations", "Passed: 0.46 to 0.94"],
        ["Champion survives unchanged", "elitism and map reset work", "first genome of a generation equals the champion; a new seed resets", "Passed"],
    ], [3.9, 3.7, 5.5, 3.1], "Test plan and prototype results")
    table(doc, ["Risk", "Why it matters", "Mitigation"], [
        ["Runaway difficulty", "enemies could become unbeatable", "shared biomass budget, 2.1 cap, gene limits"],
        ["Loss of variety", "the population could converge on one design", "constant mutation, random generation zero, looks-only genes"],
        ["Unreadable enemy types", "players must still tell Wolf from Bear", "Hue limited to 30 degrees around the type colour"],
        ["Feels unfair or random", "players need to understand why enemies change", "named champion on the HUD, loot scales with danger"],
        ["Hard to test", "random systems hide bugs", "seeded randomness and four automated tests"],
        ["Lineages lost on resume", "save files do not store the population", "documented limit; a restored match starts a fresh Blight"],
    ], [3.6, 5.6, 7.0], "Risks and mitigations")

    # ================= REFERENCES =================
    doc.add_page_break()
    heading(doc, "", "Reference list")

    refs = [
        [("Alhijawi, B. and Awajan, A., 2024. Genetic algorithms: theory, genetic operators, solutions, and applications. ", False),
         ("Evolutionary Intelligence", True),
         (", [e-journal] 17(3), pp.1245-1256. Available at: <https://doi.org/10.1007/s12065-023-00822-6> [Accessed " + ACCESSED + "].", False)],
        [("Epic Games, [s.a.]. Material inputs in Unreal Engine 5.8 documentation. [online] Available at: "
          "<https://dev.epicgames.com/documentation/en-us/unreal-engine/material-inputs-in-unreal-engine> [Accessed " + ACCESSED + "].", False)],
        [("Guo, Z., Thawonmas, R. and Ren, X., 2024. Rethinking dynamic difficulty adjustment for video game design. ", False),
         ("Entertainment Computing", True),
         (", [e-journal] 50, p.100663. Available at: <https://doi.org/10.1016/j.entcom.2024.100663> [Accessed " + ACCESSED + "].", False)],
        [("Hind, D. and Harvey, C., 2024. Using a NEAT approach with curriculums for dynamic content generation in video games. ", False),
         ("Personal and Ubiquitous Computing", True),
         (", [e-journal] 28(3-4), pp.629-641. Available at: <https://doi.org/10.1007/s00779-024-01801-z> [Accessed " + ACCESSED + "].", False)],
        [("Maleki, M.F. and Zhao, R., 2024. Procedural content generation in games: a survey with insights on emerging LLM integration. ", False),
         ("Proceedings of the AAAI Conference on Artificial Intelligence and Interactive Digital Entertainment", True),
         (", [e-journal] 20(1), pp.167-178. Available at: <https://doi.org/10.1609/aiide.v20i1.31877> [Accessed " + ACCESSED + "].", False)],
        [("Manda, H., 2026a. Evolution loop. [Personal drawing]. [s.l.]: Unpublished.", False)],
        [("Manda, H., 2026b. Biomass budget and gene multipliers. [Personal drawing]. [s.l.]: Unpublished.", False)],
        [("Manda, H., 2026c. Illustrative lineage over three generations. [Personal drawing]. [s.l.]: Unpublished.", False)],
        [("Manda, H., 2026d. Four example mutants. [Personal drawing]. [s.l.]: Unpublished.", False)],
        [("Manda, H., 2026e. The existing game with the feature locations marked. [Personal screenshot]. [s.l.]: Unpublished.", False)],
        [("Manda, H., 2026f. Architecture of the Evolving Blight feature. [Personal drawing]. [s.l.]: Unpublished.", False)],
        [("Manda, H., 2026g. Sequence of events for one wave. [Personal drawing]. [s.l.]: Unpublished.", False)],
        [("Manda, H., 2026h. ProceduralGenGade3B (Version " + BRANCH + ") [Source code]. Available at: <" + GITHUB + "/tree/" + BRANCH + "> [Accessed " + ACCESSED + "].", False)],
        [("Mortazavi, F., Moradi, H. and Vahabie, A.-H., 2024. Dynamic difficulty adjustment approaches in video games: a systematic literature review. ", False),
         ("Multimedia Tools and Applications", True),
         (", [e-journal] 83(35), pp.83227-83274. Available at: <https://doi.org/10.1007/s11042-024-18768-x> [Accessed " + ACCESSED + "].", False)],
        [("Togelius, J., Khalifa, A., Earle, S., Green, M.C. and Soros, L., 2024. Evolutionary machine learning and games. In: W. Banzhaf, P. Machado and M. Zhang, eds. ", False),
         ("Handbook of evolutionary machine learning", True),
         (". Singapore: Springer, pp.715-737. Available at: <https://doi.org/10.1007/978-981-99-3814-8_25> [Accessed " + ACCESSED + "].", False)],
        [("Viana, B.M.F., Pereira, L.T. and Toledo, C.F.M., 2022. Illuminating the space of enemies through MAP-Elites. ", False),
         ("2022 IEEE Conference on Games (CoG)", True),
         (". Beijing, China, 21-24 August 2022. Piscataway, NJ: IEEE, pp.17-24. Available at: <https://doi.org/10.1109/CoG51982.2022.9893621> [Accessed " + ACCESSED + "].", False)],
        [("Wattanapornprom, W., Satsuk, C., Sirisakornsakul, T., Jamornthawuch, T., Rodkaew, Y. and Susutti, W., 2024. Automating tower defense game level design with evolutionary algorithms. ", False),
         ("2024 28th International Computer Science and Engineering Conference (ICSEC)", True),
         (". Khon Kaen, Thailand, 6-8 November 2024. Piscataway, NJ: IEEE, pp.1-6. Available at: <https://doi.org/10.1109/ICSEC62781.2024.10770629> [Accessed " + ACCESSED + "].", False)],
    ]
    for ref in refs:
        p = para(doc, "", align="left", after=5, line=1.18)
        p.paragraph_format.left_indent = Cm(1.0)
        p.paragraph_format.first_line_indent = Cm(-1.0)
        for text, ital in ref:
            set_font(p.add_run(text), BODY_FONT, 9.5, False, ital, INK)

    # ---- word count: body paragraphs only (headings, captions, tables, pseudo-code and references are excluded) ----
    words = sum(len(re.findall(r"\b[\w'%.\-\[\]]+\b", t)) for t in BODY)
    wc_cell["Body word count"].runs[-1].text = (
        f"{words} words  (headings, captions, tables, pseudo-code and references excluded)")
    doc.save(OUT)
    return words


if __name__ == "__main__":
    total = build()
    print(f"Saved {OUT}")
    print(f"Body word count: {total}")
    print("Whitespace-split count (how Word counts):", sum(len(t.split()) for t in BODY))
