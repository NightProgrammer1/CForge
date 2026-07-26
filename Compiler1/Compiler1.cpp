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
#include "Constant Folder.h"


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



	std::pmr::monotonic_buffer_resource arena(1024 * 1024);

	// 2. Allocate AST nodes using pmr allocator
	std::pmr::polymorphic_allocator<NodeExpr> alloc(&arena);

	Parser parser(tokens, alloc);

	auto exitNode = parser.ParseProgram();

	SemanticAnalyzer analyzer(exitNode.value());
	analyzer.Analyze();


	auto folder = ConstantFolder(exitNode.value(), alloc);
	

	folder.FoldProgram();
	Generator gen(std::move(exitNode.value()));

	
	auto assembly = gen.GenerateProgram();


	std::string outputPath = (argc >= 3) ? argv[2] : "out.s";
	std::ofstream out(outputPath);
	if (!out.is_open()) {
		std::cerr << "Error: Could not open output file: " << outputPath << "\n";
		return EXIT_FAILURE;
	}
	out << assembly;

	out.close();

	return 0;
}
