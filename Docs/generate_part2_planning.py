# -*- coding: utf-8 -*-
"""Generate the GADE7322 Part 2 procedural enemy waves planning document (editable .docx)."""

import re
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Circle, Rectangle

from docx import Document
from docx.shared import Pt, Inches, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn

ROOT = Path(r"C:\Users\TUF\Documents\Unreal\ProceduralGenGade3B")
FIGS = ROOT / "Docs" / "Figures"
OUT = ROOT / "Docs" / "GADE7322_Part2_Procedural_Waves_Planning_Updated.docx"
AUTHOR_FULL = "Harriet Manda"

GITHUB = "https://github.com/harrietaleck/ProceduralGenGade3B"
BRANCH = "part2"
ACCESSED = "2 October 2026"

FIG1 = FIGS / "Part2_Figure1_DirectorLoop.png"
FIG2 = FIGS / "Part2_Figure2_ThreatBudget.png"
FIG3 = FIGS / "Part2_Figure3_LaneChoice.png"

BLUE = "#2b6cb0"
LIGHT = "#e8f1fb"


# ---------------------------------------------------------------------------------------------
# Diagrams
# ---------------------------------------------------------------------------------------------

def draw_director_loop():
    fig, ax = plt.subplots(figsize=(9, 5.2), dpi=200)
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 6)
    ax.axis("off")

    boxes = [
        (0.3, 3.6, "1. Read the player", "tower damage, kill speed,\ndefenders lost, banked loot,\ndefender mix per lane"),
        (3.55, 3.6, "2. Adapt difficulty", "score vs 65% target\nrating moves 0.7 - 1.6"),
        (6.8, 3.6, "3. Plan the wave", "threat budget, enemy\nweights, elites,\nspawn delays"),
        (6.8, 0.5, "4. Choose lanes", "per spawn, from live\nlane coverage;\nreroute mid-wave"),
        (3.55, 0.5, "5. Spawn and monitor", "mid-wave relief / pressure\nrecord each enemy's outcome"),
    ]
    centres = []
    for x, y, title, body in boxes:
        ax.add_patch(FancyBboxPatch((x, y), 2.9, 1.9, boxstyle="round,pad=0.05,rounding_size=0.15",
                                    fc=LIGHT, ec=BLUE, lw=2))
        ax.text(x + 1.45, y + 1.45, title, ha="center", va="center", fontsize=11, fontweight="bold", color="#1a365d")
        ax.text(x + 1.45, y + 0.7, body, ha="center", va="center", fontsize=8.5, color="#2d3748")
        centres.append((x, y))

    def arrow(x1, y1, x2, y2, label=None):
        ax.annotate("", xy=(x2, y2), xytext=(x1, y1),
                    arrowprops=dict(arrowstyle="-|>", color=BLUE, lw=2, mutation_scale=18))
        if label:
            ax.text((x1 + x2) / 2, (y1 + y2) / 2 + 0.18, label, ha="center", fontsize=8, color="#4a5568")

    arrow(3.25, 4.55, 3.5, 4.55)
    arrow(6.5, 4.55, 6.75, 4.55)
    arrow(8.25, 3.55, 8.25, 2.45)
    arrow(6.75, 1.45, 6.5, 1.45)
    ax.annotate("", xy=(1.75, 3.55), xytext=(3.5, 1.45),
                arrowprops=dict(arrowstyle="-|>", color=BLUE, lw=2, mutation_scale=18,
                                connectionstyle="angle,angleA=180,angleB=-90"))
    ax.text(1.55, 2.2, "wave cleared", ha="center", fontsize=8.5, color="#4a5568", style="italic")

    ax.text(5, 5.85, "Wave Director loop (WaveDirector.cpp / WaveManager.cpp)", ha="center",
            fontsize=12, fontweight="bold", color="#1a365d")
    fig.tight_layout()
    fig.savefig(FIG1, bbox_inches="tight")
    plt.close(fig)


def draw_threat_budget():
    waves = [1, 2, 3, 4, 5]
    base = [6 + 3.5 * (w - 1) for w in waves]

    fig, ax = plt.subplots(figsize=(8, 4.4), dpi=200)
    for diff, label, colour, style in [
        (1.6, "Dominating player (rating 1.6)", "#c53030", "-"),
        (1.0, "Starting rating (1.0)", BLUE, "-"),
        (0.7, "Struggling player (rating 0.7)", "#2f855a", "-"),
    ]:
        ax.plot(waves, [b * diff for b in base], style, color=colour, lw=2.4, marker="o", label=label)

    ax.fill_between(waves, [b * 0.7 for b in base], [b * 1.6 for b in base], color=LIGHT, alpha=0.8,
                    label="Range the director can choose from")

    for w, b in zip(waves, base):
        ax.annotate(f"{b:.1f}", (w, b), textcoords="offset points", xytext=(0, 8), ha="center",
                    fontsize=8, color=BLUE)

    ax.set_xticks(waves)
    ax.set_xlabel("Wave")
    ax.set_ylabel("Threat budget (points)")
    ax.set_title("Threat budget = (6 + 3.5 x (wave - 1)) x difficulty rating", fontsize=11, fontweight="bold")
    ax.text(1.02, 1.0, "Basic = 1 pt\nWolf = 1 pt\nBear = 3 pts", transform=ax.transAxes, va="top",
            fontsize=9, bbox=dict(boxstyle="round", fc="white", ec="#a0aec0"))
    ax.grid(alpha=0.3)
    ax.legend(loc="upper left", fontsize=8.5)
    fig.tight_layout()
    fig.savefig(FIG2, bbox_inches="tight")
    plt.close(fig)


def draw_lane_choice():
    fig, ax = plt.subplots(figsize=(7.2, 7.2), dpi=200)
    ax.set_xlim(-6, 6)
    ax.set_ylim(-6, 6)
    ax.set_aspect("equal")
    ax.axis("off")

    path_colour = "#d6bc8a"
    lanes = {"North": ((0, 0.9), (0, 5.4)), "East": ((0.9, 0), (5.4, 0)),
             "South": ((0, -0.9), (0, -5.4)), "West": ((-0.9, 0), (-5.4, 0))}
    for (x1, y1), (x2, y2) in lanes.values():
        ax.plot([x1, x2], [y1, y2], color=path_colour, lw=22, solid_capstyle="butt", zorder=1)

    ax.add_patch(Rectangle((-0.9, -0.9), 1.8, 1.8, fc="#4a5568", ec="black", zorder=3))
    ax.text(0, 0, "Tower", ha="center", va="center", color="white", fontsize=10, fontweight="bold", zorder=4)

    defenders = [(1.0, 3.2, "#f6ad55"), (-1.0, 3.8, "#4299e1"), (1.0, 2.0, "#48bb78"),
                 (2.6, 1.6, "#f6ad55"), (4.2, -1.0, "#4299e1"), (-1.0, -3.5, "#f6ad55")]
    for x, y, c in defenders:
        ax.add_patch(Circle((x, y), 0.38, fc=c, ec="black", lw=1, zorder=3))

    coverage = {"North": (0.0, 5.75, "North: coverage 31 (high)"),
                "East": (4.0, 0.75, "East: coverage 19"),
                "South": (0.0, -5.85, "South: coverage 7"),
                "West": (-3.7, 0.75, "West: coverage 0 (weakest)")}
    for x, y, text in coverage.values():
        ax.text(x, y, text, ha="center", fontsize=9, fontweight="bold", color="#2d3748")

    ax.annotate("Wolf pack\n(hunts defenders)", xy=(0.35, 4.6), xytext=(3.3, 4.9), fontsize=9,
                color="#4a5568", arrowprops=dict(arrowstyle="-|>", color="#718096", lw=2))
    for y in (4.9, 4.45, 4.0):
        ax.add_patch(Circle((0.25, y), 0.17, fc="#a0aec0", ec="black", zorder=4))

    ax.add_patch(Rectangle((-4.9, -0.35), 0.7, 0.7, fc="#7b4a1e", ec="black", zorder=4))
    ax.annotate("Bear\n(probes weakest lane)", xy=(-4.6, -0.45), xytext=(-5.6, -2.6), fontsize=9,
                color="#7b4a1e", arrowprops=dict(arrowstyle="-|>", color="#7b4a1e", lw=2))

    ax.text(0, -5.2 + -0.0, "", ha="center")
    ax.text(3.2, -4.2, "Coverage = sum of the damage\nper second of every defender\nwithin range of the lane",
            fontsize=8.5, ha="center", bbox=dict(boxstyle="round", fc="white", ec="#a0aec0"))
    ax.text(-3.4, -4.3, "Struggling player:\neven rotation N > E > S > W",
            fontsize=8.5, ha="center", color="#2f855a", bbox=dict(boxstyle="round", fc="#f0fff4", ec="#9ae6b4"))
    ax.set_title("Spawn lane choice from live lane coverage", fontsize=12, fontweight="bold")
    fig.tight_layout()
    fig.savefig(FIG3, bbox_inches="tight")
    plt.close(fig)


# ---------------------------------------------------------------------------------------------
# Document helpers (same formatting as the Part 1 report)
# ---------------------------------------------------------------------------------------------

BODY_WORDS = []


def set_run_font(run, name="Times New Roman", size=12, bold=False, italic=False):
    run.font.name = name
    run._element.rPr.rFonts.set(qn("w:eastAsia"), name)
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic


def add_para(doc, text, *, bold=False, italic=False, size=12, align="left", space_after=8, first_line=True):
    p = doc.add_paragraph()
    p.alignment = {"center": WD_ALIGN_PARAGRAPH.CENTER, "justify": WD_ALIGN_PARAGRAPH.JUSTIFY}.get(
        align, WD_ALIGN_PARAGRAPH.LEFT)
    pf = p.paragraph_format
    pf.space_after = Pt(space_after)
    pf.line_spacing = 1.5
    if first_line and align == "justify":
        pf.first_line_indent = Cm(1.27)
    run = p.add_run(text)
    set_run_font(run, size=size, bold=bold, italic=italic)
    return p


def add_body(doc, text):
    """Body paragraph that counts towards the 400-500 word limit."""
    BODY_WORDS.append(text)
    return add_para(doc, text, align="justify")


def style_headings(doc):
    for name, size in (("Heading 1", 14), ("Heading 2", 12)):
        st = doc.styles[name]
        st.font.name = "Times New Roman"
        st.font.size = Pt(size)
        st.font.bold = True
        st.font.italic = False
        st.font.color.rgb = RGBColor(0, 0, 0)
        rfonts = st.element.rPr.rFonts
        for attr in ("w:asciiTheme", "w:hAnsiTheme", "w:eastAsiaTheme", "w:cstheme"):
            if rfonts.get(qn(attr)) is not None:
                del rfonts.attrib[qn(attr)]
        rfonts.set(qn("w:ascii"), "Times New Roman")
        rfonts.set(qn("w:hAnsi"), "Times New Roman")
        rfonts.set(qn("w:eastAsia"), "Times New Roman")


def add_heading_custom(doc, text, level=1):
    p = doc.add_paragraph(style=f"Heading {level}")
    pf = p.paragraph_format
    pf.space_before = Pt(18 if level == 1 else 12)
    pf.space_after = Pt(8)
    pf.line_spacing = 1.5
    run = p.add_run(text)
    set_run_font(run, size=14 if level == 1 else 12, bold=True)
    run.font.color.rgb = RGBColor(0, 0, 0)
    return p


def add_toc(doc):
    add_heading_custom(doc, "Contents").style = doc.styles["Normal"]
    p = doc.add_paragraph()
    parts = [("begin", None), (None, 'TOC \\o "1-2" \\h \\z \\u'), ("separate", None)]
    for fld_type, instr in parts:
        run = p.add_run()
        if fld_type:
            el = OxmlElement("w:fldChar")
            el.set(qn("w:fldCharType"), fld_type)
        else:
            el = OxmlElement("w:instrText")
            el.set(qn("xml:space"), "preserve")
            el.text = instr
        run._r.append(el)
    set_run_font(p.add_run("Right-click and choose Update Field to build the contents."), italic=True)
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    p.add_run()._r.append(end)


def add_figure(doc, path, caption, width=5.8):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(10)
    p.paragraph_format.space_after = Pt(4)
    p.add_run().add_picture(str(path), width=Inches(width))
    cap = doc.add_paragraph()
    cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
    cap.paragraph_format.space_after = Pt(12)
    set_run_font(cap.add_run(caption), size=10, italic=True)


def shade_cell(cell, hex_fill):
    tc_pr = cell._element.get_or_add_tcPr()
    shd = tc_pr.makeelement(qn("w:shd"), {qn("w:val"): "clear", qn("w:color"): "auto", qn("w:fill"): hex_fill})
    tc_pr.append(shd)


def add_table(doc, header, rows, caption):
    cap = doc.add_paragraph()
    cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
    cap.paragraph_format.space_before = Pt(10)
    cap.paragraph_format.space_after = Pt(4)
    set_run_font(cap.add_run(caption), size=10, italic=True)

    table = doc.add_table(rows=1, cols=len(header))
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, text in enumerate(header):
        cell = table.rows[0].cells[i]
        cell.text = ""
        set_run_font(cell.paragraphs[0].add_run(text), size=10.5, bold=True)
        shade_cell(cell, "DCE9F7")
    for row in rows:
        cells = table.add_row().cells
        for i, text in enumerate(row):
            cells[i].text = ""
            set_run_font(cells[i].paragraphs[0].add_run(text), size=10.5)
    doc.add_paragraph().paragraph_format.space_after = Pt(6)


# ---------------------------------------------------------------------------------------------
# Document
# ---------------------------------------------------------------------------------------------

def main():
    FIGS.mkdir(parents=True, exist_ok=True)
    draw_director_loop()
    draw_threat_budget()
    draw_lane_choice()

    doc = Document()
    section = doc.sections[0]
    for side in ("top_margin", "bottom_margin", "left_margin", "right_margin"):
        setattr(section, side, Cm(2.54))
    style = doc.styles["Normal"]
    style.font.name = "Times New Roman"
    style.font.size = Pt(12)
    style._element.rPr.rFonts.set(qn("w:eastAsia"), "Times New Roman")

    # ---- Cover ----
    for _ in range(3):
        add_para(doc, "", first_line=False, space_after=0)
    style_headings(doc)
    add_para(doc, "THE INDEPENDENT INSTITUTE OF EDUCATION", bold=True, size=14, align="center")
    add_para(doc, "EMERIS VEGA SCHOOL", bold=True, align="center", space_after=18)
    add_para(doc, "GADE7322: Game Development 3B", bold=True, size=14, align="center")
    add_para(doc, "Part 2 Portfolio of Evidence (PoE)", bold=True, size=13, align="center", space_after=24)
    add_para(doc, "Planning: Procedural Enemy Waves (Harriet Manda Part)", bold=True, size=16, align="center",
             space_after=4)
    add_para(doc, "Individual planning document: adaptive wave spawning strategy", bold=True, size=13,
             align="center", space_after=28)
    add_para(doc, f"Student Name: {AUTHOR_FULL}", align="center")
    add_para(doc, "Student Number: st10252836", align="center")
    add_para(doc, "Module: GADE7322", align="center")
    add_para(doc, "Year: 2026", align="center", space_after=18)
    add_para(doc, "GitHub repository:", align="center", space_after=2)
    add_para(doc, GITHUB, italic=True, size=11, align="center", space_after=2)
    add_para(doc, f"Branch: {BRANCH}", size=11, align="center", space_after=18)
    word_count_para = add_para(doc, "Word count (body text): [calculated]", size=11, align="center")
    doc.add_page_break()

    # ---- Contents ----
    add_toc(doc)
    doc.add_page_break()

    # ---- Body ----
    add_heading_custom(doc, "Overview")
    add_body(doc,
        "Enemy waves in this tower defence game are produced by a Wave Director (WaveDirector.cpp) "
        "instead of a fixed wave table (Manda, 2026e). After every wave it runs a four-step loop: read the player, "
        "adapt the difficulty, plan the next wave and choose spawn lanes (Figure 1). Adjusting the game "
        "to the player's performance is an established way of keeping players engaged "
        "(Mortazavi, Moradi and Vahabie, 2024).")
    add_figure(doc, FIG1, "Figure 1: The Wave Director loop that runs between waves (Manda, 2026a)")

    add_heading_custom(doc, "Scaling difficulty for a consistent challenge")
    add_body(doc,
        "Each wave receives a threat budget that is spent on enemies: a Basic or Wolf costs one point and "
        "a Bear costs three. The budget starts at six points and grows by 3.5 points per wave, then is "
        "multiplied by a difficulty rating between 0.7 and 1.6 (Figure 2). Enemy health and damage also "
        "rise slightly every wave, and the gap between spawns shortens from two seconds towards 0.8 "
        "seconds. Escalation is therefore guaranteed, while the rating fits the curve to the individual "
        "player.")
    add_figure(doc, FIG2, "Figure 2: Threat budget per wave for different difficulty ratings (Manda, 2026b)")

    add_heading_custom(doc, "Adapting to skill level and play style")
    add_body(doc,
        "When a wave is cleared, the director scores the player out of 100%: tower health lost (40%), how "
        "far enemies travelled before dying (30%), defenders lost (20%) and unspent loot (10%). The target "
        "is 65%, meaning challenged but winning. The rating moves by the gap between the score and this "
        "target, so dominant players meet larger, faster and tougher waves, while struggling players face "
        "smaller waves with more Basic enemies and earn 20% more loot per kill. Guo, Thawonmas and Ren "
        "(2024) argue that difficulty adjustment should serve a clear design goal; here that goal is a "
        "consistent challenge. The director also adapts during a wave: if the tower loses 30% of its "
        "health, the gaps between remaining spawns become 50% longer, and if the player clears the board "
        "early, the next enemy is sent immediately.")

    add_heading_custom(doc, "Determining spawn locations")
    add_body(doc,
        "Enemies enter at the start of a procedurally generated path, so spawn points change with every "
        "map. Before each spawn, the director measures every lane's coverage by adding up the damage of "
        "the defenders within range of it (Figure 3). Wolves favour heavily defended lanes because they "
        "hunt defenders, whereas Bears, and Basics facing a confident player, favour the weakest lane. "
        "Struggling players receive an even rotation across lanes so that a single gap is not punished "
        "repeatedly, and wolf packs always share a lane. Mid-wave, Bears and elites can switch to a less "
        "defended path found with A* search.")
    add_figure(doc, FIG3, "Figure 3: How lane coverage decides where each enemy type enters (Manda, 2026c)",
               width=4.6)

    add_heading_custom(doc, "Deciding when to spawn which enemy type")
    add_body(doc,
        "Enemy types are introduced gradually: Basic from wave 1, Wolves from wave 2 and Bears from wave 3, "
        "each guaranteed in its first wave. The remaining budget is then spent through weighted random "
        "choice, with weights that counter the player's style (Table 1). This follows the idea of an "
        "adaptive tower defence wave manager (Hind and Harvey, 2024), but uses readable rules rather than "
        "a neural network, so behaviour stays predictable and easy to tune. From wave 3, some enemies "
        "spawn as elites whose modifier also counters the player's style. Every wave opens with a Basic "
        "enemy, and the random seed combines the map seed with the wave number.")
    add_heading_custom(doc, "Enemy type weighting by detected play style table", level=2)
    add_table(doc,
        ["Play style the director detects", "How the next wave responds", "Elite modifier"],
        [
            ["Defender-heavy (2+ defenders per lane)", "More Wolves, which leave the path to hunt defenders",
             "Splitting: breaks into two on death"],
            ["Long-range focus (40%+ Archers)", "More Wolves, whose fast packs outrun slow single shots",
             "Shielded: blocks the first three hits"],
            ["Area-damage focus (40%+ Bombs)", "More Bears, armoured single targets that resist splash damage",
             "Swift: 40% faster"],
            ["Light defence or tower only", "More Bears, which march straight to the tower",
             "Regenerating: heals over time"],
            ["Struggling (rating below 0.9)", "More Basics, even lane rotation, extra loot",
             "Fewer elites"],
            ["Dominating (rating above 1.1)", "Extra Bear weight, enemies target the weakest lane",
             "More elites"],
        ],
        "Table 1: Enemy type weighting by detected play style (Manda, 2026d)")

    # ---- Word count ----
    words = sum(len(re.findall(r"\b[\w'%.-]+\b", t)) for t in BODY_WORDS)
    word_count_para.runs[0].text = f"Word count (body text, excluding headings, captions, table and references): {words}"

    # ---- References ----
    doc.add_page_break()
    add_heading_custom(doc, "Reference list")
    refs = [
        "Guo, Z., Thawonmas, R. and Ren, X., 2024. Rethinking dynamic difficulty adjustment for video game "
        "design. Entertainment Computing, [e-journal] 50, 100663. Available at: "
        f"<https://doi.org/10.1016/j.entcom.2024.100663> [Accessed {ACCESSED}].",
        "Hind, D. and Harvey, C., 2024. Using a NEAT approach with curriculums for dynamic content "
        "generation in video games. Personal and Ubiquitous Computing, [e-journal]. Available at: "
        f"<https://doi.org/10.1007/s00779-024-01801-z> [Accessed {ACCESSED}].",
        "Manda, H., 2026a. Wave Director loop. [diagram] (Personal Collection). Unpublished.",
        "Manda, H., 2026b. Threat budget per wave for different difficulty ratings. [chart] "
        "(Personal Collection). Unpublished.",
        "Manda, H., 2026c. Spawn lane choice from live lane coverage. [diagram] (Personal Collection). "
        "Unpublished.",
        "Manda, H., 2026d. Enemy type weighting by detected play style. [table] (Personal Collection). "
        "Unpublished.",
        f"Manda, H., 2026e. ProceduralGenGade3B (Version part2) [Source code]. Available at: "
        f"<{GITHUB}/tree/part2> [Accessed {ACCESSED}].",
        "Mortazavi, F., Moradi, H. and Vahabie, A.-H., 2024. Dynamic difficulty adjustment approaches in "
        "video games: a systematic literature review. Multimedia Tools and Applications, [e-journal] 83(35), "
        f"pp.83227–83274. Available at: <https://doi.org/10.1007/s11042-024-18768-x> [Accessed {ACCESSED}].",
    ]
    for ref in refs:
        add_para(doc, ref, space_after=10)

    doc.save(OUT)
    print(f"Saved {OUT}")
    print(f"Body word count: {words}")


if __name__ == "__main__":
    main()
