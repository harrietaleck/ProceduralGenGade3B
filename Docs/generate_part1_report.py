# -*- coding: utf-8 -*-
"""Generate GADE7322 Part 1 process report (editable .docx)."""

from pathlib import Path
from docx import Document
from docx.shared import Pt, Inches, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.enum.style import WD_STYLE_TYPE
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

ROOT = Path(r"C:\Users\TUF\Documents\Unreal\ProceduralGenGade3B")
FIGS = ROOT / "Docs" / "Figures"
OUT = ROOT / "Docs" / "GADE7322_Part1_Process_Report.docx"

GITHUB = "https://github.com/harrietaleck/ProceduralGenGade3B"
BRANCH = "part1"


def set_run_font(run, name="Times New Roman", size=12, bold=False, italic=False):
    run.font.name = name
    run._element.rPr.rFonts.set(qn("w:eastAsia"), name)
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic


def add_para(doc, text, *, bold=False, italic=False, size=12, align="left", space_after=8, first_line=True):
    p = doc.add_paragraph()
    if align == "center":
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    elif align == "justify":
        p.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    else:
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
    pf = p.paragraph_format
    pf.space_after = Pt(space_after)
    pf.line_spacing = 1.5
    if first_line and align == "justify":
        pf.first_line_indent = Cm(1.27)
    run = p.add_run(text)
    set_run_font(run, size=size, bold=bold, italic=italic)
    return p


def add_heading_custom(doc, text, level=1):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.LEFT
    pf = p.paragraph_format
    pf.space_before = Pt(18 if level == 1 else 12)
    pf.space_after = Pt(8)
    pf.line_spacing = 1.5
    run = p.add_run(text)
    size = 14 if level == 1 else 12
    set_run_font(run, size=size, bold=True)
    return p


def add_figure(doc, path, caption):
    if path.exists():
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.space_before = Pt(10)
        p.paragraph_format.space_after = Pt(4)
        run = p.add_run()
        run.add_picture(str(path), width=Inches(5.6))
    cap = doc.add_paragraph()
    cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
    cap.paragraph_format.space_after = Pt(12)
    cap.paragraph_format.first_line_indent = Cm(0)
    r = cap.add_run(caption)
    set_run_font(r, size=10, italic=True)


def add_bullet(doc, text):
    p = doc.add_paragraph(style="List Bullet")
    p.paragraph_format.line_spacing = 1.5
    p.paragraph_format.space_after = Pt(4)
    p.clear()
    run = p.add_run(text)
    set_run_font(run, size=12)


def add_mixed_para(doc, parts, align="justify", first_line=True):
    """parts: list of (text, bold, italic)"""
    p = doc.add_paragraph()
    if align == "justify":
        p.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    pf = p.paragraph_format
    pf.space_after = Pt(8)
    pf.line_spacing = 1.5
    if first_line and align == "justify":
        pf.first_line_indent = Cm(1.27)
    for text, bold, italic in parts:
        run = p.add_run(text)
        set_run_font(run, size=12, bold=bold, italic=italic)
    return p


def main():
    doc = Document()

    # Page setup
    section = doc.sections[0]
    section.top_margin = Cm(2.54)
    section.bottom_margin = Cm(2.54)
    section.left_margin = Cm(2.54)
    section.right_margin = Cm(2.54)

    # Normal style
    style = doc.styles["Normal"]
    style.font.name = "Times New Roman"
    style.font.size = Pt(12)
    style._element.rPr.rFonts.set(qn("w:eastAsia"), "Times New Roman")

    # ========== COVER ==========
    for _ in range(3):
        add_para(doc, "", first_line=False, space_after=0)
    add_para(doc, "THE INDEPENDENT INSTITUTE OF EDUCATION", bold=True, size=14, align="center", first_line=False)
    add_para(doc, "School of Information Technology", bold=True, size=12, align="center", first_line=False, space_after=18)
    add_para(doc, "GADE7322 — Game Development 3B", bold=True, size=14, align="center", first_line=False)
    add_para(doc, "Part 1 Portfolio of Evidence (PoE)", bold=True, size=13, align="center", first_line=False, space_after=24)
    add_para(doc, "Process Report:", bold=True, size=16, align="center", first_line=False, space_after=4)
    add_para(
        doc,
        "Procedural Tower Defence — Development Challenges, Solutions, Unique Features and Complexity",
        bold=True,
        size=14,
        align="center",
        first_line=False,
        space_after=28,
    )
    add_para(doc, "Student Name: Harriet Aleck", size=12, align="center", first_line=False)
    add_para(doc, "Student Number: [Insert student number]", size=12, align="center", first_line=False)
    add_para(doc, "Module: GADE7322", size=12, align="center", first_line=False)
    add_para(doc, "Year: 2026", size=12, align="center", first_line=False, space_after=18)
    add_para(doc, "GitHub repository:", size=12, align="center", first_line=False, space_after=2)
    add_para(doc, GITHUB, italic=True, size=11, align="center", first_line=False, space_after=2)
    add_para(doc, f"Active development branch: {BRANCH}", size=11, align="center", first_line=False, space_after=18)
    add_para(
        doc,
        "Referencing style: Harvard Anglia (The Independent Institute of Education, 2025)",
        size=11,
        align="center",
        first_line=False,
    )

    doc.add_page_break()

    # ========== CONTENTS ==========
    add_heading_custom(doc, "Table of Contents", 1)
    toc_items = [
        "1. Introduction",
        "2. Project overview and repository",
        "3. Development process followed",
        "4. Challenges faced and how each challenge was tackled",
        "5. C++ systems implemented",
        "6. Blueprint and UMG work completed",
        "7. Unique design decisions and added complexity",
        "8. Successes achieved",
        "9. Reflection and conclusion",
        "Reference list",
        "Appendix A: Key controls and play notes",
    ]
    for item in toc_items:
        add_para(doc, item, first_line=False, space_after=4)

    doc.add_page_break()

    # ========== 1 ==========
    add_heading_custom(doc, "1. Introduction", 1)
    add_para(
        doc,
        "This process report documents the Part 1 development of a procedural tower-defence "
        "prototype built in Unreal Engine 5.8 for GADE7322 (Game Development 3B). Part 1 of the "
        "Portfolio of Evidence (PoE) requires a playable game in which the terrain is generated "
        "at runtime, multiple enemy pathways lead to a central tower, defenders can be placed "
        "strategically, and a complete game loop (resources, combat, game over, restart) is "
        "demonstrated (The Independent Institute of Education, 2026).",
        align="justify",
    )
    add_para(
        doc,
        "Procedural content generation (PCG) is widely used in games to create maps and "
        "content algorithmically, increasing variety for players while reducing the need for "
        "fully hand-authored levels (Farrokhi Maleki and Zhao, 2024). This project applies "
        "that idea to a tower-defence layout: fractal-noise terrain, carved pathways, "
        "build pads, decoration scatter, and wave-driven map growth.",
        align="justify",
    )
    add_para(
        doc,
        "The purpose of this report is to explain the development process, the challenges "
        "encountered, how those challenges were solved in C++ and Blueprint/UMG, the unique "
        "features added beyond the minimum brief, and the successes achieved. Screenshots "
        "from playtesting are included as figures. All in-text citations and the reference "
        "list follow The IIE Harvard Anglia style (The Independent Institute of Education, 2025).",
        align="justify",
    )

    # ========== 2 ==========
    add_heading_custom(doc, "2. Project overview and repository", 1)
    add_para(
        doc,
        "The project is hosted on GitHub and must be reviewed on the part1 branch (the main "
        "branch currently holds only the initial Unreal project shell). The submission "
        "repository link is:",
        align="justify",
    )
    add_para(doc, GITHUB, italic=True, align="center", first_line=False)
    add_para(doc, f"Branch containing Part 1 work: {BRANCH}", bold=True, align="center", first_line=False)
    add_para(
        doc,
        "Core technologies used include Unreal Engine 5.8, C++ gameplay systems, UMG "
        "(Unreal Motion Graphics) for the match HUD and end screens, and Git with Git LFS "
        "for binary assets such as .uasset and .umap files (Epic Games, 2025a). The default "
        "play map is TowerDefense.umap with ATDGameMode as the game mode.",
        align="justify",
    )

    # ========== 3 ==========
    add_heading_custom(doc, "3. Development process followed", 1)
    add_para(
        doc,
        "Development was organised around the Part 1 rubric sections: procedural terrain and "
        "tower; defender placement; enemy spawning and combat; complete game loop; UI/UX; "
        "and suitable complexity (The Independent Institute of Education, 2026). Work "
        "progressed iteratively:",
        align="justify",
    )
    add_bullet(doc, "Stage A — Procedural terrain mesh, seed randomisation, multi-path carving, and tower placement.")
    add_bullet(doc, "Stage B — Defenders, build pads, click placement, affordability checks, and combat.")
    add_bullet(doc, "Stage C — Enemy spawner, wave manager, path following, and attack behaviour.")
    add_bullet(doc, "Stage D — Economy (loot, spend, upkeep), pause/restart, and game-over/victory.")
    add_bullet(doc, "Stage E — UMG HUD, warning banners, camera zoom, decorations, and polish.")
    add_para(
        doc,
        "Version control commits on part1 record this progression (for example map expansion "
        "after waves, HUD overlays, combat polish, and decoration scatter). Pair collaboration "
        "is planned so that UI and art assets can be developed on a separate branch and "
        "merged into part1 when ready, while gameplay systems remain on part1.",
        align="justify",
    )

    # ========== 4 ==========
    add_heading_custom(doc, "4. Challenges faced and how each challenge was tackled", 1)

    add_heading_custom(doc, "4.1 Challenge: Runtime terrain must be different every game", 2)
    add_para(
        doc,
        "Challenge. Hand-built landscapes would not satisfy the brief’s requirement for "
        "procedural generation that changes each session.",
        align="justify",
    )
    add_para(
        doc,
        "Solution. AProceduralTerrain builds a grid mesh with UProceduralMeshComponent at "
        "runtime, using a seeded FRandomStream and fractal (fBm) value noise for height "
        "(Epic Games, 2025b). PrepareForNewGame() randomises the seed unless a fixed seed "
        "is locked for debugging. Generation is triggered explicitly by the GameMode to "
        "avoid BeginPlay ordering bugs between the GameMode and the terrain actor.",
        align="justify",
    )
    add_para(
        doc,
        "Noise-based methods remain a practical PCG approach for terrain heightfields "
        "(Farrokhi Maleki and Zhao, 2024). Paths are then carved from border spawn points "
        "to the centre tower cell, with Chaikin smoothing to reduce blocky waypoint corners.",
        align="justify",
    )

    add_heading_custom(doc, "4.2 Challenge: Invalid maps breaking gameplay", 2)
    add_para(
        doc,
        "Challenge. Early generations sometimes produced incomplete path data or failed "
        "tower placement, which would start a broken match.",
        align="justify",
    )
    add_para(
        doc,
        "Solution. ValidateGeneratedWorld() enforces at least three paths, valid grid data, "
        "and build slots. GenerateTerrain() retries with a new seed (bounded attempts). "
        "TDGameMode adds a whole-world validation gate (tower placement tolerance, pad "
        "markers, non-overlapping bounds) and regenerates the world before gameplay starts.",
        align="justify",
    )

    add_heading_custom(doc, "4.3 Challenge: Enemy movement looked jerky", 2)
    add_para(
        doc,
        "Challenge. Enemies teleported between dense waypoints after path smoothing, "
        "which looked unnatural.",
        align="justify",
    )
    add_para(
        doc,
        "Solution. Waypoint thinning, horizontal-only movement, acceleration/deceleration "
        "into corners, and turn interpolation were added in AEnemy::MoveAlongPath(). "
        "Spawn no longer teleports the enemy onto waypoint 0 after the spawner already "
        "placed it.",
        align="justify",
    )

    add_heading_custom(doc, "4.4 Challenge: Enemies did not appear to attack defenders", 2)
    add_para(
        doc,
        "Challenge. Combat code existed (FindTargetInRange), but playtests suggested "
        "defenders were ignored.",
        align="justify",
    )
    add_para(
        doc,
        "Solution. Line-of-sight traces were hitting the procedural terrain mesh between "
        "the path and off-path build pads. Traces now ignore AProceduralTerrain. Attack "
        "and detection ranges were increased so enemies can stop beside pads one cell "
        "off the lane. Enemies also face their target while attacking so combat reads "
        "clearly on screen.",
        align="justify",
    )

    add_heading_custom(doc, "4.5 Challenge: HUD messages overlapping and game over looking weak", 2)
    add_para(
        doc,
        "Challenge. Canvas-drawn “Not Enough Loot” sat under the UMG “Wave Active” text. "
        "Game over was a small canvas DrawText, not a proper screen.",
        align="justify",
    )
    add_figure(
        doc,
        FIGS / "Figure1_WaveActive_HUD.png",
        "Figure 1: Early HUD overlap between wave status and insufficient-funds feedback "
        "(Aleck, 2026a).",
    )
    add_para(
        doc,
        "Solution. Insufficient funds moved to UTDWarningBannerWidget (bottom-centre toast). "
        "Game over and victory use UTDEndScreenWidget — a full-screen dim overlay with large "
        "centred title text and “Press R to restart”, created by the GameMode at a higher "
        "viewport Z-order than the match HUD (Epic Games, 2025c).",
        align="justify",
    )
    add_figure(
        doc,
        FIGS / "Figure2_GameOver.png",
        "Figure 2: Gameplay state requiring a clear, centred game-over presentation "
        "(Aleck, 2026b).",
    )

    add_heading_custom(doc, "4.6 Challenge: Camera could not show the whole map", 2)
    add_para(
        doc,
        "Challenge. After wave expansion the grid grows toward MaxGridSize; hero MaxZoom "
        "and spring-arm collision clipped zoom-out.",
        align="justify",
    )
    add_para(
        doc,
        "Solution. AHeroCharacter now scales MaxZoom from map world size "
        "(GridSize × CellSize × multiplier), refreshes limits when zooming, widens FOV, "
        "allows steeper pitch, and disables boom collision so hills no longer shorten the arm.",
        align="justify",
    )

    add_heading_custom(doc, "4.7 Challenge: Defenders used hitscan debug lines", 2)
    add_para(
        doc,
        "Challenge. The tower fired visible ball projectiles, but defenders fell back to "
        "hitscan with yellow debug lines because ProjectileClass was unset.",
        align="justify",
    )
    add_para(
        doc,
        "Solution. ADefender now defaults ProjectileClass to AProjectile and configures "
        "orange ball visuals via ConfigureVisuals(), matching the tower’s combat language "
        "while remaining visually distinct.",
        align="justify",
    )

    # ========== 5 ==========
    add_heading_custom(doc, "5. C++ systems implemented", 1)
    add_para(
        doc,
        "Most Part 1 gameplay is implemented in C++ so systems compile without depending "
        "on unfinished Blueprint assets. Major classes include:",
        align="justify",
    )
    add_bullet(doc, "AProceduralTerrain — mesh generation, paths, slots, expansion, decoration scatter.")
    add_bullet(doc, "ATDGameMode — economy, world spawn/validate, HUD creation, game over.")
    add_bullet(doc, "ATower / ADefender / AEnemy / AProjectile — combat and health.")
    add_bullet(doc, "AEnemySpawner / AWaveManager — interval spawning and wave progression.")
    add_bullet(doc, "ATDPlayerController — click placement, pause (P), restart (R).")
    add_bullet(doc, "AHeroCharacter — third-person walk + tactical camera zoom/orbit.")
    add_bullet(doc, "UTDHUDWidget / UTDEndScreenWidget / UTDWarningBannerWidget — UMG logic in C++.")
    add_bullet(doc, "ATerrainProp — trees, rocks, and building placeholders scattered on terrain cells.")
    add_bullet(doc, "UHealthComponent / UDamageFlashComponent — shared health and hit feedback.")
    add_para(
        doc,
        "Unreal’s official documentation for procedural mesh construction and UMG was used "
        "as the technical reference while implementing these systems (Epic Games, 2025b; "
        "Epic Games, 2025c).",
        align="justify",
    )
    add_figure(
        doc,
        FIGS / "Figure3_Gameplay_Tower.png",
        "Figure 3: In-game procedural map with tower, pads, and active match UI "
        "(Aleck, 2026c).",
    )

    # ========== 6 ==========
    add_heading_custom(doc, "6. Blueprint and UMG work completed", 1)
    add_para(
        doc,
        "Although gameplay is C++-heavy, Blueprint/UMG assets are required for designed UI "
        "and level setup:",
        align="justify",
    )
    add_bullet(
        doc,
        "WBP_MatchHUD (Content/UI) — Widget Blueprint parented to UTDHUDWidget. Bound "
        "widgets: WaveText, WaveStatusText, LootText, TowerHealthBar, TowerHealthText, "
        "EnemiesRemainingText.",
    )
    add_bullet(doc, "TowerDefense.umap — default play map containing the procedural terrain actor.")
    add_bullet(doc, "BP_StrategyCamera — Blueprint camera-related asset retained in Content/Blueprints.")
    add_para(
        doc,
        "End-screen and warning banners were implemented as pure C++ UserWidgets so they "
        "do not depend on extra Widget Blueprint authoring, while still participating in "
        "the UMG viewport stack. Future art/UI collaboration can replace placeholder "
        "meshes by assigning TreeMeshes, RockMeshes, and BuildingMeshes on the terrain "
        "actor without rewriting scatter logic.",
        align="justify",
    )

    # ========== 7 ==========
    add_heading_custom(doc, "7. Unique design decisions and added complexity", 1)
    add_para(
        doc,
        "Beyond the minimum Part 1 checklist, the following features were added to strengthen "
        "complexity and player experience:",
        align="justify",
    )
    add_bullet(doc, "Post-wave whole-grid expansion (ring growth) with new build pads.")
    add_bullet(doc, "Branching lanes forked from existing paths after waves.")
    add_bullet(doc, "Defender upkeep economy (loot drain per living defender each wave).")
    add_bullet(doc, "Chaikin path smoothing + waypoint thinning for readable enemy movement.")
    add_bullet(doc, "World validation/retry loops before match start.")
    add_bullet(doc, "Playable hero with map-scaled camera zoom.")
    add_bullet(doc, "Procedural decoration scatter (trees, rocks, buildings) using the map seed.")
    add_bullet(doc, "Tower destruction debris/dust and coloured projectile balls.")
    add_bullet(doc, "Victory after three escalating waves (shorter demo loop) plus full game-over screen.")
    add_para(
        doc,
        "These extras align with the brief’s expectation that solutions should show "
        "appropriate module complexity and problem-solving, not only the bare minimum "
        "(The Independent Institute of Education, 2026). Growing the map after waves also "
        "responds to lecture feedback that the world should feel progressive rather than static.",
        align="justify",
    )

    # ========== 8 ==========
    add_heading_custom(doc, "8. Successes achieved", 1)
    add_para(
        doc,
        "Against the Part 1 rubric, the prototype meets the functional requirements for "
        "terrain generation, multi-path tower defence, defender placement off-path, enemy "
        "spawning and combat, resource loop, and UI feedback for health, loot, cost, pause, "
        "restart, and game over. Successes specifically worth highlighting are:",
        align="justify",
    )
    add_bullet(doc, "A reproducible, seeded procedural pipeline that still feels different each restart (R).")
    add_bullet(doc, "A validated world gate that refuses to start gameplay on broken layouts.")
    add_bullet(doc, "Clear separation of Canvas HUD (world health bars/info) and UMG (match/end UI).")
    add_bullet(doc, "Combat readability after defender LOS and projectile fixes.")
    add_bullet(doc, "GitHub version history on part1 documenting iterative improvement.")
    add_para(
        doc,
        "Remaining polish for higher UI marks includes swapping placeholder meshes for "
        "authored assets and further visual design on WBP_MatchHUD by the collaborating "
        "partner on a dedicated UI branch.",
        align="justify",
    )

    # ========== 9 ==========
    add_heading_custom(doc, "9. Reflection and conclusion", 1)
    add_para(
        doc,
        "Part 1 established a playable procedural tower-defence foundation in C++ with "
        "supporting UMG/Blueprint assets. The hardest technical problems were generation "
        "correctness, combat perception (LOS), HUD layering, and camera coverage as the "
        "map grew. Each was solved with targeted system changes rather than temporary "
        "workarounds. The project repository for marking and collaboration is:",
        align="justify",
    )
    add_para(doc, GITHUB + " (branch: part1)", bold=True, align="center", first_line=False)
    add_para(
        doc,
        "Future Parts will extend enemy/defender variety, smarter procedural spawning, "
        "upgrades, and additional procedural features as required by the PoE roadmap "
        "(The Independent Institute of Education, 2026).",
        align="justify",
    )

    doc.add_page_break()

    # ========== REFERENCE LIST ==========
    add_heading_custom(doc, "Reference list", 1)
    add_para(
        doc,
        "Aleck, H., 2026a. Wave Active HUD overlapping insufficient-funds feedback during playtest. "
        "[photograph] (Personal Collection). Unpublished.",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "Aleck, H., 2026b. Game over state shown during tower-defence playtest. "
        "[photograph] (Personal Collection). Unpublished.",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "Aleck, H., 2026c. Procedural tower-defence match showing tower, build pads and HUD. "
        "[photograph] (Personal Collection). Unpublished.",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "Aleck, H., 2026d. ProceduralGenGade3B (Version part1) [Source code]. Available at: "
        f"<{GITHUB}/tree/part1> [Accessed 4 September 2026].",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "Epic Games, 2025a. Unreal Engine 5 documentation. [online] Available at: "
        "<https://dev.epicgames.com/documentation/en-us/unreal-engine/> "
        "[Accessed 4 September 2026].",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "Epic Games, 2025b. UProceduralMeshComponent. [online] Available at: "
        "<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/"
        "ProceduralMeshComponent/UProceduralMeshComponent> "
        "[Accessed 4 September 2026].",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "Epic Games, 2025c. UUserWidget. [online] Available at: "
        "<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/Blueprint/UUserWidget> "
        "[Accessed 4 September 2026].",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "Farrokhi Maleki, M. and Zhao, R., 2024. Procedural content generation in games: "
        "A survey with insights on emerging LLM integration. Proceedings of the AAAI "
        "Conference on Artificial Intelligence and Interactive Digital Entertainment, "
        "[e-journal] 20(1), pp.167–178. Available at: "
        "<https://doi.org/10.1609/aiide.v20i1.31877> [Accessed 4 September 2026].",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "The Independent Institute of Education (IIE), 2025. Harvard Anglia style reference "
        "guide – adapted for The IIE. [pdf] The Independent Institute of Education.",
        first_line=False,
        space_after=10,
    )
    add_para(
        doc,
        "The Independent Institute of Education (IIE), 2026. Game Development 3B "
        "[GADE7322 Part 1 PoE brief and assessment rubric]. The Independent Institute of "
        "Education: Unpublished.",
        first_line=False,
        space_after=10,
    )

    # ========== APPENDIX ==========
    doc.add_page_break()
    add_heading_custom(doc, "Appendix A: Key controls and play notes", 1)
    add_bullet(doc, "WASD — move hero")
    add_bullet(doc, "Right mouse drag — orbit/tilt camera")
    add_bullet(doc, "Mouse wheel — zoom (scales with map size)")
    add_bullet(doc, "Left click build pad — place defender (if affordable and free)")
    add_bullet(doc, "P — pause / unpause")
    add_bullet(doc, "R — restart (new seed / new map)")
    add_para(
        doc,
        "Open ProceduralGenGade3B.uproject in Unreal Engine 5.8, ensure branch part1 is "
        "checked out, run git lfs pull for assets, then Play on TowerDefense.",
        align="justify",
        first_line=False,
    )

    # Placeholder note for student number
    add_para(
        doc,
        "Note: Replace “[Insert student number]” on the cover page before Arc submission. "
        "You may also add your collaborator’s name if required by the pair-programming "
        "submission instructions.",
        italic=True,
        first_line=False,
        space_after=6,
    )

    doc.save(OUT)
    print(f"Wrote: {OUT}")


if __name__ == "__main__":
    main()
