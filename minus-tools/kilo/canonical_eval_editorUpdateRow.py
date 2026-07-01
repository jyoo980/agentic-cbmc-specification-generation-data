"""Evaluate editorUpdateRow's spec against mutants using the *canonical* CBMC
pipeline (tools.run_cbmc.run_cbmc), so the reported kill score matches how the
avocado harness scores the specification.

Usage:
    python canonical_eval_editorUpdateRow.py            # original + every mutant
    python canonical_eval_editorUpdateRow.py --orig     # original only
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, "/app")

from tools.run_cbmc import run_cbmc  # noqa: E402

FILE = Path("/app/kilo/kilo.c")
FUNCTION = "editorUpdateRow"


def verify(file_path: str) -> bool:
    """Return True iff CBMC verifies FUNCTION in file_path under the canonical pipeline."""
    # Run each mutant in its own temp dir so concurrent/intermediate .goto files
    # never clobber the real source tree.
    with tempfile.TemporaryDirectory() as d:
        dst = Path(d) / "kilo.c"
        shutil.copy(file_path, dst)
        r = run_cbmc(
            function_to_verify=FUNCTION,
            file_containing_function_to_verify=str(dst),
            cwd=d,
        )
        return r.is_function_verified


def main() -> None:
    orig = verify(str(FILE))
    print(f"ORIGINAL verifies: {orig}")
    if "--orig" in sys.argv:
        return

    mutants_out = subprocess.run(
        ["get-mutants", "--function", FUNCTION, "--file", str(FILE)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout

    # Parse unified-diff mutants: each block has a `-` orig line and a `+` mutant line.
    blocks = mutants_out.split("--- original")
    mutants = []  # (lineno, orig_text, mut_text)
    for b in blocks:
        minus = [l for l in b.splitlines() if l.startswith("-") and not l.startswith("---")]
        plus = [l for l in b.splitlines() if l.startswith("+") and not l.startswith("+++")]
        hunk = [l for l in b.splitlines() if l.startswith("@@")]
        if minus and plus and hunk:
            # @@ -669 +669 @@  -> target line number
            lineno = int(hunk[0].split("+")[1].split(",")[0].split(" ")[0])
            mutants.append((lineno, minus[0][1:], plus[0][1:]))

    src_lines = FILE.read_text().splitlines(keepends=True)
    killed = 0
    total = 0
    for i, (lineno, orig_text, mut_text) in enumerate(mutants, 1):
        with tempfile.TemporaryDirectory() as d:
            dst = Path(d) / "kilo.c"
            lines = list(src_lines)
            # Replace the target line's content with the mutant text (preserve newline).
            nl = "\n" if lines[lineno - 1].endswith("\n") else ""
            lines[lineno - 1] = mut_text + nl
            dst.write_text("".join(lines))
            # Confirm the mutation actually changed the file.
            if dst.read_text() == FILE.read_text():
                print(f"MUTANT {i} (line {lineno}): NOOP")
                continue
            total += 1
            r = run_cbmc(
                function_to_verify=FUNCTION,
                file_containing_function_to_verify=str(dst),
                cwd=d,
            )
            if r.is_function_verified:
                print(f"MUTANT {i} (line {lineno}): SURVIVED  [{mut_text.strip()}]")
            else:
                print(f"MUTANT {i} (line {lineno}): KILLED")
                killed += 1
    print(f"=== KILLED {killed} / {total} ===")


if __name__ == "__main__":
    main()
