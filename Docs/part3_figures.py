# -*- coding: utf-8 -*-
"""Figures for the GADE7322 Part 3 planning document (Evolving Blight).

Every figure is drawn from code so it can be regenerated. Palette matches the game's forest theme.
"""

from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Circle, Rectangle
from matplotlib.colors import hsv_to_rgb
from matplotlib import image as mpimg

ROOT = Path(r"C:\Users\TUF\Documents\Unreal\ProceduralGenGade3B")
FIGS = ROOT / "Docs" / "Figures"

# Forest palette
DEEP = "#1f5f4a"      # deep green
MOSS = "#4a8c5c"
LEAF = "#e6f2e8"
GOLD = "#d9a441"
BARK = "#6b4a2e"
PARCH = "#f8f4e8"
INK = "#24332d"
GREY = "#6b7a72"
RED = "#b5382f"
TEAL = "#2a7f8f"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "text.color": INK,
    "axes.edgecolor": GREY,
    "axes.labelcolor": INK,
    "xtick.color": INK,
    "ytick.color": INK,
})

PATHS = {
    "loop": FIGS / "Part3_Fig1_EvolutionLoop.png",
    "budget": FIGS / "Part3_Fig2_BiomassBudget.png",
    "lineage": FIGS / "Part3_Fig3_Lineage.png",
    "gallery": FIGS / "Part3_Fig4_MutantGallery.png",
    "game": FIGS / "Part3_Fig5_CurrentGame.png",
    "architecture": FIGS / "Part3_Fig6_Architecture.png",
    "swimlane": FIGS / "Part3_Fig7_Swimlane.png",
}


def _box(ax, x, y, w, h, fc=LEAF, ec=DEEP, lw=2.0, rounding=0.12):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle=f"round,pad=0.02,rounding_size={rounding}",
                                fc=fc, ec=ec, lw=lw))


def _save(fig, key):
    fig.savefig(PATHS[key], bbox_inches="tight", facecolor="white")
    plt.close(fig)


# ---------------------------------------------------------------------------------------------
# Figure 1: the evolution loop
# ---------------------------------------------------------------------------------------------

def draw_loop():
    fig, ax = plt.subplots(figsize=(10.4, 5.0), dpi=200)
    ax.set_xlim(-5.9, 5.9)
    ax.set_ylim(-3.3, 3.3)
    ax.axis("off")

    w, h = 3.3, 1.35
    nodes = [  # (title, body, x, y)
        ("1  Spawn", "each enemy is given a\ngenome by EnemyEvolution", -4.0, 1.55),
        ("2  Fight", "genes set stats, size,\nshader motion and colour", 0.0, 1.55),
        ("3  Measure fitness", "path progress before death,\n+0.5 for reaching the tower", 4.0, 1.55),
        ("4  Select parents", "best 40% of the wave\n(at least two)", 4.0, -1.55),
        ("5  Breed", "champion copied; others =\ncrossover + mutation", 0.0, -1.55),
        ("6  New generation", "biomass budget +3%,\nnew names and looks", -4.0, -1.55),
    ]
    for title, body, cx, cy in nodes:
        _box(ax, cx - w / 2, cy - h / 2, w, h, fc=LEAF, ec=DEEP)
        ax.text(cx, cy + 0.3, title, ha="center", va="center", fontsize=11.5, fontweight="bold", color=DEEP)
        ax.text(cx, cy - 0.22, body, ha="center", va="center", fontsize=8.6, color=INK, linespacing=1.35)

    def arrow(p, q):
        ax.add_patch(FancyArrowPatch(p, q, arrowstyle="-|>", mutation_scale=22, lw=3, color=GOLD, zorder=1))

    gap = 0.07
    arrow((-4.0 + w / 2 + gap, 1.55), (0.0 - w / 2 - gap, 1.55))
    arrow((0.0 + w / 2 + gap, 1.55), (4.0 - w / 2 - gap, 1.55))
    arrow((4.0, 1.55 - h / 2 - gap), (4.0, -1.55 + h / 2 + gap))
    arrow((4.0 - w / 2 - gap, -1.55), (0.0 + w / 2 + gap, -1.55))
    arrow((0.0 - w / 2 - gap, -1.55), (-4.0 + w / 2 + gap, -1.55))
    arrow((-4.0, -1.55 + h / 2 + gap), (-4.0, 1.55 - h / 2 - gap))

    ax.add_patch(FancyBboxPatch((-2.55, -0.42), 5.1, 0.84, boxstyle="round,pad=0.02,rounding_size=0.4", fc=PARCH,
                                ec=BARK, lw=2.2, zorder=2))
    ax.text(0, 0, "one generation  =  one wave", ha="center", va="center", fontsize=12.5, fontweight="bold",
            color=BARK, zorder=3)
    ax.text(0, 3.2, "Evolving Blight: the evolution loop that runs once per wave", ha="center", fontsize=13,
            fontweight="bold", color=DEEP)
    _save(fig, "loop")


# ---------------------------------------------------------------------------------------------
# Figure 2: biomass budget and what each gene does
# ---------------------------------------------------------------------------------------------

def draw_budget():
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10.4, 4.5), dpi=200, gridspec_kw={"width_ratios": [1.05, 1]})

    names = ["Tank", "Balanced", "Glass\ncannon", "Speedster"]
    h = np.array([0.85, 0.50, 0.20, 0.25])
    s = np.array([0.15, 0.50, 0.35, 0.95])
    f = np.array([0.50, 0.50, 0.95, 0.30])
    x = np.arange(len(names))
    ax1.bar(x, h, color=MOSS, width=0.62, label="Hardiness (health)")
    ax1.bar(x, s, bottom=h, color=TEAL, width=0.62, label="Swiftness (speed)")
    ax1.bar(x, f, bottom=h + s, color=RED, width=0.62, label="Fury (damage)")
    ax1.axhline(1.5, color=BARK, ls="--", lw=2)
    ax1.text(3.45, 1.55, "biomass budget = 1.5", ha="right", fontsize=9, color=BARK, fontweight="bold")
    ax1.set_xticks(x)
    ax1.set_xticklabels(names, fontsize=9)
    ax1.set_ylim(0, 1.95)
    ax1.set_ylabel("Combined value of the three power genes")
    ax1.set_title("Same budget, different builds", fontsize=11, fontweight="bold", color=DEEP)
    ax1.legend(loc="upper left", fontsize=8, frameon=False, ncol=1)
    ax1.grid(axis="y", alpha=0.25)
    for sp in ("top", "right"):
        ax1.spines[sp].set_visible(False)

    g = np.linspace(0.05, 1.0, 100)
    ax2.plot(g, 0.5 + g, color=MOSS, lw=2.6, label="Health  x (0.5 + Hardiness)")
    ax2.plot(g, 0.6 + 0.8 * g, color=TEAL, lw=2.6, label="Speed  x (0.6 + 0.8 Swiftness)")
    ax2.plot(g, 0.5 + g, color=RED, lw=2.6, ls=(0, (1, 1.6)), label="Damage  x (0.5 + Fury)")
    ax2.plot(g, 0.75 + 0.5 * g, color=BARK, lw=2.6, label="Size  x (0.75 + 0.5 Girth)")
    ax2.axhline(1.0, color=GREY, lw=1, ls=":")
    ax2.axvline(0.5, color=GREY, lw=1, ls=":")
    ax2.scatter([0.5], [1.0], s=90, color=GOLD, zorder=5, ec=BARK)
    ax2.annotate("average gene (0.5)\n= original enemy", (0.5, 1.0), xytext=(0.58, 0.64), fontsize=8.5,
                 arrowprops=dict(arrowstyle="->", color=BARK), color=BARK)
    ax2.set_xlabel("Gene value")
    ax2.set_ylabel("Multiplier applied to the enemy")
    ax2.set_title("What a gene value means", fontsize=11, fontweight="bold", color=DEEP)
    ax2.legend(loc="upper left", fontsize=7.8, frameon=False)
    ax2.grid(alpha=0.25)
    for sp in ("top", "right"):
        ax2.spines[sp].set_visible(False)
    fig.tight_layout()
    _save(fig, "budget")


# ---------------------------------------------------------------------------------------------
# Figure 3: an illustrative family tree over three generations
# ---------------------------------------------------------------------------------------------

def draw_lineage():
    fig, ax = plt.subplots(figsize=(11.2, 6.0), dpi=200)
    ax.set_xlim(-0.2, 11.6)
    ax.set_ylim(0, 6.6)
    ax.axis("off")

    cols = [1.55, 5.6, 9.65]
    titles = ["Generation 0  (random, seeded by the map)", "Generation 1  (bred from gen 0)", "Generation 2  (bred from gen 1)"]
    for cx, t in zip(cols, titles):
        ax.text(cx, 6.35, t, ha="center", fontsize=9.5, fontweight="bold", color=DEEP)

    # node = (column, y, name, fitness, role, hue)
    gen0 = [("Moldspore the Swift", 1.00, "parent", 0.30), ("Zipfang the Furious", 0.82, "parent", 0.55),
            ("Gobmaw the Steady", 0.74, "parent", 0.12), ("Rendback the Bloated", 0.35, "out", 0.80),
            ("Skitrot the Swift", 0.28, "out", 0.42), ("Plumpgnaw the Steady", 0.12, "out", 0.65)]
    gen1 = [("Moldspore the Swift", 1.00, "champion", 0.30), ("Zipspore the Swift", 1.30, "parent", 0.45),
            ("Mobmaw the Furious", 0.90, "parent", 0.20), ("Zipmaw the Steady", 0.60, "out", 0.58),
            ("Mold-gnaw the Swift", 0.45, "out", 0.35)]
    gen2 = [("Zipspore the Swift", 1.30, "champion", 0.45), ("Zipmold the Swift", 1.38, "child", 0.40),
            ("Mobspore the Furious", 1.05, "child", 0.25), ("Zipmaw the Swift", 0.80, "child", 0.55),
            ("Gobspore the Steady", 0.55, "child", 0.15)]

    def place(data, cx, top=5.5, gap=0.92):
        pts = []
        for i, (name, fit, role, hue) in enumerate(data):
            y = top - i * gap
            pts.append((cx, y))
            edge = {"champion": GOLD, "parent": DEEP}.get(role, "#b8c4bc")
            lw = 3.4 if role == "champion" else 2.2
            fc = hsv_to_rgb((hue, 0.45, 0.93)) if role != "out" else "#eceeed"
            ax.add_patch(FancyBboxPatch((cx - 1.55, y - 0.3), 3.1, 0.6, boxstyle="round,pad=0.02,rounding_size=0.1",
                                        fc=fc, ec=edge, lw=lw, zorder=3))
            ax.text(cx - 1.4, y + 0.07, name, fontsize=7.6, fontweight="bold", va="center", color=INK, zorder=4)
            ax.text(cx - 1.4, y - 0.16, f"fitness {fit:.2f}" + ("   champion" if role == "champion" else ""),
                    fontsize=6.8, va="center", color=GREY if role == "out" else INK, zorder=4)
        return pts

    p0 = place(gen0, cols[0])
    p1 = place(gen1, cols[1], gap=1.05)
    p2 = place(gen2, cols[2], gap=1.05)

    def link(a, b, color=MOSS, ls="-", lw=1.8, rad=0.0):
        ax.add_patch(FancyArrowPatch((a[0] + 1.55, a[1]), (b[0] - 1.55, b[1]), arrowstyle="-|>", mutation_scale=11,
                                     lw=lw, color=color, ls=ls, connectionstyle=f"arc3,rad={rad}", zorder=2))

    # champion survives unchanged
    link(p0[0], p1[0], color=GOLD, ls="--", lw=2.6)
    link(p1[1], p2[0], color=GOLD, ls="--", lw=2.6)
    # crossover children (two parents each)
    for parent_pair, child in [((0, 1), 1), ((0, 2), 2), ((1, 2), 3), ((0, 0), 4)]:
        for pi in set(parent_pair):
            link(p0[pi], p1[child], rad=0.05)
    for parent_pair, child in [((1, 2), 1), ((1, 1), 2), ((2, 0), 3), ((1, 0), 4)]:
        for pi in set(parent_pair):
            if pi == 0:
                continue
            link(p1[pi], p2[child], rad=0.05)

    # mutation stars
    for pt in (p1[3], p2[3]):
        ax.text(pt[0] + 1.42, pt[1] + 0.25, "*", fontsize=18, color=GOLD, fontweight="bold", ha="center", va="center", zorder=5)

    ax.text(0.0, 0.25, "Gold dashed = champion copied unchanged     Green outline = parent     Grey = not selected     "
                      "Gold star = a gene mutated\nIllustrative values: the real run is seeded by the map seed and shaped by how the player defends.",
            fontsize=8, color=GREY, va="center")
    _save(fig, "lineage")


# ---------------------------------------------------------------------------------------------
# Figure 4: four example mutants
# ---------------------------------------------------------------------------------------------

def draw_gallery():
    mutants = [
        ("Grubmaw the Bloated", dict(H=0.85, S=0.15, F=0.50, G=0.95, P=0.10, hue=0.30), "slow, tough, huge"),
        ("Skitrot the Swift", dict(H=0.25, S=0.95, F=0.30, G=0.35, P=0.45, hue=0.55), "fast, fragile, small"),
        ("Rendfang the Furious", dict(H=0.20, S=0.35, F=0.95, G=0.60, P=0.95, hue=0.02), "spiky, hits hard"),
        ("Mossback the Steady", dict(H=0.50, S=0.50, F=0.50, G=0.70, P=0.25, hue=0.40), "balanced"),
    ]
    fig = plt.figure(figsize=(10.4, 4.6), dpi=200)
    gs = fig.add_gridspec(2, 4, height_ratios=[3.1, 1.0], hspace=0.2, wspace=0.62)
    theta = np.linspace(0, 2 * np.pi, 500)
    for i, (name, g, note) in enumerate(mutants):
        ax = fig.add_subplot(gs[0, i], projection="polar")
        radius = (0.5 + 0.5 * g["G"]) * (1 + 0.3 * g["P"] * np.sin(11 * theta))
        ax.fill(theta, radius, color=hsv_to_rgb((g["hue"], 0.62, 0.8)), ec=INK, lw=1.6)
        ax.set_ylim(0, 1.6)
        ax.axis("off")
        ax.set_title(name, fontsize=9, fontweight="bold", color=DEEP, pad=2)

        bx = fig.add_subplot(gs[1, i])
        vals = [g["H"], g["S"], g["F"]]
        bx.barh([2, 1, 0], vals, color=[MOSS, TEAL, RED], height=0.7)
        bx.set_xlim(0, 1)
        bx.set_yticks([2, 1, 0])
        bx.set_yticklabels(["Hard.", "Swift.", "Fury"], fontsize=6.8)
        bx.set_xticks([])
        for sp in ("top", "right", "bottom"):
            bx.spines[sp].set_visible(False)
        for yy, v in zip([2, 1, 0], vals):
            bx.text(1.04, yy, f"{v:.2f}", va="center", fontsize=6.8)
        bx.set_xlabel(note, fontsize=8, color=GREY, labelpad=2)
    fig.suptitle("Genes decide size, ripple, colour and strength, so every mutant looks and plays differently",
                 fontsize=11, fontweight="bold", color=DEEP, y=1.0)
    _save(fig, "gallery")


# ---------------------------------------------------------------------------------------------
# Figure 5: the existing game with the new feature marked
# ---------------------------------------------------------------------------------------------

def draw_game(screenshot: Path):
    img = mpimg.imread(str(screenshot))
    crop = img[100:820, 100:1380]  # the game window only
    fig, ax = plt.subplots(figsize=(10.0, 5.65), dpi=200)
    ax.imshow(crop)
    ax.axis("off")
    # A gold line in the gap between the "Map Seed" and "Director" lines marks where the new HUD line is inserted,
    # without covering any of the existing text.
    ax.plot([20, 705], [350, 350], color=GOLD, lw=4, solid_capstyle="round")
    ax.annotate("NEW: \"Blight generation N | champion: <name>\"\nappears here on the HUD after each wave",
                xy=(705, 350), xytext=(790, 300), fontsize=10.5, fontweight="bold", color="white",
                bbox=dict(boxstyle="round,pad=0.4", fc=DEEP, ec=GOLD, lw=2),
                arrowprops=dict(arrowstyle="-|>", color=GOLD, lw=2.6))
    ax.annotate("Enemies: each one now carries\na genome that sets its stats and look", xy=(642, 184), xytext=(790, 120),
                fontsize=10.5, fontweight="bold", color="white",
                bbox=dict(boxstyle="round,pad=0.4", fc=BARK, ec=GOLD, lw=2),
                arrowprops=dict(arrowstyle="-|>", color=GOLD, lw=2.6))
    fig.tight_layout(pad=0.2)
    _save(fig, "game")


# ---------------------------------------------------------------------------------------------
# Figure 6: how it fits into the existing code
# ---------------------------------------------------------------------------------------------

def draw_architecture():
    fig, ax = plt.subplots(figsize=(11.2, 6.4), dpi=200)
    ax.set_xlim(0, 11.4)
    ax.set_ylim(0, 6.6)
    ax.axis("off")

    def node(x, y, w, h, title, lines, new=False):
        _box(ax, x, y, w, h, fc=("#fff6dc" if new else LEAF), ec=(GOLD if new else DEEP), lw=(3.2 if new else 2.0))
        ax.text(x + w / 2, y + h - 0.3, title, ha="center", va="center", fontsize=9.8, fontweight="bold", color=DEEP)
        for i, line in enumerate(lines):
            ax.text(x + w / 2, y + h - 0.68 - i * 0.28, line, ha="center", va="center", fontsize=7.7, color=INK)
        if new:
            ax.text(x + w - 0.12, y + h - 0.1, "NEW", ha="right", va="top", fontsize=7, fontweight="bold", color=BARK)

    L, M, R = 0.2, 4.1, 7.9
    node(L, 4.95, 3.3, 1.25, "AWaveManager", ["spawns and tracks every enemy", "asks for a genome at each spawn"])
    node(R, 4.95, 3.3, 1.25, "UWaveDirector", ["threat budget, lanes and elites", "unchanged by the new feature"])
    node(L, 2.75, 3.3, 1.5, "AEnemy", ["ApplyGenome(): health, speed,", "damage, loot, size, colour,", "shader values"], new=True)
    node(M, 2.75, 3.1, 1.5, "UEnemyEvolution", ["NextGenome()  RecordOutcome()", "EndWave()  EnsureSeeded()", "selection, crossover, mutation"], new=True)
    node(R, 2.75, 3.3, 1.5, "TDHUD", ["Blight generation N", "champion name and fitness"], new=True)
    node(L, 0.35, 3.3, 1.3, "M_CorruptionPulse", ["vertex displacement shader", "PulseSpeed, PulseAmplitude"])
    node(M, 0.35, 3.1, 1.3, "FEnemyGenome", ["6 genes, generation, name", "ClampToBudget()  BuildName()"], new=True)
    node(R, 0.35, 3.3, 1.3, "EnemyEvolutionTests", ["4 automated tests: budget,", "seed, selection, champion"], new=True)

    def arrow(p, q):
        ax.add_patch(FancyArrowPatch(p, q, arrowstyle="-|>", mutation_scale=16, lw=2.3, color=MOSS, zorder=1))

    def label(x, y, text, ha="center"):
        ax.text(x, y, text, fontsize=7.8, color=BARK, ha=ha, va="center", fontweight="bold",
                bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="none"))

    arrow((3.5, 5.6), (7.9, 5.6));                label(5.7, 5.6, "existing link")
    arrow((1.2, 4.95), (1.2, 4.25));              label(1.05, 4.6, "ApplyGenome()", ha="right")
    arrow((2.7, 4.95), (4.7, 4.25));              label(2.55, 4.45, "NextGenome()", ha="right")
    arrow((6.5, 4.25), (3.5, 5.15));              label(5.55, 4.82, "RecordOutcome() / EndWave()")
    arrow((5.65, 2.75), (5.65, 1.65));            label(5.75, 2.2, "creates and breeds", ha="left")
    arrow((1.85, 2.75), (1.85, 1.65));            label(1.95, 2.2, "sets shader values", ha="left")
    arrow((7.2, 3.5), (7.9, 3.5));                arrow((7.9, 1.0), (7.2, 1.0));                ax.text(7.55, 1.3, "tests", fontsize=7.2, color=BARK, ha="center", fontweight="bold")
    ax.text(5.7, 6.45, "Evolving Blight: how the feature plugs into the existing game", ha="center", fontsize=12,
            fontweight="bold", color=DEEP)
    _save(fig, "architecture")


# ---------------------------------------------------------------------------------------------
# Figure 7: swimlane of one wave
# ---------------------------------------------------------------------------------------------

def draw_swimlane():
    fig, ax = plt.subplots(figsize=(10.4, 5.6), dpi=200)
    ax.set_xlim(0, 10.4)
    ax.set_ylim(0, 6.2)
    ax.axis("off")
    lanes = ["AWaveManager", "UEnemyEvolution", "AEnemy", "TDHUD"]
    xs = [1.2, 3.9, 6.6, 9.3]
    for x, name in zip(xs, lanes):
        _box(ax, x - 1.0, 5.4, 2.0, 0.55, fc=DEEP, ec=DEEP)
        ax.text(x, 5.675, name, color="white", ha="center", va="center", fontsize=9, fontweight="bold")
        ax.plot([x, x], [0.35, 5.4], color="#b9c9bf", lw=1.6, ls=(0, (4, 3)), zorder=0)

    steps = [
        (0, 1, 4.8, "1  at every spawn: NextGenome()"),
        (1, 0, 4.25, "returns genome (champion first, then children)"),
        (0, 2, 3.7, "2  ApplyGenome(genome)  -> stats, size, shader"),
        (2, 2, 3.15, "3  enemy walks its lane and fights"),
        (2, 0, 2.6, "4  dies or reaches tower: progress, leaked?"),
        (0, 1, 2.05, "5  RecordOutcome(genome, progress, leaked)"),
        (0, 1, 1.5, "6  wave cleared: EndWave() -> breed next generation"),
        (1, 3, 0.95, "7  HUD shows generation and champion name"),
    ]
    for a, b, y, text in steps:
        if a == b:
            ax.add_patch(FancyArrowPatch((xs[a] + 0.1, y + 0.18), (xs[a] + 0.1, y - 0.18), connectionstyle="arc3,rad=-1.6",
                                         arrowstyle="-|>", mutation_scale=13, lw=2, color=GOLD))
            ax.text(xs[a] + 0.55, y, text, fontsize=8, va="center", color=INK)
            continue
        ax.add_patch(FancyArrowPatch((xs[a], y), (xs[b], y), arrowstyle="-|>", mutation_scale=15, lw=2.2,
                                     color=MOSS if b > a else BARK))
        ax.text((xs[a] + xs[b]) / 2, y + 0.16, text, fontsize=8, ha="center", color=INK,
                bbox=dict(boxstyle="round,pad=0.12", fc="white", ec="none"))
    ax.text(5.2, 6.15, "Sequence of events for one wave", ha="center", fontsize=12, fontweight="bold", color=DEEP)
    _save(fig, "swimlane")


def draw_all(screenshot: Path):
    FIGS.mkdir(parents=True, exist_ok=True)
    draw_loop()
    draw_budget()
    draw_lineage()
    draw_gallery()
    draw_game(screenshot)
    draw_architecture()
    draw_swimlane()
    return PATHS
