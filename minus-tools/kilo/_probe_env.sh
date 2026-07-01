#!/bin/bash
# Probe the environment to find the canonical run_cbmc and its callee-replacement policy.
echo "=== which run_cbmc ==="
which run_cbmc 2>&1
echo "=== find run_cbmc ==="
find / -name 'run_cbmc*' 2>/dev/null
echo "=== avocado_verify location ==="
find / -name 'avocado_verify.py' 2>/dev/null
echo "=== tools dir ==="
ls -la /app/tools 2>&1
ls -la /app/eval 2>&1
ls -la /app/stubs 2>&1
