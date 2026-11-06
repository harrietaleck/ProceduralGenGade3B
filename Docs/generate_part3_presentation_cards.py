# -*- coding: utf-8 -*-
"""Generate the Part 3 video presentation cards (Word).

Every card has three panels:  SAY (what to speak),  SHOW IN GAME (what to do on screen),  SHOW IN CODE (the exact
file, exact line numbers and the code itself). The code panels are cut straight out of the source files by anchor
text, so the line numbers printed in the document always match the real files at the time it is generated.
"""

import re
import sys
from pathlib import Path

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Pt, Cm, RGBColor

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_part3_planning as gp  # noqa: E402  (shared styling helpers)

ROOT = Path(r"C:\Users\TUF\Documents\Unreal\ProceduralGenGade3B")
SRC = ROOT / "Source" / "ProceduralGenGade3B"
OUT = ROOT / "Docs" / "GADE7322_Part3_Presentation_Cards.docx"

DEEP, GOLD, BARK, INK, GREY, WHITE = gp.DEEP, gp.GOLD, gp.BARK, gp.INK, gp.GREY, gp.WHITE
WPM = 130  # comfortable speaking pace for the time estimate


# ---------------------------------------------------------------------------------------------
# Code snippets, cut from the real source files
# ---------------------------------------------------------------------------------------------

def _read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace").splitlines()


def snippet(file, start, end=None, *, to_closing_brace=False, extra_after=0, max_lines=None):
    """Return (first_line, last_line, [(line_no, text)]). Anchors are matched as substrings of a line."""
    lines = _read(file)
    first = next((i for i, l in enumerate(lines) if start in l), None)
    if first is None:
        raise ValueError(f"anchor not found in {file}: {start!r}")
    if to_closing_brace:
        last = next(i for i in range(first, len(lines)) if lines[i].rstrip() == "}")
    elif end is not None:
        last = next((i for i in range(first, len(lines)) if end in lines[i]), None)
        if last is None:
            raise ValueError(f"end anchor not found in {file}: {end!r}")
    else:
        last = first
    last = min(len(lines) - 1, last + extra_after)
    if max_lines:
        last = min(last, first + max_lines - 1)
    out = [(i + 1, lines[i].expandtabs(4).rstrip()) for i in range(first, last + 1)]
    return first + 1, last + 1, out


# ---------------------------------------------------------------------------------------------
# The cards
# ---------------------------------------------------------------------------------------------

CARDS = [
    dict(
        title="Introduction", secs=20,
        say="Hi, I'm Harriet Manda, student number ST10252836. This is our tower defence game, built in Unreal Engine 5 "
            "with C++. For Part 3 I'll show the upgrades, two shaders, two visual effects, and my own procedural "
            "feature: enemies that evolve from wave to wave.",
        show=["Have the game window ready on the start screen (StartScreenLvl).",
              "Smile and look at the camera while you say the first sentence.",
              "Click Start to load the TowerDefense level while you finish speaking."],
        code=[],
        point=[],
    ),
    dict(
        title="The game in 25 seconds", secs=25,
        say="Every match builds a new terrain from a seed, with four lanes leading to the central tower. A Wave "
            "Director adapts each wave to how the player is doing. Everything I show next sits on top of that "
            "existing game, so I will point out each Part 3 requirement as it happens.",
        show=["The match is running: terrain, four brown lanes, the tower in the middle.",
              "Press R once to restart and show that the terrain is different (new map seed on the HUD).",
              "Point at the HUD: wave number, resources, tower health, director line."],
        code=[],
        point=[],
    ),
    dict(
        title="Upgrades: two tiers each", secs=45,
        say="Every defender type and the tower now has two upgrade tiers. I press B to open the upgrade screen and buy "
            "tier one for the tower. The same button now offers tier two, which is stronger and costs more. Buying "
            "it raises health, damage and fire rate, and the unit changes visually: a gold orb floats above it, and "
            "tier two adds a glowing ring. In code, TryActivate works out the next tier, refuses a duplicate "
            "purchase and charges once.",
        show=["Press B to open the Defenders screen. Point at the Tower button.",
              "Click the Tower upgrade: the button turns green and reads 'TIER I - CLICK FOR II'.",
              "Click it again: it turns gold and reads 'TIER II - MAX'. Close with B.",
              "Zoom the camera on the tower, then on a placed defender: show the orb and the ground ring."],
        code=[
            dict(file="DefenderUpgradeComponent.cpp", start="const int32 NextTier = Slot.Tier + 1;", end="return;", extra_after=1,
                 why="The guard: work out the next tier and refuse a third purchase, so a click on an active upgrade can never charge twice."),
            dict(file="DefenderUpgradeComponent.cpp", start="if (NextTier == 1)", end="StartTierTwo(Upgrade, Targets);", extra_after=1,
                 why="Tier I activates the upgrade; tier II raises the same units to the stronger stats."),
            dict(file="DefenderUpgradeComponent.cpp", start="const FUpgradeConfig Tier2Configs[] = {", end="};",
                 why="Tier II stats live in one table: about 30% stronger for 1.6 to 2 times the price."),
            dict(file="UpgradeVisualComponent.cpp", start="void UUpgradeVisualComponent::SetTier", to_closing_brace=True,
                 why="Shows the tier on the unit: orb at tier I, orb plus ring at tier II, gold body tint. It never scales the mesh."),
        ],
        point=["Tiers reuse the same button, so no UI blueprint had to change.",
               "Fixed a bug on the way: clicking an already-active upgrade used to charge again for no effect."],
    ),
    dict(
        title="Shaders", secs=40,
        say="I made two shaders in one material. First, vertex displacement: M_CorruptionPulse pushes every vertex "
            "along its normal by a sine wave of time and world position, so the enemy bodies ripple. Here is the "
            "graph in the material editor. Second, the same material blends each enemy's base colour with a pulse "
            "colour, so the colour breathes. C++ sets the parameters per enemy, so no two mutants ripple the same way.",
        show=["Switch to the Unreal editor: open Content/Materials/Displacement/M_CorruptionPulse.",
              "Point at the chain: Time x PulseSpeed + WorldPosition phase -> Sine -> x PulseAmplitude -> x VertexNormalWS -> World Position Offset.",
              "Point at the Lerp into Base Color and the Emissive output.",
              "Back to the game: watch enemies walking down a lane, ripple and colour pulse visible."],
        code=[
            dict(file="Enemy.cpp", start="// Prefer the pulsing displacement shader.", end="BodyColor.B * 1.6f", extra_after=2,
                 why="Loads the displacement material and writes the type colour and a brighter pulse colour into it."),
            dict(file="Enemy.cpp", start="// ---- Shader. These are the parameters of M_CorruptionPulse", end="EmissiveStrength",
                 extra_after=1, why="Each genome sets PulseSpeed, PulseAmplitude and EmissiveStrength, so every mutant ripples differently."),
        ],
        point=["World Position Offset is the material output that moves vertices: it is what makes this a vertex-displacement shader.",
               "The material was built with the editor's material graph, scripted from Python so it can be rebuilt."],
    ),
    dict(
        title="Visual effects: particles and a post-process", secs=40,
        say="There are two custom effects. Belinda built the Niagara particle systems: tower shots, upgrade bursts "
            "and enemy spawns and deaths. I built the screen-space effect. It is a post-process material that drains "
            "the colour and pulses a red vignette. A component reads the tower's health every frame. Below forty-five "
            "percent the effect fades in, and every hit adds a flash. Watch the screen as the tower takes damage.",
        show=["Let the tower shoot: point out the muzzle flash and projectile trail (Niagara).",
              "Buy an upgrade: point out the upgrade burst around the tower.",
              "Place no defenders for a moment and let enemies reach the tower.",
              "Show the red vignette and grey colour drain growing as tower health falls (about 45% health and below)."],
        code=[
            dict(file="DangerScreenEffectComponent.cpp", start="const float Percent = Health->GetHealthPercent();", end="return FMath::Clamp(Danger + HitFlashRemaining", extra_after=1,
                 why="Reads tower health, flashes on every drop, and returns the target intensity from 0 to 1."),
            dict(file="DangerScreenEffectComponent.cpp", start="const float Target = ComputeTargetIntensity(DeltaTime);", end='Material->SetScalarParameterValue(TEXT("Intensity")',
                 why="Eases the picture towards the target and writes it into the material's Intensity parameter."),
        ],
        point=["The post-process material (PP_TowerDanger) is applied through an unbound post-process volume, so it works with any camera.",
               "Scene Texture -> Desaturation, plus a screen-centre distance vignette and a sine 'heartbeat'."],
    ),
    dict(
        title="My procedural feature: Evolving Blight", secs=45,
        say="My own procedural feature is Evolving Blight. Instead of fixed enemy types, every enemy carries a genome "
            "of six genes. Hardiness, Swiftness and Fury share a fixed biomass budget, so a fast enemy has to be "
            "weaker elsewhere. Girth, Pulse and Hue only change how it looks. The code clamps every genome to the "
            "budget, and each gene maps to a multiplier that equals one at the average.",
        show=["Hold up the planning document, Figure 1 (the loop) and Figure 2 (biomass budget), for about five seconds each.",
              "Back in game: point at enemies with different sizes, colours and ripple speeds on the same lane.",
              "Point at the HUD line 'Blight generation N | champion: <name>' after the first wave ends."],
        code=[
            dict(file="EnemyGenome.h", start="float HealthMultiplier() const", end="float PowerRating() const",
                 why="What each gene means. Every multiplier is 1.0 at a gene value of 0.5, so an average genome plays like the old enemy."),
            dict(file="EnemyGenome.cpp", start="void FEnemyGenome::ClampToBudget", to_closing_brace=True,
                 why="Keeps genes in range and, if the three power genes exceed the budget, shrinks them together so a tank stays a tank."),
        ],
        point=["Named from the two strongest genes, for example 'Moldspore the Swift'.",
               "Seeded by the map seed, so the same map and play reproduce the same Blight."],
    ),
    dict(
        title="How the evolution works", secs=40,
        say="Evolution lives in one class, UEnemyEvolution. When a wave ends, EndWave sorts the enemies by fitness, "
            "which is how far they travelled plus a bonus for reaching the tower, and keeps the best forty percent as "
            "parents. The champion is copied unchanged. MakeChild does uniform crossover and mutates each gene with a "
            "twenty-five percent chance. I tested it automatically: in the selection test, average swiftness rises "
            "from 0.46 to 0.94 over eight generations.",
        show=["Show the Output Log line: 'EnemyEvolution: generation 0 ended ... Champion ...' (filter the log for EnemyEvolution).",
              "Show the Automation window or terminal with the four ProceduralGen.EnemyEvolution tests passing.",
              "Optional: show the lineage figure (Figure 3) in the planning document."],
        code=[
            dict(file="EnemyEvolution.cpp", start="// Uniform crossover: every gene", end="Child.Name = FEnemyGenome::BuildName(Child, Stream);",
                 why="Uniform crossover from two parents, then mutation, then the biomass clamp and a generated name."),
            dict(file="EnemyEvolution.cpp", start="WaveResults.Sort(", end="bChampionIssued = false;",
                 why="Selection: sort by fitness, keep the top slice as parents, remember the champion."),
            dict(file="EnemyEvolutionTests.cpp", start='AddInfo(FString::Printf(TEXT("Mean Swiftness', end="selection pushes",
                 why="The test that proves selection works: the mean must rise by more than 0.08."),
        ],
        point=["Four tests: budget respected, same seed same Blight, evolution climbs, champion survives unchanged.",
               "Run them with: Automation RunTests ProceduralGen.EnemyEvolution"],
    ),
    dict(
        title="Plugging it into the game", secs=25,
        say="The Wave Manager asks for a genome at every spawn and reports every outcome. ApplyGenome turns the genes "
            "into health, speed, damage, loot, size and shader values. It runs before the elite modifier, so "
            "everything stacks. The seed comes from the map.",
        show=["Stay in the game, a wave in progress.",
              "Point at one fast small enemy and one big slow enemy walking side by side."],
        code=[
            dict(file="WaveManager.cpp", start="FEnemyGenome SpawnedGenome;", end="bGaveGenome = true;", extra_after=1,
                 why="At every spawn: seed from the map, take the next genome, apply it to the enemy."),
            dict(file="Enemy.cpp", start="// ---- Stats. Every multiplier is 1.0", end="ResourceReward = FMath::RoundToInt",
                 why="Genes become health, damage, speed and loot. Danger pays: stronger genomes drop more."),
        ],
        point=[],
    ),
    dict(
        title="Balance: waves react to upgrades", secs=25,
        say="To keep the game balanced with upgrades, the Wave Director measures the average upgrade tier when a "
            "wave ends, before the upgrades reset. It grows the next wave's threat budget by twelve percent per "
            "tier. The HUD tells the player when their upgrades made a wave bigger.",
        show=["Buy the tower upgrade during a wave and survive until the wave ends.",
              "On the next-wave forecast line point at 'threat N, +x% for upgrades'."],
        code=[
            dict(file="WaveDirector.cpp", start="// Upgrades only last for one wave", end="LastWaveUpgradeStrength = MeasureUpgradeStrength(World);",
                 why="Measured at wave end, just before the game mode resets the upgrades."),
            dict(file="WaveDirector.cpp", start="// A defence that used upgrades is stronger", end="Plan.ThreatBudget *= Plan.UpgradeBudgetFactor;",
                 why="The next wave's budget scales with how strongly the player upgraded."),
        ],
        point=["The tower counts as one unit, so a tower-only defence is still measured.",
               "UpgradeBudgetPerTier = 0.12 is an editable property, no recompile needed to tune it."],
    ),
    dict(
        title="Challenges and how I solved them", secs=25,
        say="Three problems taught me the most. The damage flash reset enemy size after every hit, so I added "
            "ScaleBase. A settings screen rebound its buttons every frame, writing twelve thousand log lines in two "
            "minutes, so it now binds once. And the director had to measure upgrades before they reset.",
        show=["Stay on the game or show the code editor.",
              "Optional: show the log file size before/after to make the 12,450 lines concrete."],
        code=[
            dict(file="DamageFlashComponent.h", start="void ScaleBase(float Factor)", end="void ScaleBase(float Factor)",
                 why="The flash restores BaseScale after a hit, so a permanent size change has to update it too."),
            dict(file="TDGameMode_UI.inl", start="// Each settings widget only needs its handlers attached once.", end="BoundSettingsScreens.Add(Screen);",
                 why="Bind once per widget instead of every frame: 12,450 log lines in two minutes became 1."),
        ],
        point=["Found by reading the log of a smoke test, not by guessing.",
               "Also replaced a missing Cascade asset with a Niagara system when the tower is destroyed."],
    ),
    dict(
        title="Conclusion", secs=15,
        say="To summarise: two-tier upgrades, a vertex displacement shader, a post-process effect, and an evolving "
            "enemy ecosystem, all tested and integrated into the game. Thank you for watching.",
        show=["Return to a clear shot of the game with the tower in the middle.",
              "Stop the recording after a one-second pause."],
        code=[],
        point=[],
    ),
]


# ---------------------------------------------------------------------------------------------
# Document building
# ---------------------------------------------------------------------------------------------

def mmss(sec):
    return f"{int(sec // 60)}:{int(sec % 60):02d}"


def panel(doc, label, colour_hex, fill_hex, label_colour):
    """A one-cell table with a coloured left bar and a small label. Returns the cell."""
    tbl = doc.add_table(rows=1, cols=1)
    tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
    cell = tbl.rows[0].cells[0]
    gp.shade(cell, fill_hex)
    gp.cell_margins(cell, 110, 110, 220, 160)
    gp.cell_borders(cell, left=(40, colour_hex), top=None, bottom=None, right=None)
    gp.set_col_widths(tbl, [16.4])
    # A panel never splits across two pages: Word moves the whole panel to the next page instead.
    gp.OxmlElement  # (imported helper, kept for clarity)
    cant_split = gp.OxmlElement("w:cantSplit")
    tbl.rows[0]._tr.get_or_add_trPr().append(cant_split)
    cell.text = ""
    p = cell.paragraphs[0]
    p.paragraph_format.space_after = Pt(3)
    gp.set_font(p.add_run(label), gp.HEAD_FONT, 9, True, color=label_colour)
    return cell


def spacer(doc, pts=5):
    p = gp.para(doc, "", after=0, line=1.0)
    p.paragraph_format.space_after = Pt(pts)
    for r in p.runs:
        r.font.size = Pt(2)


def add_card(doc, index, total, card, clock_start):
    words = len(card["say"].split())
    est = words / WPM * 60
    # ---- header band ----
    band = doc.add_table(rows=1, cols=2)
    band.alignment = WD_TABLE_ALIGNMENT.CENTER
    left, right = band.rows[0].cells
    for c in (left, right):
        gp.shade(c, "1F5F4A")
        gp.cell_margins(c, 150, 150, 220, 220)
        gp.cell_borders(c, top=None, left=None, right=None, bottom=(30, "D9A441"))
        c.text = ""
    p = left.paragraphs[0]
    p.paragraph_format.space_after = Pt(0)
    gp.set_font(p.add_run(f"CARD {index} OF {total}"), gp.HEAD_FONT, 8.5, True, color=GOLD)
    q = left.add_paragraph()
    q.paragraph_format.space_after = Pt(0)
    gp.set_font(q.add_run(card["title"]), gp.HEAD_FONT, 19, True, color=WHITE)
    r = right.paragraphs[0]
    r.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    r.paragraph_format.space_after = Pt(0)
    gp.set_font(r.add_run(f"{mmss(card['secs'])}"), gp.HEAD_FONT, 22, True, color=GOLD)
    s = right.add_paragraph()
    s.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    s.paragraph_format.space_after = Pt(0)
    gp.set_font(s.add_run(f"clock {mmss(clock_start)} to {mmss(clock_start + card['secs'])}"), gp.HEAD_FONT, 8.5, color=WHITE)
    gp.set_col_widths(band, [12.4, 4.0])
    spacer(doc, 8)

    # ---- SAY ----
    cell = panel(doc, f"SAY    (about {words} words, roughly {mmss(est)} at a calm pace)", "1F5F4A", "EEF6F0", DEEP)
    q = cell.add_paragraph()
    q.paragraph_format.space_after = Pt(2)
    q.paragraph_format.line_spacing = 1.35
    gp.set_font(q.add_run(card["say"]), gp.BODY_FONT, 12.5, color=INK)
    spacer(doc)

    # ---- SHOW IN GAME ----
    cell = panel(doc, "SHOW IN GAME", "D9A441", "FFF8E6", BARK)
    for n, step in enumerate(card["show"], 1):
        q = cell.add_paragraph()
        q.paragraph_format.space_after = Pt(2)
        q.paragraph_format.left_indent = Cm(0.6)
        q.paragraph_format.first_line_indent = Cm(-0.6)
        gp.set_font(q.add_run(f"{n}.  "), gp.HEAD_FONT, 10, True, color=BARK)
        gp.set_font(q.add_run(step), gp.HEAD_FONT, 10, color=INK)
    spacer(doc)

    # ---- point out ----
    if card["point"]:
        q = gp.para(doc, "", align="left", after=1)
        gp.set_font(q.add_run("If you have time, also point out:"), gp.HEAD_FONT, 9.5, True, color=DEEP)
        for t in card["point"]:
            q = gp.para(doc, "", align="left", after=1, line=1.15)
            q.paragraph_format.left_indent = Cm(0.6)
            q.paragraph_format.first_line_indent = Cm(-0.4)
            gp.set_font(q.add_run("\u2022  " + t), gp.HEAD_FONT, 9.5, color=INK)
    if card["point"]:
        spacer(doc, 3)

    # ---- SHOW IN CODE ----
    if card["code"]:
        for item in card["code"]:
            first, last, lines = snippet(item["file"], item["start"], item.get("end"),
                                         to_closing_brace=item.get("to_closing_brace", False),
                                         extra_after=item.get("extra_after", 0))
            cell = panel(doc, "SHOW IN CODE", "24332D", "F3F4F2", DEEP)
            q = cell.add_paragraph()
            q.paragraph_format.space_after = Pt(1)
            gp.set_font(q.add_run(f"Source/ProceduralGenGade3B/{item['file']}"), gp.HEAD_FONT, 10, True, color=INK)
            gp.set_font(q.add_run(f"     lines {first} to {last}     (Ctrl+G, type {first})"), gp.HEAD_FONT, 9.5, color=GREY)
            q = cell.add_paragraph()
            q.paragraph_format.space_after = Pt(3)
            gp.set_font(q.add_run("Say: " + item["why"]), gp.HEAD_FONT, 9.5, False, True, DEEP)
            shown = lines if len(lines) <= 34 else lines[:34] + [(None, "... (continues in the file)")]
            for no, text in shown:
                q = cell.add_paragraph()
                q.paragraph_format.space_after = Pt(0)
                q.paragraph_format.line_spacing = 1.0
                prefix = f"{no:>4}  " if no else "      "
                gp.set_font(q.add_run(prefix + text), "Consolas", 7.6, color=INK)
            spacer(doc, 4)
    else:
        cell = panel(doc, "SHOW IN CODE", "24332D", "F3F4F2", DEEP)
        q = cell.add_paragraph()
        gp.set_font(q.add_run("No code on this card. Keep the game on screen."), gp.HEAD_FONT, 10, False, True, GREY)
        spacer(doc)



def build():
    doc = Document()
    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21.0), Cm(29.7)
    sec.left_margin = sec.right_margin = Cm(2.3)
    sec.top_margin, sec.bottom_margin = Cm(1.8), Cm(1.6)
    sec.different_first_page_header_footer = True
    normal = doc.styles["Normal"]
    normal.font.name = gp.BODY_FONT
    normal.font.size = Pt(11)

    hp = sec.header.paragraphs[0]
    gp.set_font(hp.add_run("GADE7322 Part 3  |  Video presentation cards  |  Harriet Manda  st10252836"), gp.HEAD_FONT, 8.5, color=GREY)
    fp = sec.footer.paragraphs[0]
    fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    gp.set_font(fp.add_run("Page "), gp.HEAD_FONT, 8.5, color=GREY)
    gp.add_field(fp, "PAGE", 8.5)

    total_secs = sum(c["secs"] for c in CARDS)

    # ================= cover / how to use =================
    band = doc.add_table(rows=1, cols=1)
    band.alignment = WD_TABLE_ALIGNMENT.CENTER
    cell = band.rows[0].cells[0]
    gp.shade(cell, "1F5F4A")
    gp.cell_margins(cell, 300, 300, 400, 400)
    gp.cell_borders(cell, top=None, left=None, right=None, bottom=(36, "D9A441"))
    cell.text = ""
    p = cell.paragraphs[0]
    p.paragraph_format.space_after = Pt(4)
    gp.set_font(p.add_run("GADE7322 \u00b7 PART 3 \u00b7 VIDEO PRESENTATION"), gp.HEAD_FONT, 9, True, color=GOLD)
    q = cell.add_paragraph()
    q.paragraph_format.space_after = Pt(2)
    gp.set_font(q.add_run("Presentation cards"), gp.HEAD_FONT, 32, True, color=WHITE)
    q = cell.add_paragraph()
    gp.set_font(q.add_run(f"{len(CARDS)} cards \u00b7 about {mmss(total_secs)} \u00b7 say it, show it, then show the code"),
                gp.HEAD_FONT, 12, False, True, WHITE)
    gp.set_col_widths(band, [16.4])
    spacer(doc, 10)

    gp.callout(doc, "How to use these cards", [
        "Each card has three panels. SAY is the script. SHOW IN GAME is what to do on screen while you say it. SHOW IN CODE gives the exact file, the exact line numbers and the code itself, with a one-line note on what to say about it.",
        "Speak from the key points, not word for word. The brief rewards a confident, natural delivery. Practise three times out loud against the clock.",
        "The brief wants five to six minutes, individual, with audio. These cards run " + mmss(total_secs) + ". Record in one take per card if it helps, then join them.",
        "Line numbers are real. In Visual Studio or Rider press Ctrl+G and type the first line number to jump straight to the code.",
    ])

    rows = []
    t = 0
    for i, c in enumerate(CARDS, 1):
        rows.append([str(i), c["title"], mmss(c["secs"]), f"{mmss(t)} to {mmss(t + c['secs'])}"])
        t += c["secs"]
    gp.table(doc, ["Card", "Topic", "Length", "Clock"], rows, [1.4, 9.0, 2.2, 3.8], None)

    gp.callout(doc, "Before you press record (checklist)", [
        "[ ] Latest build compiled and the editor closed, then launch the game on the TowerDefense level (not the editor viewport).",
        "[ ] Have these files open in tabs, in this order: DefenderUpgradeComponent.cpp, UpgradeVisualComponent.cpp, Enemy.cpp, DangerScreenEffectComponent.cpp, EnemyGenome.h/.cpp, EnemyEvolution.cpp, EnemyEvolutionTests.cpp, WaveManager.cpp, WaveDirector.cpp.",
        "[ ] Open M_CorruptionPulse in the material editor and leave it in a separate window.",
        "[ ] Have the planning document open at Figures 1 to 3.",
        "[ ] Run the automated tests once beforehand so the result is ready to show: Automation RunTests ProceduralGen.EnemyEvolution",
        "[ ] Make sure you have upgrade currency (wallet) in the menu so tier I and tier II can both be bought.",
        "[ ] Close chat apps and notifications. Test your microphone: the brief marks a presentation with no audio as a fail.",
    ], fill="FFF8E6", bar="D9A441")

    gp.table(doc, ["Key", "Does", "Key", "Does"], [
        ["B", "Open or close the upgrade screen", "1 2 3 4", "Pick Basic, Archer, Bomb, Strong defender"],
        ["R", "Restart with a new map", "P", "Pause"],
        ["G", "Show the wave director graph", "Tab", "Switch strong defender mode"],
        ["U", "Upgrade the tower beam", "F5", "Quick save"],
        ["Left click", "Place the selected defender on a pad", "N", "Toggle the NavMesh debug view"],
    ], [2.0, 6.2, 2.2, 6.0], None)
    doc.add_page_break()

    # ================= the cards =================
    clock = 0
    for i, card in enumerate(CARDS, 1):
        add_card(doc, i, len(CARDS), card, clock)
        clock += card["secs"]
        doc.add_page_break()

    # ================= Q and A =================
    gp.heading(doc, "", "Questions you might be asked")
    gp.table(doc, ["Possible question", "Suggested answer"], [
        ["Why a genetic algorithm and not a neural network?", "Its rules stay readable, testable and tunable, which mattered for balance. Earlier tower defence work used both approaches (Hind and Harvey, 2024; Wattanapornprom, et al., 2024)."],
        ["What stops the enemies becoming unbeatable?", "Three limits: the shared biomass budget, a growth cap of 2.1, and gene limits. Evolution only redistributes a fixed budget."],
        ["How do you know it works?", "Four automated tests, including a selection test where mean swiftness rises from 0.46 to 0.94, plus play-testing and log checks."],
        ["Which parts are yours and which are Belinda's?", "Mine: terrain, the Wave Director, the second upgrade tier, the vertex shader, the post-process, upgrade scaling and Evolving Blight. Belinda: the base upgrade system, Niagara effects, music, save/resume and the menu screens."],
        ["Why is the upgrade wave-scoped?", "It keeps upgrades a per-wave decision and lets the director measure them. Prices and numbers are editable in one table."],
        ["What would you do with more time?", "Save the evolving population, add a bestiary of past champions, and replace the placeholder meshes with animated characters."],
    ], [5.6, 10.8], "Likely questions and answers")

    doc.save(OUT)
    return total_secs


if __name__ == "__main__":
    total = build()
    print(f"Saved {OUT}")
    print("Total time:", mmss(total))
    for i, c in enumerate(CARDS, 1):
        w = len(c["say"].split())
        print(f"  card {i:>2} {c['title'][:42]:<42} planned {mmss(c['secs'])}  say {w:>3} words = {mmss(w / WPM * 60)}")
