# -*- coding: utf-8 -*-
"""Generate the GADE7322 Part 3 custom procedural generation feature planning document (editable .docx).

Reuses the page formatting helpers from generate_part2_planning.py so both planning documents look the same.
"""

import re
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
from matplotlib.colors import hsv_to_rgb

from docx import Document
from docx.shared import Pt, Cm

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_part2_planning as base  # noqa: E402  (shared helpers: add_para, add_body, add_figure, ...)

ROOT = Path(r"C:\Users\TUF\Documents\Unreal\ProceduralGenGade3B")
FIGS = ROOT / "Docs" / "Figures"
OUT = ROOT / "Docs" / "GADE7322_Part3_Procedural_Feature_Planning.docx"
ACCESSED = "6 November 2026"
BRANCH = "part3"

FIG1 = FIGS / "Part3_Figure1_EvolutionLoop.png"
FIG2 = FIGS / "Part3_Figure2_BiomassBudget.png"
FIG3 = FIGS / "Part3_Figure3_MutantGallery.png"

BLUE, LIGHT = base.BLUE, base.LIGHT


# ---------------------------------------------------------------------------------------------
# Diagrams
# ---------------------------------------------------------------------------------------------

def draw_evolution_loop():
    fig, ax = plt.subplots(figsize=(9, 4.8), dpi=200)
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 5.4)
    ax.axis("off")

    boxes = [
        (0.2, 3.0, "1. Spawn", "each enemy receives a\ngenome from the\ncurrent generation"),
        (3.55, 3.0, "2. Fight", "genes set stats, size,\nshader and colour"),
        (6.9, 3.0, "3. Measure fitness", "path progress before\ndeath, bonus for\nreaching the tower"),
        (6.9, 0.3, "4. Select", "top 40% of the wave\nbecome parents"),
        (3.55, 0.3, "5. Breed", "champion kept, others =\ncrossover + mutation"),
        (0.2, 0.3, "6. Next generation", "stronger lineages,\nnew names, new looks"),
    ]
    for x, y, title, body in boxes:
        ax.add_patch(FancyBboxPatch((x, y), 2.9, 1.85, boxstyle="round,pad=0.05,rounding_size=0.15",
                                    fc=LIGHT, ec=BLUE, lw=2))
        ax.text(x + 1.45, y + 1.42, title, ha="center", va="center", fontsize=11, fontweight="bold", color="#1a365d")
        ax.text(x + 1.45, y + 0.68, body, ha="center", va="center", fontsize=8.5, color="#2d3748")

    def arrow(x1, y1, x2, y2):
        ax.annotate("", xy=(x2, y2), xytext=(x1, y1),
                    arrowprops=dict(arrowstyle="-|>", color=BLUE, lw=2, mutation_scale=18))

    arrow(3.15, 3.9, 3.5, 3.9)
    arrow(6.5, 3.9, 6.85, 3.9)
    arrow(8.35, 2.95, 8.35, 2.2)
    arrow(6.85, 1.2, 6.5, 1.2)
    arrow(3.5, 1.2, 3.15, 1.2)
    ax.annotate("", xy=(1.65, 2.95), xytext=(1.65, 2.2),
                arrowprops=dict(arrowstyle="<|-", color=BLUE, lw=2, mutation_scale=18))
    ax.text(5.0, 5.2, "Evolving Blight: one generation per wave (EnemyEvolution.cpp)", ha="center",
            fontsize=12, fontweight="bold", color="#1a365d")
    fig.tight_layout()
    fig.savefig(FIG1, bbox_inches="tight")
    plt.close(fig)


def draw_biomass_budget():
    archetypes = ["Tank", "Balanced", "Glass cannon", "Speedster"]
    hardiness = [0.85, 0.50, 0.25, 0.20]
    swiftness = [0.15, 0.50, 0.30, 0.95]
    fury = [0.50, 0.50, 0.95, 0.35]

    fig, ax = plt.subplots(figsize=(7.6, 4.2), dpi=200)
    x = np.arange(len(archetypes))
    ax.bar(x, hardiness, color="#2f855a", label="Hardiness (health)")
    ax.bar(x, swiftness, bottom=hardiness, color="#2b6cb0", label="Swiftness (speed)")
    ax.bar(x, fury, bottom=np.add(hardiness, swiftness), color="#c53030", label="Fury (damage)")
    ax.axhline(1.5, color="black", ls="--", lw=1.5)
    ax.text(3.45, 1.53, "biomass budget = 1.5", ha="right", fontsize=9)
    ax.set_xticks(x)
    ax.set_xticklabels(archetypes)
    ax.set_ylim(0, 1.8)
    ax.set_ylabel("Gene value (sum of the three)")
    ax.set_title("Every genome spends the same biomass, so strength in one gene costs the others",
                 fontsize=10.5, fontweight="bold")
    ax.legend(loc="upper left", fontsize=8.5, ncol=3)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(FIG2, bbox_inches="tight")
    plt.close(fig)


def draw_mutant_gallery():
    # (name, girth, pulse/fury spikiness, hue 0..1, swiftness wobble speed label)
    mutants = [
        ("Grubmaw the Bloated", 0.95, 0.10, 0.28, "slow, tough"),
        ("Skitrot the Swift", 0.35, 0.45, 0.55, "fast, fragile"),
        ("Rendfang the Furious", 0.60, 0.95, 0.02, "spiky, deadly"),
        ("Mossback the Steady", 0.70, 0.25, 0.40, "balanced"),
    ]
    fig, axes = plt.subplots(1, 4, figsize=(9.6, 3.2), dpi=200, subplot_kw={"projection": "polar"})
    theta = np.linspace(0, 2 * np.pi, 400)
    for ax, (name, girth, spikes, hue, note) in zip(axes, mutants):
        radius = (0.5 + 0.5 * girth) * (1 + 0.28 * spikes * np.sin(11 * theta))
        colour = hsv_to_rgb((hue, 0.65, 0.78))
        ax.fill(theta, radius, color=colour, ec="black", lw=1.4)
        ax.set_ylim(0, 1.5)
        ax.axis("off")
        ax.set_title(name, fontsize=8.5, fontweight="bold", pad=2)
        ax.text(0.5, 0.02, note, ha="center", va="center", fontsize=8, color="#2d3748", transform=ax.transAxes)
    fig.suptitle("Genes decide size, spikiness and colour, so each mutant looks different", fontsize=10.5,
                 fontweight="bold")
    fig.tight_layout()
    fig.savefig(FIG3, bbox_inches="tight")
    plt.close(fig)


# ---------------------------------------------------------------------------------------------
# Document
# ---------------------------------------------------------------------------------------------

def main():
    FIGS.mkdir(parents=True, exist_ok=True)
    draw_evolution_loop()
    draw_biomass_budget()
    draw_mutant_gallery()

    doc = Document()
    section = doc.sections[0]
    for side in ("top_margin", "bottom_margin", "left_margin", "right_margin"):
        setattr(section, side, Cm(2.54))
    style = doc.styles["Normal"]
    style.font.name = "Times New Roman"
    style.font.size = Pt(12)

    # ---- Cover ----
    for _ in range(3):
        base.add_para(doc, "", first_line=False, space_after=0)
    base.style_headings(doc)
    base.add_para(doc, "THE INDEPENDENT INSTITUTE OF EDUCATION", bold=True, size=14, align="center")
    base.add_para(doc, "EMERIS VEGA SCHOOL", bold=True, align="center", space_after=18)
    base.add_para(doc, "GADE7322: Game Development 3B", bold=True, size=14, align="center")
    base.add_para(doc, "Part 3 Portfolio of Evidence (PoE)", bold=True, size=13, align="center", space_after=24)
    base.add_para(doc, "Planning: Custom Procedural Generation Feature", bold=True, size=16, align="center", space_after=4)
    base.add_para(doc, "Evolving Blight: enemies that evolve from wave to wave", bold=True, size=13,
                  align="center", space_after=28)
    base.add_para(doc, f"Student Name: {base.AUTHOR_FULL}", align="center")
    base.add_para(doc, "Student Number: st10252836", align="center")
    base.add_para(doc, "Module: GADE7322", align="center")
    base.add_para(doc, "Year: 2026", align="center", space_after=18)
    base.add_para(doc, "GitHub repository:", align="center", space_after=2)
    base.add_para(doc, base.GITHUB, italic=True, size=11, align="center", space_after=2)
    base.add_para(doc, f"Branch: {BRANCH}", size=11, align="center", space_after=18)
    word_count_para = base.add_para(doc, "Word count (body text): [calculated]", size=11, align="center")
    doc.add_page_break()

    base.add_toc(doc)
    doc.add_page_break()

    # ---- Body (400-500 words) ----
    base.add_heading_custom(doc, "Overview")
    base.add_body(doc,
        "The enemies in the game come from three fixed types, so waves can start to feel familiar. Evolving "
        "Blight replaces that sameness with a living population. Every enemy carries a genome of six genes "
        "that decides its stats, size, colour and how its body ripples. After each wave, the enemies that got "
        "closest to the tower breed the next generation, so the Blight adapts to how the player defends "
        "(Togelius et al., 2011) (Figure 1). This gives the player a reason to experiment, because a defence "
        "that works against one generation may fail against the next.")
    base.add_figure(doc, FIG1, "Figure 1: The evolution loop that runs once per wave (Manda, 2026a)")

    base.add_heading_custom(doc, "The genome and its biomass budget")
    base.add_body(doc,
        "A genome holds six genes between 0 and 1: Hardiness (health), Swiftness (speed), Fury (damage), "
        "Girth (size), Pulse (how violently the body ripples) and Hue (colour shift). Hardiness, Swiftness "
        "and Fury share a fixed biomass budget, so becoming stronger in one gene means becoming weaker in "
        "another (Figure 2). This keeps evolution interesting: it produces slow tanks, fast glass cannons and "
        "everything between, rather than simply inflating every enemy. The budget grows by 3% each "
        "generation, so pressure rises gently.")
    base.add_figure(doc, FIG2, "Figure 2: Different genomes spending the same biomass (Manda, 2026b)", width=5.2)

    base.add_heading_custom(doc, "Selection, breeding and mutation")
    base.add_body(doc,
        "Fitness is how far along its path an enemy travelled before dying, with a bonus for reaching the "
        "tower. When a wave is cleared, the top 40% become parents. The champion is copied unchanged "
        "(elitism) and every other child is made by uniform crossover of two parents, followed by mutation, "
        "where each gene has a 25% chance of shifting by a small random amount (Holland, 1975). Generation "
        "zero is random but seeded by the map seed, so a map always begins the same way (Shaker, Togelius "
        "and Nelson, 2016).")

    base.add_heading_custom(doc, "Appearance, names and the player's view")
    base.add_body(doc,
        "The genome is shown rather than hidden. Girth scales the body, while Pulse and Swiftness drive the "
        "amplitude and speed of the vertex displacement shader. Hue rotates the colour within 30 degrees of "
        "the enemy type colour, so Wolves and Bears stay recognisable (Figure 3). Each genome also receives "
        "a generated name, such as Skitrot the Swift, built from syllable tables, and the HUD shows the "
        "current generation and its champion.")
    base.add_figure(doc, FIG3, "Figure 3: Four mutants produced by different genes (Manda, 2026c)", width=6.2)

    base.add_heading_custom(doc, "Integration and balance")
    base.add_body(doc,
        "A new EnemyEvolution object lives beside the Wave Director. The Wave Manager requests a genome for "
        "every spawn and reports each enemy's fitness when it dies or reaches the tower. The director's threat "
        "budget is unchanged, because the biomass budget keeps average power stable. Stronger genomes also "
        "drop proportionally more loot, so danger is rewarded. Because every gene is clamped and evolution "
        "only redistributes a limited budget, a lineage cannot spiral out of control. Because the process "
        "is seeded and driven by how each match is played, no two runs tell the same story of champions, "
        "yet the same map and the same play always reproduce the same Blight, which makes testing and "
        "balancing practical.")

    # ---- Word count ----
    words = sum(len(re.findall(r"\b[\w'%.-]+\b", t)) for t in base.BODY_WORDS)
    word_count_para.runs[0].text = f"Word count (body text, excluding headings, captions and references): {words}"

    # ---- References ----
    doc.add_page_break()
    base.add_heading_custom(doc, "Reference list")
    refs = [
        "Holland, J.H., 1975. Adaptation in natural and artificial systems. Ann Arbor: University of Michigan Press.",
        "Manda, H., 2026a. Evolution loop. [diagram] (Personal Collection). Unpublished.",
        "Manda, H., 2026b. Biomass budget of different genomes. [chart] (Personal Collection). Unpublished.",
        "Manda, H., 2026c. Mutant gallery. [diagram] (Personal Collection). Unpublished.",
        f"Manda, H., 2026d. ProceduralGenGade3B (Version {BRANCH}) [Source code]. Available at: "
        f"<{base.GITHUB}/tree/{BRANCH}> [Accessed {ACCESSED}].",
        "Shaker, N., Togelius, J. and Nelson, M.J., 2016. Procedural content generation in games. Cham: Springer.",
        "Togelius, J., Yannakakis, G.N., Stanley, K.O. and Browne, C., 2011. Search-based procedural content "
        "generation: a taxonomy and survey. IEEE Transactions on Computational Intelligence and AI in Games, "
        "[e-journal] 3(3), pp.172-186. Available at: <https://doi.org/10.1109/TCIAIG.2011.2148116> "
        f"[Accessed {ACCESSED}].",
    ]
    for ref in refs:
        base.add_para(doc, ref, space_after=10)

    doc.save(OUT)
    print(f"Saved {OUT}")
    print(f"Body word count: {words}")


if __name__ == "__main__":
    main()
