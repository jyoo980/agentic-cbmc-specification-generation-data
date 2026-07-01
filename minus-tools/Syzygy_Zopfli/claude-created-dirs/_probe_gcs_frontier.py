#!/usr/bin/env python3
"""Find the depth frontier for the GetCostStat db_sign mutant (cheapest, no table read)."""
import subprocess, sys, os

CFILE = "/app/Syzygy_Zopfli/c_code/zopfli.c"
FUNCTION = "GetCostStat"
CALLEES = ["ZopfliGetDistExtraBits", "ZopfliGetDistSymbol",
           "ZopfliGetLengthExtraBits", "ZopfliGetLengthSymbol"]
WORKDIR = "/app/_kill_work_gcs"
TIMEOUT = 120

RET = '        return lbits + dbits + stats->ll_symbols[lsym] + stats->d_symbols[dsym];'
db_sign = RET.replace('lbits + dbits', 'lbits - dbits')

def build(src_lines, tag):
    cfile = f"/app/Syzygy_Zopfli/c_code/_src_{tag}.c"
    open(cfile, "w").write("\n".join(src_lines))
    goto = f"{WORKDIR}/{tag}.goto"
    chk = f"{WORKDIR}/chk_{tag}.goto"
    subprocess.run(["goto-cc", "-o", goto, cfile, "--function", FUNCTION],
                   check=True, capture_output=True, text=True)
    subprocess.run(["goto-instrument", "--partial-loops", "--unwind", "5", goto, goto],
                   check=True, capture_output=True, text=True)
    cmd = ["goto-instrument"]
    for c in CALLEES:
        cmd += ["--replace-call-with-contract", c]
    cmd += ["--enforce-contract", FUNCTION, goto, chk]
    subprocess.run(cmd, check=True, capture_output=True, text=True)
    return chk

def run(chk, depth):
    try:
        r = subprocess.run(["cbmc", chk, "--function", FUNCTION, "--depth", str(depth)],
                           capture_output=True, text=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return "TIMEOUT"
    out = r.stdout + r.stderr
    if "VERIFICATION SUCCESSFUL" in out: return "SURVIVED"
    if "VERIFICATION FAILED" in out: return "KILLED"
    return "UNKNOWN"

def main():
    os.makedirs(WORKDIR, exist_ok=True)
    src = open(CFILE).read().split("\n")
    ln = next(i for i, l in enumerate(src) if l == RET)
    ml = list(src); ml[ln] = db_sign
    chk = build(ml, "dbsign")
    for depth in [int(x) for x in sys.argv[1].split(",")]:
        print(f"db_sign @depth {depth} -> {run(chk, depth)}", flush=True)

if __name__ == "__main__":
    main()
