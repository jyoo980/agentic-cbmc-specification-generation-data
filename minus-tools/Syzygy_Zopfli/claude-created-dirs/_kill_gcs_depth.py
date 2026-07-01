#!/usr/bin/env python3
"""Test GetCostStat sign mutants at configurable --depth to probe vacuity."""
import subprocess, sys, os

CFILE = "/app/Syzygy_Zopfli/c_code/zopfli.c"
FUNCTION = "GetCostStat"
CALLEES = ["ZopfliGetDistExtraBits", "ZopfliGetDistSymbol",
           "ZopfliGetLengthExtraBits", "ZopfliGetLengthSymbol"]
WORKDIR = "/app/_kill_work_gcs"

DEPTHS = [int(x) for x in (sys.argv[1].split(',') if len(sys.argv) > 1 else ["200", "400", "600", "1000"])]

# current source line numbers
RET = '        return lbits + dbits + stats->ll_symbols[lsym] + stats->d_symbols[dsym];'
muts = {
    'd_sign':  RET.replace('+ stats->d_symbols[dsym]', '- stats->d_symbols[dsym]'),
    'll_sign': RET.replace('dbits + stats->ll_symbols[lsym]', 'dbits - stats->ll_symbols[lsym]'),
    'db_sign': RET.replace('lbits + dbits', 'lbits - dbits'),
}

def run(cfile, tag, depth):
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
    r = subprocess.run(["cbmc", chk, "--function", FUNCTION, "--depth", str(depth)],
                       capture_output=True, text=True)
    out = r.stdout + r.stderr
    if "VERIFICATION SUCCESSFUL" in out: return "SURVIVED"
    if "VERIFICATION FAILED" in out: return "KILLED"
    return "UNKNOWN"

def main():
    os.makedirs(WORKDIR, exist_ok=True)
    src = open(CFILE).read().split("\n")
    ln = next(i for i, l in enumerate(src) if l == RET)
    for depth in DEPTHS:
        # baseline: original at this depth
        open("/app/Syzygy_Zopfli/c_code/_mgcs.c", "w").write("\n".join(src))
        print(f"depth {depth}: original -> {run('/app/Syzygy_Zopfli/c_code/_mgcs.c','orig_'+str(depth),depth)}")
        for name, newline in muts.items():
            ml = list(src); ml[ln] = newline
            open("/app/Syzygy_Zopfli/c_code/_mgcs.c", "w").write("\n".join(ml))
            print(f"depth {depth}:   {name} -> {run('/app/Syzygy_Zopfli/c_code/_mgcs.c', name+'_'+str(depth), depth)}")
    os.path.exists("/app/Syzygy_Zopfli/c_code/_mgcs.c") and os.remove("/app/Syzygy_Zopfli/c_code/_mgcs.c")

if __name__ == "__main__":
    main()
