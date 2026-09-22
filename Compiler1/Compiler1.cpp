// Compiler1.cpp : Defines the entry point for the application.
//
#include "Compiler1.h"



int main(int argc, char* argv[])
{
	
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <source_file.cpp> [output_file.asm]\n";
		return EXIT_FAILURE;
	}

	std::string inputPath = argv[1];

	std::ifstream input(inputPath);
	if (!input.is_open()) {
		std::cerr << "Error: Could not open input file: " << inputPath << "\n";
		return EXIT_FAILURE;
	}

	std::stringstream buffer;
	buffer << input.rdbuf();
	input.close();


	std::string content = buffer.str();
    Tokenizer tokenizer(content);

	std::vector<Token> tokens = tokenizer.tokenize(content);


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


	std::string outputPath = (argc >= 3) ? argv[2] : "out.s";
	std::ofstream out(outputPath);
	if (!out.is_open()) {
		std::cerr << "Error: Could not open output file: " << outputPath << "\n";
		return EXIT_FAILURE;
	}
	out << assembly;
	std::cout << assembly << std::endl;
	out.close();

	

	return 0;
}

