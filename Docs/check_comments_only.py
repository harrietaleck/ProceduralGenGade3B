# -*- coding: utf-8 -*-
"""Check that only comments changed in the C++ source compared with git HEAD.

Usage: python check_comments_only.py [file names...]
With no arguments every changed file under Source/ProceduralGenGade3B is checked.
"""

import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCE = "Source/ProceduralGenGade3B"


def strip_comments(text: str) -> str:
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                i += 1
        elif c == "/" and nxt == "*":
            i += 2
            while i < n - 1 and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            out.append(" ")
        elif c in "\"'":
            quote = c
            out.append(c)
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    out.append(text[i])
                    i += 1
                out.append(text[i])
                i += 1
            if i < n:
                out.append(text[i])
            i += 1
        else:
            out.append(c)
            i += 1
    # Whitespace and blank lines do not matter, only the code tokens.
    return " ".join("".join(out).split())


def changed_files():
    result = subprocess.run(["git", "diff", "--name-only", "HEAD", "--", SOURCE],
                            cwd=REPO, capture_output=True, text=True, check=True)
    return [line for line in result.stdout.splitlines() if line.strip()]


def main():
    if len(sys.argv) > 1:
        files = [f"{SOURCE}/{name}" for name in sys.argv[1:]]
    else:
        files = changed_files()

    problems = 0
    for rel in files:
        old = subprocess.run(["git", "show", f"HEAD:{rel}"], cwd=REPO,
                             capture_output=True, text=True, encoding="utf-8", errors="replace")
        if old.returncode != 0:
            print(f"SKIP (new file) {rel}")
            continue
        new = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        if strip_comments(old.stdout) == strip_comments(new):
            print(f"OK   {rel}")
        else:
            problems += 1
            a, b = strip_comments(old.stdout), strip_comments(new)
            pos = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]), min(len(a), len(b)))
            print(f"CODE CHANGED {rel}\n   old: ...{a[max(0, pos - 80):pos + 80]}\n   new: ...{b[max(0, pos - 80):pos + 80]}")

    print(f"\n{len(files)} file(s) checked, {problems} with code changes")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
