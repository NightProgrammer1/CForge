// Compiler1.cpp : Defines the entry point for the application.
//

#include "Compiler1.h"
#include "tokenization.h"
#include "Generator.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "SemanticAnalyzer.h"
#include "IRLowering.h"
#include "ThreeAdressCode.h"
#include "BasicBlockBuilder.h"
#include "LivenessAnalysis.h"
#include "PreMachineInstruction.h"
#include "PostMachineInstruction.h"
#include "debug.h"



int Compile(int argc, char* argv[], NodeProg& exitNode, std::string_view assembly)
{
	bool print_ast = false;
	bool print_assembly = false;
	std::string input_file = "";
	std::string output_file = "";

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--print-ast") {
			print_ast = true;
		}
		else if (arg == "--print-assembly") {
			print_assembly = true;
		}
		else if (arg == "-o" && i + 1 < argc) {
			output_file = argv[++i]; 
		}
		else if (input_file.empty()) {
			input_file = arg; 
		}
	}

	if (print_ast) {
		PrintAST(exitNode);
		std::cout << std::endl;
	}
	if (print_assembly) {
		std::cout << assembly << std::endl;
	}

	if (!output_file.empty()) {
		// Write assembly to the specified output file
		std::string outputPath = output_file + ".asm";
		std::ofstream out(outputPath);
		if (!out.is_open()) {
			std::cerr << "Error: Could not open output file: " << outputPath << "\n";
			exit(EXIT_FAILURE);
		}
		out << assembly;
		out.close();
	}
	else {
		std::string outputPath = input_file.substr(0, input_file.find_last_of('.')) + ".asm";
		std::ofstream out(outputPath);
		if (!out.is_open()) {
			std::cerr << "Error: Could not open output file: " << outputPath << "\n";
			exit(EXIT_FAILURE);
		}
		out << assembly;
		out.close();
	}

	return 0;
}

int main(int argc, char* argv[])
{
	
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <source_file.cpp> [output_file.asm]\n";
		exit(EXIT_FAILURE);
	}

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


	std::pmr::monotonic_buffer_resource arena((1024 * 1024) * 4);

	std::pmr::polymorphic_allocator<NodeExpr> alloc(&arena);
	std::pmr::polymorphic_allocator<Instruction> allocTAC(&arena);


	Parser parser(tokens, alloc);

	auto exitNode = parser.ParseProgram();

	SemanticAnalyzer analyzer(exitNode.value(), alloc);
	analyzer.Analyze();


//	auto folder = ConstantFolder(exitNode.value(), alloc);
	
	IRLowering lowerer(exitNode.value(), alloc);
	auto ir = lowerer.Lower();

	TACLowering tac(ir, allocTAC);

	auto t = tac.Lower();

	BasicBlockBuilder builder(t, arena);
	auto cfg = builder.Create();


	PreMachineIR machine(cfg);
	auto machIns = machine.Lower();

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

	X86Generator gen(machIns);

	
	auto assembly = gen.GenerateProgram();


	return Compile(argc, argv, exitNode.value(), assembly);
	

	return 0;
}

