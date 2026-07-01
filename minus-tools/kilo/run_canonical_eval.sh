#!/bin/bash
# Wrapper to run the canonical (tools.run_cbmc) mutation evaluation for editorUpdateRow.
# Usage: ./run_canonical_eval.sh [--orig]
cd /app/kilo
PYTHONPATH=/app /app/.venv/bin/python canonical_eval_editorUpdateRow.py "$@"
