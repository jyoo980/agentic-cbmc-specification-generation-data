#!/usr/bin/env python3
"""Diagnostic: does removing the two forall preconditions shave depth for db_sign?"""
import subprocess, sys, os

CFILE = "/app/Syzygy_Zopfli/c_code/zopfli.c"
FUNCTION = "GetCostStat"
CALLEES = ["ZopfliGetDistExtraBits", "ZopfliGetDistSymbol",
           "ZopfliGetLengthExtraBits", "ZopfliGetLengthSymbol"]
WORKDIR = "/app/_kill_work_gcs"
TIMEOUT = 150

RET = '        return lbits + dbits + stats->ll_symbols[lsym] + stats->d_symbols[dsym];'
db_sign = RET.replace('lbits + dbits', 'lbits - dbits')

# forall precondition block to optionally strip
FORALL_START = "__CPROVER_requires(__CPROVER_forall {"
FORALL_END = "((SymbolStats *)context)->d_symbols[k2] >= 0 })"

def strip_foralls(src):
    out = []
    skip = False
    for l in src:
        if l.strip().startswith("__CPROVER_requires(__CPROVER_forall {"):
            skip = True
        if skip:
            if l.strip().endswith("})"):
                skip = False
            continue
        out.append(l)
    return out

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
    os.remove(cfile)
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
    nf = strip_foralls(src)
    print("stripped %d lines" % (len(src) - len(nf)), flush=True)
    ln = next(i for i, l in enumerate(nf) if l == RET)
    ml = list(nf); ml[ln] = db_sign
    chk = build(ml, "dbsign_nf")
    for depth in [int(x) for x in sys.argv[1].split(",")]:
        print(f"db_sign(no-forall) @depth {depth} -> {run(chk, depth)}", flush=True)

if __name__ == "__main__":
    main()
