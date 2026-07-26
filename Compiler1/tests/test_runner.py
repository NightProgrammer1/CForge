import os
import subprocess
import sys

COMPILER_BIN = "./build/Compiler1"
TEST_DIR = "Compiler1/tests"

# Expected exit codes for each test file
TEST_EXPECTATIONS = {
    "01_folding.cpp": 15,
    "02_folding.cpp": 15
}

GREEN = "\033[92m"
RED = "\033[91m"
RESET = "\033[0m"

def run_suite():
    if not os.path.exists(COMPILER_BIN):
        print(f"{RED}Error: Compiler binary '{COMPILER_BIN}' not found!{RESET}")
        sys.exit(1)

    test_files = sorted([f for f in os.listdir(TEST_DIR) if f.endswith(".cpp")])
    passed = 0
    failed = 0

    print("==================================================")
    print("   RUNNING COMPILER TEST SUITE (ASM & LINK)")
    print("==================================================\n")

    for file_name in test_files:
        c_file_path = os.path.join(TEST_DIR, file_name)
        asm_out_path = os.path.join(TEST_DIR, file_name.replace(".c", ".s"))
        bin_out_path = os.path.join(TEST_DIR, file_name.replace(".c", ".exe"))

        try:
            comp_res = subprocess.run([COMPILER_BIN, c_file_path, "-o", asm_out_path], capture_output=True, text=True)
            if comp_res.returncode != 0:
                print(f"[{RED}FAIL{RESET}] {file_name} -> Your compiler failed!")
                print(f"       stderr: {comp_res.stderr.strip()}")
                failed += 1
                continue

            gcc_res = subprocess.run(["gcc", asm_out_path, "-o", bin_out_path], capture_output=True, text=True)
            if gcc_res.returncode != 0:
                print(f"[{RED}FAIL{RESET}] {file_name} -> Assembler/Linker (gcc) failed on generated .s code!")
                print(f"       stderr: {gcc_res.stderr.strip()}")
                failed += 1
                continue

            exec_res = subprocess.run([f"./{bin_out_path}"])
            actual_exit = exec_res.returncode
            expected_exit = TEST_EXPECTATIONS.get(file_name, 0)

            # Clean up generated artifacts
            if os.path.exists(asm_out_path): os.remove(asm_out_path)
            if os.path.exists(bin_out_path): os.remove(bin_out_path)

            if actual_exit == expected_exit:
                print(f"[{GREEN}PASS{RESET}] {file_name:<25} (Exit code: {actual_exit})")
                passed += 1
            else:
                print(f"[{RED}FAIL{RESET}] {file_name:<25} (Expected {expected_exit}, got {actual_exit})")
                failed += 1

        except Exception as e:
            print(f"[{RED}FAIL{RESET}] {file_name} -> Test script error: {e}")
            failed += 1

    print("\n--------------------------------------------------")
    print(f"Results: {GREEN}{passed} Passed{RESET}, {RED if failed else RESET}{failed} Failed{RESET}")
    print("--------------------------------------------------")

    if failed > 0:
        sys.exit(1)

if __name__ == "__main__":
    run_suite()
