#!/bin/bash
# Mutation-test editorOpen under the CANONICAL harness pipeline
# (goto-cc + string.c stub, --partial-loops --unwind 5, --replace-call-with-contract
#  editorInsertRow, --enforce-contract editorOpen, cbmc --depth 200).
#
# Usage: ./run_mutants_editorOpen.sh <SRC.c>
# A mutant is KILLED iff the canonical pipeline does NOT verify (cbmc rc != 0).
set -u
SRC=${1:-/app/kilo/kilo.c}
STUB=/app/stubs/string.c
WORK=$(mktemp -d)
FN=editorOpen

# Each mutant: a label, the exact original substring, and its replacement.
# Substrings are chosen to be unique within editorOpen.
declare -a LABELS=(
  "M1:951 ==r -> !=r"
  "M2:951 ==n -> !=n"
  "M3:950 !=-1 -> ==-1"
  "M4:940 errno!= -> =="
  "M5:951 [len-1]r -> [len+1]r"
  "M6:951 [len-1]n -> [len+1]n"
  "M7:944 strlen+1 -> -1"
  "M8:951 && -> ||"
  "M9:951 || -> &&"
)
declare -a OLDS=(
  "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))"
  "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))"
  "getline(&line,&linecap,fp)) != -1)"
  "if (errno != ENOENT)"
  "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))"
  "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))"
  "size_t fnlen = strlen(filename)+1;"
  "if (linelen && (line"
  "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))"
)
declare -a NEWS=(
  "line[linelen-1] == '\\n' || line[linelen-1] != '\\r'))"
  "line[linelen-1] != '\\n' || line[linelen-1] == '\\r'))"
  "getline(&line,&linecap,fp)) == -1)"
  "if (errno == ENOENT)"
  "line[linelen-1] == '\\n' || line[linelen+1] == '\\r'))"
  "line[linelen+1] == '\\n' || line[linelen-1] == '\\r'))"
  "size_t fnlen = strlen(filename)-1;"
  "if (linelen || (line"
  "line[linelen-1] == '\\n' && line[linelen-1] == '\\r'))"
)

run_pipeline () { # $1 = source file, $2 = tag
  local src=$1 tag=$2
  local g="$WORK/$tag.goto" c="$WORK/$tag-check.goto"
  goto-cc -o "$g" "$src" "$STUB" --function $FN >/dev/null 2>&1 || { echo "GOTOCC_FAIL"; return; }
  goto-instrument --partial-loops --unwind 5 "$g" "$g" >/dev/null 2>&1
  goto-instrument --replace-call-with-contract editorInsertRow \
      --enforce-contract $FN "$g" "$c" >/dev/null 2>&1 || { echo "INSTR_FAIL"; return; }
  if cbmc "$c" --function $FN --depth 200 >/dev/null 2>&1; then
    echo "VERIFIED"
  else
    echo "FAILED"
  fi
}

echo "### Baseline (unmutated): $(run_pipeline "$SRC" base)"
echo

killed=0; total=${#LABELS[@]}
for i in "${!LABELS[@]}"; do
  mfile="$WORK/m$i.c"
  # Build mutant via python literal replacement (avoids sed escaping pitfalls).
  python3 - "$SRC" "$mfile" "${OLDS[$i]}" "${NEWS[$i]}" <<'PY'
import sys
src, dst, old, new = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
text = open(src).read()
if old not in text:
    sys.stderr.write("OLD-NOT-FOUND\n"); sys.exit(3)
if text.count(old) != 1:
    sys.stderr.write("OLD-NOT-UNIQUE count=%d\n" % text.count(old)); sys.exit(4)
open(dst,"w").write(text.replace(old,new))
PY
  if [ $? -ne 0 ]; then echo "${LABELS[$i]}: PATCH_ERROR"; continue; fi
  res=$(run_pipeline "$mfile" "m$i")
  if [ "$res" = "VERIFIED" ]; then
    echo "${LABELS[$i]}: SURVIVED"
  else
    echo "${LABELS[$i]}: KILLED ($res)"
    killed=$((killed+1))
  fi
done
echo
echo "KILL SCORE: $killed / $total"
rm -rf "$WORK"
