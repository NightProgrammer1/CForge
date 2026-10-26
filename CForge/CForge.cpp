#include "CForge.h"
#include <cstdio>
#include <array>
#include <fstream>
#include <sstream>

std::string executeCommand(const std::string& cmd, int& exitCode) {
	std::string output = "";
	std::array<char, 128> buffer;

	std::string fullCmd = cmd + " 2>&1";

	FILE* pipe = popen(fullCmd.c_str(), "r");
	if (!pipe) {
		exitCode = -1;
		return "Error: Failed to create pipe.";
	}

	while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
		output += buffer.data();
	}

	int status = pclose(pipe);
	exitCode = WEXITSTATUS(status);

	return output;
}

int Compile(int argc, char* argv[], std::string_view assembly)
{
	bool print_assembly = false;
	std::string input_file;
	std::string custom_output_file;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--print-assembly") {
			print_assembly = true;
		}
		else if (arg == "-o" && i + 1 < argc) {
			custom_output_file = argv[++i];
		}
		else if (input_file.empty() && arg[0] != '-') {
			input_file = arg;
		}
	}

	std::string baseName = "output";
	if (!input_file.empty()) {
		size_t lastdot = input_file.find_last_of('.');
		baseName = (lastdot == std::string::npos) ? input_file : input_file.substr(0, lastdot);
	}

	std::string outputPath = baseName + ".asm";
	std::string objPath = baseName + ".o";

	std::string exePath;
	if (!custom_output_file.empty()) {
		exePath = custom_output_file;
	}
	else {
		exePath = baseName + ".out";
	}

	if (print_assembly) {
		std::cout << assembly << std::endl;
		std::ofstream out(outputPath);
		out << assembly;
		return 0;
	}

	{
		std::ofstream out(outputPath);
		if (!out.is_open()) {
			return -1;
		}
		out << assembly;
	}

	std::string nasmCommand = "nasm -f " + std::string(NASM_FORMAT) + " " + outputPath + " -o " + objPath;
	int nasmResult = 0;
	std::string nasmOutput = executeCommand(nasmCommand, nasmResult);

	if (nasmResult != 0) {
		std::remove(outputPath.c_str());
		return nasmResult;
	}

	std::string linkCommand = "ld " + objPath + " -o " + exePath;
	int linkResult = 0;
	std::string linkOutput = executeCommand(linkCommand, linkResult);

	if (linkResult != 0) {
		std::remove(outputPath.c_str());
		std::remove(objPath.c_str());
		return linkResult;
	}

	std::remove(outputPath.c_str());
	std::remove(objPath.c_str());

	return 0;
}
int main(int argc, char* argv[])
{
	if (argc < 2) {
		std::cerr << "CForge C Compiler\n\n"
			<< "Usage: cf <source_file> [options]\n\n"
			<< "Options:\n"
			<< "  -o <file>          Specify the output binary filename\n"
			<< "  -g, --debug        Enable compiler performance profiling & timers\n"
			<< "  --print-ast        Print the Abstract Syntax Tree (AST) and exit\n"
			<< "  --print-assembly   Print the generated x86_64 assembly code\n"
			<< "  -O0                Disable optimization passes\n";
		exit(EXIT_FAILURE);
	}

	bool enable_profiling = false;
	bool enable_optimization = true;

	// Scan arguments for debug/optimization flags
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "-g" || arg == "--debug") {
			enable_profiling = true;
		}
		else if (arg == "-O0") {
			enable_optimization = false; 
		}
	}

	ScopedTimer totalTimer("Total Compilation", enable_profiling);

	std::string inputPath = argv[1];

	std::ifstream input(inputPath);
	if (!input.is_open()) {
		std::cerr << "Error: Could not open input file: " << inputPath << "\n";
		exit(EXIT_FAILURE);
	}

	std::stringstream buffer;
	buffer << input.rdbuf();
	input.close();

	std::string content = buffer.str();
	std::vector<Token> tokens = Tokenizer::tokenize(content);

	std::pmr::monotonic_buffer_resource arena((1024 * 1024) * 4); // 4 MB
	std::pmr::polymorphic_allocator<char> genericAlloc(&arena);

	Parser parser(tokens, genericAlloc);
	auto exitNode = parser.ParseProgram();
	if (!exitNode.has_value()) {
		std::cerr << "Error: Parsing failed.\n";
		exit(EXIT_FAILURE);
	}

	// --- SEMANTIC ANALYSIS ---
	{
		ScopedTimer t("Semantic Analysis", enable_profiling);
		SemanticAnalyzer analyzer(exitNode.value(), genericAlloc);
		analyzer.Analyze();
	}

	// --- CHECK FOR --print-ast ---
	for (int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "--print-ast") {
			PrintAST(exitNode.value());
			std::cout << std::endl;
			return 0;
		}
	}

	MachineProgram machIns;
	{
		ScopedTimer t("Constant Folding & Lowering", enable_profiling);

		// --- CONSTANT FOLDER / OPTIMIZATIONS ---
		if (enable_optimization) {
			auto folder = ConstantFolder(exitNode.value(), genericAlloc);
			folder.FoldProgram();
		}

		// --- IR LOWERING & CODEGEN ---
		IRLowering lowerer(exitNode.value(), genericAlloc);
		auto ir = lowerer.Lower();

		TACLowering tac(ir, genericAlloc);
		auto tacInstructions = tac.Lower();

		BasicBlockBuilder builder(tacInstructions, arena);
		auto cfg = builder.Create();

		PreMachineIR machine(cfg);
		machIns = machine.Lower();
	}

	{
		ScopedTimer t("Liveness Analysis & Register Allocation", enable_profiling);
		LivenessAnalysis analysis;
		for (auto& function : machIns.functions)
		{
			RegisterAllocator reg;
			auto liveInt = analysis.GetLiveIntervals(function);
			auto data = reg.AllocateReg(liveInt, function);

			function.stack_size = data.first;
			function.usedCalleeSaved = data.second;
		}

		PostMachineIR postMachIns(machIns);
		postMachIns.Lower();
	}

	std::string assembly;
	{
		ScopedTimer t("Assembly Generation", enable_profiling);
		X86Generator gen(machIns);
		assembly = gen.GenerateProgram();
	}

	return Compile(argc, argv, assembly);
}