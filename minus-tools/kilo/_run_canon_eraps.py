import sys
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

res = run_cbmc(
    function_to_verify="editorRowAppendString",
    file_containing_function_to_verify="/app/kilo/kilo.c",
)
print("VERIFIED:", res.is_function_verified)
print("STR:", str(res))
print("RC:", res.returncode, "failed_step:", res.failed_step)
print("----RESPONSE (head)----")
print(res.response[:6000])
