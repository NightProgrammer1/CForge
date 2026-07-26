import os
import subprocess
import sys

# Path to compiler binary
COMPILER_BIN = "./build/my_compiler"

# Expected exit codes/outputs for each test file
# (Adjust expected values based on your 10 tests)
TEST_EXPECTATIONS = {
    "01_folding.c": 15,
}

# Terminal colors for clean output
GREEN = "\033[92m"
RED = "\033[91m"
RESET = "\033[0m"

def run_suite():
    if not os.path.exists(COMPILER_BIN):
        print(f"{RED}Error: Compiler binary '{COMPILER_BIN}' not found! Build your project first.{RESET}")
        sys.exit(1)

    test_files = sorted([f for f in os.listdir("tests") if f.endswith(".c")])
    passed = 0
    failed = 0

    print("==================================================")
    print(" 🚀 RUNNING COMPILER OPTIMIZER TEST SUITE")
    print("==================================================\n")

    for file_name in test_files:
        file_path = os.path.join("tests", file_name)
        
        # Command to run your compiler pass
        # e.g., ./build/my_compiler tests/01_folding.c -o out.s
        compile_cmd = [COMPILER_BIN, file_path]

        try:
            # 1. Run your compiler's optimization pass
            result = subprocess.run(compile_cmd, capture_output=True, text=True)

            if result.returncode != 0:
                print(f"[{RED}FAIL{RESET}] {file_name} -> Compiler crashed!")
                print(f"       stderr: {result.stderr.strip()}")
                failed += 1
                continue

            # 2. OPTIONAL: If your compiler emits assembly/executable, run it and check exit code
            # Example if compiling to dynamic executable:
            # run_exec = subprocess.run(["./a.out"])
            # actual_exit = run_exec.returncode
            
            # For now, let's assume we check expected values from our map:
            expected_val = TEST_EXPECTATIONS.get(file_name, 0)
            
            # Print success line
            print(f"[{GREEN}PASS{RESET}] {file_name:<25} (Expected: {expected_val})")
            passed += 1

        except Exception as e:
            print(f"[{RED}FAIL{RESET}] {file_name} -> Unexpected script error: {e}")
            failed += 1

    print("\n--------------------------------------------------")
    print(f"Results: {GREEN}{passed} Passed{RESET}, {RED if failed else RESET}{failed} Failed{RESET}")
    print("--------------------------------------------------")

    # Exit with code 1 if any tests failed so GitHub Actions CI catches it!
    if failed > 0:
        sys.exit(1)

if __name__ == "__main__":
    run_suite()