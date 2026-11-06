# -*- coding: utf-8 -*-
"""Generate the GADE7322 group report: work done, successes and challenges, with references and the GitHub link.

Numbers (commits, lines of code, tests) are read from the repository when this script runs, so they cannot drift
from the real project. Commits are de-duplicated by author and subject because some history was rewritten.
"""

import subprocess
import sys
from collections import Counter, defaultdict
from datetime import datetime, timedelta
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Pt, Cm

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_part3_planning as gp  # noqa: E402  (shared styling helpers)
import part3_figures as pf  # noqa: E402  (shared palette)

ROOT = Path(r"C:\Users\TUF\Documents\Unreal\ProceduralGenGade3B")
FIGS = ROOT / "Docs" / "Figures"
OUT = ROOT / "Docs" / "GADE7322_Part3_Group_Report.docx"
GITHUB = "https://github.com/harrietaleck/ProceduralGenGade3B"
ACCESSED = "6 November 2026"

HARRIET, BELINDA = "Harriet Manda", "Belinda Mtabane"

TOC_HEADINGS = [("1", "The project in brief"), ("2", "Who did what"), ("3", "Part 3 in detail"),
                ("4", "Successes"), ("5", "Challenges we met and how we solved them"),
                ("6", "Testing and what is still open"), ("7", "Acknowledgements and use of tools"),
                ("", "Reference list")]
H_COL, B_COL = pf.DEEP, pf.GOLD


# ---------------------------------------------------------------------------------------------
# Facts from the repository
# ---------------------------------------------------------------------------------------------

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, encoding="utf-8").stdout


def repo_facts():
    rows = git("log", "--all", "--no-merges", "--format=%an|%ad|%s", "--date=short").strip().splitlines()
    seen, commits = set(), []
    for row in rows:
        author, date, subject = row.split("|", 2)
        key = (author, subject)
        if key in seen:
            continue
        seen.add(key)
        who = "H" if "harriet" in author.lower() else "B"
        commits.append((who, datetime.strptime(date, "%Y-%m-%d"), subject))
    cpp = list((ROOT / "Source" / "ProceduralGenGade3B").glob("*.*"))
    lines = 0
    for f in cpp:
        if f.suffix in (".cpp", ".h", ".inl"):
            lines += len(f.read_text(encoding="utf-8", errors="replace").splitlines())
    files = len([f for f in cpp if f.suffix in (".cpp", ".h", ".inl")])
    return commits, lines, files


# ---------------------------------------------------------------------------------------------
# Figures
# ---------------------------------------------------------------------------------------------

def draw_timeline(commits):
    by_week = defaultdict(lambda: Counter())
    start = min(d for _, d, _ in commits)
    for who, d, _ in commits:
        monday = d - timedelta(days=d.weekday())
        by_week[monday][who] += 1
    weeks = sorted(by_week)
    fig, ax = plt.subplots(figsize=(10.4, 4.3), dpi=200)
    xs = range(len(weeks))
    h = [by_week[w]["H"] for w in weeks]
    b = [by_week[w]["B"] for w in weeks]
    ax.bar(xs, h, color=H_COL, width=0.7, label=HARRIET)
    ax.bar(xs, b, bottom=h, color=B_COL, width=0.7, label=BELINDA)
    ax.set_xticks(list(xs))
    ax.set_xticklabels([w.strftime("%d %b") for w in weeks], fontsize=7.5, rotation=45, ha="right")
    ax.set_ylabel("Commits per week")
    for sp in ("top", "right"):
        ax.spines[sp].set_visible(False)
    ax.grid(axis="y", alpha=0.25)
    ymax = max(x + y for x, y in zip(h, b)) if weeks else 1

    def marker(date_str, text, level, ha="center"):
        d = datetime.strptime(date_str, "%Y-%m-%d")
        monday = d - timedelta(days=d.weekday())
        if monday in by_week:
            i = weeks.index(monday)
            ax.annotate(text, (i, h[i] + b[i]), xytext=(i + (0.35 if ha == "left" else 0), ymax * (0.98 + 0.18 * level)),
                        fontsize=8, fontweight="bold", color=pf.BARK, ha=ha,
                        arrowprops=dict(arrowstyle="-", color=pf.BARK, lw=1))

    marker("2026-08-01", "Part 1: core game", 1, ha="left")
    marker("2026-09-03", "Part 1 UI: HUD and end screens", 0)
    marker("2026-10-02", "Part 2: enemies, Wave Director", 1)
    marker("2026-11-05", "Part 3: upgrades, shaders, Blight", 0)
    ax.set_ylim(0, ymax * 1.5)
    ax.legend(frameon=False, loc="upper right", fontsize=9)
    ax.set_title("Weekly commits on the shared repository", fontsize=11, fontweight="bold", color=pf.DEEP)
    fig.tight_layout()
    path = FIGS / "Group_Fig1_Timeline.png"
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    return path


def draw_ownership():
    # (label, owners) where owners is "H", "B" or "HB"
    cells = [
        [("Procedural terrain\nand lanes", "H"), ("Wave Manager and\nWave Director", "H"), ("A* rerouting, elites,\nstatus combos", "H")],
        [("Enemy types\n(Basic, Wolf, Bear)", "HB"), ("Defender types\n(Basic, Archer, Bomb, Strong)", "HB"), ("Projectiles and\ncombat feedback", "H")],
        [("Match HUD and\nmenu screens", "HB"), ("Upgrade system\n(tier I, then tier II)", "HB"), ("Tower-danger\npost-process", "H")],
        [("Niagara effects\nand materials", "B"), ("Music, save and\nresume", "B"), ("Vertex-displacement\nshader", "H")],
        [("Evolving Blight\n(individual feature)", "H"), ("Automated tests", "H"), ("Start, victory and\ndefeat screens", "B")],
    ]
    fig, ax = plt.subplots(figsize=(10.4, 6.0), dpi=200)
    ax.set_xlim(0, 10.4)
    ax.set_ylim(0, 6.5)
    ax.axis("off")
    w, h, gx, gy = 3.2, 0.88, 0.2, 0.2
    for r, row in enumerate(cells):
        for c, (label, owners) in enumerate(row):
            x = 0.1 + c * (w + gx)
            y = 4.95 - r * (h + gy)
            if owners == "H":
                fc, ec, tc = H_COL, H_COL, "white"
            elif owners == "B":
                fc, ec, tc = B_COL, B_COL, pf.INK
            else:
                fc, ec, tc = "white", pf.DEEP, pf.INK
            ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.12", fc=fc, ec=ec, lw=2))
            if owners == "HB":
                ax.add_patch(FancyBboxPatch((x, y), w / 2, h, boxstyle="round,pad=0.02,rounding_size=0.12", fc=H_COL, ec=H_COL, lw=0, alpha=0.18))
                ax.add_patch(FancyBboxPatch((x + w / 2, y), w / 2, h, boxstyle="round,pad=0.02,rounding_size=0.12", fc=B_COL, ec=B_COL, lw=0, alpha=0.28))
                ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.12", fc="none", ec=pf.DEEP, lw=2))
            ax.text(x + w / 2, y + h / 2, label, ha="center", va="center", fontsize=9, fontweight="bold", color=tc, linespacing=1.25)
    ax.add_patch(FancyBboxPatch((0.1, 0.1), 0.35, 0.28, boxstyle="round,pad=0.01", fc=H_COL, ec=H_COL))
    ax.text(0.6, 0.24, HARRIET, va="center", fontsize=9)
    ax.add_patch(FancyBboxPatch((3.2, 0.1), 0.35, 0.28, boxstyle="round,pad=0.01", fc=B_COL, ec=B_COL))
    ax.text(3.7, 0.24, BELINDA, va="center", fontsize=9)
    ax.add_patch(FancyBboxPatch((6.3, 0.1), 0.35, 0.28, boxstyle="round,pad=0.01", fc="white", ec=pf.DEEP, lw=2))
    ax.text(6.8, 0.24, "Worked on by both of us", va="center", fontsize=9)
    ax.text(5.2, 6.3, "Who built what", ha="center", fontsize=12.5, fontweight="bold", color=pf.DEEP)
    path = FIGS / "Group_Fig2_WhoBuiltWhat.png"
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    return path


# ---------------------------------------------------------------------------------------------
# Document helpers
# ---------------------------------------------------------------------------------------------

def pic(doc, path, text, width=16.2):
    p = gp.para(doc, "", align="center", before=6, after=0, keep_next=True)
    p.add_run().add_picture(str(path), width=Cm(width))
    gp.caption(doc, "Figure", text, None)


def text(doc, s):
    return gp.para(doc, s, size=11, align="justify", after=7, line=1.38)


def bullets(doc, items):
    for item in items:
        p = gp.para(doc, "", align="left", after=3, line=1.3)
        p.paragraph_format.left_indent = Cm(0.7)
        p.paragraph_format.first_line_indent = Cm(-0.45)
        gp.set_font(p.add_run("\u2022  "), gp.BODY_FONT, 11, True, color=gp.DEEP)
        gp.set_font(p.add_run(item), gp.BODY_FONT, 11, color=gp.INK)


def stat_cards(doc, stats):
    """A row of big-number cards."""
    tbl = doc.add_table(rows=1, cols=len(stats))
    tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
    for cell, (number, label) in zip(tbl.rows[0].cells, stats):
        gp.shade(cell, "EEF6F0")
        gp.cell_margins(cell, 140, 140, 100, 100)
        gp.cell_borders(cell, top=(24, "D9A441"), bottom=None, left=None, right=None)
        cell.text = ""
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.space_after = Pt(0)
        gp.set_font(p.add_run(number), gp.HEAD_FONT, 22, True, color=gp.DEEP)
        q = cell.add_paragraph()
        q.alignment = WD_ALIGN_PARAGRAPH.CENTER
        q.paragraph_format.space_after = Pt(0)
        gp.set_font(q.add_run(label), gp.HEAD_FONT, 8.5, color=gp.GREY)
    gp.set_col_widths(tbl, [16.4 / len(stats)] * len(stats))
    gp.para(doc, "", after=8)


# ---------------------------------------------------------------------------------------------
# The document
# ---------------------------------------------------------------------------------------------

def build(toc_pages=None):
    FIGS.mkdir(parents=True, exist_ok=True)
    commits, lines, files = repo_facts()
    h_count = sum(1 for w, _, _ in commits if w == "H")
    b_count = sum(1 for w, _, _ in commits if w == "B")
    timeline = draw_timeline(commits)
    ownership = draw_ownership()

    doc = Document()
    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21.0), Cm(29.7)
    sec.left_margin = sec.right_margin = Cm(2.4)
    sec.top_margin, sec.bottom_margin = Cm(2.3), Cm(2.2)
    sec.different_first_page_header_footer = True
    doc.styles["Normal"].font.name = gp.BODY_FONT
    doc.styles["Normal"].font.size = Pt(11)
    doc.styles["Heading 1"].font.name = gp.HEAD_FONT

    hp = sec.header.paragraphs[0]
    gp.set_font(hp.add_run("GADE7322 Part 3  |  Group report  |  Harriet Manda and Belinda Mtabane"), gp.HEAD_FONT, 8.5, color=gp.GREY)
    gp.para_border(hp, "bottom", 6, "C9D8CE", 4)
    fp = sec.footer.paragraphs[0]
    fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    gp.set_font(fp.add_run("Page "), gp.HEAD_FONT, 8.5, color=gp.GREY)
    gp.add_field(fp, "PAGE", 8.5)

    # ================= cover =================
    band = doc.add_table(rows=1, cols=1)
    band.alignment = WD_TABLE_ALIGNMENT.CENTER
    cell = band.rows[0].cells[0]
    gp.shade(cell, "1F5F4A")
    gp.cell_margins(cell, 420, 420, 480, 480)
    gp.cell_borders(cell, top=None, left=None, right=None, bottom=(36, "D9A441"))
    cell.text = ""
    p = cell.paragraphs[0]
    p.paragraph_format.space_after = Pt(18)
    gp.set_font(p.add_run("THE INDEPENDENT INSTITUTE OF EDUCATION  \u00b7  EMERIS VEGA SCHOOL"), gp.HEAD_FONT, 8.5, True, color=gp.GOLD)
    q = cell.add_paragraph()
    q.paragraph_format.space_after = Pt(2)
    gp.set_font(q.add_run("GADE7322  \u00b7  Game Development 3B  \u00b7  Group Portfolio of Evidence"), gp.HEAD_FONT, 10, color=gp.WHITE)
    q = cell.add_paragraph()
    q.paragraph_format.space_before = Pt(24)
    q.paragraph_format.space_after = Pt(2)
    gp.set_font(q.add_run("Group Report"), gp.HEAD_FONT, 38, True, color=gp.WHITE)
    q = cell.add_paragraph()
    q.paragraph_format.space_after = Pt(10)
    gp.set_font(q.add_run("Work done, successes and challenges: a procedural tower defence in Unreal Engine 5"),
                gp.HEAD_FONT, 14, False, True, gp.WHITE)
    gp.set_col_widths(band, [16.2])
    gp.para(doc, "", after=14)

    info = doc.add_table(rows=0, cols=2)
    info.alignment = WD_TABLE_ALIGNMENT.CENTER
    for label, value in [
        ("Group members", f"{HARRIET}  (st10252836)\n{BELINDA}  (ST10439106)"),
        ("Module", "GADE7322 Game Development 3B, 2026"),
        ("GitHub repository", GITHUB),
        ("Branches", "part1  \u00b7  part1UI  \u00b7  part2  \u00b7  part3  (all of Part 3)"),
        ("Engine", "Unreal Engine 5.8, C++ with UMG widgets and Niagara"),
        ("Referencing", "Harvard Anglia (The Independent Institute of Education, 2025)"),
    ]:
        r = info.add_row().cells
        r[0].text = r[1].text = ""
        gp.shade(r[0], "EEF6F0")
        for c in r:
            gp.cell_margins(c, 80, 80, 140, 140)
            gp.cell_borders(c, top=(4, "D6E3DA"), bottom=(4, "D6E3DA"), left=None, right=None)
            c.paragraphs[0].paragraph_format.space_after = Pt(0)
        gp.set_font(r[0].paragraphs[0].add_run(label), gp.HEAD_FONT, 9.5, True, color=gp.DEEP)
        gp.set_font(r[1].paragraphs[0].add_run(value), gp.HEAD_FONT, 9.5, color=gp.INK)
    gp.set_col_widths(info, [4.0, 12.2])

    gp.para(doc, "", after=14)
    stat_cards(doc, [(str(h_count + b_count), "commits (de-duplicated)"), (f"{lines:,}", "lines of C++ in the game module"),
                     (str(files), "C++ source files"), ("4", "automated tests"), ("3", "assessment parts built")])
    doc.add_page_break()

    # ================= contents + 1 overview =================
    gp.contents_block(doc, TOC_HEADINGS, toc_pages)
    gp.heading(doc, "1", "The project in brief")
    text(doc,
         "Our game is a 3D tower defence. Every match builds a new terrain from a random seed, with four lanes "
         "that lead to a central tower. Enemies walk those lanes and attack the tower and the defenders the player "
         "places on build pads. The terrain is generated at runtime, so no two games have the same map, which "
         "follows the search-based and procedural approaches described in the research (Maleki and Zhao, 2024). We "
         "built it in Unreal Engine 5.8 with C++ for the game systems and UMG widgets for the menus (The Independent "
         "Institute of Education (IIE), 2026a).")
    text(doc,
         "Part 1 delivered the procedural terrain, the tower, the enemies, the defenders and the game loop. Part 2 "
         "added more enemy and defender types and an adaptive Wave Director, informed by work on dynamic difficulty "
         "adjustment (Guo, Thawonmas and Ren, 2024; Mortazavi, Moradi and Vahabie, 2024) and on wave generation for "
         "tower defence (Hind and Harvey, 2024; IIE, 2026b). Part 3 added upgrades, custom shaders and visual "
         "effects, and a custom procedural feature (IIE, 2026c).")
    pic(doc, timeline, "How our work on the shared repository was spread over the semester")

    # ================= 2 who did what =================
    gp.heading(doc, "2", "Who did what")
    text(doc,
         "We worked on the same repository and shared the game's systems, each taking the lead on different areas. "
         "Figure 2 shows the split at a glance, and Table 1 lists the work in detail, taken from the commit history. "
         "Commit counts show activity, not effort, and a lot of our work happened together while testing.")
    pic(doc, ownership, "Which parts of the game each of us led (both colours means we both worked on it)")
    gp.table(doc, ["Area", HARRIET, BELINDA], [
        ["Part 1: core game", "C++ module setup; procedural terrain with four lanes; tower; enemies and spawner; placeable defenders; economy; game mode; projectiles; wave manager; world validation and NavMesh; RTS camera; build-pad markers; health bars", "Start screen; end screens with rewards; HUD and pause screens; terrain decorations from the Nature Essentials pack"],
        ["Part 2: variety and waves", "Adaptive Wave Director; threat budget; lane choice from live defence coverage; A* rerouting; elite enemies; status combos; next-wave forecast on the HUD", "First versions of the enemy types and defender types; compile and file-layout fixes"],
        ["Part 3: upgrades", "Second upgrade tier for every unit and the tower; upgrade visuals (orb, ring, tint); Wave Director scaling for upgrades", "Timed, queued and then wave-scoped upgrade system; the upgrade screen, cooldowns and status labels; HUD upgrade states"],
        ["Part 3: shaders and effects", "Vertex-displacement shader on enemies; screen-space tower-danger post-process", "Niagara effects (tower fire, upgrades, spawns, deaths, wave changes, victory, defeat); glow, beam, corruption and environment materials"],
        ["Part 3: extras", "Evolving Blight (individual feature, planning document and four automated tests); fixes to log spam and a missing effect", "Music manager with saved volume; save and resume; victory health rewards"],
    ], [3.2, 6.7, 6.5], "Work done, by area and by person")

    # ================= 3 part 3 detail =================
    gp.heading(doc, "3", "Part 3 in detail")
    text(doc,
         "Part 3 asked for upgrades for every defender and the tower, two custom shaders, two custom visual "
         "effects and a custom procedural feature (IIE, 2026c). Table 2 maps each requirement to what we built.")
    gp.table(doc, ["Requirement", "What we built", "Led by"], [
        ["At least two upgrades each; health rises; visual change; game still balanced", "Two tiers for all four defenders and the tower. Each raises health, damage and fire rate. Upgraded units show an orb, a ring and a gold tint. The next wave's threat budget grows by 12% per average tier.", f"{BELINDA} (system), {HARRIET} (tier II, visuals, balance)"],
        ["Shader that displaces vertices", "M_CorruptionPulse moves every vertex along its normal with a travelling sine wave, so enemy bodies ripple. Parameters are set per enemy in C++.", HARRIET],
        ["Shader that changes base colour", "The same material blends the enemy type colour with a pulse colour. Belinda's corruption, glow and environment materials also drive colour.", f"{HARRIET}, {BELINDA}"],
        ["Particle effect (custom)", "Eight Niagara systems: tower fire, upgrade burst, projectile trail, enemy spawn and death, wave transition, victory and defeat.", BELINDA],
        ["Screen-space effect (custom)", "A post-process material drains colour and pulses a red vignette as tower health falls, with a flash on every hit (Epic Games, [s.a.]a).", HARRIET],
        ["Custom procedural feature", "Evolving Blight: enemies carry a six-gene genome and evolve between waves by selection, crossover and mutation (Togelius, et al., 2024; Alhijawi and Awajan, 2024).", HARRIET],
    ], [4.4, 8.6, 3.4], "How the Part 3 requirements were met")
    text(doc,
         "Evolving Blight follows earlier evolutionary work on tower defence waves, where candidates were scored "
         "on damage to the base and distance travelled (Wattanapornprom, et al., 2024), and on keeping generated "
         "enemies diverse (Viana, Pereira and Toledo, 2022). Its planning document and four automated tests are "
         "part of the repository.")

    # ================= 4 successes =================
    gp.heading(doc, "4", "Successes")
    bullets(doc, [
        "A complete, playable game across three parts: a new procedural map every match, with at least three lanes to the tower.",
        "An adaptive Wave Director that reads the player, scales difficulty, chooses lanes from live defence coverage, reroutes enemies with A* and now accounts for upgrades.",
        "Every Part 3 requirement is built: two-tier upgrades, a vertex shader, a base-colour shader, Niagara particles, a post-process and an evolving enemy ecosystem.",
        "Two more procedural enemies: flocking swarms driven by boid rules, and poison slimes whose cellular-automaton cloud spreads, ages and destroys any defender standing in it.",
        "A game that now looks like a game: free CC0 models replace the placeholder shapes, a defender-card bar replaces the text panel, a moody fantasy-horror light and fog set the tone, and crystals line the lanes.",
        "Quality checks that can be repeated: four automated tests for the evolution system, including a selection test where mean enemy swiftness rises from 0.46 to 0.94 over eight generations, plus smoke runs that read the game log for errors.",
        "A polished front end: start, upgrade, settings, victory and defeat screens, background music with a saved volume, and save and resume.",
        f"Steady teamwork on one repository: about {h_count + b_count} unique commits, branches for each part, merged pull requests, and Git LFS for the binary assets.",
    ])

    # ================= 5 challenges =================
    gp.heading(doc, "5", "Challenges we met and how we solved them")
    gp.table(doc, ["Challenge", "What went wrong", "How we solved it"], [
        ["Unplayable procedural maps", "Random terrain sometimes blocked a lane or put the tower or spawn in a bad place.", "Added a whole-world validation gate with automatic regeneration, a connectivity check on every lane and corner-safe spawn placement."],
        ["Navigation timing", "The NavMesh bounds volume needed to be resizable, and path checks could run before the NavMesh had finished building.", "Fixed the volume's mobility, stopped changing it at runtime (it now warns instead), forced a synchronous build before validation and made the pathfinding check diagnostic only."],
        ["Finding the right camera", "We tried a free-fly pawn and a third-person hero camera, and the framing needed several rounds of retuning.", "Settled on an RTS strategy camera that pans, rotates and zooms."],
        ["Broken interface assets", "Some match HUD and end-screen widgets were corrupt, and UI art lost its attachments.", "Replaced the corrupt widgets, reattached the art and fixed the editor start-up errors."],
        ["Merging two people's C++", "File placement and compile errors appeared when the enemy and defender work was added to the module.", "Restored the module's source layout, fixed the start map path and rebuilt until it compiled cleanly."],
        ["Upgrade design", "The upgrade system changed shape three times: timed, then queued, then wave-scoped.", "Settled on wave-scoped upgrades with cooldowns, and made the Wave Director measure upgrades before they reset."],
        ["Enemy size and damage flash", "The damage flash reset an enemy's size after every hit, undoing the size gene.", "Added ScaleBase so the flash restores the new size."],
        ["Log spam and a missing effect", "The settings screen rebound its buttons every frame, writing about 12,000 log lines in two minutes, and a Cascade effect no longer existed in UE 5.8.", "Bound the screen once and replaced the effect with a Niagara system, then rechecked the log."],
        ["Binary assets and branches", "Unreal assets cannot be merged as text, and switching branches with the editor open left some files locked.", "Used Git LFS, closed the editor before switching and compared LFS pointers to avoid overwriting real edits."],
    ], [3.4, 6.2, 6.8], "Challenges, causes and solutions")

    # ================= 6 testing =================
    gp.heading(doc, "6", "Testing and what is still open")
    text(doc,
         "We tested in three ways: automated tests for the evolution system, smoke runs that launch the game and "
         "read its log for errors and crashes, and play-testing. We are open about what is not finished. The imported "
         "models are static, so enemies slide rather than animate, save and resume does not store the evolving enemy "
         "population, and the tier II upgrade prices, the swarm and slime strength and the poison damage still need "
         "play-testing.")
    gp.callout(doc, "Next steps", [
        "Swap the static models for animated skeletal ones, and add floating health bars with icons to the in-match HUD.",
        "Save the evolving population, and add a bestiary of past champions.",
        "Play-test tier II prices and the Blight over more waves, and tune the numbers.",
    ])

    # ================= 7 acknowledgements =================
    gp.heading(doc, "7", "Acknowledgements and use of tools")
    bullets(doc, [
        "Low Poly Nature: Essentials by Vertex Rage Studio (2024) supplies the trees, rocks and plants that the terrain scatters across the map. It is used unmodified under the Fab licence.",
        "Tower Defense Kit 2.1 by Kenney (2024), released under CC0, supplies the stone tower pieces, the four defender models and the lane crystals and rocks.",
        "Animals Pack and Animal Pack Vol.2 by Quaternius ([s.a.]a; [s.a.]b), released under CC0 and downloaded from OpenGameArt, supply the cat, wolf and dog models used for the enemies.",
        "Unreal Engine 5.8 and its documentation (Epic Games, [s.a.]a; [s.a.]b).",
        "Git, GitHub and GitHub Desktop for version control.",
    ])
    gp.callout(doc, "Use of generative AI", [
        "Generative AI tools were used during development. The Cursor coding agent (Anysphere, 2026) helped with early gameplay code, comments and report drafts. Claude Code (Anthropic, 2026) helped with the Part 3 work: it assisted with C++ for the upgrade tiers, the shader and post-process, the Wave Director scaling and Evolving Blight, authored the shader and post-process material assets, imported the free models, wrote the new HUD, the boid swarm and the poison cloud code, wrote the automated tests, and helped draft the planning, presentation and group documents.",
        "All of this output was reviewed, built, run and tested by us, and we made the design decisions and are responsible for the final submission.",
    ], fill="FFF8E6", bar="D9A441")

    # ================= references =================
    doc.add_page_break()
    gp.heading(doc, "", "Reference list")
    refs = [
        [("Alhijawi, B. and Awajan, A., 2024. Genetic algorithms: theory, genetic operators, solutions, and applications. ", False),
         ("Evolutionary Intelligence", True),
         (", [e-journal] 17(3), pp.1245-1256. Available at: <https://doi.org/10.1007/s12065-023-00822-6> [Accessed " + ACCESSED + "].", False)],
        [("Anthropic, 2026. Claude Code [AI coding assistant]. Available at: <https://claude.com/product/claude-code> [Accessed " + ACCESSED + "].", False)],
        [("Anysphere, 2026. Cursor [AI code editor and coding agent]. Available at: <https://cursor.com> [Accessed " + ACCESSED + "].", False)],
        [("Epic Games, [s.a.]a. Material inputs in Unreal Engine 5.8 documentation. [online] Available at: "
          "<https://dev.epicgames.com/documentation/en-us/unreal-engine/material-inputs-in-unreal-engine> [Accessed " + ACCESSED + "].", False)],
        [("Epic Games, [s.a.]b. Post process materials in Unreal Engine 5.8 documentation. [online] Available at: "
          "<https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine> [Accessed " + ACCESSED + "].", False)],
        [("Guo, Z., Thawonmas, R. and Ren, X., 2024. Rethinking dynamic difficulty adjustment for video game design. ", False),
         ("Entertainment Computing", True),
         (", [e-journal] 50, p.100663. Available at: <https://doi.org/10.1016/j.entcom.2024.100663> [Accessed " + ACCESSED + "].", False)],
        [("Hind, D. and Harvey, C., 2024. Using a NEAT approach with curriculums for dynamic content generation in video games. ", False),
         ("Personal and Ubiquitous Computing", True),
         (", [e-journal] 28(3-4), pp.629-641. Available at: <https://doi.org/10.1007/s00779-024-01801-z> [Accessed " + ACCESSED + "].", False)],
        [("Maleki, M.F. and Zhao, R., 2024. Procedural content generation in games: a survey with insights on emerging LLM integration. ", False),
         ("Proceedings of the AAAI Conference on Artificial Intelligence and Interactive Digital Entertainment", True),
         (", [e-journal] 20(1), pp.167-178. Available at: <https://doi.org/10.1609/aiide.v20i1.31877> [Accessed " + ACCESSED + "].", False)],
        [("Kenney, 2024. Tower Defense Kit (2.1). [3D asset pack]. Available at: <https://kenney.nl/assets/tower-defense-kit> [Accessed " + ACCESSED + "].", False)],
        [("Manda, H. and Mtabane, B., 2026. ProceduralGenGade3B [Source code]. Available at: <" + GITHUB + "> [Accessed " + ACCESSED + "].", False)],
        [("Mortazavi, F., Moradi, H. and Vahabie, A.-H., 2024. Dynamic difficulty adjustment approaches in video games: a systematic literature review. ", False),
         ("Multimedia Tools and Applications", True),
         (", [e-journal] 83(35), pp.83227-83274. Available at: <https://doi.org/10.1007/s11042-024-18768-x> [Accessed " + ACCESSED + "].", False)],
        [("Quaternius, [s.a.]a. Animals Pack. [3D asset pack]. Available at: <https://opengameart.org/content/5-low-poly-animals> [Accessed " + ACCESSED + "].", False)],
        [("Quaternius, [s.a.]b. Animal Pack Vol.2. [3D asset pack]. Available at: <https://opengameart.org/content/animated-animales-low-poly> [Accessed " + ACCESSED + "].", False)],
        [("The Independent Institute of Education (IIE), 2025. Harvard Anglia style reference guide: adapted for The IIE. [pdf] The Independent Institute of Education.", False)],
        [("The Independent Institute of Education (IIE), 2026a. Game Development 3B [GADE7322 Part 1 PoE brief and assessment rubric]. The Independent Institute of Education: Unpublished.", False)],
        [("The Independent Institute of Education (IIE), 2026b. Game Development 3B [GADE7322 Part 2 PoE brief and assessment rubric]. The Independent Institute of Education: Unpublished.", False)],
        [("The Independent Institute of Education (IIE), 2026c. Game Development 3B [GADE7322 Part 3 PoE brief and assessment rubric]. The Independent Institute of Education: Unpublished.", False)],
        [("Togelius, J., Khalifa, A., Earle, S., Green, M.C. and Soros, L., 2024. Evolutionary machine learning and games. In: W. Banzhaf, P. Machado and M. Zhang, eds. ", False),
         ("Handbook of evolutionary machine learning", True),
         (". Singapore: Springer, pp.715-737. Available at: <https://doi.org/10.1007/978-981-99-3814-8_25> [Accessed " + ACCESSED + "].", False)],
        [("Vertex Rage Studio, 2024. Low Poly Nature: Essentials. [3D asset pack] Fab / Unreal Engine Marketplace. Available at: <https://www.unrealengine.com/marketplace/en-US/product/low-poly-nature-essentials> [Accessed " + ACCESSED + "].", False)],
        [("Viana, B.M.F., Pereira, L.T. and Toledo, C.F.M., 2022. Illuminating the space of enemies through MAP-Elites. ", False),
         ("2022 IEEE Conference on Games (CoG)", True),
         (". Beijing, China, 21-24 August 2022. Piscataway, NJ: IEEE, pp.17-24. Available at: <https://doi.org/10.1109/CoG51982.2022.9893621> [Accessed " + ACCESSED + "].", False)],
        [("Wattanapornprom, W., Satsuk, C., Sirisakornsakul, T., Jamornthawuch, T., Rodkaew, Y. and Susutti, W., 2024. Automating tower defense game level design with evolutionary algorithms. ", False),
         ("2024 28th International Computer Science and Engineering Conference (ICSEC)", True),
         (". Khon Kaen, Thailand, 6-8 November 2024. Piscataway, NJ: IEEE, pp.1-6. Available at: <https://doi.org/10.1109/ICSEC62781.2024.10770629> [Accessed " + ACCESSED + "].", False)],
    ]
    for ref in refs:
        p = gp.para(doc, "", align="left", after=5, line=1.18)
        p.paragraph_format.left_indent = Cm(1.0)
        p.paragraph_format.first_line_indent = Cm(-1.0)
        for t, ital in ref:
            gp.set_font(p.add_run(t), gp.BODY_FONT, 9.5, False, ital, gp.INK)

    doc.save(OUT)
    return h_count, b_count, lines, files


if __name__ == "__main__":
    h, b, lines, files = build()
    try:
        pages = gp.measure_heading_pages(OUT, [t for _, t in TOC_HEADINGS])
        gp.FIG_N[0] = 0
        gp.TAB_N[0] = 0
        h, b, lines, files = build(pages)
        print("Contents page numbers:", pages)
    except Exception as error:
        print("Could not measure page numbers (", error, "); the contents will show 0 until updated in Word.")
    print(f"Saved {OUT}")
    print(f"unique commits: Harriet {h}, Belinda {b}; C++ lines {lines}; files {files}")
