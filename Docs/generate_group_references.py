# -*- coding: utf-8 -*-
"""Generate the GADE7322 group document: GitHub links, references and acknowledgements (.docx)."""

from pathlib import Path

from docx import Document
from docx.shared import Pt, Cm
from docx.oxml.ns import qn

from generate_part2_planning import (
    add_para, add_heading_custom, add_table, add_toc, style_headings, GITHUB,
)

ROOT = Path(r"C:\Users\TUF\Documents\Unreal\ProceduralGenGade3B")
OUT = ROOT / "Docs" / "GADE7322_Group_Links_References_Acknowledgements.docx"
ACCESSED = "2 October 2026"
ASSET_URL = "https://www.unrealengine.com/marketplace/en-US/product/low-poly-nature-essentials"


def add_bullet(doc, text):
    p = add_para(doc, text, align="left", first_line=False, space_after=4)
    p.style = doc.styles["List Bullet"]
    return p


def main():
    doc = Document()
    section = doc.sections[0]
    for side in ("top_margin", "bottom_margin", "left_margin", "right_margin"):
        setattr(section, side, Cm(2.54))
    normal = doc.styles["Normal"]
    normal.font.name = "Times New Roman"
    normal.font.size = Pt(12)
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), "Times New Roman")
    style_headings(doc)

    # ---- Cover ----
    for _ in range(3):
        add_para(doc, "", first_line=False, space_after=0)
    add_para(doc, "THE INDEPENDENT INSTITUTE OF EDUCATION", bold=True, size=14, align="center")
    add_para(doc, "EMERIS VEGA SCHOOL", bold=True, align="center", space_after=18)
    add_para(doc, "GADE7322: Game Development 3B", bold=True, size=14, align="center")
    add_para(doc, "Portfolio of Evidence (PoE): Group Project", bold=True, size=13, align="center", space_after=24)
    add_para(doc, "Project Links, References and Acknowledgements", bold=True, size=16, align="center",
             space_after=4)
    add_para(doc, "Procedural Tower Defence (ProceduralGenGade3B)", bold=True, size=13, align="center",
             space_after=28)
    add_para(doc, "Group members:", bold=True, align="center", space_after=2)
    add_para(doc, "Harriet Manda (st10252836)", align="center", space_after=2)
    add_para(doc, "Belinda Mtabane ([Insert student number])", align="center", space_after=18)
    add_para(doc, "Module: GADE7322", align="center")
    add_para(doc, "Year: 2026", align="center", space_after=18)
    add_para(doc, "GitHub repository:", align="center", space_after=2)
    add_para(doc, GITHUB, italic=True, size=11, align="center", space_after=18)
    add_para(doc, "Referencing style: Harvard Anglia (The Independent Institute of Education, 2025)",
             size=11, align="center")
    doc.add_page_break()

    add_toc(doc)
    doc.add_page_break()

    # ---- 1. Repository ----
    add_heading_custom(doc, "Project repository")
    add_para(doc,
             "All source code, Unreal Engine content and project files for the group game are stored in "
             "one GitHub repository (Manda and Mtabane, 2026). The branches below hold each stage of the "
             "Portfolio of Evidence.", align="justify")
    add_table(doc,
              ["Item", "Link or detail"],
              [
                  ["Repository", GITHUB],
                  ["Part 1 (procedural terrain and core loop)", f"{GITHUB}/tree/part1"],
                  ["Part 1 user interface work", f"{GITHUB}/tree/part1UI"],
                  ["Part 2 (enemy types, defenders, procedural waves)", f"{GITHUB}/tree/part2"],
                  ["Engine", "Unreal Engine 5.8, C++ with UMG widgets"],
                  ["Plugins", "ProceduralMeshComponent, Modeling Tools Editor Mode"],
                  ["How to run", "Check out the branch, run git lfs pull, open ProceduralGenGade3B.uproject "
                                 "and press Play on the TowerDefense map"],
              ],
              "Table 1: Repository links and project details")

    # ---- 2. Acknowledgements ----
    add_heading_custom(doc, "Acknowledgements")

    add_heading_custom(doc, "Third-party assets", level=2)
    add_bullet(doc,
               "Low Poly Nature: Essentials by Vertex Rage Studio (2024) supplies the trees, rocks, plants and "
               "props that the terrain scatters across the map (Content/VRS_LowPolyNatureEssentials). It was "
               "used under the Fab / Unreal Engine Marketplace licence and was not modified.")
    add_bullet(doc,
               "User interface screen artwork in Content/UI/SourceArt (start, settings, defenders, upgrades, "
               "game over and victory screens): [confirm creator and tool or source].")

    add_heading_custom(doc, "Engine, tools and documentation", level=2)
    add_bullet(doc,
               "Unreal Engine 5.8 and its documentation (Epic Games, 2025a), in particular "
               "UProceduralMeshComponent for the runtime terrain mesh (Epic Games, 2025b) and UUserWidget "
               "for the HUD and menu screens (Epic Games, 2025c).")
    add_bullet(doc, "Git, GitHub and GitHub Desktop for version control and collaboration.")

    add_heading_custom(doc, "Algorithms and research", level=2)
    add_bullet(doc,
               "Enemy rerouting uses the A* search algorithm (Hart, Nilsson and Raphael, 1968) over the "
               "procedural path network.")
    add_bullet(doc,
               "Enemy paths and rerouted routes are smoothed with Chaikin's corner-cutting algorithm "
               "(Chaikin, 1974).")
    add_bullet(doc,
               "Procedural generation design was informed by Farrokhi Maleki and Zhao (2024). The adaptive "
               "wave director was informed by research on dynamic difficulty adjustment (Guo, Thawonmas and "
               "Ren, 2024; Mortazavi, Moradi and Vahabie, 2024) and adaptive wave generation in tower "
               "defence (Hind and Harvey, 2024).")
    add_bullet(doc,
               "Assessment requirements come from the GADE7322 Part 1 and Part 2 briefs and rubrics "
               "(The Independent Institute of Education, 2026a; 2026b).")

    add_heading_custom(doc, "Use of generative AI", level=2)
    add_para(doc,
             "The Cursor AI coding agent (Anysphere, 2026) was used during development to help write and "
             "debug C++ gameplay code, rewrite code comments in plain language, and draft planning and "
             "report documents. Commits made with this tool are marked in the GitHub history with a "
             "\"Co-authored-by: Cursor\" line. All AI-assisted output was reviewed, play-tested and adjusted "
             "by the group, and the group made the design decisions, tested the game and is responsible for "
             "the final submission. [Group members: confirm or adjust this statement so it matches how each "
             "of you used AI tools.]", align="justify")

    # ---- 3. References ----
    doc.add_page_break()
    add_heading_custom(doc, "Reference list")
    refs = [
        "Anysphere, 2026. Cursor [AI code editor and coding agent]. Available at: <https://cursor.com> "
        f"[Accessed {ACCESSED}].",
        "Chaikin, G.M., 1974. An algorithm for high-speed curve generation. Computer Graphics and Image "
        "Processing, [e-journal] 3(4), pp.346–349. Available at: "
        f"<https://doi.org/10.1016/0146-664X(74)90028-8> [Accessed {ACCESSED}].",
        "Epic Games, 2025a. Unreal Engine 5 documentation. [online] Available at: "
        f"<https://dev.epicgames.com/documentation/en-us/unreal-engine/> [Accessed {ACCESSED}].",
        "Epic Games, 2025b. UProceduralMeshComponent. [online] Available at: "
        "<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ProceduralMeshComponent/"
        f"UProceduralMeshComponent> [Accessed {ACCESSED}].",
        "Epic Games, 2025c. UUserWidget. [online] Available at: "
        "<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/Blueprint/UUserWidget> "
        f"[Accessed {ACCESSED}].",
        "Farrokhi Maleki, M. and Zhao, R., 2024. Procedural content generation in games: A survey with "
        "insights on emerging LLM integration. Proceedings of the AAAI Conference on Artificial "
        "Intelligence and Interactive Digital Entertainment, [e-journal] 20(1), pp.167–178. Available at: "
        f"<https://doi.org/10.1609/aiide.v20i1.31877> [Accessed {ACCESSED}].",
        "Guo, Z., Thawonmas, R. and Ren, X., 2024. Rethinking dynamic difficulty adjustment for video game "
        "design. Entertainment Computing, [e-journal] 50, 100663. Available at: "
        f"<https://doi.org/10.1016/j.entcom.2024.100663> [Accessed {ACCESSED}].",
        "Hart, P.E., Nilsson, N.J. and Raphael, B., 1968. A formal basis for the heuristic determination of "
        "minimum cost paths. IEEE Transactions on Systems Science and Cybernetics, [e-journal] 4(2), "
        f"pp.100–107. Available at: <https://doi.org/10.1109/TSSC.1968.300136> [Accessed {ACCESSED}].",
        "Hind, D. and Harvey, C., 2024. Using a NEAT approach with curriculums for dynamic content "
        "generation in video games. Personal and Ubiquitous Computing, [e-journal]. Available at: "
        f"<https://doi.org/10.1007/s00779-024-01801-z> [Accessed {ACCESSED}].",
        "Manda, H. and Mtabane, B., 2026. ProceduralGenGade3B [Source code]. Available at: "
        f"<{GITHUB}> [Accessed {ACCESSED}].",
        "Mortazavi, F., Moradi, H. and Vahabie, A.-H., 2024. Dynamic difficulty adjustment approaches in "
        "video games: a systematic literature review. Multimedia Tools and Applications, [e-journal] 83(35), "
        f"pp.83227–83274. Available at: <https://doi.org/10.1007/s11042-024-18768-x> [Accessed {ACCESSED}].",
        "The Independent Institute of Education (IIE), 2025. Harvard Anglia style reference guide – adapted "
        "for The IIE. [pdf] The Independent Institute of Education.",
        "The Independent Institute of Education (IIE), 2026a. Game Development 3B [GADE7322 Part 1 PoE brief "
        "and assessment rubric]. The Independent Institute of Education: Unpublished.",
        "The Independent Institute of Education (IIE), 2026b. Game Development 3B [GADE7322 Part 2 PoE brief "
        "and assessment rubric]. The Independent Institute of Education: Unpublished.",
        "Vertex Rage Studio, 2024. Low Poly Nature: Essentials. [3D asset pack] Fab / Unreal Engine "
        f"Marketplace. Available at: <{ASSET_URL}> [Accessed {ACCESSED}].",
    ]
    for ref in refs:
        add_para(doc, ref, space_after=10)

    doc.save(OUT)
    print(f"Saved {OUT}")


if __name__ == "__main__":
    main()
